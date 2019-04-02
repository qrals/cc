#include <fstream>
#include <sstream>
#include <iostream>
#include <utility>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <list>

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
        auto sym = source[i];
        i++;
        std::string val;
        val += sym;
        std::vector<std::string> keywords = {"int", "return"};
        std::vector<char> tmp = {
            '{', '}', '(', ')', ';', '-', '~', '!', '+', '/', '*'
        };
        if (contains(tmp, sym)) {
            auto s = std::string(1, sym);
            res.push_back(t_lexeme(s, s));
        } else if (is_digit(sym)) {
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

auto parse_term(std::list<t_lexeme>& lexemes) {
    auto res = parse_factor(lexemes);
    while (not lexemes.empty()) {
        auto name = lexemes.front().name;
        if (not contains({"*", "/"}, name)) {
            break;
        }
        lexemes.pop_front();
        auto f = parse_factor(lexemes);
        res = t_ast("bin_op", name, {res, f});
    }
    return res;
}

t_ast parse_exp(std::list<t_lexeme>& lexemes) {
    auto res = parse_term(lexemes);
    while (not lexemes.empty()) {
        auto name = lexemes.front().name;
        if (not contains({"+", "-"}, name)) {
            break;
        }
        lexemes.pop_front();
        auto t = parse_term(lexemes);
        res = t_ast("bin_op", name, {res, t});
    }
    return res;
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

void print_spaces(unsigned n) {
    for (auto i = 0u; i < n; i++) {
        std::cout << " ";
    }
}

void print(const t_ast& t, unsigned level = 0) {
    print_spaces(4 * level);
    std::cout << t.name;
    if (t.value.size() > 0) {
        std::cout << " : " << t.value;
    }
    std::cout << "\n";
    for (auto& c : t.children) {
        print(c, level + 1);
    }
}

auto instr(std::string& a, const std::string& b) {
    a += "    "; a += b; a += "\n";
}

std::string gen_exp_asm(const t_ast& ast) {
    std::string res;
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
    std::stringstream buf;
    buf << is.rdbuf();
    auto src = buf.str();
    auto tokens = lex(src);
    try {
        auto ast = parse_program(tokens);
        // print(ast);
        std::ofstream os(argv[2]);
        if (!os.good()) {
            std::cerr << "error : could not open output file\n";
            return 1;
        }
        os << gen_asm(ast);
    } catch (const std::runtime_error& e) {
        std::cout << e.what() << "\n";
        return 1;
    }
}
