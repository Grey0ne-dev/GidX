#include "gidx/crawl_service.h"
#include "gidx/tokenizer.h"
#include "crawl.grpc.pb.h"
#include "crawl.pb.h"

#include <grpcpp/grpcpp.h>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

namespace gidx {

// --- Worker (Server) ---

class CrawlServiceImpl final : public gidx::CrawlService::Service {
public:
    grpc::Status Crawl(grpc::ServerContext* /*context*/,
                       const gidx::CrawlRequest* request,
                       gidx::CrawlResponse* response) override {
        response->set_doc_id(request->doc_id());
        response->set_url(request->url());

        try {
            // For now, use a simple curl-like fetch (placeholder)
            // In production this would use libcurl or asio
            std::string html = fetch_url(request->url());

            if (html.empty()) {
                response->set_success(false);
                response->set_error("Empty response");
                return grpc::Status::OK;
            }

            // Tokenize
            auto tokens = tokenize(html);
            for (const auto& t : tokens) {
                response->add_tokens(t);
            }

            // Raw text for snippets
            response->set_raw_text(strip_html(html));

            // Extract links
            auto links = extract_links(html, request->url());
            for (const auto& l : links) {
                response->add_discovered_urls(l);
            }

            response->set_success(true);
        } catch (const std::exception& e) {
            response->set_success(false);
            response->set_error(e.what());
        }

        return grpc::Status::OK;
    }

    grpc::Status ReportStatus(grpc::ServerContext* /*context*/,
                              const gidx::StatusRequest* /*request*/,
                              gidx::StatusResponse* response) override {
        response->set_pages_crawled(pages_crawled_);
        response->set_pages_failed(pages_failed_);
        return grpc::Status::OK;
    }

private:
    uint32_t pages_crawled_ = 0;
    uint32_t pages_failed_ = 0;

    // Simple URL fetch using curl command
    static std::string fetch_url(const std::string& url) {
        // Use popen for simplicity; production would use libcurl
        std::string cmd = "curl -sL --max-time 10 --max-filesize 1048576 " + url + " 2>/dev/null";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";

        std::string result;
        char buf[4096];
        while (fgets(buf, sizeof(buf), pipe)) {
            result += buf;
        }
        pclose(pipe);
        return result;
    }

    // Extract href links from HTML
    static std::vector<std::string> extract_links(const std::string& html, const std::string& base_url) {
        std::vector<std::string> links;
        // Simple regex for href attributes
        std::regex href_re(R"(href\s*=\s*["']([^"']+)["'])", std::regex::icase);
        auto begin = std::sregex_iterator(html.begin(), html.end(), href_re);
        auto end = std::sregex_iterator();

        std::string base_domain;
        size_t proto_end = base_url.find("://");
        if (proto_end != std::string::npos) {
            size_t slash = base_url.find('/', proto_end + 3);
            base_domain = base_url.substr(0, slash != std::string::npos ? slash : base_url.size());
        }

        for (auto it = begin; it != end; ++it) {
            std::string link = (*it)[1].str();
            // Resolve relative URLs
            if (link.starts_with("//")) {
                link = "https:" + link;
            } else if (link.starts_with("/")) {
                link = base_domain + link;
            } else if (!link.starts_with("http")) {
                continue; // skip mailto:, javascript:, etc.
            }
            links.push_back(link);
        }
        return links;
    }
};

// Worker pimpl
struct CrawlWorker::Impl {
    std::string listen_addr;
    std::unique_ptr<grpc::Server> server;
    CrawlServiceImpl service;
};

CrawlWorker::CrawlWorker(const std::string& listen_addr) : impl_(std::make_unique<Impl>()) {
    impl_->listen_addr = listen_addr;
}

CrawlWorker::~CrawlWorker() { shutdown(); }

void CrawlWorker::run() {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(impl_->listen_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&impl_->service);
    impl_->server = builder.BuildAndStart();
    std::cout << "[Worker] Listening on " << impl_->listen_addr << std::endl;
    impl_->server->Wait();
}

void CrawlWorker::shutdown() {
    if (impl_ && impl_->server) {
        impl_->server->Shutdown();
    }
}

// --- Master (Client) ---

struct CrawlMaster::Impl {
    struct WorkerChannel {
        std::string address;
        std::unique_ptr<gidx::CrawlService::Stub> stub;
    };
    std::vector<WorkerChannel> workers;
    size_t next_worker = 0;
};

CrawlMaster::CrawlMaster() : impl_(std::make_unique<Impl>()) {}
CrawlMaster::~CrawlMaster() = default;

void CrawlMaster::add_worker(const std::string& address) {
    auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    impl_->workers.push_back({address, gidx::CrawlService::NewStub(channel)});
}

CrawlResult CrawlMaster::crawl(const std::string& url, uint32_t doc_id) {
    if (impl_->workers.empty()) {
        return {doc_id, url, {}, "", false, "No workers available", {}};
    }

    // Round-robin worker selection
    auto& worker = impl_->workers[impl_->next_worker % impl_->workers.size()];
    impl_->next_worker++;

    gidx::CrawlRequest request;
    request.set_url(url);
    request.set_doc_id(doc_id);

    gidx::CrawlResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(15));

    auto status = worker.stub->Crawl(&context, request, &response);

    CrawlResult result;
    result.doc_id = response.doc_id();
    result.url = response.url();
    result.raw_text = response.raw_text();

    if (status.ok() && response.success()) {
        result.success = true;
        result.tokens.assign(response.tokens().begin(), response.tokens().end());
        result.discovered_urls.assign(response.discovered_urls().begin(), response.discovered_urls().end());
    } else {
        result.success = false;
        result.error = status.ok() ? response.error() : status.error_message();
    }

    return result;
}

} // namespace gidx
