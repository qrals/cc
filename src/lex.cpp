#include <string>
#include <vector>

#include "misc.hpp"
#include "lex.hpp"

namespace {
    auto is_id_char(char ch) {
        return ch == '_' or isalnum(ch);
    }

    auto is_digit(char ch) {
        return '0' <= ch and ch <= '9';
    }
}

std::vector<t_lexeme> lex(const std::string& source) {
    std::vector<t_lexeme> res;
    auto i = 0u;
    while (i < source.size()) {
        std::vector<std::string> keywords = {
            "int", "return", "if", "else", "while", "for", "do",
            "continue", "break"
        };
        std::vector<std::string> tt = {
            "&&", "||", "==", "!=", "<=", ">=", "<", ">", "=", "?", ":",
            "{", "}", "(", ")", ";", "-", "~", "!", "+", "/", "*", "%", "&",
            "[", "]"
        };
        auto found = false;
        for (auto& t : tt) {
            if (source.substr(i, t.size()) == t) {
                res.push_back(t_lexeme(t, t));
                found = true;
                i += t.size();
                break;
            }
        }
        if (found) {
            continue;
        }
        auto sym = source[i];
        i++;
        std::string val;
        val += sym;
        if (is_digit(sym)) {
            while (i < source.size() and isdigit(source[i])) {
                val += source[i];
                i++;
            }
            res.push_back(t_lexeme("literal", val));
        } else if (is_id_char(sym)) {
            while (i < source.size() and is_id_char(source[i])) {
                val += source[i];
                i++;
            }
            if (contains(keywords, val)) {
                res.push_back(t_lexeme("keyword", val));
            } else {
                res.push_back(t_lexeme("identifier", val));
            }
        }
    }
    return res;
}
