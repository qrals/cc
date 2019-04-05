#include <vector>
#include <string>
#include <stdexcept>
#include <functional>

#include "misc.hpp"
#include "ast.hpp"

const std::vector<std::string> un_ops = {"-", "!", "~"};

namespace {
    auto& get_front(std::list<t_lexeme>& lexemes) {
        if (lexemes.empty()) {
            throw std::runtime_error("parse error");
        } else {
            return lexemes.front();
        }
    }

    auto pop_value(std::list<t_lexeme>& lexemes, const std::string& value) {
        auto lexeme = get_front(lexemes);
        lexemes.pop_front();
        if (lexeme.value != value) {
            throw std::runtime_error("parse error");
        }
    }

    auto pop_lexeme(std::list<t_lexeme>& lexemes, const t_lexeme& l) {
        auto lexeme = get_front(lexemes);
        lexemes.pop_front();
        if (lexeme != l) {
            throw std::runtime_error("parse error");
        }
    }

    auto pop_name(std::list<t_lexeme>& lexemes, const std::string& name) {
        auto lexeme = get_front(lexemes);
        lexemes.pop_front();
        if (lexeme.name != name) {
            throw std::runtime_error("parse error");
        }
        return lexeme.value;
    }

    t_ast parse_exp(std::list<t_lexeme>&);

    auto parse_factor(std::list<t_lexeme>& lexemes) {
        auto front = get_front(lexemes);
        lexemes.pop_front();
        if (front.name == "(") {
            auto exp = parse_exp(lexemes);
            pop_name(lexemes, ")");
            return exp;
        } else if (contains(un_ops, front.name)) {
            auto f = parse_factor(lexemes);
            return t_ast("un_op", front.name, {f});
        } else if (front.name == "literal") {
            return t_ast("constant", front.value);
        } else if (front.name == "identifier") {
            return t_ast("variable", front.value);
        } else {
            throw std::runtime_error("parse error");
        }
    }

    auto parse_left_assoc_bin_op(
        const std::vector<std::string>& ops,
        std::function<t_ast(std::list<t_lexeme>&)> parse_subexp,
        std::list<t_lexeme>& lexemes
        ) {
        auto res = parse_subexp(lexemes);
        while (not lexemes.empty()) {
            auto op = lexemes.front().name;
            if (not contains(ops, op)) {
                break;
            }
            lexemes.pop_front();
            auto t = parse_subexp(lexemes);
            res = t_ast("bin_op", op, {res, t});
        }
        return res;
    }

    t_ast parse_right_assoc_bin_op(
        const std::vector<std::string>& ops,
        std::function<t_ast(std::list<t_lexeme>&)> parse_subexp,
        std::list<t_lexeme>& lexemes
        ) {
        auto res = parse_subexp(lexemes);
        if (not lexemes.empty()) {
            auto op = lexemes.front().name;
            if (contains(ops, op)) {
                lexemes.pop_front();
                auto t = parse_right_assoc_bin_op(ops, parse_subexp, lexemes);
                res = t_ast("bin_op", op, {res, t});
            }
        }
        return res;
    }

    auto parse_mul_exp(std::list<t_lexeme>& lexemes) {
        return parse_left_assoc_bin_op({"*", "/"}, parse_factor, lexemes);
    }

    auto parse_plus_exp(std::list<t_lexeme>& lexemes) {
        return parse_left_assoc_bin_op({"+", "-"}, parse_mul_exp, lexemes);
    }

    auto parse_lt_exp(std::list<t_lexeme>& lexemes) {
        std::vector<std::string> ops = {"<", "<=", ">", ">="};
        return parse_left_assoc_bin_op(ops, parse_plus_exp, lexemes);
    }

    auto parse_eqeq_exp(std::list<t_lexeme>& lexemes) {
        return parse_left_assoc_bin_op({"==", "!="}, parse_lt_exp, lexemes);
    }

    auto parse_and_exp(std::list<t_lexeme>& lexemes) {
        return parse_left_assoc_bin_op({"&&"}, parse_eqeq_exp, lexemes);
    }

    auto parse_or_exp(std::list<t_lexeme>& lexemes) {
        return parse_left_assoc_bin_op({"||"}, parse_and_exp, lexemes);
    }

    t_ast parse_cond_exp(std::list<t_lexeme>& lexemes) {
        auto x = parse_or_exp(lexemes);
        if (get_front(lexemes) == t_lexeme{"?", "?"}) {
            lexemes.pop_front();
            auto y = parse_exp(lexemes);
            pop_lexeme(lexemes, {":", ":"});
            auto z = parse_cond_exp(lexemes);
            return t_ast("tern_op", "?:", {x, y, z});
        } else {
            return x;
        }
    }

    auto parse_eq_exp(std::list<t_lexeme>& lexemes) {
        return parse_right_assoc_bin_op({"="}, parse_cond_exp, lexemes);
    }

    t_ast parse_exp(std::list<t_lexeme>& lexemes) {
        return parse_eq_exp(lexemes);
    }

    t_ast parse_statement(std::list<t_lexeme>& lexemes) {
        if (lexemes.empty()) {
            throw std::runtime_error("parse error");
        }
        if (lexemes.front() == t_lexeme{"keyword", "if"}) {
            lexemes.pop_front();
            pop_lexeme(lexemes, {"(", "("});
            std::vector<t_ast> children;
            children.push_back(parse_exp(lexemes));
            pop_lexeme(lexemes, {")", ")"});
            children.push_back(parse_statement(lexemes));
            if (get_front(lexemes) == t_lexeme{"keyword", "else"}) {
                lexemes.pop_front();
                children.push_back(parse_statement(lexemes));
            }
            return t_ast("if", children);
        } else if (lexemes.front() == t_lexeme{"keyword", "int"}) {
            lexemes.pop_front();
            auto name = pop_name(lexemes, "identifier");
            std::vector<t_ast> children;
            if (get_front(lexemes) == t_lexeme{"=", "="}) {
                lexemes.pop_front();
                children.push_back(parse_exp(lexemes));
                pop_lexeme(lexemes, {";", ";"});
            } else {
                pop_lexeme(lexemes, {";", ";"});
            }
            return t_ast("declaration", name, children);
        } else if (lexemes.front() == t_lexeme{"keyword", "return"}) {
            lexemes.pop_front();
            auto child = parse_exp(lexemes);
            pop_lexeme(lexemes, {";", ";"});
            return t_ast("return", {child});
        } else {
            auto child = parse_exp(lexemes);
            pop_lexeme(lexemes, {";", ";"});
            return t_ast("exp", {child});
        }
    }

    auto parse_function(std::list<t_lexeme>& lexemes) {
        pop_value(lexemes, "int");
        auto name = pop_name(lexemes, "identifier");
        pop_value(lexemes, "(");
        pop_value(lexemes, ")");
        pop_value(lexemes, "{");
        std::vector<t_ast> children;
        while (true) {
            if (get_front(lexemes).name == "}") {
                lexemes.pop_front();
                break;
            }
            children.push_back(parse_statement(lexemes));
        }
        return t_ast("function", name, children);
    }
}

t_ast parse_program(std::list<t_lexeme>& lexemes) {
    return t_ast("program", {parse_function(lexemes)});
}
