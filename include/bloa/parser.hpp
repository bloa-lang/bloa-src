#pragma once
#include "bloa/ast.hpp"
#include <string>
#include <vector>

namespace bloa {

std::vector<std::string> split_lines(const std::string &code);
int indent_level(const std::string &line);

// ParseError carries a message and location (line, column)
struct ParseError : public std::runtime_error {
    int line;
    int col;
    ParseError(const std::string &msg, int line_, int col_)
            : std::runtime_error(msg), line(line_), col(col_) {}
};

// parse_block returns pair: NodeList and next index
std::pair<NodeList, int> parse_block(const std::vector<std::string> &lines,
                                     int start_idx = 0, int base_indent = 0);

} // namespace bloa
