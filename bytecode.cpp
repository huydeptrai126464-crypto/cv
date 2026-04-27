#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <stack>
#include <stdexcept>
#include <cctype>
#include <utility>
using namespace std;

enum class Op {
    PUSH, ADD, SUB, MUL, DIV,
    PRINT, POP, DUP, SWAP,
    JMP, JZ, JNZ, CALL, RET,
    GT, LT, EQ, INPUT,
    HALT
};

struct RawIns {
    Op op;
    long long arg = 0;
    string label; // for JMP/JZ/JNZ/CALL before patching
};

struct CompiledBlock {
    vector<RawIns> code;
    unordered_map<string, int> labels;
};

static string trim(string s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) ++a;
    while (b > a && isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

static string strip_comment(const string& s) {
    size_t p = s.find("//");
    return trim(p == string::npos ? s : s.substr(0, p));
}

static bool starts_with(const string& s, const string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

static string normalize_label(string s) {
    s = trim(s);
    if (!s.empty() && s[0] == '@') s = trim(s.substr(1));
    return s;
}

static string op_name(Op op) {
    switch (op) {
        case Op::PUSH: return "PUSH";
        case Op::ADD:  return "ADD";
        case Op::SUB:  return "SUB";
        case Op::MUL:  return "MUL";
        case Op::DIV:  return "DIV";
        case Op::PRINT: return "PRINT";
        case Op::POP:   return "POP";
        case Op::DUP:   return "DUP";
        case Op::SWAP:  return "SWAP";
        case Op::JMP:   return "JMP";
        case Op::JZ:    return "JZ";
        case Op::JNZ:   return "JNZ";
        case Op::CALL:  return "CALL";
        case Op::RET:   return "RET";
        case Op::GT:    return "GT";
        case Op::LT:    return "LT";
        case Op::EQ:    return "EQ";
        case Op::INPUT: return "INPUT";
        case Op::HALT:  return "HALT";
    }
    return "HALT";
}

struct Token {
    enum Type { NUM, PLUS, MINUS, STAR, HASH, LP, RP, END } type;
    long long value = 0;
};

struct ExprCompiler {
    vector<Token> toks;
    size_t pos = 0;
    vector<RawIns>& out;

    ExprCompiler(const string& expr, vector<RawIns>& out_ref) : out(out_ref) {
        lex(expr);
    }

    void lex(const string& s) {
        size_t i = 0;
        while (i < s.size()) {
            char c = s[i];
            if (isspace((unsigned char)c)) {
                ++i;
                continue;
            }
            if (isdigit((unsigned char)c)) {
                long long v = 0;
                while (i < s.size() && isdigit((unsigned char)s[i])) {
                    v = v * 10 + (s[i] - '0');
                    ++i;
                }
                toks.push_back({Token::NUM, v});
                continue;
            }
            if (c == '+') toks.push_back({Token::PLUS, 0});
            else if (c == '-') toks.push_back({Token::MINUS, 0});
            else if (c == '*') toks.push_back({Token::STAR, 0});
            else if (c == '#') toks.push_back({Token::HASH, 0});
            else if (c == '(') toks.push_back({Token::LP, 0});
            else if (c == ')') toks.push_back({Token::RP, 0});
            else {
                throw runtime_error(string("Unknown char in expression: '") + c + "'");
            }
            ++i;
        }
        toks.push_back({Token::END, 0});
    }

    const Token& peek() const { return toks[pos]; }

    bool match(Token::Type t) {
        if (peek().type == t) {
            ++pos;
            return true;
        }
        return false;
    }

    void expect(Token::Type t, const string& msg) {
        if (!match(t)) throw runtime_error(msg);
    }

    void parse_factor() {
        if (match(Token::MINUS)) {
            out.push_back({Op::PUSH, 0, ""});
            parse_factor();
            out.push_back({Op::SUB, 0, ""});
            return;
        }

        if (match(Token::LP)) {
            parse_expr();
            expect(Token::RP, "Missing ')'");
            return;
        }

        if (peek().type == Token::NUM) {
            out.push_back({Op::PUSH, peek().value, ""});
            ++pos;
            return;
        }

        throw runtime_error("Bad expression factor");
    }

    void parse_term() {
        parse_factor();
        while (true) {
            if (match(Token::STAR)) {
                parse_factor();
                out.push_back({Op::MUL, 0, ""});
            } else if (match(Token::HASH)) {
                parse_factor();
                out.push_back({Op::DIV, 0, ""});
            } else {
                break;
            }
        }
    }

    void parse_expr() {
        parse_term();
        while (true) {
            if (match(Token::PLUS)) {
                parse_term();
                out.push_back({Op::ADD, 0, ""});
            } else if (match(Token::MINUS)) {
                parse_term();
                out.push_back({Op::SUB, 0, ""});
            } else {
                break;
            }
        }
    }

    void compile() {
        if (peek().type == Token::END) throw runtime_error("Empty expression");
        parse_expr();
        if (peek().type != Token::END) throw runtime_error("Bad expression trailing tokens");
    }
};

static vector<string> read_lines(const string& path) {
    ifstream in(path);
    if (!in) throw runtime_error("Cannot open source file: " + path);
    vector<string> lines;
    string s;
    while (getline(in, s)) lines.push_back(s);
    return lines;
}

struct FuncBlock {
    string name;
    vector<string> lines;
};

static void parse_source(
    const vector<string>& raw,
    vector<FuncBlock>& funcs,
    vector<string>& main_lines
) {
    bool seen_start = false;

    for (size_t i = 0; i < raw.size(); ++i) {
        string line = strip_comment(raw[i]);
        if (line.empty()) continue;

        if (line == "__start__:") {
            seen_start = true;
            main_lines.push_back(line);
            continue;
        }

        if (starts_with(line, "@")) {
            size_t lb = line.find('{');
            if (lb == string::npos) throw runtime_error("Function header must be like: @name {");
            string name = trim(line.substr(1, lb - 1));
            if (name.empty()) throw runtime_error("Empty function name");

            FuncBlock fn;
            fn.name = name;

            ++i;
            bool closed = false;
            for (; i < raw.size(); ++i) {
                string body = strip_comment(raw[i]);
                if (body.empty()) continue;
                if (body == "}") {
                    closed = true;
                    break;
                }
                fn.lines.push_back(body);
            }
            if (!closed) throw runtime_error("Missing '}' for function: " + name);

            funcs.push_back(move(fn));
            continue;
        }

        if (seen_start) {
            main_lines.push_back(line);
        }
    }
}

static void emit_expr(const string& expr, vector<RawIns>& out) {
    ExprCompiler ec(expr, out);
    ec.compile();
}

static CompiledBlock compile_block(const vector<string>& lines, bool is_main) {
    CompiledBlock b;

    for (const string& line0 : lines) {
        string line = trim(line0);
        if (line.empty()) continue;
        if (line == "}") continue;

        if (!line.empty() && line.back() == ':') {
            string lab = trim(line.substr(0, line.size() - 1));
            if (lab.empty()) throw runtime_error("Empty label");
            if (b.labels.count(lab)) throw runtime_error("Duplicate label in block: " + lab);
            b.labels[lab] = (int)b.code.size();
            continue;
        }

        string cmd, rest;
        {
            stringstream ss(line);
            ss >> cmd;
            getline(ss, rest);
            rest = trim(rest);
        }

        if (cmd == "push") {
            if (rest.empty()) throw runtime_error("push needs an expression");
            emit_expr(rest, b.code);
        } else if (cmd == "print") {
            if (!rest.empty()) emit_expr(rest, b.code);
            b.code.push_back({Op::PRINT, 0, ""});
        } else if (cmd == "pop") {
            b.code.push_back({Op::POP, 0, ""});
        } else if (cmd == "dup") {
            b.code.push_back({Op::DUP, 0, ""});
        } else if (cmd == "swap") {
            b.code.push_back({Op::SWAP, 0, ""});
        } else if (cmd == "add") {
            b.code.push_back({Op::ADD, 0, ""});
        } else if (cmd == "sub") {
            b.code.push_back({Op::SUB, 0, ""});
        } else if (cmd == "mul") {
            b.code.push_back({Op::MUL, 0, ""});
        } else if (cmd == "div") {
            b.code.push_back({Op::DIV, 0, ""});
        } else if (cmd == "gt") {
            b.code.push_back({Op::GT, 0, ""});
        } else if (cmd == "lt") {
            b.code.push_back({Op::LT, 0, ""});
        } else if (cmd == "eq") {
            b.code.push_back({Op::EQ, 0, ""});
        } else if (cmd == "input") {
            b.code.push_back({Op::INPUT, 0, ""});
        } else if (cmd == "jmp" || cmd == "jz" || cmd == "jnz" || cmd == "call") {
            if (rest.empty()) throw runtime_error(cmd + " needs a label");
            string lab = normalize_label(rest);
            Op op = Op::JMP;
            if (cmd == "jz") op = Op::JZ;
            else if (cmd == "jnz") op = Op::JNZ;
            else if (cmd == "call") op = Op::CALL;
            b.code.push_back({op, 0, lab});
        } else if (cmd == "ret") {
            b.code.push_back({Op::RET, 0, ""});
        } else if (cmd == "halt") {
            b.code.push_back({Op::HALT, 0, ""});
        } else {
            throw runtime_error("Unknown command: " + cmd);
        }
    }

    if (is_main) {
        if (b.code.empty() || b.code.back().op != Op::HALT) {
            b.code.push_back({Op::HALT, 0, ""});
        }
    } else {
        if (b.code.empty() || (b.code.back().op != Op::RET && b.code.back().op != Op::HALT)) {
            b.code.push_back({Op::RET, 0, ""});
        }
    }

    return b;
}

int main(int argc, char** argv) {
    try {
        string src_path = "code.cv";
        string out_path = "program.bc";
        if (argc >= 2) src_path = argv[1];
        if (argc >= 3) out_path = argv[2];

        vector<string> raw = read_lines(src_path);

        vector<FuncBlock> funcs;
        vector<string> main_lines;
        parse_source(raw, funcs, main_lines);

        bool has_start = false;
        for (auto& s : main_lines) {
            if (s == "__start__:") {
                has_start = true;
                break;
            }
        }
        if (!has_start) {
            throw runtime_error("Missing __start__:");
        }

        vector<CompiledBlock> compiled_funcs;
        for (auto& fn : funcs) {
            CompiledBlock b = compile_block(fn.lines, false);
            if (b.labels.count(fn.name)) throw runtime_error("Duplicate label: " + fn.name);
            b.labels[fn.name] = 0;
            compiled_funcs.push_back(move(b));
        }

        CompiledBlock main_block = compile_block(main_lines, true);

        vector<RawIns> program;
        unordered_map<string, int> globals;

        auto append_block = [&](CompiledBlock& b) {
            int base = (int)program.size();

            for (auto& kv : b.labels) {
                const string& name = kv.first;
                int pos = kv.second;
                if (globals.count(name)) throw runtime_error("Duplicate global label: " + name);
                globals[name] = base + pos;
            }

            for (auto& ins : b.code) program.push_back(ins);
        };

        for (auto& b : compiled_funcs) append_block(b);
        append_block(main_block);

        if (!globals.count("__start__")) {
            throw runtime_error("Entry label __start__ not found");
        }

        for (auto& ins : program) {
            if (!ins.label.empty()) {
                if (!globals.count(ins.label)) {
                    throw runtime_error("Unknown label: " + ins.label);
                }
                ins.arg = globals[ins.label];
            }
        }

        ofstream out(out_path);
        if (!out) throw runtime_error("Cannot open output file: " + out_path);

        out << "ENTRY " << globals["__start__"] << "\n";
        for (auto& ins : program) {
            out << op_name(ins.op);
            switch (ins.op) {
                case Op::PUSH:
                case Op::JMP:
                case Op::JZ:
                case Op::JNZ:
                case Op::CALL:
                    out << ' ' << ins.arg;
                    break;
                default:
                    break;
            }
            out << "\n";
        }

        cout << "Compiled " << src_path << " -> " << out_path << "\n";
        cout << "Entry = " << globals["__start__"] << "\n";
        cout << "Instructions = " << program.size() << "\n";
        return 0;
    } catch (const exception& e) {
        cerr << "[bytecode] " << e.what() << "\n";
        return 1;
    }
}