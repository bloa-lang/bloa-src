#pragma once
#include "bloa/ast.hpp"
#include <string>
#include <vector>

namespace bloa {

std::vector<std::string> split_lines(const std::string &code);
int indent_level(const std::string &line);

// parse_block returns pair: NodeList and next index
std::pair<NodeList,int> parse_block(const std::vector<std::string> &lines, int start_idx = 0, int base_indent = 0);

} // namespace bloa
