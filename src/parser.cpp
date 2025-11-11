#include "bloa/parser.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace bloa {

std::vector<std::string> split_lines(const std::string &code) {
    std::string s = code;
    std::vector<std::string> out;
    std::string line;
    std::istringstream iss(s);
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

std::pair<NodeList,int> parse_block(const std::vector<std::string> &lines, int start_idx, int base_indent) {
    int idx = start_idx;
    NodeList nodes;
    while (idx < (int)lines.size()) {
        const std::string raw_line = lines[idx];
        std::string stripped = raw_line;
        // trim right newline already removed
        auto ltrim = [](std::string s)->std::string {
            size_t pos = s.find_first_not_of(" \t\r\n");
            if (pos==std::string::npos) return std::string();
            return s.substr(pos);
        };
        auto rtrim = [](std::string s)->std::string {
            size_t pos = s.find_last_not_of(" \t\r\n");
            if (pos==std::string::npos) return std::string();
            return s.substr(0,pos+1);
        };
        if (ltrim(raw_line).empty() || ltrim(raw_line).rfind("#",0)==0) { idx++; continue; }
        int indent = indent_level(raw_line);
        if (indent < base_indent) break;
        if (indent > base_indent) throw std::runtime_error("Unexpected indent at line " + std::to_string(idx+1));
        std::string line = ltrim(rtrim(raw_line));
        // STATEMENT_RULES
        if (starts_with(line, "say ")) {
            nodes.push_back(std::make_shared<Say>(line.substr(4)));
            idx++; continue;
        }
        if (starts_with(line, "ask ")) {
            std::string after = line.substr(4);
            auto pos = after.find("->");
            if (pos==std::string::npos) throw std::runtime_error("Invalid ask syntax at line " + std::to_string(idx+1));
            std::string left = after.substr(0,pos);
            std::string right = after.substr(pos+2);
            nodes.push_back(std::make_shared<Ask>(ltrim(left), ltrim(right)));
            idx++; continue;
        }
        if (starts_with(line, "import ")) {
            nodes.push_back(std::make_shared<Import>(line.substr(7)));
            idx++; continue;
        }
        if (line=="return") {
            nodes.push_back(std::make_shared<Return>(std::optional<std::string>{}));
            idx++; continue;
        }
        if (starts_with(line, "return ")) {
            nodes.push_back(std::make_shared<Return>(std::optional<std::string>(line.substr(7))));
            idx++; continue;
        }
        // if / elif / else (build nested ifs)
        if (starts_with(line, "if ") && line.back()==':') {
            std::string cond = line.substr(3, line.size()-4);
            auto [then_block, next_idx] = parse_block(lines, idx+1, base_indent+4);
            NodeList else_block;
            int curr_next = next_idx;
            while (curr_next < (int)lines.size() && indent_level(lines[curr_next]) == base_indent) {
                std::string stripped2 = split_lines(lines[curr_next])[0];
                std::string t = stripped2;
                // trim
                size_t p = t.find_first_not_of(" \t");
                if (p!=std::string::npos) t = t.substr(p);
                if (t=="else:") {
                    auto res = parse_block(lines, curr_next+1, base_indent+4);
                    else_block = res.first;
                    curr_next = res.second;
                    break;
                }
                if (starts_with(t, "elif ") && t.back()==':') {
                    std::string elif_cond = t.substr(5, t.size()-6);
                    auto [elif_then, after_elif] = parse_block(lines, curr_next+1, base_indent+4);
                    // nest
                    NodeList nested;
                    nested.push_back(std::make_shared<If>(elif_cond, elif_then, NodeList{}));
                    else_block = nested;
                    curr_next = after_elif;
                    continue;
                }
                break;
            }
            nodes.push_back(std::make_shared<If>(cond, then_block, else_block));
            idx = curr_next;
            continue;
        }
        if (starts_with(line, "repeat ") && line.size()>7 && line.rfind(" times:") == line.size()-7) {
            std::string times_part = line.substr(7, line.size()-7-7);
            auto res = parse_block(lines, idx+1, base_indent+4);
            nodes.push_back(std::make_shared<Repeat>(times_part, res.first));
            idx = res.second;
            continue;
        }
        if (starts_with(line, "function ") && line.back()==':') {
            std::string header = line.substr(9, line.size()-10);
            auto pos = header.find('(');
            if (pos==std::string::npos) throw std::runtime_error("Invalid function header at line " + std::to_string(idx+1));
            std::string name = header.substr(0,pos);
            std::string params_raw = header.substr(pos+1, header.size()-pos-2);
            std::vector<std::string> params;
            std::istringstream iss(params_raw);
            std::string tok;
            while (std::getline(iss, tok, ',')) {
                // trim
                size_t a = tok.find_first_not_of(" \t");
                if (a==std::string::npos) continue;
                size_t b = tok.find_last_not_of(" \t");
                params.push_back(tok.substr(a, b-a+1));
            }
            auto res = parse_block(lines, idx+1, base_indent+4);
            nodes.push_back(std::make_shared<FunctionDef>(name, params, res.first));
            idx = res.second;
            continue;
        }
        if (line=="else:") {
            throw std::runtime_error("Unexpected 'else:' at line " + std::to_string(idx+1));
        }
        if (starts_with(line, "while ") && line.back()==':') {
            std::string cond = line.substr(6, line.size()-7);
            auto res = parse_block(lines, idx+1, base_indent+4);
            nodes.push_back(std::make_shared<While>(cond, res.first));
            idx = res.second;
            continue;
        }
        if (line=="break") { nodes.push_back(std::make_shared<Break>()); idx++; continue; }
        if (line=="continue") { nodes.push_back(std::make_shared<Continue>()); idx++; continue; }
        if (starts_with(line, "for ") && line.find(" in ")!=std::string::npos && line.back()==':') {
            std::string header = line.substr(4, line.size()-5);
            auto pos = header.find(" in ");
            std::string var = header.substr(0,pos);
            std::string iterable = header.substr(pos+4);
            auto res = parse_block(lines, idx+1, base_indent+4);
            nodes.push_back(std::make_shared<ForIn>(var, iterable, res.first));
            idx = res.second;
            continue;
        }
        if (line=="try:") {
            auto [try_block, next_idx] = parse_block(lines, idx+1, base_indent+4);
            NodeList except_block;
            if (next_idx < (int)lines.size() && indent_level(lines[next_idx])==base_indent && split_lines(lines[next_idx])[0]=="except:") {
                auto res = parse_block(lines, next_idx+1, base_indent+4);
                except_block = res.first;
                next_idx = res.second;
            }
            nodes.push_back(std::make_shared<TryExcept>(try_block, except_block));
            idx = next_idx;
            continue;
        }
        // assignment
        if (line.find('=')!=std::string::npos && !(line.find("==")!=std::string::npos)) {
            auto pos = line.find('=');
            std::string left = line.substr(0,pos);
            std::string right = line.substr(pos+1);
            // trim left
            size_t a = left.find_first_not_of(" \t");
            if (a!=std::string::npos) left = left.substr(a);
            size_t b = left.find_last_not_of(" \t");
            if (b!=std::string::npos) left = left.substr(0,b+1);
            // validate name
            if (!left.empty() && (isalpha(left[0]) || left[0]=='_')) {
                bool ok = true;
                for (char ch : left) if (!(isalnum(ch) || ch=='_')) ok = false;
                if (ok) { nodes.push_back(std::make_shared<Assign>(left, right)); idx++; continue; }
            }
        }
        // function call form (simple)
        if (line.find('(')!=std::string::npos && line.back()==')') {
            auto pos = line.find('(');
            std::string name = line.substr(0,pos);
            // trim name
            size_t a = name.find_first_not_of(" \t");
            size_t b = name.find_last_not_of(" \t");
            if (a!=std::string::npos && b!=std::string::npos) name = name.substr(a,b-a+1);
            bool is_ident = true;
            for (char ch : name) if (!(isalnum(ch) || ch=='_' )) is_ident = false;
            if (is_ident) {
                std::string args_raw = line.substr(pos+1, line.size()-pos-2);
                std::vector<std::string> args;
                std::istringstream iss(args_raw);
                std::string tok;
                while (std::getline(iss, tok, ',')) {
                    size_t a2 = tok.find_first_not_of(" \t");
                    if (a2==std::string::npos) continue;
                    size_t b2 = tok.find_last_not_of(" \t");
                    args.push_back(tok.substr(a2,b2-a2+1));
                }
                nodes.push_back(std::make_shared<FunctionCall>(name, args));
                idx++; continue;
            }
        }
        // fallback
        nodes.push_back(std::make_shared<ExprStmt>(line));
        idx++;
    }
    return {nodes, idx};
}

} // namespace bloa
