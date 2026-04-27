#include <iostream>
#include <cstdlib>
using namespace std;

int main(int argc, char** argv) {
    if (argc < 2) {
        cout << "Usage: Cv <file.cv>\n";
        return 1;
    }

    string file = argv[1];

    // compile -> bytecode
    string cmd1 = "./bytecode " + file;
    if (system(cmd1.c_str()) != 0) {
        cerr << "Compile failed\n";
        return 1;
    }

    // run VM
    string cmd2 = "./vm program.bc";
    if (system(cmd2.c_str()) != 0) {
        cerr << "Runtime error\n";
        return 1;
    }

    return 0;
}