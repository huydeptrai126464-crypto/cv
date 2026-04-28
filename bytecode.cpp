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
#include <iomanip>
using namespace std;

enum class Op {
    PUSH, ADD, SUB, MUL, DIV,
    PRINT, POP, DUP, SWAP,
    JMP, JZ, JNZ, CALL, RET,
    GT, LT, EQ, INPUT, PRINT_STR,
    LOAD, STORE, LOADI, STOREI,
    HALT
};

struct RawIns {
    Op op;
    long long arg = 0;
    string label;
};

struct CompiledBlock {
    vector<RawIns> code;
    unordered_map<string, int> labels;
};

static vector<string> g_string_pool;
static unordered_map<string, int> g_string_ids;
static unordered_map<string, int> g_mem_slots;
static int g_next_mem_slot = 0;
static int g_auto_id = 0;

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

static bool is_integer(const string& s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '-' || s[0] == '+') ++i;
    if (i >= s.size()) return false;
    for (; i < s.size(); ++i) {
        if (!isdigit((unsigned char)s[i])) return false;
    }
    return true;
}

static bool is_identifier(const string& s) {
    if (s.empty()) return false;
    if (!(isalpha((unsigned char)s[0]) || s[0] == '_')) return false;
    for (size_t i = 1; i < s.size(); ++i) {
        char c = s[i];
        if (!(isalnum((unsigned char)c) || c == '_')) return false;
    }
    return true;
}

static string normalize_label(string s) {
    s = trim(s);
    if (!s.empty() && s[0] == '@') s = trim(s.substr(1));
    return s;
}

static string decode_string_literal(const string& raw) {
    string s = trim(raw);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s = s.substr(1, s.size() - 2);
        string out;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '\\' && i + 1 < s.size()) {
                char n = s[++i];
                switch (n) {
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case 'r': out.push_back('\r'); break;
                    case '\\': out.push_back('\\'); break;
                    case '"': out.push_back('"'); break;
                    case '0': out.push_back('\0'); break;
                    default: out.push_back(n); break;
                }
            } else {
                out.push_back(c);
            }
        }
        return out;
    }
    return s;
}

static int intern_string(const string& s) {
    auto it = g_string_ids.find(s);
    if (it != g_string_ids.end()) return it->second;
    int id = (int)g_string_pool.size();
    g_string_pool.push_back(s);
    g_string_ids[s] = id;
    return id;
}

static long long resolve_mem_operand(const string& text) {
    string t = trim(text);
    if (t.empty()) throw runtime_error("Missing memory operand");
    if (is_integer(t)) return stoll(t);

    if (!is_identifier(t)) {
        throw runtime_error("Invalid memory operand: " + t);
    }

    auto it = g_mem_slots.find(t);
    if (it != g_mem_slots.end()) return it->second;

    int slot = g_next_mem_slot++;
    g_mem_slots[t] = slot;
    return slot;
}

static string op_name(Op op) {
    switch (op) {
        case Op::PUSH:  return "PUSH";
        case Op::ADD:   return "ADD";
        case Op::SUB:   return "SUB";
        case Op::MUL:   return "MUL";
        case Op::DIV:   return "DIV";
        case Op::PRINT: return "PRINT";
        case Op::PRINT_STR: return "PRINT_STR";
        case Op::LOAD:  return "LOAD";
        case Op::STORE: return "STORE";
        case Op::LOADI: return "LOADI";
        case Op::STOREI: return "STOREI";
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
            else throw runtime_error(string("Unknown char in expression: '") + c + "'");
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
            } else break;
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
            } else break;
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

static vector<string> tokenize_source(const vector<string>& raw) {
    vector<string> out;
    for (const string& r : raw) {
        string s = strip_comment(r);
        string cur;
        for (char c : s) {
            if (c == '{' || c == '}') {
                string t = trim(cur);
                if (!t.empty()) out.push_back(t);
                cur.clear();
                out.push_back(string(1, c));
            } else {
                cur.push_back(c);
            }
        }
        string t = trim(cur);
        if (!t.empty()) out.push_back(t);
    }
    return out;
}

struct FuncBlock {
    string name;
    vector<string> toks;
};

static void parse_source(
    const vector<string>& raw,
    vector<FuncBlock>& funcs,
    vector<string>& main_toks
) {
    vector<string> toks = tokenize_source(raw);
    bool seen_start = false;

    for (size_t i = 0; i < toks.size(); ++i) {
        string tk = trim(toks[i]);
        if (tk.empty()) continue;

        if (tk == "__start__:") {
            seen_start = true;
            main_toks.push_back(tk);
            continue;
        }

        if (starts_with(tk, "@")) {
            if (i + 1 >= toks.size() || trim(toks[i + 1]) != "{") {
                throw runtime_error("Function header must be like: @name {");
            }

            string name = trim(tk.substr(1));
            if (name.empty()) throw runtime_error("Empty function name");

            ++i;
            int depth = 1;
            vector<string> body;

            for (++i; i < toks.size(); ++i) {
                string t = trim(toks[i]);
                if (t == "{") {
                    ++depth;
                    body.push_back(t);
                    continue;
                }
                if (t == "}") {
                    --depth;
                    if (depth == 0) break;
                    body.push_back(t);
                    continue;
                }
                body.push_back(t);
            }

            if (depth != 0) throw runtime_error("Missing '}' for function: " + name);

            FuncBlock fn;
            fn.name = name;
            fn.toks = move(body);
            funcs.push_back(move(fn));
            continue;
        }

        if (seen_start) main_toks.push_back(tk);
    }
}

static void emit_expr(const string& expr, vector<RawIns>& out) {
    ExprCompiler ec(expr, out);
    ec.compile();
}

static bool split_comparison(const string& cond, string& lhs, string& op, string& rhs) {
    int depth = 0;
    for (size_t i = 0; i < cond.size(); ++i) {
        char c = cond[i];
        if (c == '(') ++depth;
        else if (c == ')') --depth;
        if (depth != 0) continue;

        if (i + 1 < cond.size()) {
            string two = cond.substr(i, 2);
            if (two == "==" || two == "!=" || two == ">=" || two == "<=") {
                lhs = trim(cond.substr(0, i));
                op  = two;
                rhs = trim(cond.substr(i + 2));
                return true;
            }
        }

        if (c == '>' || c == '<') {
            lhs = trim(cond.substr(0, i));
            op  = string(1, c);
            rhs = trim(cond.substr(i + 1));
            return true;
        }
    }
    return false;
}

static vector<string> split_top_level(const string& s, char sep) {
    vector<string> parts;
    string cur;
    int depth = 0;

    for (char c : s) {
        if (c == '(') ++depth;
        else if (c == ')') --depth;

        if (c == sep && depth == 0) {
            parts.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(trim(cur));
    return parts;
}

static string extract_paren_content(const string& s) {
    size_t lp = s.find('(');
    if (lp == string::npos) throw runtime_error("Missing '('");
    int depth = 0;
    for (size_t i = lp; i < s.size(); ++i) {
        if (s[i] == '(') ++depth;
        else if (s[i] == ')') {
            --depth;
            if (depth == 0) return trim(s.substr(lp + 1, i - lp - 1));
        }
    }
    throw runtime_error("Missing ')'");
}

static string new_label(const string& base) {
    return base + "_" + to_string(g_auto_id++);
}

static void mark_label(CompiledBlock& b, const string& lab) {
    if (lab.empty()) throw runtime_error("Empty label");
    if (b.labels.count(lab)) throw runtime_error("Duplicate label in block: " + lab);
    b.labels[lab] = (int)b.code.size();
}
static void emit_condition_false_jump(const string& cond, vector<RawIns>& out, const string& false_label) {
    string s = trim(cond);
    if (s.empty()) return;

    string lhs, op, rhs;
    if (split_comparison(s, lhs, op, rhs)) {
        if (lhs.empty() || rhs.empty()) throw runtime_error("Bad comparison condition: " + cond);

        emit_expr(lhs, out);
        emit_expr(rhs, out);

        if (op == ">") {
            out.push_back({Op::GT, 0, ""});
            out.push_back({Op::JZ, 0, false_label});
        } else if (op == "<") {
            out.push_back({Op::LT, 0, ""});
            out.push_back({Op::JZ, 0, false_label});
        } else if (op == "==") {
            out.push_back({Op::EQ, 0, ""});
            out.push_back({Op::JZ, 0, false_label});
        } else if (op == "!=") {
            out.push_back({Op::EQ, 0, ""});
            out.push_back({Op::JNZ, 0, false_label});
        } else if (op == ">=") {
            out.push_back({Op::LT, 0, ""});
            out.push_back({Op::JNZ, 0, false_label});
        } else if (op == "<=") {
            out.push_back({Op::GT, 0, ""});
            out.push_back({Op::JNZ, 0, false_label});
        } else {
            throw runtime_error("Unknown comparison operator in condition");
        }
        return;
    }

    emit_expr(s, out);
    out.push_back({Op::JZ, 0, false_label});
}

static void compile_simple_stmt(const string& line, CompiledBlock& b) {
    string t = trim(line);
    if (t.empty()) return;

    if (!t.empty() && t.back() == ':') {
        string lab = trim(t.substr(0, t.size() - 1));
        if (lab.empty()) throw runtime_error("Empty label");
        mark_label(b, lab);
        return;
    }

    string cmd, rest;
    {
        stringstream ss(t);
        ss >> cmd;
        getline(ss, rest);
        rest = trim(rest);
    }

    if (cmd == "push") {
        if (rest.empty()) throw runtime_error("push needs an expression");
        emit_expr(rest, b.code);
    }
    else if (cmd == "push_str") {
        if (rest.empty()) throw runtime_error("push_str needs a string literal");
        string s = decode_string_literal(rest);
        int sid = intern_string(s);
        b.code.push_back({Op::PUSH, sid, ""});
    }
    else if (cmd == "print") {
        if (!rest.empty()) emit_expr(rest, b.code);
        b.code.push_back({Op::PRINT, 0, ""});
    }
    else if (cmd == "print_str") {
        b.code.push_back({Op::PRINT_STR, 0, ""});
    }
    else if (cmd == "load") {
        if (rest.empty()) throw runtime_error("load needs an index or variable name");
        long long idx = resolve_mem_operand(rest);
        b.code.push_back({Op::LOAD, idx, ""});
    }
    else if (cmd == "store") {
        if (rest.empty()) throw runtime_error("store needs an index or variable name");
        long long idx = resolve_mem_operand(rest);
        b.code.push_back({Op::STORE, idx, ""});
    }
    else if (cmd == "loadi") {
        if (!rest.empty()) throw runtime_error("loadi takes no operand");
        b.code.push_back({Op::LOADI, 0, ""});
    }
    else if (cmd == "storei") {
        if (!rest.empty()) throw runtime_error("storei takes no operand");
        b.code.push_back({Op::STOREI, 0, ""});
    }
    else if (cmd == "pop") {
        b.code.push_back({Op::POP, 0, ""});
    }
    else if (cmd == "dup") {
        b.code.push_back({Op::DUP, 0, ""});
    }
    else if (cmd == "swap") {
        b.code.push_back({Op::SWAP, 0, ""});
    }
    else if (cmd == "add") {
        b.code.push_back({Op::ADD, 0, ""});
    }
    else if (cmd == "sub") {
        b.code.push_back({Op::SUB, 0, ""});
    }
    else if (cmd == "mul") {
        b.code.push_back({Op::MUL, 0, ""});
    }
    else if (cmd == "div") {
        b.code.push_back({Op::DIV, 0, ""});
    }
    else if (cmd == "gt") {
        b.code.push_back({Op::GT, 0, ""});
    }
    else if (cmd == "lt") {
        b.code.push_back({Op::LT, 0, ""});
    }
    else if (cmd == "eq") {
        b.code.push_back({Op::EQ, 0, ""});
    }
    else if (cmd == "input") {
        b.code.push_back({Op::INPUT, 0, ""});
    }
    else if (cmd == "jmp" || cmd == "jz" || cmd == "jnz" || cmd == "call") {
        if (rest.empty()) throw runtime_error(cmd + " needs a label");
        string lab = normalize_label(rest);

        Op op = Op::JMP;
        if (cmd == "jz") op = Op::JZ;
        else if (cmd == "jnz") op = Op::JNZ;
        else if (cmd == "call") op = Op::CALL;

        b.code.push_back({op, 0, lab});
    }
    else if (cmd == "ret") {
        b.code.push_back({Op::RET, 0, ""});
    }
    else if (cmd == "halt") {
        b.code.push_back({Op::HALT, 0, ""});
    }
    else {
        throw runtime_error("Unknown command: " + cmd);
    }
}

static void compile_block_body(const vector<string>& toks, size_t& pos, CompiledBlock& b);

static void compile_if_chain(const vector<string>& toks, size_t& pos, CompiledBlock& b, const string& end_label) {
    if (pos >= toks.size()) throw runtime_error("Unexpected end in if");
    string header = trim(toks[pos++]);

    bool is_else_if = starts_with(header, "else if");
    bool is_if = starts_with(header, "if");
    if (!is_if && !is_else_if) throw runtime_error("Bad if header: " + header);

    string cond = extract_paren_content(header);
    if (trim(cond).empty()) throw runtime_error("Empty condition in if");

    string false_label = new_label("if_else");
    emit_condition_false_jump(cond, b.code, false_label);

    compile_block_body(toks, pos, b);

    b.code.push_back({Op::JMP, 0, end_label});
    mark_label(b, false_label);

    if (pos < toks.size()) {
        string next = trim(toks[pos]);
        if (starts_with(next, "else if")) {
            compile_if_chain(toks, pos, b, end_label);
            return;
        }
        if (next == "else") {
            ++pos;
            compile_block_body(toks, pos, b);
            return;
        }
    }
}

static void compile_if_stmt(const vector<string>& toks, size_t& pos, CompiledBlock& b) {
    string end_label = new_label("if_end");
    compile_if_chain(toks, pos, b, end_label);
    mark_label(b, end_label);
}

static void compile_while_stmt(const vector<string>& toks, size_t& pos, CompiledBlock& b) {
    if (pos >= toks.size()) throw runtime_error("Unexpected end in while");
    string header = trim(toks[pos++]);
    if (!starts_with(header, "while")) throw runtime_error("Bad while header: " + header);

    string cond = extract_paren_content(header);
    if (trim(cond).empty()) throw runtime_error("Empty condition in while");

    string lstart = new_label("while_start");
    string lend = new_label("while_end");

    mark_label(b, lstart);
    emit_condition_false_jump(cond, b.code, lend);
    compile_block_body(toks, pos, b);
    b.code.push_back({Op::JMP, 0, lstart});
    mark_label(b, lend);
}

static void compile_for_stmt(const vector<string>& toks, size_t& pos, CompiledBlock& b) {
    if (pos >= toks.size()) throw runtime_error("Unexpected end in for");
    string header = trim(toks[pos++]);
    if (!starts_with(header, "for")) throw runtime_error("Bad for header: " + header);

    string inside = extract_paren_content(header);
    vector<string> parts = split_top_level(inside, ';');
    if (parts.size() != 3) throw runtime_error("for needs exactly 3 parts: for (init; cond; update)");

    string init = trim(parts[0]);
    string cond = trim(parts[1]);
    string update = trim(parts[2]);

    if (!init.empty()) compile_simple_stmt(init, b);

    string lstart = new_label("for_start");
    string lend = new_label("for_end");

    mark_label(b, lstart);
    if (!cond.empty()) emit_condition_false_jump(cond, b.code, lend);

    compile_block_body(toks, pos, b);

    if (!update.empty()) compile_simple_stmt(update, b);
    b.code.push_back({Op::JMP, 0, lstart});
    mark_label(b, lend);
}

static void compile_block_tokens(const vector<string>& toks, size_t& pos, CompiledBlock& b) {
    while (pos < toks.size()) {
        string t = trim(toks[pos]);
        if (t.empty()) {
            ++pos;
            continue;
        }

        if (t == "{") {
            ++pos;
            continue;
        }

        if (t == "}") {
            return;
        }

        if (starts_with(t, "if")) {
            compile_if_stmt(toks, pos, b);
            continue;
        }

        if (starts_with(t, "while")) {
            compile_while_stmt(toks, pos, b);
            continue;
        }

        if (starts_with(t, "for")) {
            compile_for_stmt(toks, pos, b);
            continue;
        }

        compile_simple_stmt(t, b);
        ++pos;
    }
}

static void compile_block_body(const vector<string>& toks, size_t& pos, CompiledBlock& b) {
    if (pos >= toks.size() || trim(toks[pos]) != "{") {
        throw runtime_error("Missing '{'");
    }
    ++pos;
    compile_block_tokens(toks, pos, b);
    if (pos >= toks.size() || trim(toks[pos]) != "}") {
        throw runtime_error("Missing '}'");
    }
    ++pos;
}

static CompiledBlock compile_block(const vector<string>& toks, bool is_main) {
    CompiledBlock b;
    size_t pos = 0;
    compile_block_tokens(toks, pos, b);

    if (pos != toks.size()) {
        string t = trim(toks[pos]);
        if (t == "}") throw runtime_error("Unexpected '}'");
        throw runtime_error("Unexpected tokens at end of block");
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

        g_string_pool.clear();
        g_string_ids.clear();
        g_mem_slots.clear();
        g_next_mem_slot = 0;
        g_auto_id = 0;

        vector<string> raw = read_lines(src_path);

        vector<FuncBlock> funcs;
        vector<string> main_toks;
        parse_source(raw, funcs, main_toks);

        bool has_start = false;
        for (auto& s : main_toks) {
            if (s == "__start__:") {
                has_start = true;
                break;
            }
        }
        if (!has_start) throw runtime_error("Missing __start__:");

        vector<CompiledBlock> compiled_funcs;
        for (auto& fn : funcs) {
            CompiledBlock b = compile_block(fn.toks, false);
            if (b.labels.count(fn.name)) throw runtime_error("Duplicate label: " + fn.name);
            b.labels[fn.name] = 0;
            compiled_funcs.push_back(move(b));
        }

        CompiledBlock main_block = compile_block(main_toks, true);

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
        for (size_t i = 0; i < g_string_pool.size(); ++i) {
            out << "STR " << i << ' ' << quoted(g_string_pool[i]) << "\n";
        }

        for (auto& ins : program) {
            out << op_name(ins.op);
            switch (ins.op) {
                case Op::PUSH:
                case Op::JMP:
                case Op::JZ:
                case Op::JNZ:
                case Op::CALL:
                case Op::LOAD:
                case Op::STORE:
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
        cout << "Strings = " << g_string_pool.size() << "\n";
        return 0;
    } catch (const exception& e) {
        cerr << "[bytecode] " << e.what() << "\n";
        return 1;
    }
}