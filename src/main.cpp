#include "bloa/interpreter.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

#define BLOA_VERSION "0.1.0-alpha"

void print_help() {
    std::cout <<
        "BLOA — Lightweight scripting language\n"
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

    bloa::Interpreter interp(".");
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

    // Menjalankan file .bloa
    std::ifstream ifs(arg);
    if (!ifs) {
        std::cerr << "Unable to open file: " << arg << std::endl;
        return 1;
    }

    std::string src((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    bloa::Interpreter interp(".");

    try {
        interp.run(src, arg);
    } catch (const std::exception &e) {
        std::cerr << "[Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}