#include <string>
#include <unordered_map>
#include <stdexcept>
#include <iostream>

#include "asm_gen.hpp"

namespace {
    std::string res;
    t_ast ast;

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
        res += "    "; res += s0; res += s1; res += "\n";
    }

    class t_var_map {
        std::unordered_map<std::string, unsigned> map;
        std::unordered_map<std::string, unsigned> outer_map;
        unsigned var_cnt;

    public:
        t_var_map() {
            var_cnt = 0;
        }

        t_var_map(const t_var_map& vm) {
            outer_map = vm.outer_map;
            var_cnt = vm.var_cnt;
            for (const auto& v : vm.map) {
                outer_map[v.first] = v.second;
            }
        }

        auto insert_var(const std::string& var_name) {
            map[var_name] = var_cnt;
            var_cnt++;
            a("subq $4, %rsp");
        }

        auto contains(const std::string& var_name) {
            return map.count(var_name) or outer_map.count(var_name);
        }

        auto can_declare(const std::string& var_name) {
            return map.count(var_name) == 0;
        }

        auto get_var_adr(const std::string& var_name) {
            unsigned x;
            if (map.count(var_name)) {
                x = map[var_name];
            } else {
                x = outer_map[var_name];
            }
            return std::string("-") + std::to_string(4 * (x + 1)) + "(%rbp)";
        }

        auto erase() {
            auto x = std::to_string(4 * map.size());
            a(std::string("addq $") + x + ", %rsp");
        }

        // auto get(const std::string& var_name) {
        //     return map[var_name];
        // }
    };

    auto boolify_eax() {
        a("cmpl $0, %eax");
        a("movl $0, %eax");
        a("setne %al");
    }

    auto set_eax(std::string v) {
        a(std::string("movl ") + v + ", %eax");
    }

    auto gen_con(std::string s) {
        return std::string("$") + s;
    }

    void gen_exp(const t_ast& ast, t_var_map& var_map) {
        if (ast.name == "exp") {
            gen_exp(ast.children[0], var_map);
            return;
        }
        auto rel_bin_op = [&]() {
            gen_exp(ast.children[0], var_map);
            a("push %rax");
            gen_exp(ast.children[1], var_map);
            a("pop %rbx");
            a("cmp %eax, %ebx");
            a("movl $0, %eax");
        };
        if (ast.name == "un_op") {
            gen_exp(ast.children[0], var_map);
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
            auto& var_name = ast.value;
            if (not var_map.contains(var_name)) {
                throw std::runtime_error("undeclared identifier");
            }
            set_eax(var_map.get_var_adr(ast.value));
        } else if (ast.name == "bin_op") {
            if (ast.value == "=") {
                auto& lval = ast.children[0];
                if (lval.name != "variable") {
                    throw std::runtime_error("bad lvalue");
                }
                auto var_name = lval.value;
                if (not var_map.contains(var_name)) {
                    throw std::runtime_error("undeclared identifier");
                }
                auto exp = ast.children[1];
                gen_exp(exp, var_map);
                a("movl %eax, ", var_map.get_var_adr(var_name));
            } else if (ast.value == "||") {
                gen_exp(ast.children[0], var_map);
                boolify_eax();
                auto end = make_label();
                auto l0 = make_label();
                a("cmpl $0, %eax");
                a("je ", l0);
                a("jmp ", end);
                put_label(l0);
                gen_exp(ast.children[1], var_map);
                boolify_eax();
                put_label(end);
            } else if (ast.value == "&&") {
                gen_exp(ast.children[0], var_map);
                boolify_eax();
                auto end = make_label();
                auto l0 = make_label();
                a("cmpl $0, %eax");
                a("jne ", l0);
                a("jmp ", end);
                put_label(l0);
                gen_exp(ast.children[1], var_map);
                boolify_eax();
                put_label(end);
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
                gen_exp(ast.children[0], var_map);
                a("push %rax");
                gen_exp(ast.children[1], var_map);
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
                gen_exp(ast.children[0], var_map);
                a("cmpl $0, %eax");
                a("jne ", cond_true);
                a("jmp ", cond_false);
                put_label(cond_true);
                gen_exp(ast.children[1], var_map);
                a("jmp ", end);
                put_label(cond_false);
                gen_exp(ast.children[2], var_map);
                put_label(end);
            }
        }
    }

    void gen_compound_statement(const t_ast&, t_var_map);

    void gen_statement(const t_ast& c, t_var_map& var_map) {
        if (c.name == "if") {
            auto cond_true = make_label();
            auto cond_false = make_label();
            auto end = make_label();
            gen_exp(c.children[0], var_map);
            a("cmpl $0, %eax");
            a("jne ", cond_true);
            a("jmp ", cond_false);
            put_label(cond_true);
            gen_statement(c.children[1], var_map);
            a("jmp ", end);
            put_label(cond_false);
            if (c.children.size() == 3) {
                gen_statement(c.children[2], var_map);
            }
            put_label(end);
        } else if (c.name == "exp") {
            gen_exp(c.children[0], var_map);
        } else if (c.name == "return") {
            gen_exp(c.children[0], var_map);
            a("jmp main_end");
        } else if (c.name == "compound_statement") {
            gen_compound_statement(c, var_map);
        }
    }

    void gen_block_item(const t_ast& ast, t_var_map& var_map) {
        if (ast.name == "declaration") {
            auto var_name = ast.value;
            if (not var_map.can_declare(var_name)) {
                throw std::runtime_error("variable redefinition");
            }
            var_map.insert_var(var_name);
            if (not ast.children.empty()) {
                gen_exp(ast.children[0], var_map);
                a("movl %eax, ", var_map.get_var_adr(var_name));
            }
        } else {
            gen_statement(ast, var_map);
        }
    }

    void gen_compound_statement(const t_ast& ast, t_var_map outer_var_map) {
        t_var_map var_map(outer_var_map);
        for (auto& c : ast.children) {
            gen_block_item(c, var_map);
        }
        var_map.erase();
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
        gen_block_item(c, var_map);
    }
    a("movl $0, %eax");
    put_label("main_end");
    a("mov %rbp, %rsp");
    a("pop %rbp");
    a("ret");
    return res;
}
