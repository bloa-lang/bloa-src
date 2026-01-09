#include "bloa/parser.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace bloa {

std::vector<std::string> split_lines(const std::string &code) {
    std::vector<std::string> out;
    std::string line;
    std::istringstream iss(code);
    while (std::getline(iss, line)) {
        out.push_back(line);
    }
    return out;
}

int indent_level(const std::string &line) {
    int count = 0;
    for (char ch : line) {
        if (ch == ' ') count++;
        else if (ch == '\t') count += 4;
        else break;
    }
    return count;
}

static bool starts_with(const std::string &s, const std::string &p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

static std::string ltrim(const std::string &s) {
    size_t pos = s.find_first_not_of(" \t\r\n");
    if (pos == std::string::npos) return "";
    return s.substr(pos);
}

static std::string rtrim(const std::string &s) {
    size_t pos = s.find_last_not_of(" \t\r\n");
    if (pos == std::string::npos) return "";
    return s.substr(0, pos + 1);
}

std::pair<NodeList,int> parse_block(
    const std::vector<std::string> &lines,
    int start_idx,
    int base_indent
) {
    int idx = start_idx;
    NodeList nodes;

    while (idx < (int)lines.size()) {
        const std::string raw_line = lines[idx];
        std::string line = ltrim(rtrim(raw_line));

        if (line.empty() || line.rfind("#", 0) == 0) {
            idx++;
            continue;
        }

        int indent = indent_level(raw_line);
        if (indent < base_indent) break;
        if (indent > base_indent)
            throw std::runtime_error("Unexpected indent at line " + std::to_string(idx + 1));

        /* block end */
        if (line == "}") {
            idx++;
            break;
        }

        /* say */
        if (starts_with(line, "say ")) {
            nodes.push_back(std::make_shared<Say>(line.substr(4)));
            idx++;
            continue;
        }

        /* ask */
        if (starts_with(line, "ask ")) {
            auto pos = line.find("->");
            if (pos == std::string::npos)
                throw std::runtime_error("Invalid ask syntax at line " + std::to_string(idx + 1));
            nodes.push_back(std::make_shared<Ask>(
                ltrim(line.substr(4, pos - 4)),
                ltrim(line.substr(pos + 2))
            ));
            idx++;
            continue;
        }

        /* use */
        if (starts_with(line, "use ") && line.back() == ';') {
            std::string mod = line.substr(4, line.size() - 5);
            nodes.push_back(std::make_shared<Import>(mod));
            idx++;
            continue;
        }

        /* require */
        if (starts_with(line, "require ")) {
            std::string path = line.substr(8);
            if (!path.empty() && path.back() == ';')
                path.pop_back();
            nodes.push_back(std::make_shared<Require>(path));
            idx++;
            continue;
        }

        /* return */
        if (line == "return;") {
            nodes.push_back(std::make_shared<Return>(std::optional<std::string>{}));
            idx++;
            continue;
        }
        if (starts_with(line, "return ")) {
            std::string expr = line.substr(7);
            if (!expr.empty() && expr.back() == ';')
                expr.pop_back();
            nodes.push_back(std::make_shared<Return>(expr));
            idx++;
            continue;
        }

        /* if */
        if (starts_with(line, "if (") && line.back() == '{') {
            std::string cond = line.substr(4, line.size() - 6);
            auto [then_block, next] = parse_block(lines, idx + 1, base_indent);
            NodeList else_block;

            if (next < (int)lines.size()) {
                std::string next_line = ltrim(rtrim(lines[next]));
                if (next_line == "else {") {
                    auto res = parse_block(lines, next + 1, base_indent);
                    else_block = res.first;
                    next = res.second;
                }
            }

            nodes.push_back(std::make_shared<If>(cond, then_block, else_block));
            idx = next;
            continue;
        }

        /* while */
        if (starts_with(line, "while (") && line.back() == '{') {
            std::string cond = line.substr(7, line.size() - 9);
            auto res = parse_block(lines, idx + 1, base_indent);
            nodes.push_back(std::make_shared<While>(cond, res.first));
            idx = res.second;
            continue;
        }

        /* foreach */
        if (starts_with(line, "foreach (") && line.back() == '{') {
            std::string header = line.substr(9, line.size() - 11);
            auto pos = header.find(" as ");
            if (pos == std::string::npos)
                throw std::runtime_error("Invalid foreach syntax at line " + std::to_string(idx + 1));
            std::string iterable = header.substr(0, pos);
            std::string var = header.substr(pos + 4);
            auto res = parse_block(lines, idx + 1, base_indent);
            nodes.push_back(std::make_shared<ForIn>(var, iterable, res.first));
            idx = res.second;
            continue;
        }

        /* function */
        if (starts_with(line, "function ") && line.back() == '{') {
            std::string header = line.substr(9, line.size() - 10);
            auto pos = header.find('(');
            if (pos == std::string::npos)
                throw std::runtime_error("Invalid function syntax at line " + std::to_string(idx + 1));

            std::string name = header.substr(0, pos);
            std::string params_raw = header.substr(pos + 1, header.size() - pos - 2);

            std::vector<std::string> params;
            std::istringstream iss(params_raw);
            std::string tok;
            while (std::getline(iss, tok, ',')) {
                tok = ltrim(rtrim(tok));
                if (!tok.empty()) params.push_back(tok);
            }

            auto res = parse_block(lines, idx + 1, base_indent);
            nodes.push_back(std::make_shared<FunctionDef>(name, params, res.first));
            idx = res.second;
            continue;
        }

        /* class */
        if (starts_with(line, "class ") && line.back() == '{') {
            std::string name = line.substr(6, line.size() - 7);
            auto res = parse_block(lines, idx + 1, base_indent);
            nodes.push_back(std::make_shared<ClassDef>(name, res.first));
            idx = res.second;
            continue;
        }

        /* assignment */
        if (line.find('=') != std::string::npos && line.find("==") == std::string::npos) {
            auto pos = line.find('=');
            std::string left = ltrim(rtrim(line.substr(0, pos)));
            std::string right = ltrim(rtrim(line.substr(pos + 1)));
            if (!right.empty() && right.back() == ';')
                right.pop_back();

            if (!left.empty() && (isalpha(left[0]) || left[0] == '_')) {
                bool ok = true;
                for (char c : left)
                    if (!(isalnum(c) || c == '_')) ok = false;
                if (ok) {
                    nodes.push_back(std::make_shared<Assign>(left, right));
                    idx++;
                    continue;
                }
            }
        }

        /* function call */
        if (line.find('(') != std::string::npos && line.back() == ';') {
            std::string call = line.substr(0, line.size() - 1);
            auto pos = call.find('(');
            std::string name = ltrim(rtrim(call.substr(0, pos)));

            bool ok = !name.empty();
            for (char c : name)
                if (!(isalnum(c) || c == '_')) ok = false;

            if (ok) {
                std::string args_raw = call.substr(pos + 1, call.size() - pos - 2);
                std::vector<std::string> args;
                std::istringstream iss(args_raw);
                std::string tok;
                while (std::getline(iss, tok, ',')) {
                    tok = ltrim(rtrim(tok));
                    if (!tok.empty()) args.push_back(tok);
                }
                nodes.push_back(std::make_shared<FunctionCall>(name, args));
                idx++;
                continue;
            }
        }

        nodes.push_back(std::make_shared<ExprStmt>(line));
        idx++;
    }

    return { nodes, idx };
}

} // namespace bloa
