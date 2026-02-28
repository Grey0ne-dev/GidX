#include "gidx/tokenizer.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace gidx {

static const std::unordered_set<std::string> STOP_WORDS = {
    "a", "an", "the", "is", "are", "was", "were", "be", "been", "being",
    "have", "has", "had", "do", "does", "did", "will", "would", "shall",
    "should", "may", "might", "must", "can", "could", "am", "it", "its",
    "in", "on", "at", "to", "for", "of", "with", "by", "from", "as",
    "into", "through", "during", "before", "after", "above", "below",
    "between", "out", "off", "over", "under", "again", "further", "then",
    "once", "here", "there", "when", "where", "why", "how", "all", "both",
    "each", "few", "more", "most", "other", "some", "such", "no", "nor",
    "not", "only", "own", "same", "so", "than", "too", "very", "just",
    "because", "but", "and", "or", "if", "while", "about", "up", "that",
    "this", "these", "those", "he", "she", "they", "we", "you", "i", "me",
    "him", "her", "us", "them", "my", "your", "his", "our", "their", "what",
    "which", "who", "whom"
};

std::string strip_html(const std::string& html) {
    std::string result;
    result.reserve(html.size());
    bool in_tag = false;
    bool in_script = false;

    for (size_t i = 0; i < html.size(); ++i) {
        if (!in_tag && html[i] == '<') {
            if (i + 7 < html.size()) {
                std::string tag_start = html.substr(i + 1, 6);
                std::transform(tag_start.begin(), tag_start.end(), tag_start.begin(), ::tolower);
                if (tag_start == "script" || tag_start.substr(0, 5) == "style") {
                    in_script = true;
                }
            }
            
            if (i + 2 < html.size() && html[i + 1] == '/') {
                std::string close_tag = html.substr(i + 2, 6);
                std::transform(close_tag.begin(), close_tag.end(), close_tag.begin(), ::tolower);
                if (close_tag.substr(0, 6) == "script" || close_tag.substr(0, 5) == "style") {
                    in_script = false;
                }
            }
            in_tag = true;
        } else if (in_tag && html[i] == '>') {
            in_tag = false;
            result += ' ';
        } else if (!in_tag && !in_script) {
            result += html[i];
        }
    }
    return result;
}

std::string to_lower(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool is_stop_word(const std::string& word) {
    return STOP_WORDS.count(word) > 0;
}

std::vector<std::string> tokenize(const std::string& html) {
    std::string text = strip_html(html);
    text = to_lower(text);

    std::vector<std::string> tokens;
    std::string current;

    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            current += c;
        } else {
            if (!current.empty()) {
                if (!is_stop_word(current)) {
                    tokens.push_back(std::move(current));
                }
                current.clear();
            }
        }
    }
    if (!current.empty() && !is_stop_word(current)) {
        tokens.push_back(std::move(current));
    }

    return tokens;
}

} // namespace gidx
