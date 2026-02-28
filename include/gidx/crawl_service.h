#pragma once

#include <memory>
#include <string>
#include <functional>

namespace gidx {

struct CrawlResult {
    uint32_t doc_id;
    std::string url;
    std::vector<std::string> tokens;
    std::string raw_text;
    bool success;
    std::string error;
    std::vector<std::string> discovered_urls;
};

using CrawlCallback = std::function<void(const CrawlResult&)>;

class CrawlWorker {
public:
    explicit CrawlWorker(const std::string& listen_addr = "0.0.0.0:50051");
    ~CrawlWorker();

    void run();       // blocking
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class CrawlMaster {
public:
    CrawlMaster();
    ~CrawlMaster();

    // Add a worker endpoint
    void add_worker(const std::string& address);

    // Send a URL to a worker and get results
    CrawlResult crawl(const std::string& url, uint32_t doc_id);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gidx
