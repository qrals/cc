#pragma once

#include <list>
#include <string>
#include <vector>
#include "lex.hpp"

struct t_ast {
    std::string name;
    std::string value;
    std::vector<t_ast> children;

    t_ast() {
    }

    typedef std::vector<t_ast> t_ast_vec;
    t_ast(const std::string& n, const std::string& v, const t_ast_vec& c) {
        name = n;
        value = v;
        children = c;
    }

    t_ast(const std::string& n) {
        name = n;
    }

    t_ast(const std::string& n, const std::vector<t_ast>& c) {
        name = n;
        children = c;
    }

    t_ast(const std::string& n, const std::string& v) {
        name = n;
        value = v;
    }
};

t_ast parse_program(std::list<t_lexeme>&);
