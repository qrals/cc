#include <vector>
#include <string>
#include <stdexcept>
#include <functional>
#include <iostream>

#include "misc.hpp"
#include "ast.hpp"

const std::vector<std::string> un_ops = {"-", "!", "~"};

namespace {
    auto& get_front(std::list<t_lexeme>& ll) {
        if (ll.empty()) {
            throw std::runtime_error("parse error");
        } else {
            return ll.front();
        }
    }

    auto pop_value(std::list<t_lexeme>& ll, const std::string& value) {
        auto lexeme = get_front(ll);
        ll.pop_front();
        if (lexeme.value != value) {
            throw std::runtime_error("parse error");
        }
    }

    auto pop_lexeme(std::list<t_lexeme>& ll, const t_lexeme& l) {
        auto lexeme = get_front(ll);
        ll.pop_front();
        if (lexeme != l) {
            throw std::runtime_error("parse error");
        }
    }

    auto pop_name(std::list<t_lexeme>& ll, const std::string& name) {
        auto lexeme = get_front(ll);
        ll.pop_front();
        if (lexeme.name != name) {
            throw std::runtime_error("parse error");
        }
        return lexeme.value;
    }

    t_ast parse_exp(std::list<t_lexeme>&);

    auto parse_factor(std::list<t_lexeme>& ll) {
        auto front = get_front(ll);
        ll.pop_front();
        if (front.name == "(") {
            auto exp = parse_exp(ll);
            pop_name(ll, ")");
            return exp;
        } else if (contains(un_ops, front.name)) {
            auto f = parse_factor(ll);
            return t_ast("un_op", front.name, {f});
        } else if (front.name == "literal") {
            return t_ast("constant", front.value);
        } else if (front.name == "identifier") {
            if (not ll.empty() and ll.front() == t_lexeme({"(", "("})) {
                ll.pop_front();
                pop_lexeme(ll, {")", ")"});
                return t_ast("function_call", {front.value});
            } else {
                return t_ast("variable", front.value);
            }
        } else {
            throw std::runtime_error("parse error");
        }
    }

    auto parse_left_assoc_bin_op(
        const std::vector<std::string>& ops,
        std::function<t_ast(std::list<t_lexeme>&)> parse_subexp,
        std::list<t_lexeme>& ll
        ) {
        auto res = parse_subexp(ll);
        while (not ll.empty()) {
            auto op = ll.front().name;
            if (not contains(ops, op)) {
                break;
            }
            ll.pop_front();
            auto t = parse_subexp(ll);
            res = t_ast("bin_op", op, {res, t});
        }
        return res;
    }

    t_ast parse_right_assoc_bin_op(
        const std::vector<std::string>& ops,
        std::function<t_ast(std::list<t_lexeme>&)> parse_subexp,
        std::list<t_lexeme>& ll
        ) {
        auto res = parse_subexp(ll);
        if (not ll.empty()) {
            auto op = ll.front().name;
            if (contains(ops, op)) {
                ll.pop_front();
                auto t = parse_right_assoc_bin_op(ops, parse_subexp, ll);
                res = t_ast("bin_op", op, {res, t});
            }
        }
        return res;
    }

    auto parse_mul_exp(std::list<t_lexeme>& ll) {
        return parse_left_assoc_bin_op({"*", "/", "%"}, parse_factor, ll);
    }

    auto parse_plus_exp(std::list<t_lexeme>& ll) {
        return parse_left_assoc_bin_op({"+", "-"}, parse_mul_exp, ll);
    }

    auto parse_lt_exp(std::list<t_lexeme>& ll) {
        std::vector<std::string> ops = {"<", "<=", ">", ">="};
        return parse_left_assoc_bin_op(ops, parse_plus_exp, ll);
    }

    auto parse_eqeq_exp(std::list<t_lexeme>& ll) {
        return parse_left_assoc_bin_op({"==", "!="}, parse_lt_exp, ll);
    }

    auto parse_and_exp(std::list<t_lexeme>& ll) {
        return parse_left_assoc_bin_op({"&&"}, parse_eqeq_exp, ll);
    }

    auto parse_or_exp(std::list<t_lexeme>& ll) {
        return parse_left_assoc_bin_op({"||"}, parse_and_exp, ll);
    }

    t_ast parse_cond_exp(std::list<t_lexeme>& ll) {
        auto x = parse_or_exp(ll);
        if (get_front(ll) == t_lexeme{"?", "?"}) {
            ll.pop_front();
            auto y = parse_exp(ll);
            pop_lexeme(ll, {":", ":"});
            auto z = parse_cond_exp(ll);
            return t_ast("tern_op", "?:", {x, y, z});
        } else {
            return x;
        }
    }

    auto parse_eq_exp(std::list<t_lexeme>& ll) {
        return parse_right_assoc_bin_op({"="}, parse_cond_exp, ll);
    }

    t_ast parse_exp(std::list<t_lexeme>& ll) {
        return parse_eq_exp(ll);
    }

    t_ast parse_block_item(std::list<t_lexeme>&);

    t_ast parse_opt_exp(std::list<t_lexeme>& ll, const t_lexeme& l) {
        std::vector<t_ast> children;
        if (get_front(ll) == l) {
            ll.pop_front();
        } else {
            children.push_back(parse_exp(ll));
            pop_lexeme(ll, l);
        }
        return t_ast("opt_exp", children);
    }

    t_ast parse_exp_statement(std::list<t_lexeme>& ll) {
        auto children = parse_opt_exp(ll, {";", ";"}).children;
        return t_ast("exp_statement", children);
    }

    auto parse_declaration(std::list<t_lexeme>& ll) {
        pop_lexeme(ll, {"keyword", "int"});
        auto name = pop_name(ll, "identifier");
        std::vector<t_ast> children;
        if (get_front(ll) == t_lexeme{"=", "="}) {
            ll.pop_front();
            children.push_back(parse_exp(ll));
        }
        pop_lexeme(ll, {";", ";"});
        return t_ast("declaration", name, children);
    }

    t_ast parse_statement(std::list<t_lexeme>& ll) {
        if (ll.empty()) {
            throw std::runtime_error("parse error");
        }
        if (ll.front() == t_lexeme{"{", "{"}) {
            ll.pop_front();
            std::vector<t_ast> children;
            while (get_front(ll) != t_lexeme{"}", "}"}) {
                children.push_back(parse_block_item(ll));
            }
            ll.pop_front();
            return t_ast("compound_statement", children);
        } else if (ll.front() == t_lexeme{"keyword", "if"}) {
            ll.pop_front();
            pop_lexeme(ll, {"(", "("});
            std::vector<t_ast> children;
            children.push_back(parse_exp(ll));
            pop_lexeme(ll, {")", ")"});
            children.push_back(parse_statement(ll));
            if (get_front(ll) == t_lexeme{"keyword", "else"}) {
                ll.pop_front();
                children.push_back(parse_statement(ll));
            }
            return t_ast("if", children);
        } else if (ll.front() == t_lexeme{"keyword", "return"}) {
            ll.pop_front();
            auto child = parse_exp(ll);
            pop_lexeme(ll, {";", ";"});
            return t_ast("return", {child});
        } else if (ll.front() == t_lexeme{"keyword", "while"}) {
            ll.pop_front();
            pop_lexeme(ll, {"(", "("});
            std::vector<t_ast> children;
            children.push_back(parse_exp(ll));
            pop_lexeme(ll, {")", ")"});
            children.push_back(parse_statement(ll));
            return t_ast("while", children);
        } else if (ll.front() == t_lexeme{"keyword", "do"}) {
            ll.pop_front();
            std::vector<t_ast> children;
            children.push_back(parse_statement(ll));
            pop_lexeme(ll, {"keyword", "while"});
            pop_lexeme(ll, {"(", "("});
            children.push_back(parse_exp(ll));
            pop_lexeme(ll, {")", ")"});
            pop_lexeme(ll, {";", ";"});
            return t_ast("do_while", children);
        } else if (ll.front() == t_lexeme{"keyword", "for"}) {
            ll.pop_front();
            std::vector<t_ast> children;
            pop_lexeme(ll, {"(", "("});
            if (ll.front() == t_lexeme{"keyword", "int"}) {
                children.push_back(parse_declaration(ll));
            } else {
                children.push_back(parse_opt_exp(ll, {";", ";"}));
            }
            children.push_back(parse_opt_exp(ll, {";", ";"}));
            children.push_back(parse_opt_exp(ll, {")", ")"}));
            children.push_back(parse_statement(ll));
            return t_ast("for", children);
        } else if (ll.front() == t_lexeme{"keyword", "break"}) {
            ll.pop_front();
            pop_lexeme(ll, {";", ";"});
            return t_ast("break");
        } else if (ll.front() == t_lexeme{"keyword", "continue"}) {
            ll.pop_front();
            pop_lexeme(ll, {";", ";"});
            return t_ast("continue");
        } else {
            return parse_exp_statement(ll);
        }
    }

    t_ast parse_block_item(std::list<t_lexeme>& ll) {
        if (ll.empty()) {
            throw std::runtime_error("parse error");
        }
        if (ll.front() == t_lexeme{"keyword", "int"}) {
            return parse_declaration(ll);
        } else {
            return parse_statement(ll);
        }
    }

    auto parse_function(std::list<t_lexeme>& ll) {
        pop_value(ll, "int");
        auto name = pop_name(ll, "identifier");
        pop_value(ll, "(");
        pop_value(ll, ")");
        pop_value(ll, "{");
        std::vector<t_ast> children;
        while (true) {
            if (get_front(ll).name == "}") {
                ll.pop_front();
                break;
            }
            children.push_back(parse_block_item(ll));
        }
        return t_ast("function", name, children);
    }
}

t_ast parse_program(std::list<t_lexeme>& ll) {
    std::vector<t_ast> children;
    while (not ll.empty()) {
        children.push_back(parse_function(ll));
    }
    return t_ast("program", children);
}
