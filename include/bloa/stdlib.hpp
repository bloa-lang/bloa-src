#pragma once
#include "bloa/env.hpp"

namespace bloa {

void register_stdlib(std::shared_ptr<Environment> env);
Value handle_builtin(const std::string &marker, const std::vector<Value> &args);

}  // namespace bloa