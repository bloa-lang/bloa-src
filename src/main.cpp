#include <fstream>
#include <iostream>
#include <sstream>

#include "bloa/interpreter.hpp"
#include "bloa/stdlib.hpp"

#define BLOA_VERSION "1.0.0-RC1"

void print_help() {
  std::cout << "BLOA — Lightweight scripting language\n"
               "Usage:\n"
               "  bloa <script.bloa>     Run a BLOA script\n"
               "  bloa                   Start interactive REPL mode\n"
               "  bloa --version, -v     Show version information\n"
               "  bloa --help, -h        Show this help message\n"
               "\n"
               "Examples:\n"
               "  bloa demo.bloa\n"
               "  bloa --version\n"
               "  bloa\n";
}

void start_repl() {
  std::cout << "BLOA " << BLOA_VERSION << " Interactive Mode\n";
  std::cout << "Type 'exit' or press Ctrl+D to quit.\n";

  bloa::Interpreter interp("");
  std::string line;

  while (true) {
    std::cout << "bloa> ";
    if (!std::getline(std::cin, line)) break;
    if (line == "exit") break;

    try {
      interp.run(line, "<repl>");
    } catch (const std::exception &e) {
      std::cerr << "[Error] " << e.what() << std::endl;
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    start_repl();
    return 0;
  }

  std::string arg = argv[1];

  if (arg == "--version" || arg == "-v") {
    std::cout << "BLOA version " << BLOA_VERSION << std::endl;
    return 0;
  }

  if (arg == "--help" || arg == "-h") {
    print_help();
    return 0;
  }

  std::string src;
  if (arg.size() >= 5 && arg.substr(arg.size() - 5) == ".baar") {
    try {
      auto entries = bloa::read_archive(arg);
      std::string code;
      for (const auto &entry : entries) {
        if (entry.first == "main.bloa" || entry.first == "index.bloa") {
          code = entry.second;
          break;
        }
      }
      if (code.empty()) {
        for (const auto &entry : entries) {
          if (entry.first.size() >= 5 && entry.first.substr(entry.first.size() - 5) == ".bloa") {
            code = entry.second;
            break;
          }
        }
      }
      if (code.empty() && !entries.empty()) code = entries[0].second;
      if (code.empty()) {
        std::cerr << "Baar archive contains no executable entry: " << arg << std::endl;
        return 1;
      }
      src = std::move(code);
    } catch (const std::exception &e) {
      std::cerr << "[BLOA Error] " << e.what() << "\n";
      std::cerr << "  File: " << arg << "\n";
      return 1;
    }
  } else {
    std::ifstream ifs(arg);
    if (!ifs) {
      std::cerr << "Unable to open file: " << arg << std::endl;
      return 1;
    }

    src.assign((std::istreambuf_iterator<char>(ifs)),
               std::istreambuf_iterator<char>());
  }

  bloa::Interpreter interp("");

  try {
    interp.run(src, arg);
  } catch (const std::exception &e) {
    std::cerr << "[Error] " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
