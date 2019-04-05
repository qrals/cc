#include <string>
#include <unordered_map>
#include <stdexcept>

#include "asm_gen.hpp"

namespace {
    std::string res;
    t_ast ast;

    typedef std::unordered_map<std::string, unsigned> t_var_map;
    t_var_map var_map;

    std::string make_label() {
        static auto label_count = 0u;
        std::string l = "l";
        l += std::to_string(label_count);
        label_count++;
        return l;
    }

    void put_label(const std::string& l) {
        res += l + ":\n";
    }

    auto a(const std::string& s) {
        res += "    "; res += s; res += "\n";
    }

    auto a(const std::string& s0, const std::string& s1) {
        res += "    "; res += s0; res += " "; res += s1; res += "\n";
    }

    auto boolify_eax() {
        a("cmpl $0, %eax");
        a("movl $0, %eax");
        a("setne %al");
    }

    auto gen_var_adr(unsigned x) {
        return std::string("-") + std::to_string(4 * (x + 1)) + "(%rbp)";
    }

    auto set_eax(std::string v) {
        a(std::string("movl ") + v + ", %eax");
    }

    auto gen_con(std::string s) {
        return std::string("$") + s;
    }

    auto set_var_to_eax(const std::string& var_name) {
        a(std::string("movl %eax, ") + gen_var_adr(var_map[var_name]));
    }

    auto make_var(const std::string& var_name) {
        var_map[var_name] = var_map.size();
        a("subq $4, %rsp");
    }

    auto var_exists(const std::string& var_name) {
        return var_map.count(var_name) > 0;
    }

    void set_eax_to_exp(const t_ast& ast) {
        if (ast.name == "exp") {
            set_eax_to_exp(ast.children[0]);
            return;
        }
        auto rel_bin_op = [&]() {
            set_eax_to_exp(ast.children[0]);
            a("push %rax");
            set_eax_to_exp(ast.children[1]);
            a("pop %rbx");
            a("cmp %eax, %ebx");
            a("movl $0, %eax");
        };
        if (ast.name == "un_op") {
            set_eax_to_exp(ast.children[0]);
            if (ast.value == "-") {
                a("neg %eax");
            } else if (ast.value == "~") {
                a("not %eax");
            } else if (ast.value == "!") {
                a("cmpl $0, %eax");
                a("movl $0, %eax");
                a("sete %al");
            }
        } else if (ast.name == "constant") {
            set_eax(gen_con(ast.value));
        } else if (ast.name == "variable") {
            set_eax(gen_var_adr(var_map.at(ast.value)));
        } else if (ast.name == "bin_op") {
            if (ast.value == "=") {
                auto& lval = ast.children[0];
                if (lval.name != "variable") {
                    throw std::runtime_error("bad lvalue");
                }
                auto var_name = lval.value;
                auto exp = ast.children[1];
                set_eax_to_exp(exp);
                set_var_to_eax(var_name);
            } else if (ast.value == "||") {
                set_eax_to_exp(ast.children[0]);
                boolify_eax();
                a("push %rax");
                set_eax_to_exp(ast.children[1]);
                boolify_eax();
                a("pop %rbx");
                a("or %ebx, %eax");
            } else if (ast.value == "&&") {
                set_eax_to_exp(ast.children[0]);
                boolify_eax();
                a("push %rax");
                set_eax_to_exp(ast.children[1]);
                boolify_eax();
                a("pop %rbx");
                a("and %ebx, %eax");
            } else if (ast.value == "==") {
                rel_bin_op();
                a("sete %al");
            } else if (ast.value == "!=") {
                rel_bin_op();
                a("setne %al");
            } else if (ast.value == "<") {
                rel_bin_op();
                a("setl %al");
            } else if (ast.value == "<=") {
                rel_bin_op();
                a("setle %al");
            } else if (ast.value == ">") {
                rel_bin_op();
                a("setg %al");
            } else if (ast.value == ">=") {
                rel_bin_op();
                a("setge %al");
            } else {
                set_eax_to_exp(ast.children[0]);
                a("push %rax");
                set_eax_to_exp(ast.children[1]);
                a("pop %rbx");
                if (ast.value == "+") {
                    a("add %ebx, %eax");
                } else if (ast.value == "-") {
                    a("sub %eax, %ebx");
                    a("mov %ebx, %eax");
                } else if (ast.value == "*") {
                    a("imul %ebx, %eax");
                } else if (ast.value == "/") {
                    a("mov %eax, %ecx");
                    a("mov %ebx, %eax");
                    a("movl $0, %edx");
                    a("idiv %ecx");
                }
            }
        } else if (ast.name == "tern_op") {
            if (ast.value == "?:") {
                auto cond_true = make_label();
                auto cond_false = make_label();
                auto end = make_label();
                set_eax_to_exp(ast.children[0]);
                a("cmpl $0, %eax");
                a("jne", cond_true);
                a("jmp", cond_false);
                put_label(cond_true);
                set_eax_to_exp(ast.children[1]);
                a("jmp", end);
                put_label(cond_false);
                set_eax_to_exp(ast.children[2]);
                put_label(end);
            }
        }
    }

    void gen_statement_asm(const t_ast& c) {
        if (c.name == "if") {
            auto cond_true = make_label();
            auto cond_false = make_label();
            auto end = make_label();
            set_eax_to_exp(c.children[0]);
            a("cmpl $0, %eax");
            a("jne", cond_true);
            a("jmp", cond_false);
            put_label(cond_true);
            gen_statement_asm(c.children[1]);
            a("jmp", end);
            put_label(cond_false);
            if (c.children.size() == 3) {
                gen_statement_asm(c.children[2]);
            }
            put_label(end);
        } else if (c.name == "declaration") {
            auto var_name = c.value;
            if (var_exists(var_name)) {
                throw std::runtime_error("variable redefinition");
            }
            make_var(var_name);
            if (not c.children.empty()) {
                set_eax_to_exp(c.children[0]);
                set_var_to_eax(var_name);
            }
        } else if (c.name == "exp") {
            set_eax_to_exp(c.children[0]);
        } else if (c.name == "return") {
            set_eax_to_exp(c.children[0]);
            a("mov %rbp, %rsp");
            a("pop %rbp");
            a("ret");
        }
    }
}

std::string gen_asm(const t_ast& ast) {
    auto& fun_ast = ast.children[0];
    res += ".globl main\n";
    put_label("main");
    a("push %rbp");
    a("mov %rsp, %rbp");
    t_var_map var_map;
    for (auto& c : fun_ast.children) {
        gen_statement_asm(c);
    }
    return res;
}
