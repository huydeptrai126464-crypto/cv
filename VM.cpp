#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <cctype>
using namespace std;

enum class Op {
    PUSH, ADD, SUB, MUL, DIV,
    PRINT, POP, DUP, SWAP,
    JMP, JZ, JNZ, CALL, RET,
    GT, LT, EQ, INPUT,PRINT_STR,
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
    if (s == "PUSH")  return Op::PUSH;
    if (s == "ADD")   return Op::ADD;
    if (s == "SUB")   return Op::SUB;
    if (s == "MUL")   return Op::MUL;
    if (s == "DIV")   return Op::DIV;
    if (s == "PRINT") return Op::PRINT;
    if (s == "PRINT_STR") return Op::PRINT_STR;
    if (s == "POP")   return Op::POP;
    if (s == "DUP")   return Op::DUP;
    if (s == "SWAP")  return Op::SWAP;
    if (s == "JMP")   return Op::JMP;
    if (s == "JZ")    return Op::JZ;
    if (s == "JNZ")   return Op::JNZ;
    if (s == "CALL")  return Op::CALL;
    if (s == "RET")   return Op::RET;
    if (s == "GT")    return Op::GT;
    if (s == "LT")    return Op::LT;
    if (s == "EQ")    return Op::EQ;
    if (s == "INPUT") return Op::INPUT;
    if (s == "HALT")  return Op::HALT;
    throw runtime_error("Unknown opcode: " + s);
}

struct VM {
    vector<long long> stack;
    vector<int> callstack;

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

    void run(const vector<Ins>& p, int entry) {
        int ip = entry;

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
    vector<char> buf;

    while (true) {
        if (!need(1, "print_str underflow")) return;
        long long c = stack.back();
        stack.pop_back();

        if (c == 0) break;
        buf.push_back((char)c);
    }

    reverse(buf.begin(), buf.end());

    for (char c : buf) cout << c;
    cout << "\n";
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

        string first;
        int entry = 0;
        if (!(in >> first >> entry) || first != "ENTRY") {
            throw runtime_error("Bad bytecode header");
        }

        vector<Ins> program;
        string opstr;
        while (in >> opstr) {
            Ins ins;
            ins.op = parse_op(opstr);

            if (ins.op == Op::PUSH || ins.op == Op::JMP || ins.op == Op::JZ ||
                ins.op == Op::JNZ  || ins.op == Op::CALL) {
                if (!(in >> ins.arg)) throw runtime_error("Missing operand for " + opstr);
            }

            program.push_back(ins);
        }

        VM vm;
        vm.run(program, entry);
        return 0;
    } catch (const exception& e) {
        cerr << "[vm] " << e.what() << "\n";
        return 1;
    }
}