#pragma once
#include <utility>
#include <vector>

#include "bloa/env.hpp"

namespace bloa {

void register_stdlib(std::shared_ptr<Environment> env);
Value handle_builtin(const std::string &marker, const std::vector<Value> &args,
                     std::shared_ptr<Environment> env = nullptr);
std::vector<std::pair<std::string, std::string>> read_archive(
    const std::string &path);
void write_archive(
    const std::string &path,
    const std::vector<std::pair<std::string, std::string>> &entries);

}  // namespace bloa