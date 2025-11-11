#include "bloa/interpreter.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout << "Usage: bloa <script.bloa>\n";
        return 1;
    }
    std::string path = argv[1];
    std::ifstream ifs(path);
    if (!ifs) {
        std::cerr << "Unable to open file: " << path << std::endl;
        return 1;
    }
    std::string src((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    bloa::Interpreter interp("."); // stdlib path = current directory
    interp.run(src, path);
    return 0;
}
