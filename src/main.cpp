#include <fstream>
#include <sstream>
#include <iostream>
#include <utility>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <list>
#include <functional>
#include <initializer_list>
#include <unordered_map>

#include "ast.hpp"
#include "asm_gen.hpp"
#include "lex.hpp"

std::ofstream log;

void print(const t_ast& t, unsigned level = 0) {
    auto print_spaces = [&](unsigned n) {
        for (auto i = 0u; i < n; i++) {
            log << " ";
        }
    };
    print_spaces(4 * level);
    log << t.name;
    if (t.value.size() > 0) {
        log << " : " << t.value;
    }
    log << "\n";
    for (auto& c : t.children) {
        print(c, level + 1);
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "error : bad argument list\n";
        return 1;
    }
    std::ifstream is(argv[1]);
    if (!is.good()) {
        std::cerr << "error : could not open input file\n";
        return 1;
    }

    log.open("log.txt");
    auto sep = [&]() {
        log << "\n";
        log << "-------------\n";
        log << "\n";
    };

    std::stringstream buf;
    buf << is.rdbuf();
    auto src = buf.str();
    auto tokens = lex(src);

    for (auto& l : tokens) {
        log << l.name << " -- " << l.value << "\n";
    }
    sep();

    try {
        auto ast = parse_program(tokens);
        print(ast);
        sep();
        auto res = gen_asm(ast);
        log << res;
        std::ofstream os(argv[2]);
        if (!os.good()) {
            std::cerr << "error : could not open output file\n";
            return 1;
        }
        os << res;
    } catch (const std::runtime_error& e) {
        std::cerr << "error : " << e.what() << "\n";
        return 1;
    }
}
