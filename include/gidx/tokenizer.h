#pragma once

#include <string>
#include <vector>

namespace gidx {

// Strip HTML tags from raw content
std::string strip_html(const std::string& html);

// Lowercase the entire string
std::string to_lower(const std::string& text);

// Check if a word is a stop word
bool is_stop_word(const std::string& word);

// Full pipeline: strip HTML -> split into words -> normalize -> remove stop words
std::vector<std::string> tokenize(const std::string& html);

} // namespace gidx
