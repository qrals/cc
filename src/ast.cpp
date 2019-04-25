#include <vector>
#include <string>
#include <stdexcept>
#include <functional>
#include <iostream>

#include "misc.hpp"
#include "ast.hpp"

namespace {
    std::vector<t_lexeme> ll;
    unsigned idx;

    auto init(std::vector<t_lexeme>& n_ll) {
        ll = n_ll;
        idx = 0;
    }

    auto& peek() {
        if (idx < ll.size()) {
            return ll[idx];
        } else {
            throw std::runtime_error("parse error");
        }
    }

    auto advance() {
        idx++;
    }

    auto empty() {
        return idx >= ll.size();
    }

    auto cmp(const std::string& name) {
        return not empty() and peek().name == name;
    }

    auto pop_lexeme(const t_lexeme& l) {
        auto lexeme = peek();
        advance();
        if (lexeme != l) {
            throw std::runtime_error("parse error");
        }
    }

    auto pop(const std::string& name) {
        auto lexeme = peek();
        advance();
        if (lexeme.name != name) {
            throw std::runtime_error("parse error");
        }
        return lexeme.value;
    }

    auto pop_punctuator(const std::string& s) {
        pop(s);
    }

    auto pop_keyword(const std::string& kw) {
        auto& lexeme = peek();
        if (lexeme == t_lexeme{"keyword", kw}) {
            advance();
        } else {
            throw std::runtime_error("parse error");
        }
    }

    t_ast exp();

    auto prim_exp() {
        auto front = peek();
        if (front.name == "identifier") {
            advance();
            return t_ast("identifier", front.value);
        } else if (front.name == "literal") {
            advance();
            return t_ast("constant", front.value);
        } else if (front.name == "(") {
            advance();
            auto res = exp();
            pop(")");
            return res;
        } else {
            throw std::runtime_error("parse error");
        }
    }

    auto postfix_exp() {
        auto res = prim_exp();
        while (not empty()) {
            auto front = peek();
            if (front.name == "(") {
                advance();
                pop(")");
                res = t_ast("function_call", {res});
            } else if (front.name == "[") {
                advance();
                auto e = exp();
                pop("]");
                res = t_ast("un_op", "*", {t_ast("bin_op", "+", {res, e})});
            } else {
                break;
            }
        }
        return res;
    }

    t_ast cast_exp();

    auto un_exp() {
        auto front = peek();
        std::vector<std::string> un_ops = {"&", "*", "+", "-", "~", "!"};
        if (contains(un_ops, front.name)) {
            advance();
            auto e = cast_exp();
            return t_ast("un_op", front.name, {e});
        } else {
            return postfix_exp();
        }
    }

    t_ast cast_exp() {
        return un_exp();
    }

    auto left_assoc_bin_op(
        const std::vector<std::string>& ops,
        std::function<t_ast()> subexp
        ) {
        auto res = subexp();
        while (not empty()) {
            auto front = peek();
            auto& op = front.name;
            if (not contains(ops, op)) {
                break;
            }
            advance();
            auto t = subexp();
            res = t_ast("bin_op", op, {res, t});
        }
        return res;
    }

    auto mul_exp() {
        return left_assoc_bin_op({"*", "/", "%"}, cast_exp);
    }

    auto add_exp() {
        return left_assoc_bin_op({"+", "-"}, mul_exp);
    }

    auto shift_exp() {
        return add_exp();
    }

    auto rel_exp() {
        return left_assoc_bin_op({"<", "<=", ">", ">="}, shift_exp);
    }

    auto eql_exp() {
        return left_assoc_bin_op({"==", "!="}, rel_exp);
    }

    auto bit_and_exp() {
        return left_assoc_bin_op({"&"}, eql_exp);
    }

    auto bit_xor_exp() {
        return left_assoc_bin_op({"^"}, bit_and_exp);
    }

    auto bit_or_exp() {
        return left_assoc_bin_op({"|"}, bit_xor_exp);
    }

    auto and_exp() {
        return left_assoc_bin_op({"&&"}, bit_or_exp);
    }

    auto or_exp() {
        return left_assoc_bin_op({"||"}, and_exp);
    }

    t_ast cond_exp() {
        auto x = or_exp();
        if (not empty() and peek().name == "?") {
            advance();
            auto y = exp();
            pop(":");
            auto z = cond_exp();
            return t_ast("tern_op", "?:", {x, y, z});
        } else {
            return x;
        }
    }

    t_ast assign_exp() {
        auto old_idx = idx;
        auto x = un_exp();
        t_ast res;
        if (not empty()) {
            auto front = peek();
            auto& op = front.name;
            std::vector<std::string> ops = {"="};
            if (contains(ops, op)) {
                advance();
                auto y = assign_exp();
                res = t_ast("bin_op", op, {x, y});
            } else {
                idx = old_idx;
                res = cond_exp();
            }
        } else {
            res = x;
        }
        return res;
    }

    t_ast exp() {
        return left_assoc_bin_op({","}, assign_exp);
    }

    t_ast const_exp() {
        return cond_exp();
    }

    t_ast block_item();

    t_ast opt_exp(const t_lexeme& end) {
        std::vector<t_ast> children;
        if (peek() == end) {
            advance();
        } else {
            children.push_back(exp());
            pop_lexeme(end);
        }
        return t_ast("opt_exp", children);
    }

    auto exp_statement() {
        auto children = opt_exp({";", ";"}).children;
        return t_ast("exp_statement", children);
    }

    t_ast pointer_opt();

    t_ast pointer() {
        pop("*");
        if (cmp("*")) {
            return t_ast("pointer", {pointer()});
        } else {
            return t_ast("pointer");
        }
    }

    t_ast pointer_opt() {
        if (cmp("*")) {
            return pointer();
        } else {
            return t_ast();
        }
    }

    void declarator(t_ast& type, std::string& name) {
        if (cmp("*")) {
            advance();
            type = t_ast("pointer", {type});
            declarator(type, name);
        } else {
            if (cmp("identifier")) {
                name = pop("identifier");
            } else {
                pop("(");
                declarator(type, name);
                pop(")");
            }
            while (true) {
                if (cmp("[")) {
                    advance();
                    auto size = const_exp();
                    pop("]");
                    type = t_ast("array", {type, size});
                } else {
                    break;
                }
            }
        }
    }

    auto declaration_specifiers() {
        advance();
        return t_ast({t_ast("int")});
    }

    auto declaration() {
        auto type = declaration_specifiers();
        std::string name;
        declarator(type, name);
        std::vector<t_ast> children = {type};
        if (peek() == t_lexeme{"=", "="}) {
            advance();
            children.push_back(assign_exp());
        }
        pop_punctuator(";");
        return t_ast("declaration", name, children);
    }

    auto compound_statement() {
        advance();
        std::vector<t_ast> children;
        while (peek().name != "}") {
            children.push_back(block_item());
        }
        advance();
        return t_ast("compound_statement", children);
    }

    t_ast statement();

    auto selection_statement() {
        advance();
        pop("(");
        std::vector<t_ast> children;
        children.push_back(exp());
        pop(")");
        children.push_back(statement());
        if (peek() == t_lexeme{"keyword", "else"}) {
            advance();
            children.push_back(statement());
        }
        return t_ast("if", children);
    }

    auto jump_statement() {
        auto front = peek();
        t_ast res;
        if (front.name == "keyword") {
            if (front.value == "return") {
                advance();
                auto child = exp();
                pop(";");
                res = t_ast("return", {child});
            } else if (front.value == "break") {
                advance();
                pop(";");
                res = t_ast("break");
            } else if (front.value == "continue") {
                advance();
                pop(";");
                res = t_ast("continue");
            }
        } else {
            throw std::runtime_error("parse error");
        }
        return res;
    }

    auto iteration_statement() {
        auto front = peek();
        t_ast res;
        if (front.name == "keyword") {
            if (front.value == "while") {
                advance();
                pop("(");
                std::vector<t_ast> children;
                children.push_back(exp());
                pop(")");
                children.push_back(statement());
                res = t_ast("while", children);
            } else if (front.value == "do") {
                advance();
                std::vector<t_ast> children;
                children.push_back(statement());
                pop_keyword("while");
                pop_punctuator("(");
                children.push_back(exp());
                pop_punctuator(")");
                pop_punctuator(";");
                res = t_ast("do_while", children);
            } else if (front.value == "for") {
                advance();
                std::vector<t_ast> children;
                pop_punctuator("(");
                if (peek() == t_lexeme{"keyword", "int"}) {
                    children.push_back(declaration());
                } else {
                    children.push_back(opt_exp({";", ";"}));
                }
                children.push_back(opt_exp({";", ";"}));
                children.push_back(opt_exp({")", ")"}));
                children.push_back(statement());
                res = t_ast("for", children);
            }
        } else {
            throw std::runtime_error("parse error");
        }
        return res;
    }

    auto labeled_statement() {
        return t_ast();
    }

    typedef std::vector<std::string> t_strv;

    t_ast statement() {
        if (peek().name == "{") {
            return compound_statement();
        } else if (peek().name == "keyword") {
            auto v = peek().value;
            auto jump_keywords = t_strv{"goto", "continue", "break", "return"};
            if (contains(t_strv{"if", "switch"}, v)) {
                return selection_statement();
            } else if (contains(t_strv{"while", "do", "for"}, v)) {
                return iteration_statement();
            } else if (contains(jump_keywords, v)) {
                return jump_statement();
            } else if (contains({"case", "default"}, v)) {
                return labeled_statement();
            } else {
                throw std::runtime_error("parse error");
            }
        } else {
            return exp_statement();
        }
    }

    t_ast block_item() {
        if (empty()) {
            throw std::runtime_error("parse error");
        }
        if (peek() == t_lexeme{"keyword", "int"}) {
            return declaration();
        } else {
            return statement();
        }
    }

    auto function_definition() {
        pop_keyword("int");
        auto func_name = pop("identifier");
        pop_punctuator("(");
        pop_punctuator(")");
        auto children = compound_statement().children;
        return t_ast("function", func_name, children);
    }
}

t_ast parse_program(std::vector<t_lexeme>& n_ll) {
    init(n_ll);
    std::vector<t_ast> children;
    while (not empty()) {
        children.push_back(function_definition());
    }
    return t_ast("program", children);
}
