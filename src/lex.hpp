#pragma once

#include <list>
#include <string>
#include <initializer_list>

struct t_lexeme {
    std::string name;
    std::string value;

    t_lexeme(const std::string& n_name, const std::string& n_value) {
        name = n_name;
        value = n_value;
    }

    t_lexeme(const std::initializer_list<std::string>& l) {
        name = *(l.begin());
        value = *(l.begin() + 1);
    }

    bool operator==(const t_lexeme& l) const {
        return name == l.name and value == l.value;
    }

    bool operator!=(const t_lexeme& l) const {
        return !((*this) == l);
    }
};

std::list<t_lexeme> lex(const std::string&);
