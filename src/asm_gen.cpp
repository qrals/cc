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

    auto a(const std::string& x, const std::string& y, const std::string& z) {
        res += "    "; res += x; res += " ";
        res += y; res += ", "; res += z; res += "\n";
    }

    auto gen_con(std::string s) {
        return std::string("$") + s;
    }

    unsigned get_size(const t_ast& type) {
        if (type.name == "array") {
            auto arr_size_exp =  type.children[1];
            if (arr_size_exp.name != "constant") {
                throw std::runtime_error("bad array size");
            }
            auto arr_size = unsigned(std::stoul(arr_size_exp.value));
            return get_size(type.children[0]) * arr_size;
        } else {
            return 8u;
        }
    }

    class t_var_map {
        struct t_var_data {
            unsigned offset;
            t_ast type;
        };

        std::unordered_map<std::string, t_var_data> map;
        std::unordered_map<std::string, t_var_data> outer_map;
        unsigned cur_offset;
        unsigned base_offset;

    public:
        t_var_map() {
            base_offset = 0;
            cur_offset = 0;
        }

        t_var_map(const t_var_map& vm) {
            outer_map = vm.outer_map;
            cur_offset = vm.cur_offset;
            base_offset = vm.cur_offset;
            for (const auto& v : vm.map) {
                outer_map[v.first] = v.second;
            }
        }

        auto insert_var(const std::string& var_name, const t_ast& type) {
            auto var_size = get_size(type);
            cur_offset += var_size;
            a("subq", gen_con(std::to_string(var_size)), "%rsp");
            map[var_name] = {cur_offset, type};
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
                x = map[var_name].offset;
            } else {
                x = outer_map[var_name].offset;
            }
            return std::string("-") + std::to_string(x) + "(%rbp)";
        }

        auto get_var_type(const std::string& var_name) {
            t_ast res;
            if (map.count(var_name)) {
                res = map[var_name].type;
            } else {
                res = outer_map[var_name].type;
            }
            return res;
        }

        auto erase() {
            auto x = std::to_string(cur_offset - base_offset);
            a("addq", gen_con(x), "%rsp");
        }
    };

    struct t_context {
        t_var_map var_map;
        std::string loop_body_end;
        std::string loop_end;
        std::string func_name;
    };

    auto boolify_rax() {
        a("cmpq $0, %rax");
        a("movq $0, %rax");
        a("setne %al");
    }

    auto set_rax(std::string v) {
        a(std::string("movq ") + v + ", %rax");
    }

    t_ast gen_exp(const t_ast& ast, t_var_map& var_map) {
        auto rel_bin_op = [&]() {
            gen_exp(ast.children[0], var_map);
            a("push %rax");
            gen_exp(ast.children[1], var_map);
            a("pop %rbx");
            a("cmp %rax, %rbx");
            a("movq $0, %rax");
        };
        auto res_type = t_ast({t_ast("int")});
        if (ast.name == "un_op") {
            if (ast.value == "&") {
                auto x = ast.children[0];
                if (x.name == "un_op" and x.value == "*") {
                    res_type = gen_exp(x.children[0], var_map);
                } else {
                    auto name = ast.children[0].value;
                    auto adr = var_map.get_var_adr(name);
                    auto type = var_map.get_var_type(name);
                    a("lea", adr, "%rax");
                    res_type = t_ast("pointer", {type});
                }
            } else {
                auto type = gen_exp(ast.children[0], var_map);
                if (ast.value == "-") {
                    a("neg %rax");
                } else if (ast.value == "~") {
                    a("not %rax");
                } else if (ast.value == "!") {
                    a("cmpq $0, %rax");
                    a("movq $0, %rax");
                    a("sete %al");
                } else if (ast.value == "*") {
                    if (type.children[0].name != "array") {
                        a("movq (%rax), %rax");
                    }
                    if (type.name != "pointer") {
                        throw std::runtime_error("bad dereferencing");
                    }
                    res_type = type.children[0];
                    if (res_type.name == "array") {
                        res_type = t_ast("pointer", {res_type.children[0]});
                    }
                }
            }
        } else if (ast.name == "constant") {
            set_rax(gen_con(ast.value));
        } else if (ast.name == "identifier") {
            auto& var_name = ast.value;
            if (not var_map.contains(var_name)) {
                throw std::runtime_error("undeclared identifier");
            }
            auto type = var_map.get_var_type(var_name);
            auto adr = var_map.get_var_adr(var_name);
            if (type.name == "array") {
                res_type = t_ast("pointer", {type.children[0]});
                a("lea", adr, "%rax");
            } else {
                res_type = type;
                set_rax(adr);
            }
        } else if (ast.name == "function_call") {
            auto& func_name = ast.value;
            a("push %rbx");
            a("push %rcx");
            a("push %rdx");
            a("call ", func_name);
            a("pop %rdx");
            a("pop %rcx");
            a("pop %rbx");
        } else if (ast.name == "bin_op") {
            if (ast.value == "=") {
                auto& lval = ast.children[0];
                if (lval.name == "un_op" and lval.value == "*") {
                    auto type = gen_exp(lval.children[0], var_map);
                    if (type.name != "pointer") {
                        throw std::runtime_error("bad dereferencing");
                    }
                    res_type = type.children[0];
                    a("push %rax");
                    gen_exp(ast.children[1], var_map);
                    a("pop %rbx");
                    a("movq %rax, (%rbx)");
                } else {
                    if (lval.name != "identifier") {
                        throw std::runtime_error("bad lvalue");
                    }
                    auto var_name = lval.value;
                    if (not var_map.contains(var_name)) {
                        throw std::runtime_error("undeclared identifier");
                    }
                    res_type = var_map.get_var_type(var_name);
                    auto exp = ast.children[1];
                    gen_exp(exp, var_map);
                    a("movq %rax, ", var_map.get_var_adr(var_name));
                }
            } else if (ast.value == "||") {
                gen_exp(ast.children[0], var_map);
                boolify_rax();
                auto end = make_label();
                auto l0 = make_label();
                a("cmpq $0, %rax");
                a("je ", l0);
                a("jmp ", end);
                put_label(l0);
                gen_exp(ast.children[1], var_map);
                boolify_rax();
                put_label(end);
            } else if (ast.value == "&&") {
                gen_exp(ast.children[0], var_map);
                boolify_rax();
                auto end = make_label();
                auto l0 = make_label();
                a("cmpq $0, %rax");
                a("jne ", l0);
                a("jmp ", end);
                put_label(l0);
                gen_exp(ast.children[1], var_map);
                boolify_rax();
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
                auto b_type = gen_exp(ast.children[1], var_map);
                a("push %rax");
                auto a_type = gen_exp(ast.children[0], var_map);
                a("pop %rbx");
                if (ast.value == "+") {
                    if (a_type.name == "pointer") {
                        auto elt_size = get_size(a_type.children[0]);
                        a("movq", gen_con(std::to_string(elt_size)), "%rcx");
                        a("imul %rcx, %rbx");
                        res_type = a_type;
                    } else if (b_type.name == "pointer") {
                        auto elt_size = get_size(b_type.children[0]);
                        a("movq", gen_con(std::to_string(elt_size)), "%rcx");
                        a("imul %rcx, %rax");
                        res_type = b_type;
                    }
                    a("add %rbx, %rax");
                } else if (ast.value == "-") {
                    a("sub %rbx, %rax");
                } else if (ast.value == "*") {
                    a("imul %rbx, %rax");
                } else if (ast.value == "/") {
                    a("movq $0, %rdx");
                    a("idiv %rbx");
                } else if (ast.value == "%") {
                    a("movq $0, %rdx");
                    a("idiv %rbx");
                    a("movq %rdx, %rax");
                }
            }
        } else if (ast.name == "tern_op") {
            if (ast.value == "?:") {
                auto cond_true = make_label();
                auto cond_false = make_label();
                auto end = make_label();
                gen_exp(ast.children[0], var_map);
                a("cmpq $0, %rax");
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
        return res_type;
    }

    void gen_compound_statement(const t_ast&, const t_context&);

    void gen_declaration(const t_ast& ast, t_context& ctx) {
        auto var_name = ast.value;
        if (not ctx.var_map.can_declare(var_name)) {
            throw std::runtime_error("variable redefinition");
        }
        ctx.var_map.insert_var(var_name, ast.children[0]);
        if (ast.children.size() == 2) {
            gen_exp(ast.children[1], ctx.var_map);
            a("movq %rax, ", ctx.var_map.get_var_adr(var_name));
        }
    }

    void gen_statement(const t_ast& c, t_context& ctx) {
        if (c.name == "if") {
            auto cond_true = make_label();
            auto cond_false = make_label();
            auto end = make_label();
            gen_exp(c.children[0], ctx.var_map);
            a("cmpq $0, %rax");
            a("jne ", cond_true);
            a("jmp ", cond_false);
            put_label(cond_true);
            gen_statement(c.children[1], ctx);
            a("jmp ", end);
            put_label(cond_false);
            if (c.children.size() == 3) {
                gen_statement(c.children[2], ctx);
            }
            put_label(end);
        } else if (c.name == "exp_statement") {
            if (not c.children.empty()) {
                gen_exp(c.children[0], ctx.var_map);
            }
        } else if (c.name == "return") {
            gen_exp(c.children[0], ctx.var_map);
            a("jmp ", ctx.func_name + "_end");
        } else if (c.name == "compound_statement") {
            gen_compound_statement(c, ctx);
        } else if (c.name == "while") {
            t_context nctx = ctx;
            nctx.loop_end = make_label();
            nctx.loop_body_end = make_label();
            auto loop_begin = make_label();
            auto loop_body = make_label();
            put_label(loop_begin);
            gen_exp(c.children[0], nctx.var_map);
            a("cmpq $0, %rax");
            a("jne ", loop_body);
            a("jmp ", nctx.loop_end);
            put_label(loop_body);
            gen_statement(c.children[1], nctx);
            put_label(nctx.loop_body_end);
            a("jmp ", loop_begin);
            put_label(nctx.loop_end);
        } else if (c.name == "do_while") {
            t_context nctx = ctx;
            nctx.loop_end = make_label();
            nctx.loop_body_end = make_label();
            auto loop_begin = make_label();
            put_label(loop_begin);
            gen_statement(c.children[0], nctx);
            put_label(nctx.loop_body_end);
            gen_exp(c.children[1], nctx.var_map);
            a("cmpq $0, %rax");
            a("je ", nctx.loop_end);
            a("jmp ", loop_begin);
            put_label(nctx.loop_end);
        } else if (c.name == "for") {
            auto loop_begin = make_label();
            auto loop_body = make_label();
            t_context nctx = ctx;
            nctx.loop_end = make_label();
            nctx.loop_body_end = make_label();
            auto init_exp = c.children[0];
            if (init_exp.name == "declaration") {
                gen_declaration(init_exp, nctx);
            } else {
                if (not init_exp.children.empty()) {
                    gen_exp(init_exp.children[0], nctx.var_map);
                }
            }
            put_label(loop_begin);
            auto& ctrl_exp = c.children[1];
            if (not ctrl_exp.children.empty()) {
                gen_exp(ctrl_exp.children[0], nctx.var_map);
                a("cmpq $0, %rax");
                a("jne ", loop_body);
                a("jmp ", nctx.loop_end);
            }
            put_label(loop_body);
            gen_statement(c.children[3], nctx);
            put_label(nctx.loop_body_end);
            auto& post_exp = c.children[2];
            if (not post_exp.children.empty()) {
                gen_exp(post_exp.children[0], nctx.var_map);
            }
            a("jmp ", loop_begin);
            put_label(nctx.loop_end);
            nctx.var_map.erase();
        } else if (c.name == "break") {
            if (ctx.loop_end == "") {
                throw std::runtime_error("break outside of a loop");
            }
            a("jmp ", ctx.loop_end);
        } else if (c.name == "continue") {
            if (ctx.loop_body_end == "") {
                throw std::runtime_error("continue outside of a loop");
            }
            a("jmp ", ctx.loop_body_end);
        }
    }

    void gen_block_item(const t_ast& ast, t_context& ctx) {
        if (ast.name == "declaration") {
            gen_declaration(ast, ctx);
        } else {
            gen_statement(ast, ctx);
        }
    }

    void gen_compound_statement(const t_ast& ast, const t_context& ctx) {
        t_context nctx = ctx;
        for (auto& c : ast.children) {
            gen_block_item(c, nctx);
        }
        nctx.var_map.erase();
    }

    auto gen_function(const t_ast& ast) {
        auto& func_name = ast.value;
        res += ".globl "; res += func_name; res += "\n";
        put_label(func_name);
        a("push %rbp");
        a("mov %rsp, %rbp");
        t_context ctx;
        ctx.func_name = func_name;
        for (auto& c : ast.children) {
            gen_block_item(c, ctx);
        }
        a("movq $0, %rax");
        put_label(func_name + "_end");
        a("mov %rbp, %rsp");
        a("pop %rbp");
        a("ret");
    }
}

std::string gen_asm(const t_ast& ast) {
    for (auto& c : ast.children) {
        gen_function(c);
    }
    return res;
}
