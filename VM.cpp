#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <iomanip>
using namespace std;

enum class Op {
    PUSH, ADD, SUB, MUL, DIV,
    PRINT, POP, DUP, SWAP,
    JMP, JZ, JNZ, CALL, RET,
    GT, LT, EQ, INPUT, PRINT_STR,
    LOAD, STORE, LOADI, STOREI,
    AND, OR, XOR, NOT, SHL, SHR,
    HALT
};

struct Ins {
    Op op;
    long long arg = 0;
};

static string trim(string s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) ++a;
    while (b > a && isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

static Op parse_op(const string& s) {
    if (s == "PUSH")      return Op::PUSH;
    if (s == "ADD")       return Op::ADD;
    if (s == "SUB")       return Op::SUB;
    if (s == "MUL")       return Op::MUL;
    if (s == "DIV")       return Op::DIV;
    if (s == "PRINT")     return Op::PRINT;
    if (s == "PRINT_STR") return Op::PRINT_STR;
    if (s == "POP")       return Op::POP;
    if (s == "DUP")       return Op::DUP;
    if (s == "SWAP")      return Op::SWAP;
    if (s == "JMP")       return Op::JMP;
    if (s == "JZ")        return Op::JZ;
    if (s == "JNZ")       return Op::JNZ;
    if (s == "CALL")      return Op::CALL;
    if (s == "RET")       return Op::RET;
    if (s == "GT")        return Op::GT;
    if (s == "LT")        return Op::LT;
    if (s == "EQ")        return Op::EQ;
    if (s == "INPUT")     return Op::INPUT;
    if (s == "LOAD")      return Op::LOAD;
    if (s == "STORE")     return Op::STORE;
    if (s == "LOADI")     return Op::LOADI;
    if (s == "STOREI")    return Op::STOREI;
    if (s == "AND") return Op::AND;
    if (s == "OR")  return Op::OR;
    if (s == "XOR") return Op::XOR;
    if (s == "NOT") return Op::NOT;
    if (s == "SHL") return Op::SHL;
    if (s == "SHR") return Op::SHR;
    if (s == "HALT")      return Op::HALT;
    throw runtime_error("Unknown opcode: " + s);
}

struct VM {
    vector<long long> stack;
    vector<int> callstack;
    vector<long long> memory;
    vector<string> strings;

    VM() : memory(4096, 0) {}

    bool need(size_t n, const string& msg) {
        if (stack.size() < n) {
            cerr << "[vm] " << msg << "\n";
            return false;
        }
        return true;
    }

    bool pop2(long long& b, long long& a) {
        if (!need(2, "stack underflow")) return false;
        a = stack.back(); stack.pop_back();
        b = stack.back(); stack.pop_back();
        return true;
    }

    bool check_mem(long long idx, const string& op) {
        if (idx < 0 || idx >= (long long)memory.size()) {
            cerr << "[vm] " << op << " out of bounds: " << idx << "\n";
            return false;
        }
        return true;
    }

    bool check_str(long long idx) {
        if (idx < 0 || idx >= (long long)strings.size()) {
            cerr << "[vm] PRINT_STR bad string id: " << idx << "\n";
            return false;
        }
        return true;
    }

    void run(const vector<Ins>& p, int entry) {
        int ip = entry;
        auto popu64 = [&]() -> unsigned long long {
    unsigned long long x = (unsigned long long)stack.back();
    stack.pop_back();
    return x;
};

auto pushu64 = [&](unsigned long long x) {
    stack.push_back((long long)x);
};

        while (ip >= 0 && ip < (int)p.size()) {
            const Ins& ins = p[ip];

            switch (ins.op) {
                case Op::PUSH:
                    stack.push_back(ins.arg);
                    ++ip;
                    break;

                case Op::ADD: {
                    long long b, a;
                    if (!pop2(b, a)) return;
                    stack.push_back(b + a);
                    ++ip;
                    break;
                }

                case Op::SUB: {
                    long long b, a;
                    if (!pop2(b, a)) return;
                    stack.push_back(b - a);
                    ++ip;
                    break;
                }

                case Op::MUL: {
                    long long b, a;
                    if (!pop2(b, a)) return;
                    stack.push_back(b * a);
                    ++ip;
                    break;
                }

                case Op::DIV: {
                    long long b, a;
                    if (!pop2(b, a)) return;
                    if (a == 0) {
                        cerr << "[vm] division by zero\n";
                        return;
                    }
                    stack.push_back(b / a);
                    ++ip;
                    break;
                }

                case Op::PRINT:
                    if (!need(1, "print on empty stack")) return;
                    cout << stack.back() << "\n";
                    stack.pop_back();
                    ++ip;
                    break;

                case Op::PRINT_STR: {
                    if (!need(1, "print_str on empty stack")) return;
                    long long sid = stack.back();
                    stack.pop_back();
                    if (!check_str(sid)) return;
                    cout << strings[(size_t)sid] << "\n";
                    ++ip;
                    break;
                }

                case Op::INPUT: {
                    long long x;
                    if (!(cin >> x)) {
                        cerr << "[vm] failed to read input\n";
                        return;
                    }
                    stack.push_back(x);
                    ++ip;
                    break;
                }
                
                case Op::AND: {
    if (!need(2, "AND needs 2 values")) return;
    unsigned long long b = popu64();
    unsigned long long a = popu64();
    pushu64(a & b);
    ++ip;
    break;
}

case Op::OR: {
    if (!need(2, "OR needs 2 values")) return;
    unsigned long long b = popu64();
    unsigned long long a = popu64();
    pushu64(a | b);
    ++ip;
    break;
}

case Op::XOR: {
    if (!need(2, "XOR needs 2 values")) return;
    unsigned long long b = popu64();
    unsigned long long a = popu64();
    pushu64(a ^ b);
    ++ip;
    break;
}

case Op::NOT: {
    if (!need(1, "NOT needs 1 value")) return;
    unsigned long long a = popu64();
    pushu64(~a);
    ++ip;
    break;
}

case Op::SHL: {
    if (!need(2, "SHL needs 2 values")) return;
    unsigned long long shift = popu64();
    unsigned long long a = popu64();
    if (shift >= 64) {
        pushu64(0);
    } else {
        pushu64(a << shift);
    }
    ++ip;
    break;
}

case Op::SHR: {
    if (!need(2, "SHR needs 2 values")) return;
    unsigned long long shift = popu64();
    unsigned long long a = popu64();
    if (shift >= 64) {
        pushu64(0);
    } else {
        pushu64(a >> shift);
    }
    ++ip;
    break;
}

                case Op::POP:
                    if (!need(1, "pop on empty stack")) return;
                    stack.pop_back();
                    ++ip;
                    break;

                case Op::DUP:
                    if (!need(1, "dup on empty stack")) return;
                    stack.push_back(stack.back());
                    ++ip;
                    break;

                case Op::SWAP:
                    if (!need(2, "swap needs 2 values")) return;
                    swap(stack[stack.size() - 1], stack[stack.size() - 2]);
                    ++ip;
                    break;

                case Op::GT: {
                    long long b, a;
                    if (!pop2(b, a)) return;
                    stack.push_back(b > a ? 1 : 0);
                    ++ip;
                    break;
                }

                case Op::LT: {
                    long long b, a;
                    if (!pop2(b, a)) return;
                    stack.push_back(b < a ? 1 : 0);
                    ++ip;
                    break;
                }

                case Op::EQ: {
                    long long b, a;
                    if (!pop2(b, a)) return;
                    stack.push_back(b == a ? 1 : 0);
                    ++ip;
                    break;
                }

                case Op::JMP:
                    ip = (int)ins.arg;
                    break;

                case Op::JZ: {
                    if (!need(1, "JZ on empty stack")) return;
                    long long v = stack.back();
                    stack.pop_back();
                    ip = (v == 0) ? (int)ins.arg : ip + 1;
                    break;
                }

                case Op::JNZ: {
                    if (!need(1, "JNZ on empty stack")) return;
                    long long v = stack.back();
                    stack.pop_back();
                    ip = (v != 0) ? (int)ins.arg : ip + 1;
                    break;
                }

                case Op::CALL:
                    callstack.push_back(ip + 1);
                    ip = (int)ins.arg;
                    break;

                case Op::RET:
                    if (callstack.empty()) return;
                    ip = callstack.back();
                    callstack.pop_back();
                    break;

                case Op::LOAD: {
                    if (!check_mem(ins.arg, "LOAD")) return;
                    stack.push_back(memory[(size_t)ins.arg]);
                    ++ip;
                    break;
                }

                case Op::STORE: {
                    if (!need(1, "STORE on empty stack")) return;
                    if (!check_mem(ins.arg, "STORE")) return;
                    memory[(size_t)ins.arg] = stack.back();
                    stack.pop_back();
                    ++ip;
                    break;
                }

                case Op::LOADI: {
                    if (!need(1, "LOADI needs index")) return;
                    long long idx = stack.back();
                    stack.pop_back();
                    if (!check_mem(idx, "LOADI")) return;
                    stack.push_back(memory[(size_t)idx]);
                    ++ip;
                    break;
                }

                case Op::STOREI: {
                    if (!need(2, "STOREI needs index + value")) return;
                    long long value = stack.back(); stack.pop_back();
                    long long idx = stack.back(); stack.pop_back();
                    if (!check_mem(idx, "STOREI")) return;
                    memory[(size_t)idx] = value;
                    ++ip;
                    break;
                }

                case Op::HALT:
                    return;
            }
        }
    }
};

int main(int argc, char** argv) {
    try {
        string path = "program.bc";
        if (argc >= 2) path = argv[1];

        ifstream in(path);
        if (!in) throw runtime_error("Cannot open bytecode file: " + path);

        vector<Ins> program;
        vector<string> strings;
        string line;
        bool have_entry = false;
        int entry = 0;

        while (getline(in, line)) {
            line = trim(line);
            if (line.empty()) continue;

            if (!have_entry) {
                stringstream hs(line);
                string first;
                if (!(hs >> first >> entry) || first != "ENTRY") {
                    throw runtime_error("Bad bytecode header");
                }
                have_entry = true;
                continue;
            }

            if (line.rfind("STR", 0) == 0) {
                stringstream ss(line);
                string tag;
                int id;
                string val;
                if (!(ss >> tag >> id)) throw runtime_error("Bad STR line");
                if (tag != "STR") throw runtime_error("Bad STR line");
                if (!(ss >> quoted(val))) throw runtime_error("Bad STR payload");
                if (id < 0) throw runtime_error("Negative STR id");
                if ((size_t)id >= strings.size()) strings.resize((size_t)id + 1);
                strings[(size_t)id] = val;
                continue;
            }

            stringstream ss(line);
            string opstr;
            ss >> opstr;
            if (opstr.empty()) continue;

            Ins ins;
            ins.op = parse_op(opstr);

            if (ins.op == Op::PUSH || ins.op == Op::JMP || ins.op == Op::JZ ||
                ins.op == Op::JNZ  || ins.op == Op::CALL || ins.op == Op::LOAD ||
                ins.op == Op::STORE) {
                if (!(ss >> ins.arg)) throw runtime_error("Missing operand for " + opstr);
            }

            program.push_back(ins);
        }

        if (!have_entry) throw runtime_error("Missing ENTRY header");

        VM vm;
        vm.strings = move(strings);
        vm.run(program, entry);
        return 0;
    } catch (const exception& e) {
        cerr << "[vm] " << e.what() << "\n";
        return 1;
    }
}