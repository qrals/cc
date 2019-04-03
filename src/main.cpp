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

std::ofstream log;

const std::vector<std::string> un_ops = {"-", "!", "~"};

template <typename t>
bool contains(const std::vector<t>& vec, const t& elt) {
    return std::find(vec.begin(), vec.end(), elt) != vec.end();
}

struct t_lexeme {
    std::string name;
    std::string value;
    t_lexeme(const std::string& n_name, const std::string& n_value) {
        name = n_name;
        value = n_value;
    }
};

auto is_digit(char ch) {
    return '0' <= ch and ch <= '9';
}

auto is_id_char(char ch) {
    return ch == '_' or isalnum(ch);
}

auto lex(const std::string& source) {
    std::list<t_lexeme> res;
    auto i = 0u;
    while (i < source.size()) {
        std::vector<std::string> keywords = {"int", "return"};
        std::vector<std::string> tt = {
            "&&", "||", "==", "!=", "<=", ">=", "<", ">",
            "{", "}", "(", ")", ";", "-", "~", "!", "+", "/", "*"
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

struct t_ast {
    std::string name;
    std::string value;
    std::vector<t_ast> children;
    t_ast(const std::string& n, std::string& v, const std::vector<t_ast>& c) {
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

auto pop_value(std::list<t_lexeme>& lexemes, const std::string& value) {
    if (lexemes.empty()) {
        throw std::runtime_error("parse error");
    } else {
        auto lexeme = lexemes.front();
        lexemes.pop_front();
        if (lexeme.value != value) {
            throw std::runtime_error("parse error");
        }
    }
}

auto pop_name(std::list<t_lexeme>& lexemes, const std::string& name) {
    if (lexemes.empty()) {
        throw std::runtime_error("parse error");
    } else {
        auto lexeme = lexemes.front();
        lexemes.pop_front();
        if (lexeme.name != name) {
            throw std::runtime_error("parse error");
        }
        return lexeme.value;
    }
}

t_ast parse_exp(std::list<t_lexeme>&);

auto parse_factor(std::list<t_lexeme>& lexemes) {
    if (lexemes.empty()) {
        throw std::runtime_error("parse error");
    }
    auto front = lexemes.front();
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

auto parse_eq_exp(std::list<t_lexeme>& lexemes) {
    return parse_left_assoc_bin_op({"==", "!="}, parse_lt_exp, lexemes);
}

auto parse_and_exp(std::list<t_lexeme>& lexemes) {
    return parse_left_assoc_bin_op({"&&"}, parse_eq_exp, lexemes);
}

auto parse_or_exp(std::list<t_lexeme>& lexemes) {
    return parse_left_assoc_bin_op({"||"}, parse_and_exp, lexemes);
}

t_ast parse_exp(std::list<t_lexeme>& lexemes) {
    return parse_or_exp(lexemes);
}

auto parse_statement(std::list<t_lexeme>& lexemes) {
    pop_value(lexemes, "return");
    auto child = parse_exp(lexemes);
    pop_value(lexemes, ";");
    return t_ast("statement", {child});
}

auto parse_function(std::list<t_lexeme>& lexemes) {
    pop_value(lexemes, "int");
    auto name = pop_name(lexemes, "identifier");
    pop_value(lexemes, "(");
    pop_value(lexemes, ")");
    pop_value(lexemes, "{");
    auto child = parse_statement(lexemes);
    pop_value(lexemes, "}");
    return t_ast("function", name, {child});
}

auto parse_program(std::list<t_lexeme>& lexemes) {
    return t_ast("program", {parse_function(lexemes)});
}

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

auto instr(std::string& a, const std::string& b) {
    a += "    "; a += b; a += "\n";
}

std::string gen_exp_asm(const t_ast& ast) {
    std::string res;
    auto rel_bin_op = [&]() {
        res += gen_exp_asm(ast.children[0]);
        instr(res, "push %rax");
        res += gen_exp_asm(ast.children[1]);
        instr(res, "pop %rbx");
        instr(res, "cmp %eax, %ebx");
        instr(res, "movl $0, %eax");
    };
    auto boolify = [&]() {
        instr(res, "cmpl $0, %eax");
        instr(res, "movl $0, %eax");
        instr(res, "setne %al");
    };
    if (ast.name == "un_op") {
        res += gen_exp_asm(ast.children[0]);
        if (ast.value == "-") {
            instr(res, "neg %eax");
        } else if (ast.value == "~") {
            instr(res, "not %eax");
        } else if (ast.value == "!") {
            instr(res, "cmpl $0, %eax");
            instr(res, "movl $0, %eax");
            instr(res, "sete %al");
        }
    } else if (ast.name == "constant") {
        res += "    movl $"; res += ast.value; res += ", %eax\n";
    } else if (ast.name == "bin_op") {
        if (ast.value == "||") {
            res += gen_exp_asm(ast.children[0]);
            boolify();

            instr(res, "push %rax");

            res += gen_exp_asm(ast.children[1]);
            boolify();

            instr(res, "pop %rbx");
            instr(res, "or %ebx, %eax");
        } else if (ast.value == "&&") {
            res += gen_exp_asm(ast.children[0]);
            boolify();

            instr(res, "push %rax");

            res += gen_exp_asm(ast.children[1]);
            boolify();

            instr(res, "pop %rbx");
            instr(res, "and %ebx, %eax");
        } else if (ast.value == "==") {
            rel_bin_op();
            instr(res, "sete %al");
        } else if (ast.value == "!=") {
            rel_bin_op();
            instr(res, "setne %al");
        } else if (ast.value == "<") {
            rel_bin_op();
            instr(res, "setl %al");
        } else if (ast.value == "<=") {
            rel_bin_op();
            instr(res, "setle %al");
        } else if (ast.value == ">") {
            rel_bin_op();
            instr(res, "setg %al");
        } else if (ast.value == ">=") {
            rel_bin_op();
            instr(res, "setge %al");
        } else {
            res += gen_exp_asm(ast.children[0]);
            instr(res, "push %rax");
            res += gen_exp_asm(ast.children[1]);
            instr(res, "pop %rbx");
            if (ast.value == "+") {
                instr(res, "add %ebx, %eax");
            } else if (ast.value == "-") {
                instr(res, "sub %eax, %ebx");
                instr(res, "mov %ebx, %eax");
            } else if (ast.value == "*") {
                instr(res, "imul %ebx, %eax");
            } else if (ast.value == "/") {
                instr(res, "mov %eax, %ecx");
                instr(res, "mov %ebx, %eax");
                instr(res, "movl $0, %edx");
                instr(res, "idiv %ecx");
            }
        }
    }
    return res;
}

auto gen_asm(const t_ast& ast) {
    auto& fun_ast = ast.children[0];
    std::string res = ".globl "; res += fun_ast.value; res += "\n";
    res += fun_ast.value; res += ":\n";
    auto& ret_ast = fun_ast.children[0];
    auto& exp_ast = ret_ast.children[0];
    res += gen_exp_asm(exp_ast);
    instr(res, "ret");
    return res;
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
        std::ofstream os(argv[2]);
        if (!os.good()) {
            std::cerr << "error : could not open output file\n";
            return 1;
        }
        auto res = gen_asm(ast);
        log << res;
        os << res;
    } catch (const std::runtime_error& e) {
        std::cout << e.what() << "\n";
        return 1;
    }
}
