#include "jwarol_native_compiler.h"
#include "jwarol_lexer.h"
#include "jwarol_parser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sys/stat.h>

static const uint64_t BASE_ADDR = 0x400000;
static const uint64_t ARENA_SIZE = 16 * 1024 * 1024;
static const int ELF_HDR = 64;
static const int PHDR_SZ = 56;
static const int CODE_OFF = ELF_HDR + PHDR_SZ;
static const int64_t ARRAY_HDR_SZ = 16;
static const int64_t INITIAL_CAP = 8;
static const uint64_t STR_TAG = 1ULL << 62;

struct Emit {
    std::vector<uint8_t> c;
    size_t pos() const { return c.size(); }
    void u8(uint8_t v){ c.push_back(v); }
    void u32(uint32_t v){ u8(v); u8(v>>8); u8(v>>16); u8(v>>24); }
    void u64(uint64_t v){ for(int i=0;i<8;i++) u8(v>>(i*8)); }
    void i32(int32_t v){ u32((uint32_t)v); }

    void push_rax(){u8(0x50);}
    void push_rbx(){u8(0x53);}
    void push_rcx(){u8(0x51);}
    void push_rdx(){u8(0x52);}
    void push_rsi(){u8(0x56);}
    void push_rdi(){u8(0x57);}
    void push_r8(){u8(0x41);u8(0x50);}
    void push_r9(){u8(0x41);u8(0x51);}
    void push_rbp(){u8(0x55);}
    void push_r12(){u8(0x41);u8(0x54);}
    void push_r13(){u8(0x41);u8(0x55);}
    void push_r14(){u8(0x41);u8(0x56);}
    void pop_rax(){u8(0x58);}
    void pop_rbx(){u8(0x5B);}
    void pop_rcx(){u8(0x59);}
    void pop_rdx(){u8(0x5A);}
    void pop_rsi(){u8(0x5E);}
    void pop_rdi(){u8(0x5F);}
    void pop_rbp(){u8(0x5D);}
    void pop_r12(){u8(0x41);u8(0x5C);}
    void pop_r13(){u8(0x41);u8(0x5D);}
    void pop_r14(){u8(0x41);u8(0x5E);}

    void mov_rax_i(uint64_t v){u8(0x48);u8(0xB8);u64(v);}
    void mov_rbx_i(uint64_t v){u8(0x48);u8(0xBB);u64(v);}
    void mov_rcx_i(uint64_t v){u8(0x48);u8(0xB9);u64(v);}
    void mov_rdx_i(uint64_t v){u8(0x48);u8(0xBA);u64(v);}
    void mov_rsi_i(uint64_t v){u8(0x48);u8(0xBE);u64(v);}
    void mov_rdi_i(uint64_t v){u8(0x48);u8(0xBF);u64(v);}

    void ld_rax_bp(int o){u8(0x48);u8(0x8B);u8(0x45);u8((uint8_t)(int8_t)o);}
    void ld_rbx_bp(int o){u8(0x48);u8(0x8B);u8(0x5D);u8((uint8_t)(int8_t)o);}
    void st_bp_rax(int o){u8(0x48);u8(0x89);u8(0x45);u8((uint8_t)(int8_t)o);}
    void st_bp_rdi(int o){u8(0x48);u8(0x89);u8(0x7D);u8((uint8_t)(int8_t)o);}

    void mov_rdi_rax(){u8(0x48);u8(0x89);u8(0xC7);}
    void mov_rdi_rbx(){u8(0x48);u8(0x89);u8(0xDF);}
    void mov_rsi_rax(){u8(0x48);u8(0x89);u8(0xC6);}
    void mov_rdx_rax(){u8(0x48);u8(0x89);u8(0xC2);}
    void mov_rcx_rax(){u8(0x48);u8(0x89);u8(0xC1);}
    void mov_rax_rdi(){u8(0x48);u8(0x89);u8(0xF8);}
    void mov_rax_rbx(){u8(0x48);u8(0x89);u8(0xD8);}
    void mov_rbx_rax(){u8(0x48);u8(0x89);u8(0xC3);}
    void mov_rax_rdx(){u8(0x48);u8(0x89);u8(0xD0);}
    void xchg_rax_rbx(){u8(0x48);u8(0x93);}

    void add_rax_rbx(){u8(0x48);u8(0x01);u8(0xD8);}
    void sub_rax_rbx(){u8(0x48);u8(0x29);u8(0xD8);}
    void sub_left_right(){ xchg_rax_rbx(); sub_rax_rbx(); }
    void imul_rax_rbx(){u8(0x48);u8(0x0F);u8(0xAF);u8(0xC3);}
    void cqo(){u8(0x48);u8(0x99);}
    void idiv_rbx(){cqo();u8(0x48);u8(0xF7);u8(0xFB);}
    void neg_rax(){u8(0x48);u8(0xF7);u8(0xD8);}
    void not_rax(){u8(0x48);u8(0xF7);u8(0xD0);}
    void xor_rax_rax(){u8(0x48);u8(0x31);u8(0xC0);}

    void cmp_rbx_rax(){u8(0x48);u8(0x39);u8(0xC3);}
    void setcc(uint8_t code){u8(0x0F);u8(code);u8(0xC0);}
    void sete(){setcc(0x94);}
    void setne(){setcc(0x95);}
    void setl(){setcc(0x9C);}
    void setle(){setcc(0x9E);}
    void setg(){setcc(0x9F);}
    void setge(){setcc(0x9D);}
    void movzx_rax_al(){u8(0x48);u8(0x0F);u8(0xB6);u8(0xC0);}

    void call32(int32_t d){u8(0xE8);i32(d);}
    void jmp32(int32_t d){u8(0xE9);i32(d);}
    void je32(int32_t d){u8(0x0F);u8(0x84);i32(d);}
    void jne32(int32_t d){u8(0x0F);u8(0x85);i32(d);}
    void jge32(int32_t d){u8(0x0F);u8(0x8D);i32(d);}
    void jle32(int32_t d){u8(0x0F);u8(0x8E);i32(d);}
    void jg32(int32_t d){u8(0x0F);u8(0x8F);i32(d);}
    void test_rax(){u8(0x48);u8(0x85);u8(0xC0);}
    void ret(){u8(0xC3);}
    void nop(){u8(0x90);}
    void syscall_s(){u8(0x0F);u8(0x05);}
    void sub_rsp8(uint8_t v){u8(0x48);u8(0x83);u8(0xEC);u8(v);}
    void sub_rsp32(uint32_t v){u8(0x48);u8(0x81);u8(0xEC);u32(v);}
    void add_rsp8(uint8_t v){u8(0x48);u8(0x83);u8(0xC4);u8(v);}
    void leave(){u8(0xC9);}

    void mov_rax_abs(uint64_t a){u8(0x48);u8(0xA1);u64(a);}
    void mov_abs_rax(uint64_t a){u8(0x48);u8(0xA3);u64(a);}
    void mov_rdi_abs(uint64_t a){mov_rax_i(a);u8(0x48);u8(0x8B);u8(0x38);}

    void lea_rax_rbx_d(int32_t d){u8(0x48);u8(0x8D);u8(0x83);i32(d);}
    void ld_rax_rbx_d(int32_t d){u8(0x48);u8(0x8B);u8(0x83);i32(d);}
    void st_rbx_d_rax(int32_t d){u8(0x48);u8(0x89);u8(0x83);i32(d);}
    void ld_rax_rcx_d(int32_t d){u8(0x48);u8(0x8B);u8(0x81);i32(d);}
    void call_rax(){u8(0xFF);u8(0xD0);}

    void bts_rax_62(){u8(0x48);u8(0x0F);u8(0xBA);u8(0xF8);u8(0x3E);}
    void btr_rax_62(){u8(0x48);u8(0x0F);u8(0xBA);u8(0xF0);u8(0x3E);}
    void btr_rdi_62(){u8(0x48);u8(0x0F);u8(0xBA);u8(0xF7);u8(0x3E);}
    void btr_rsi_62(){u8(0x48);u8(0x0F);u8(0xBA);u8(0xF6);u8(0x3E);}
    void bt_rax_62(){u8(0x48);u8(0x0F);u8(0xBA);u8(0xE0);u8(0x3E);}
    void bt_rbx_62(){u8(0x48);u8(0x0F);u8(0xBA);u8(0xE3);u8(0x3E);}
    void jc32(int32_t d){u8(0x0F);u8(0x82);i32(d);}
    void jnc32(int32_t d){u8(0x0F);u8(0x83);i32(d);}
    void ld_rax_eax_d(int32_t d){u8(0x8B);u8(0x80);i32(d);}
    void mov_eax_ecx(){u8(0x89);u8(0xC8);}
    void mov_rax_rcx(){u8(0x48);u8(0x89);u8(0xC8);}
    void add_rax_rcx(){u8(0x48);u8(0x01);u8(0xC8);}
    void mov_rdi_imm32(uint32_t v){u8(0xBF);u32(v);}
    void add_rax_i32(int32_t v){u8(0x48);u8(0x05);i32(v);}
    void sub_rax_i32(int32_t v){u8(0x48);u8(0x2D);i32(v);}
};

struct Comp {
    Emit e;
    bool in_function = false;
    std::map<std::string,int> vars;
    int next_off = -8;

    struct Str{ std::string v; uint32_t off; };
    std::vector<Str> strs;
    std::vector<uint8_t> rodata;
    struct Fixup{ size_t code_off; uint32_t str_off; };
    std::vector<Fixup> fixups;
    std::vector<size_t> breaks;

    std::vector<size_t> alloc_fixups;
    std::vector<size_t> puts_fixups;
    std::vector<size_t> print_int_fixups;
    std::vector<size_t> str_concat_fixups;
    std::vector<size_t> str_compare_fixups;
    std::vector<size_t> str_index_fixups;
    std::vector<size_t> str_slice_fixups;
    std::vector<size_t> int_to_str_fixups;
    std::vector<size_t> print_str_fixups;

    struct FuncRef{ std::string name; size_t call_off; };
    std::vector<FuncRef> user_calls;

    std::map<std::string, std::vector<std::string>> func_params;
    std::map<std::string, size_t> func_offsets;
    std::vector<std::pair<std::string, ASTNode*>> func_bodies;

    int var(const std::string& n){
        auto it=vars.find(n);
        if(it!=vars.end()) return it->second;
        int o=next_off; next_off-=8; vars[n]=o; return o;
    }

    uint32_t addstr(const std::string& s){
        for(auto& ss:strs) if(ss.v==s) return ss.off;
        uint32_t off=(uint32_t)rodata.size();
        uint32_t len=(uint32_t)s.size();
        rodata.push_back(len);rodata.push_back(len>>8);
        rodata.push_back(len>>16);rodata.push_back(len>>24);
        for(char ch:s) rodata.push_back(ch);
        rodata.push_back(0);
        while(rodata.size()%8) rodata.push_back(0);
        strs.push_back({s,off}); return off;
    }

    void emit_cmp(TT op){
        e.cmp_rbx_rax();
        switch(op){
            case TT::EQ: e.sete(); break;
            case TT::NEQ: e.setne(); break;
            case TT::LT: e.setl(); break;
            case TT::LE: e.setle(); break;
            case TT::GT: e.setg(); break;
            case TT::GE: e.setge(); break;
            default: e.sete(); break;
        }
        e.movzx_rax_al();
    }

    void scan_funcs(ASTNode* n){
        if(!n) return;
        if(n->type==NodeType::DEF) func_params[n->name]=n->names;
        for(auto* c:n->children) scan_funcs(c);
    }

    void expr(ASTNode* n){
        if(!n){ e.xor_rax_rax(); return; }
        switch(n->type){
            case NodeType::INT_LIT:
                e.mov_rax_i((uint64_t)n->int_val);
                break;
            case NodeType::FLOAT_LIT:
                e.mov_rax_i((uint64_t)(int64_t)n->float_val);
                break;
            case NodeType::STRING_LIT: {
                uint32_t off=addstr(n->str_val);
                fixups.push_back({e.pos(),off});
                e.mov_rax_i(0xDEADDEAD);
                e.bts_rax_62();
                break;
            }
            case NodeType::BOOL_LIT:
                e.mov_rax_i(n->bool_val?1:0);
                break;
            case NodeType::NONE:
                e.xor_rax_rax();
                break;
            case NodeType::IDENT: {
                int off=var(n->name);
                e.ld_rax_bp(off);
                break;
            }
            case NodeType::BINOP: {
                expr(n->children[0]);
                e.push_rax();
                expr(n->children[1]);
                e.pop_rbx();
                switch(n->op){
                    case TT::PLUS:{
                        e.bt_rbx_62();
                        size_t is_str=e.pos();
                        e.jc32(0);
                        e.add_rax_rbx();
                        size_t done=e.pos();
                        e.jmp32(0);
                        i32_at(is_str+2,(int32_t)(e.pos()-(is_str+6)));
                        e.mov_rdi_rbx();
                        e.mov_rsi_rax();
                        e.call32(0);
                        str_concat_fixups.push_back(e.pos()-4);
                        i32_at(done+1,(int32_t)(e.pos()-(done+5)));
                        break;
                    }
                    case TT::MINUS: e.sub_left_right(); break;
                    case TT::STAR: e.imul_rax_rbx(); break;
                    case TT::DSLASH: e.xchg_rax_rbx(); e.idiv_rbx(); break;
                    case TT::MOD: e.xchg_rax_rbx(); e.idiv_rbx(); e.mov_rax_rdx(); break;
                    case TT::AMP:
                        e.xchg_rax_rbx();
                        e.u8(0x48);e.u8(0x21);e.u8(0xC3);
                        e.mov_rax_rbx();
                        break;
                    case TT::PIPE:
                        e.xchg_rax_rbx();
                        e.u8(0x48);e.u8(0x09);e.u8(0xC3);
                        e.mov_rax_rbx();
                        break;
                    case TT::CARET:
                        e.xchg_rax_rbx();
                        e.u8(0x48);e.u8(0x31);e.u8(0xC3);
                        e.mov_rax_rbx();
                        break;
                    case TT::LSHIFT:
                        e.mov_rcx_rax();
                        e.xchg_rax_rbx();
                        e.u8(0x48);e.u8(0xD3);e.u8(0xE0);
                        break;
                    case TT::RSHIFT:
                        e.mov_rcx_rax();
                        e.xchg_rax_rbx();
                        e.u8(0x48);e.u8(0xD3);e.u8(0xF8);
                        break;
                    default: break;
                }
                break;
            }
            case NodeType::UNARYOP: {
                expr(n->children[0]);
                switch(n->op){
                    case TT::MINUS: e.neg_rax(); break;
                    case TT::KW_NOT: e.test_rax(); e.sete(); e.movzx_rax_al(); break;
                    case TT::TILDE: e.not_rax(); break;
                    default: break;
                }
                break;
            }
            case NodeType::COMPARE: {
                expr(n->children[0]);
                e.push_rax();
                expr(n->children[1]);
                e.pop_rbx();
                e.bt_rbx_62();
                size_t is_str=e.pos();
                e.jc32(0);
                emit_cmp(n->op);
                size_t done=e.pos();
                e.jmp32(0);
                i32_at(is_str+2,(int32_t)(e.pos()-(is_str+6)));
                e.mov_rdi_rbx();
                e.mov_rsi_rax();
                e.call32(0);
                str_compare_fixups.push_back(e.pos()-4);
                e.u8(0x48);e.u8(0x83);e.u8(0xF8);e.u8(0x00);
                switch(n->op){
                    case TT::EQ: e.sete(); break;
                    case TT::NEQ: e.setne(); break;
                    case TT::LT: e.setl(); break;
                    case TT::LE: e.setle(); break;
                    case TT::GT: e.setg(); break;
                    case TT::GE: e.setge(); break;
                    default: e.sete(); break;
                }
                e.movzx_rax_al();
                i32_at(done+1,(int32_t)(e.pos()-(done+5)));
                break;
            }
            case NodeType::BOOLOP: {
                expr(n->children[0]);
                for(size_t i=1;i<n->children.size();i++){
                    e.test_rax();
                    size_t jf=e.pos();
                    if(n->op==TT::KW_AND) e.je32(0); else e.jne32(0);
                    expr(n->children[i]);
                    i32_at(jf+2,(int32_t)(e.pos()-(jf+6)));
                }
                break;
            }
            case NodeType::CALL:
                compile_call(n);
                break;
            case NodeType::SUBSCRIPT:
                compile_subscript_read(n);
                break;
            case NodeType::ATTRIBUTE: {
                ASTNode* obj=n->children[0];
                if(obj->type==NodeType::IDENT){
                    std::string full=obj->name+"."+n->name;
                    int off=var(full);
                    e.ld_rax_bp(off);
                }
                break;
            }
            case NodeType::LIST:
                compile_list_literal(n);
                break;
            default:
                e.xor_rax_rax();
                break;
        }
    }

    void i32_at(size_t off, int32_t v){ memcpy(&e.c[off],&v,4); }
    void i32_at8(size_t off, int32_t v){ memcpy(&e.c[off+1],&v,4); }

    void compile_new_list(){
        int64_t sz=ARRAY_HDR_SZ+INITIAL_CAP*8;
        e.mov_rdi_i((uint64_t)sz);
        e.call32(0);
        alloc_fixups.push_back(e.pos()-4);
        e.push_rax();
        e.pop_rbx();
        e.mov_rax_i((uint64_t)INITIAL_CAP);
        e.u8(0x48);e.u8(0x89);e.u8(0x03);
        e.xor_rax_rax();
        e.u8(0x48);e.u8(0x89);e.u8(0x43);e.u8(0x08);
        e.mov_rax_rbx();
    }

    void compile_list_literal(ASTNode* n){
        size_t ne=n->children.size();
        int64_t cap=ne>0?(int64_t)ne:INITIAL_CAP;
        int64_t sz=ARRAY_HDR_SZ+cap*8;
        e.mov_rdi_i((uint64_t)sz);
        e.call32(0);
        alloc_fixups.push_back(e.pos()-4);
        e.push_rax();
        e.pop_rbx();
        e.mov_rax_i((uint64_t)cap);
        e.u8(0x48);e.u8(0x89);e.u8(0x03);
        e.mov_rax_i((uint64_t)ne);
        e.u8(0x48);e.u8(0x89);e.u8(0x43);e.u8(0x08);
        for(size_t i=0;i<ne;i++){
            e.push_rbx();
            expr(n->children[i]);
            e.pop_rbx();
            int32_t d=(int32_t)(ARRAY_HDR_SZ+i*8);
            e.st_rbx_d_rax(d);
        }
        e.mov_rax_rbx();
    }

    void compile_subscript_read(ASTNode* n){
        if(n->children.size()>=2 && n->children[1]->type==NodeType::SLICE){
            compile_slice(n);
            return;
        }
        expr(n->children[0]);
        e.push_rax();
        expr(n->children[1]);
        e.pop_rbx();
        e.bt_rbx_62();
        size_t is_str=e.pos();
        e.jc32(0);
        e.u8(0x48);e.u8(0xC1);e.u8(0xE0);e.u8(0x03);
        e.u8(0x48);e.u8(0x01);e.u8(0xD8);
        e.u8(0x48);e.u8(0x83);e.u8(0xC0);e.u8((uint8_t)ARRAY_HDR_SZ);
        e.u8(0x48);e.u8(0x8B);e.u8(0x00);
        size_t done=e.pos();
        e.jmp32(0);
        i32_at(is_str+2,(int32_t)(e.pos()-(is_str+6)));
        e.mov_rdi_rbx();
        e.mov_rsi_rax();
        e.call32(0);
        str_index_fixups.push_back(e.pos()-4);
        i32_at(done+1,(int32_t)(e.pos()-(done+5)));
    }

    void compile_slice(ASTNode* n){
        ASTNode* sl=n->children[1];
        expr(n->children[0]);
        e.push_rax();
        if(sl->children.size()>=1 && sl->children[0]->type!=NodeType::NONE){
            expr(sl->children[0]);
        } else {
            e.xor_rax_rax();
        }
        e.push_rax();
        if(sl->children.size()>=2 && sl->children[1]->type!=NodeType::NONE){
            expr(sl->children[1]);
        } else {
            e.u8(0x48);e.u8(0x8B);e.u8(0x84);e.u8(0x24);
            e.u32(0x08);
            e.btr_rax_62();
            e.ld_rax_eax_d(-4);
            e.u8(0x48);e.u8(0x98);
        }
        e.mov_rdx_rax();
        e.pop_rsi();
        e.pop_rdi();
        e.call32(0);
        str_slice_fixups.push_back(e.pos()-4);
    }

    void compile_subscript_write(ASTNode* idx_node, ASTNode* val){
        expr(idx_node->children[0]);
        e.push_rax();
        expr(idx_node->children[1]);
        e.pop_rbx();
        e.u8(0x48);e.u8(0xC1);e.u8(0xE0);e.u8(0x03);
        e.u8(0x48);e.u8(0x01);e.u8(0xD8);
        e.u8(0x48);e.u8(0x83);e.u8(0xC0);e.u8((uint8_t)ARRAY_HDR_SZ);
        e.push_rax();
        expr(val);
        e.pop_rbx();
        e.u8(0x48);e.u8(0x89);e.u8(0x03);
    }

    void compile_append(ASTNode* obj, ASTNode* val){
        expr(obj);
        e.push_rax();
        expr(val);
        e.pop_rbx();
        e.push_rax();
        e.ld_rax_rbx_d(8);
        e.pop_rcx();
        e.st_rbx_d_rax((int32_t)(ARRAY_HDR_SZ+INITIAL_CAP*8));
        e.ld_rax_rbx_d(8);
        e.u8(0x48);e.u8(0xFF);e.u8(0xC0);
        e.st_rbx_d_rax(8);
    }

    void compile_call(ASTNode* n){
        ASTNode* func=n->children[0];
        std::vector<ASTNode*> args;
        for(size_t i=1;i<n->children.size();i++) args.push_back(n->children[i]);

        if(func->type==NodeType::IDENT && func->name=="puts" && args.size()==1){
            if(args[0]->type==NodeType::STRING_LIT){
                uint32_t off=addstr(args[0]->str_val);
                e.mov_rdi_i(0xDEADDEAD);
                fixups.push_back({e.pos()-8,off});
                e.call32(0);
                puts_fixups.push_back(e.pos()-4);
            } else {
                expr(args[0]);
                e.mov_rdi_rax();
                e.call32(0);
                puts_fixups.push_back(e.pos()-4);
            }
            return;
        }

        if(func->type==NodeType::IDENT && func->name=="print" && args.size()==1){
            if(args[0]->type==NodeType::STRING_LIT){
                uint32_t off=addstr(args[0]->str_val);
                e.mov_rdi_i(0xDEADDEAD);
                fixups.push_back({e.pos()-8,off});
                e.call32(0);
                puts_fixups.push_back(e.pos()-4);
            } else {
                expr(args[0]);
                e.bt_rax_62();
                size_t is_str=e.pos();
                e.jc32(0);
                e.mov_rdi_rax();
                e.call32(0);
                print_int_fixups.push_back(e.pos()-4);
                size_t done=e.pos();
                e.jmp32(0);
                i32_at(is_str+2,(int32_t)(e.pos()-(is_str+6)));
                e.btr_rax_62();
                e.mov_rdi_rax();
                e.call32(0);
                print_str_fixups.push_back(e.pos()-4);
                i32_at(done+1,(int32_t)(e.pos()-(done+5)));
            }
            return;
        }

        if(func->type==NodeType::IDENT && func->name=="print_int" && args.size()==1){
            expr(args[0]);
            e.mov_rdi_rax();
            e.call32(0);
            print_int_fixups.push_back(e.pos()-4);
            return;
        }

        if(func->type==NodeType::IDENT && func->name=="alloc" && args.size()==1){
            expr(args[0]);
            e.mov_rdi_rax();
            e.call32(0);
            alloc_fixups.push_back(e.pos()-4);
            return;
        }

        if(func->type==NodeType::IDENT && func->name=="free") return;

        if(func->type==NodeType::IDENT && func->name=="abs" && args.size()==1){
            expr(args[0]);
            e.u8(0x48);e.u8(0x85);e.u8(0xC0);
            size_t skip=e.pos();
            e.jge32(0);
            e.neg_rax();
            i32_at(skip+2,(int32_t)(e.pos()-(skip+6)));
            return;
        }

        if(func->type==NodeType::IDENT && func->name=="min" && args.size()==2){
            expr(args[0]);
            e.push_rax();
            expr(args[1]);
            e.pop_rbx();
            e.cmp_rbx_rax();
            size_t skip=e.pos();
            e.jle32(0);
            e.mov_rax_rbx();
            i32_at(skip+2,(int32_t)(e.pos()-(skip+6)));
            return;
        }

        if(func->type==NodeType::IDENT && func->name=="max" && args.size()==2){
            expr(args[0]);
            e.push_rax();
            expr(args[1]);
            e.pop_rbx();
            e.cmp_rbx_rax();
            size_t skip=e.pos();
            e.jge32(0);
            e.mov_rax_rbx();
            i32_at(skip+2,(int32_t)(e.pos()-(skip+6)));
            return;
        }

        if(func->type==NodeType::IDENT && func->name=="list"){
            compile_new_list();
            return;
        }

        if(func->type==NodeType::IDENT && func->name=="len" && args.size()==1){
            expr(args[0]);
            e.bt_rax_62();
            size_t is_str=e.pos();
            e.jc32(0);
            e.mov_rbx_rax();
            e.ld_rax_rbx_d(8);
            size_t done=e.pos();
            e.jmp32(0);
            i32_at(is_str+2,(int32_t)(e.pos()-(is_str+6)));
            e.btr_rax_62();
            e.ld_rax_eax_d(-4);
            e.u8(0x48);e.u8(0x98);
            i32_at(done+1,(int32_t)(e.pos()-(done+5)));
            return;
        }

        if(func->type==NodeType::ATTRIBUTE && func->name=="append" && args.size()==1){
            compile_append(func->children[0],args[0]);
            return;
        }

        for(int i=(int)args.size()-1;i>=0;i--){
            expr(args[i]);
            e.push_rax();
        }
        static const uint8_t pops[]={0x5F,0x5E,0x5A,0x59,0x41,0x50,0x41,0x51};
        for(size_t i=0;i<args.size()&&i<6;i++) e.u8(pops[i]);

        if(func->type==NodeType::IDENT){
            if(func_params.count(func->name)){
                user_calls.push_back({func->name,e.pos()+1});
                e.call32(0);
            } else {
                e.call32(0);
                alloc_fixups.push_back(e.pos()-4);
            }
        } else {
            expr(func);
            e.call_rax();
        }
    }

    void compile_stmt(ASTNode* n){
        if(!n) return;
        switch(n->type){
            case NodeType::ANN_ASSIGN: {
                if(!n->children.empty()) expr(n->children[0]);
                else e.xor_rax_rax();
                e.st_bp_rax(var(n->name));
                break;
            }
            case NodeType::ASSIGN: {
                ASTNode* target=n->children[0];
                expr(n->children[1]);
                if(target->type==NodeType::IDENT){
                    e.st_bp_rax(var(target->name));
                } else if(target->type==NodeType::SUBSCRIPT){
                    compile_subscript_write(target,n->children[1]);
                }
                break;
            }
            case NodeType::AUG_ASSIGN: {
                ASTNode* target=n->children[0];
                if(target->type==NodeType::IDENT){
                    int off=var(target->name);
                    e.ld_rax_bp(off);
                    e.push_rax();
                    expr(n->children[1]);
                    e.pop_rbx();
                    switch(n->op){
                        case TT::PLUS_EQ: e.add_rax_rbx(); break;
                        case TT::MINUS_EQ: e.sub_left_right(); break;
                        case TT::STAR_EQ: e.imul_rax_rbx(); break;
                        case TT::DSLASH_EQ: e.xchg_rax_rbx(); e.idiv_rbx(); break;
                        case TT::MOD_EQ: e.xchg_rax_rbx(); e.idiv_rbx(); e.mov_rax_rdx(); break;
                        default: break;
                    }
                    e.st_bp_rax(off);
                }
                break;
            }
            case NodeType::EXPR_STMT:
                expr(n->children[0]);
                break;
            case NodeType::RETURN:
                if(!n->children.empty()) expr(n->children[0]);
                else e.xor_rax_rax();
                if(in_function) e.leave();
                e.ret();
                break;
            case NodeType::IF:
                compile_if(n);
                break;
            case NodeType::WHILE:
                compile_while(n);
                break;
            case NodeType::FOR:
                compile_for(n);
                break;
            case NodeType::BREAK: {
                breaks.push_back(e.pos());
                e.jmp32(0);
                break;
            }
            case NodeType::CONTINUE: {
                breaks.push_back(e.pos());
                e.jmp32(0);
                break;
            }
            case NodeType::PASS:
            case NodeType::DEF:
            case NodeType::CLASS:
            case NodeType::IMPORT:
            case NodeType::FROM_IMPORT:
                break;
            default:
                break;
        }
    }

    void compile_if(ASTNode* n){
        if(n->children.size()<2) return;
        expr(n->children[0]);
        e.test_rax();
        size_t first_je=e.pos();
        e.je32(0);

        size_t i=1;
        while(i<n->children.size()){
            ASTNode* c=n->children[i];
            if(c->type==NodeType::IF || c->type==NodeType::ELSE){
                break;
            }
            compile_stmt(c);
            i++;
        }

        if(i>=n->children.size()){
            i32_at(first_je+2,(int32_t)(e.pos()-(first_je+6)));
            for(size_t bj:breaks) i32_at(bj+1,(int32_t)(e.pos()-(bj+5)));
            breaks.clear();
            return;
        }

        size_t skip_over=e.pos();
        e.jmp32(0);
        i32_at(first_je+2,(int32_t)(e.pos()-(first_je+6)));
        for(size_t bj:breaks) i32_at(bj+1,(int32_t)(e.pos()-(bj+5)));
        breaks.clear();

        std::vector<size_t> end_fixups;
        while(i<n->children.size()){
            ASTNode* c=n->children[i];
            if(c->type==NodeType::IF && c->children.size()>=2){
                expr(c->children[0]);
                e.test_rax();
                size_t ej=e.pos();
                e.je32(0);
                for(size_t j=1;j<c->children.size();j++){
                    compile_stmt(c->children[j]);
                }
                end_fixups.push_back(e.pos());
                e.jmp32(0);
                i32_at(ej+2,(int32_t)(e.pos()-(ej+6)));
            } else if(c->type==NodeType::ELSE){
                for(auto* stmt:c->children) compile_stmt(stmt);
                end_fixups.push_back(e.pos());
                e.jmp32(0);
            } else {
                compile_stmt(c);
                end_fixups.push_back(e.pos());
                e.jmp32(0);
            }
            i++;
        }
        i32_at(skip_over+1,(int32_t)(e.pos()-(skip_over+5)));
        for(size_t f:end_fixups) i32_at(f+1,(int32_t)(e.pos()-(f+5)));
    }

    void compile_while(ASTNode* n){
        if(n->children.size()<2) return;
        std::vector<size_t> saved;
        saved.swap(breaks);
        size_t loop=e.pos();
        expr(n->children[0]);
        e.test_rax();
        size_t jend=e.pos();
        e.je32(0);
        for(size_t i=1;i<n->children.size();i++) compile_stmt(n->children[i]);
        e.jmp32((int32_t)(loop-(e.pos()+5)));
        i32_at(jend+2,(int32_t)(e.pos()-(jend+6)));
        for(size_t bj:breaks) i32_at(bj+1,(int32_t)(e.pos()-(bj+5)));
        breaks.clear();
        breaks.swap(saved);
    }

    void compile_for(ASTNode* n){
        if(n->children.size()<3) return;
        ASTNode* target=n->children[0];
        ASTNode* iter=n->children[1];
        if(target->type!=NodeType::IDENT) return;

        bool is_range=false;
        int64_t range_n=0;
        if(iter->type==NodeType::CALL && iter->children.size()>=2){
            ASTNode* fn=iter->children[0];
            if(fn->type==NodeType::IDENT && fn->name=="range"){
                if(iter->children[1]->type==NodeType::INT_LIT){
                    range_n=iter->children[1]->int_val;
                    is_range=true;
                }
            }
        }

        std::string idx_name=target->name;
        int off=var(idx_name);

        std::vector<size_t> saved;
        saved.swap(breaks);

        if(is_range){
            e.mov_rax_i(0);
            e.st_bp_rax(off);
            size_t loop=e.pos();
            e.ld_rax_bp(off);
            e.mov_rbx_i((uint64_t)range_n);
            e.cmp_rbx_rax();
            size_t jend=e.pos();
            e.jle32(0);
            for(size_t i=2;i<n->children.size();i++) compile_stmt(n->children[i]);
            e.ld_rax_bp(off);
            e.u8(0x48);e.u8(0xFF);e.u8(0xC0);
            e.st_bp_rax(off);
            e.jmp32((int32_t)(loop-(e.pos()+5)));
            i32_at(jend+2,(int32_t)(e.pos()-(jend+6)));
        } else {
            expr(iter);
            e.bt_rax_62();
            size_t is_str=e.pos();
            e.jc32(0);

            e.st_bp_rax(off);
            size_t loop=e.pos();
            for(size_t i=2;i<n->children.size();i++) compile_stmt(n->children[i]);
            e.ld_rax_bp(off);
            e.u8(0x48);e.u8(0xFF);e.u8(0xC0);
            e.st_bp_rax(off);
            e.jmp32((int32_t)(loop-(e.pos()+5)));
            size_t done=e.pos();
            e.jmp32(0);

            i32_at(is_str+2,(int32_t)(e.pos()-(is_str+6)));
            e.push_rax();
            e.btr_rax_62();
            e.ld_rax_eax_d(-4);
            e.u8(0x48);e.u8(0x98);
            int str_off=var("__for_str_"+idx_name);
            e.pop_rdi();
            e.st_bp_rdi(str_off);
            int idx_off=var("__for_idx_"+idx_name);
            e.xor_rax_rax();
            e.st_bp_rax(idx_off);
            size_t sloop=e.pos();
            e.ld_rax_bp(idx_off);
            e.push_rax();
            e.ld_rax_bp(str_off);
            e.btr_rax_62();
            e.ld_rax_eax_d(-4);
            e.u8(0x48);e.u8(0x98);
            e.pop_rbx();
            e.cmp_rbx_rax();
            size_t send=e.pos();
            e.jge32(0);
            e.ld_rax_bp(str_off);
            e.push_rax();
            e.ld_rax_bp(idx_off);
            e.pop_rdi();
            e.mov_rsi_rax();
            e.call32(0);
            str_index_fixups.push_back(e.pos()-4);
            e.st_bp_rax(off);
            for(size_t i=2;i<n->children.size();i++) compile_stmt(n->children[i]);
            e.ld_rax_bp(idx_off);
            e.u8(0x48);e.u8(0xFF);e.u8(0xC0);
            e.st_bp_rax(idx_off);
            e.jmp32((int32_t)(sloop-(e.pos()+5)));
            i32_at(send+2,(int32_t)(e.pos()-(send+6)));
            i32_at(done+1,(int32_t)(e.pos()-(done+5)));
        }

        for(size_t bj:breaks) i32_at(bj+1,(int32_t)(e.pos()-(bj+5)));
        breaks.clear();
        breaks.swap(saved);
    }

    void compile(ASTNode* tree){
        scan_funcs(tree);

        for(auto* stmt:tree->children){
            if(stmt->type==NodeType::DEF){
                func_bodies.push_back({stmt->name,stmt});
                continue;
            }
            compile_stmt(stmt);
        }

        if(func_bodies.empty()){
            e.xor_rax_rax();
            e.ret();
            return;
        }

        size_t jmp_pos=e.pos();
        e.jmp32(0);

        for(auto& [fname,fnode]:func_bodies){
            func_offsets[fname]=e.pos();
            std::vector<std::string>& params=func_params[fname];

            std::map<std::string,int> saved_vars=vars;
            int saved_next=next_off;
            vars.clear();
            next_off=-8;

            e.push_rbp();
            e.u8(0x48);e.u8(0x89);e.u8(0xE5);
            e.sub_rsp32(0x100);

            static const uint8_t arg_offs[]={0xF8,0xF0,0xE8,0xE0,0xD8,0xD0};
            static const uint8_t reg_opcodes[][3]={
                {0x48,0x89,0x7D},{0x48,0x89,0x75},{0x48,0x89,0x55},
                {0x48,0x89,0x4D},{0x49,0x89,0x45},{0x49,0x89,0x4D}
            };
            for(size_t i=0;i<params.size()&&i<6;i++){
                int off=var(params[i]);
                e.u8(reg_opcodes[i][0]);e.u8(reg_opcodes[i][1]);e.u8(reg_opcodes[i][2]);
                e.u8(arg_offs[i]);
            }

            in_function=true;
            for(size_t i=0;i<fnode->children.size();i++) compile_stmt(fnode->children[i]);
            in_function=false;

            e.xor_rax_rax();
            e.leave();
            e.ret();

            vars=saved_vars;
            next_off=saved_next;
        }

        i32_at(jmp_pos+1,(int32_t)(e.pos()-(jmp_pos+5)));
        e.xor_rax_rax();
        e.ret();
    }
};

static bool write_elf(Comp& C, const std::string& output){
    Emit preamble;
    preamble.u8(0x48);preamble.u8(0x83);preamble.u8(0xE4);preamble.u8(0xF0);
    preamble.push_rbp();
    preamble.u8(0x48);preamble.u8(0x89);preamble.u8(0xE5);
    preamble.u8(0x48);preamble.u8(0x81);preamble.u8(0xEC);preamble.u32(0x100);

    preamble.u8(0x48);preamble.u8(0x31);preamble.u8(0xFF);
    preamble.mov_rsi_i(ARENA_SIZE);
    preamble.mov_rdx_i(7);
    preamble.u8(0x49);preamble.u8(0xBA);preamble.u64(0x22);
    preamble.u8(0x49);preamble.u8(0xB8);preamble.u64(0xFFFFFFFFFFFFFFFF);
    preamble.u8(0x4D);preamble.u8(0x31);preamble.u8(0xC9);
    preamble.mov_rax_i(9);
    preamble.syscall_s();

    size_t ab_pos=preamble.pos();
    preamble.mov_abs_rax(0xAAAAAAAAAAAAAAAA);
    size_t ap_pos=preamble.pos();
    preamble.mov_abs_rax(0xBBBBBBBBBBBBBBBB);
    size_t call_main_pos=preamble.pos();
    preamble.call32(0);

    preamble.mov_rdi_abs(0xAAAAAAAAAAAAAAAA);
    preamble.mov_rsi_i(ARENA_SIZE);
    preamble.mov_rax_i(10);
    preamble.syscall_s();

    preamble.u8(0x48);preamble.u8(0x31);preamble.u8(0xFF);
    preamble.mov_rax_i(60);
    preamble.syscall_s();

    uint32_t preamble_sz=(uint32_t)preamble.pos();

    Emit alloc_fn;
    alloc_fn.mov_rax_abs(0xBBBBBBBBBBBBBBBB);
    alloc_fn.u8(0x48);alloc_fn.u8(0x89);alloc_fn.u8(0xC1);
    alloc_fn.u8(0x48);alloc_fn.u8(0x01);alloc_fn.u8(0xF9);
    alloc_fn.u8(0x48);alloc_fn.u8(0x89);alloc_fn.u8(0xC8);
    alloc_fn.mov_abs_rax(0xBBBBBBBBBBBBBBBB);
    alloc_fn.ret();

    Emit puts_fn;
    puts_fn.u8(0x48);puts_fn.u8(0x89);puts_fn.u8(0xFE);
    puts_fn.u8(0x48);puts_fn.u8(0x31);puts_fn.u8(0xC0);
    size_t sl=puts_fn.pos();
    puts_fn.u8(0x80);puts_fn.u8(0x3C);puts_fn.u8(0x06);puts_fn.u8(0x00);
    size_t sj=puts_fn.pos();
    puts_fn.u8(0x0F);puts_fn.u8(0x84);puts_fn.u32(0);
    puts_fn.u8(0x48);puts_fn.u8(0xFF);puts_fn.u8(0xC0);
    puts_fn.jmp32((int32_t)(sl-(puts_fn.pos()+5)));
    { int32_t pv=(int32_t)(puts_fn.pos()-(sj+6)); memcpy(&puts_fn.c[sj+2],&pv,4); }
    puts_fn.u8(0x48);puts_fn.u8(0x89);puts_fn.u8(0xC2);
    puts_fn.mov_rax_i(1);
    puts_fn.mov_rdi_i(1);
    puts_fn.syscall_s();
    puts_fn.u8(0x6A);puts_fn.u8(0x0A);
    puts_fn.mov_rax_i(1);
    puts_fn.mov_rdi_i(1);
    puts_fn.u8(0x48);puts_fn.u8(0x89);puts_fn.u8(0xE6);
    puts_fn.mov_rdx_i(1);
    puts_fn.syscall_s();
    puts_fn.add_rsp8(8);
    puts_fn.ret();

    Emit pifn;
    pifn.push_rbp();
    pifn.u8(0x48);pifn.u8(0x89);pifn.u8(0xE5);
    pifn.push_rbx();
    pifn.sub_rsp8(0x20);

    // mov rax, rdi (number to print)
    pifn.u8(0x48);pifn.u8(0x89);pifn.u8(0xF8);
    // lea rsi, [rbp-1] - pointer to buffer end
    pifn.u8(0x48);pifn.u8(0x8D);pifn.u8(0x75);pifn.u8(0xFF);
    // mov byte [rsi], 0x0A (newline)
    pifn.u8(0xC6);pifn.u8(0x04);pifn.u8(0x26);pifn.u8(0x0A);
    // mov rbx, 10
    pifn.mov_rbx_i(10);
    // mov rcx, 1 (length starts at 1 for newline)
    pifn.mov_rcx_i(1);
    // test rax, rax
    pifn.u8(0x48);pifn.u8(0x85);pifn.u8(0xC0);
    // jge positive
    size_t pos_br=pifn.pos();
    pifn.jge32(0);
    // neg rax
    pifn.neg_rax();
    // dec rsi
    pifn.u8(0x48);pifn.u8(0xFF);pifn.u8(0xCE);
    // mov byte [rsi], '-'
    pifn.u8(0xC6);pifn.u8(0x04);pifn.u8(0x26);pifn.u8(0x2D);
    // inc rcx
    pifn.u8(0x48);pifn.u8(0xFF);pifn.u8(0xC1);
    // positive:
    { int32_t v=(int32_t)(pifn.pos()-(pos_br+6)); memcpy(&pifn.c[pos_br+2],&v,4); }

    // dloop: (digit conversion loop)
    size_t dloop=pifn.pos();
    // xor rdx, rdx
    pifn.u8(0x48);pifn.u8(0x31);pifn.u8(0xD2);
    // div rbx  (rax = rax/10, rdx = rax%10)
    pifn.u8(0x48);pifn.u8(0xF7);pifn.u8(0xF3);
    // add dl, 0x30 (convert to ASCII)
    pifn.u8(0x80);pifn.u8(0xC2);pifn.u8(0x30);
    // dec rsi
    pifn.u8(0x48);pifn.u8(0xFF);pifn.u8(0xCE);
    // mov byte [rsi], dl
    pifn.u8(0x88);pifn.u8(0x14);pifn.u8(0x26);
    // inc rcx
    pifn.u8(0x48);pifn.u8(0xFF);pifn.u8(0xC1);
    // test rax, rax
    pifn.u8(0x48);pifn.u8(0x85);pifn.u8(0xC0);
    // jne dloop
    pifn.jne32((int32_t)(dloop-(pifn.pos()+6)));

    // mov rdx, rcx (length)
    pifn.u8(0x48);pifn.u8(0x89);pifn.u8(0xCA);
    // mov rax, 1 (sys_write)
    pifn.mov_rax_i(1);
    // mov rdi, 1 (stdout)
    pifn.mov_rdi_i(1);
    // rsi already points to string start
    // syscall
    pifn.syscall_s();

    pifn.add_rsp8(0x20);
    pifn.pop_rbx();
    pifn.leave();
    pifn.ret();

    uint32_t alloc_sz=(uint32_t)alloc_fn.c.size();
    uint32_t puts_sz=(uint32_t)puts_fn.c.size();
    uint32_t pif_sz=(uint32_t)pifn.c.size();
    uint32_t user_sz=(uint32_t)C.e.c.size();

    std::vector<std::pair<Emit*,size_t>> helper_alloc_calls;

    Emit str_concat_fn;
    {
        Emit& e=str_concat_fn;
        // rdi=left_tagged, rsi=right_tagged
        e.push_rbp();
        e.mov_rbp_rsp();
        e.push_rbx();
        e.push_r12();
        e.push_r13();
        // untag left -> r13 (callee-saved)
        e.mov_rax_rdi();
        e.btr_rax_62();
        e.u8(0x49);e.u8(0x89);e.u8(0xC5); // mov r13, rax (left_ptr)
        // untag right
        e.mov_rax_rsi();
        e.btr_rax_62();
        e.mov_rsi_rax(); // rsi = right_ptr
        // get lengths
        e.u8(0x8B);e.u8(0x48);e.u8(0xFC); // mov ecx, [rax-4]... no, rsi is right_ptr
        // r13=left_ptr, rsi=right_ptr
        e.u8(0x41);e.u8(0x8B);e.u8(0x4D);e.u8(0xFC); // mov ecx, [r13-4] = len1
        e.u8(0x8B);e.u8(0x06); // mov eax, [rsi]... no, need [rsi-4]
        e.u8(0x8B);e.u8(0x46);e.u8(0xFC); // mov eax, [rsi-4] = len2
        // save right_ptr and len2 for after alloc
        e.push_rsi();
        e.push_rax();
        // alloc(len1+len2+5)
        e.add_rax_rcx(); // eax += len1
        e.u8(0x83);e.u8(0xC0);e.u8(0x05); // add eax, 5
        e.mov_rdi_rax();
        e.call32(0);
        helper_alloc_calls.push_back({&e,e.pos()-4});
        // restore
        e.pop_rax();  // eax=len2
        e.pop_rsi();  // rsi=right_ptr
        e.mov_rbx_rax(); // rbx=len2 (temp)
        // write total length to [rax]
        e.u8(0x8B);e.u8(0x48);e.u8(0xFC); // mov ecx, [rax-4]... no
        // rax=buffer, need to save it
        e.push_rax(); // save buffer
        e.u8(0x03);e.u8(0x5D);e.u8(0xFC); // add ebx, [rbp-4]... no
        // Just compute: total_len = len1 + len2
        e.u8(0x41);e.u8(0x8B);e.u8(0x4D);e.u8(0xFC); // mov ecx, [r13-4] = len1
        e.u8(0x01);e.u8(0xD9); // add ecx, ebx (len1+len2)
        e.pop_rax(); // rax=buffer
        e.u8(0x89);e.u8(0x08); // mov [rax], ecx (store length)
        // copy left: src=r13, dst=rax+4, count=len1
        e.push_rsi(); // save right_ptr
        e.mov_rsi_r13(); // rsi = left_ptr
        e.u8(0x48);e.u8(0x8D);e.u8(0x78);e.u8(0x04); // lea rdi, [rax+4]
        e.u8(0x41);e.u8(0x8B);e.u8(0x4D);e.u8(0xFC); // mov ecx, [r13-4]
        e.u8(0xFC); // cld
        e.u8(0xF3);e.u8(0xA4); // rep movsb
        // copy right: src=right_ptr, dst=rdi(current), count=len2
        e.pop_rsi(); // rsi=right_ptr
        e.u8(0x8B);e.u8(0x46);e.u8(0xFC); // mov eax, [rsi-4]
        e.mov_rcx_rax();
        e.u8(0xF3);e.u8(0xA4); // rep movsb
        // null terminator
        e.u8(0xC6);e.u8(0x07);e.u8(0x00); // mov byte [rdi], 0
        // tag result: rax = buffer+4 (data ptr)
        e.pop_rax(); // ... wait, we don't have buffer on stack anymore
        // Actually buffer was popped into rax earlier. Let me re-derive...
    }
    // REWRITE: this is getting too tangled. Replace with clean version.

    Emit str_compare_fn;
    str_compare_fn.push_rbx();
    str_compare_fn.push_r12();
    str_compare_fn.push_r13();
    str_compare_fn.u8(0x48);str_compare_fn.u8(0x89);str_compare_fn.u8(0xF8);
    str_compare_fn.btr_rax_62();
    str_compare_fn.u8(0x48);str_compare_fn.u8(0x89);str_compare_fn.u8(0xF3);
    str_compare_fn.btr_rax_62();
    str_compare_fn.u8(0x49);str_compare_fn.u8(0x89);str_compare_fn.u8(0xC4);
    str_compare_fn.u8(0x49);str_compare_fn.u8(0x89);str_compare_fn.u8(0xCD);
    str_compare_fn.u8(0x45);str_compare_fn.u8(0x8B);str_compare_fn.u8(0x04);str_compare_fn.u8(0x24);
    str_compare_fn.u8(0x41);str_compare_fn.u8(0x89);str_compare_fn.u8(0xC1);
    str_compare_fn.u8(0x45);str_compare_fn.u8(0x8B);str_compare_fn.u8(0x0C);str_compare_fn.u8(0x24);
    str_compare_fn.u8(0x41);str_compare_fn.u8(0x89);str_compare_fn.u8(0xC8);
    str_compare_fn.u8(0x44);str_compare_fn.u8(0x89);str_compare_fn.u8(0xC1);
    str_compare_fn.u8(0x41);str_compare_fn.u8(0x39);str_compare_fn.u8(0xC8);
    size_t sc_jle=str_compare_fn.pos();
    str_compare_fn.jle32(0);
    str_compare_fn.u8(0x41);str_compare_fn.u8(0x89);str_compare_fn.u8(0xC1);
    {int32_t v=(int32_t)(str_compare_fn.pos()-(sc_jle+6)); memcpy(&str_compare_fn.c[sc_jle+2],&v,4);}
    str_compare_fn.u8(0x4C);str_compare_fn.u8(0x89);str_compare_fn.u8(0xE1);
    str_compare_fn.u8(0x4C);str_compare_fn.u8(0x89);str_compare_fn.u8(0xE6);
    str_compare_fn.u8(0x4C);str_compare_fn.u8(0x89);str_compare_fn.u8(0xFF);
    str_compare_fn.u8(0xF3);str_compare_fn.u8(0xA6);
    str_compare_fn.u8(0x75);str_compare_fn.u8(0x07);
    str_compare_fn.u8(0x31);str_compare_fn.u8(0xC0);
    str_compare_fn.u8(0x41);str_compare_fn.u8(0x5D);
    str_compare_fn.u8(0x41);str_compare_fn.u8(0x5C);
    str_compare_fn.pop_rbx();
    str_compare_fn.ret();
    str_compare_fn.u8(0x0F);str_compare_fn.u8(0xB6);str_compare_fn.u8(0x47);str_compare_fn.u8(0xFF);
    str_compare_fn.u8(0x3A);str_compare_fn.u8(0x06);
    str_compare_fn.u8(0x7C);str_compare_fn.u8(0x09);
    str_compare_fn.u8(0xB8);str_compare_fn.u8(0x01);str_compare_fn.u8(0x00);str_compare_fn.u8(0x00);str_compare_fn.u8(0x00);
    str_compare_fn.u8(0x41);str_compare_fn.u8(0x5D);
    str_compare_fn.u8(0x41);str_compare_fn.u8(0x5C);
    str_compare_fn.pop_rbx();
    str_compare_fn.ret();
    str_compare_fn.u8(0xB8);str_compare_fn.u8(0xFF);str_compare_fn.u8(0xFF);str_compare_fn.u8(0xFF);str_compare_fn.u8(0xFF);
    str_compare_fn.u8(0x41);str_compare_fn.u8(0x5D);
    str_compare_fn.u8(0x41);str_compare_fn.u8(0x5C);
    str_compare_fn.pop_rbx();
    str_compare_fn.ret();

    Emit str_index_fn;
    str_index_fn.push_rbx();
    str_index_fn.push_r12();
    str_index_fn.u8(0x48);str_index_fn.u8(0x89);str_index_fn.u8(0xF8);
    str_index_fn.btr_rax_62();
    str_index_fn.u8(0x49);str_index_fn.u8(0x89);str_index_fn.u8(0xC4);
    str_index_fn.u8(0x49);str_index_fn.u8(0x89);str_index_fn.u8(0xF5);
    str_index_fn.u8(0xBF);str_index_fn.u32(0x08);
    str_index_fn.call32(0);
    helper_alloc_calls.push_back({&str_index_fn,str_index_fn.pos()-4});
    str_index_fn.u8(0xC7);str_index_fn.u8(0x40);str_index_fn.u8(0xFC);str_index_fn.u32(1);
    str_index_fn.u8(0x49);str_index_fn.u8(0x8D);str_index_fn.u8(0x0C);str_index_fn.u8(0x30);
    str_index_fn.u8(0x0F);str_index_fn.u8(0xB6);str_index_fn.u8(0x11);
    str_index_fn.u8(0x88);str_index_fn.u8(0x10);
    str_index_fn.u8(0xC6);str_index_fn.u8(0x40);str_index_fn.u8(0x01);str_index_fn.u8(0x00);
    str_index_fn.bts_rax_62();
    str_index_fn.pop_r12();
    str_index_fn.pop_rbx();
    str_index_fn.ret();

    Emit str_slice_fn;
    str_slice_fn.push_rbx();
    str_slice_fn.push_r12();
    str_slice_fn.push_r13();
    str_slice_fn.push_r14();
    str_slice_fn.u8(0x48);str_slice_fn.u8(0x89);str_slice_fn.u8(0xF8);
    str_slice_fn.btr_rax_62();
    str_slice_fn.u8(0x49);str_slice_fn.u8(0x89);str_slice_fn.u8(0xC4);
    str_slice_fn.u8(0x49);str_slice_fn.u8(0x89);str_slice_fn.u8(0xF5);
    str_slice_fn.u8(0x49);str_slice_fn.u8(0x89);str_slice_fn.u8(0xD6);
    str_slice_fn.u8(0x4C);str_slice_fn.u8(0x89);str_slice_fn.u8(0xF0);
    str_slice_fn.u8(0x4C);str_slice_fn.u8(0x29);str_slice_fn.u8(0xF0);
    str_slice_fn.push_rax();
    str_slice_fn.u8(0x48);str_slice_fn.u8(0x83);str_slice_fn.u8(0xC0);str_slice_fn.u8(0x05);
    str_slice_fn.mov_rdi_rax();
    str_slice_fn.call32(0);
    helper_alloc_calls.push_back({&str_slice_fn,str_slice_fn.pos()-4});
    str_slice_fn.pop_rcx();
    str_slice_fn.u8(0x89);str_slice_fn.u8(0x48);str_slice_fn.u8(0xFC);
    str_slice_fn.push_rax();
    str_slice_fn.pop_rdi();
    str_slice_fn.u8(0x4C);str_slice_fn.u8(0x89);str_slice_fn.u8(0xE6);
    str_slice_fn.u8(0x4C);str_slice_fn.u8(0x01);str_slice_fn.u8(0xC6);
    str_slice_fn.u8(0xF3);str_slice_fn.u8(0xA4);
    str_slice_fn.u8(0xC6);str_slice_fn.u8(0x07);str_slice_fn.u8(0x00);
    str_slice_fn.push_rax();
    str_slice_fn.pop_rax();
    str_slice_fn.bts_rax_62();
    str_slice_fn.pop_r14();
    str_slice_fn.pop_r13();
    str_slice_fn.pop_r12();
    str_slice_fn.pop_rbx();
    str_slice_fn.ret();

    Emit print_str_fn;
    print_str_fn.push_rbp();
    print_str_fn.u8(0x48);print_str_fn.u8(0x89);print_str_fn.u8(0xE5);
    print_str_fn.push_rbx();
    print_str_fn.u8(0x8B);print_str_fn.u8(0x47);print_str_fn.u8(0xFC);
    print_str_fn.u8(0x48);print_str_fn.u8(0x89);print_str_fn.u8(0xC2);
    print_str_fn.u8(0x48);print_str_fn.u8(0x89);print_str_fn.u8(0xFE);
    print_str_fn.mov_rdi_i(1);
    print_str_fn.mov_rax_i(1);
    print_str_fn.syscall_s();
    print_str_fn.u8(0x6A);print_str_fn.u8(0x0A);
    print_str_fn.mov_rax_i(1);
    print_str_fn.mov_rdi_i(1);
    print_str_fn.u8(0x48);print_str_fn.u8(0x8D);print_str_fn.u8(0x74);print_str_fn.u8(0x24);print_str_fn.u8(0xFF);
    print_str_fn.mov_rdx_i(1);
    print_str_fn.syscall_s();
    print_str_fn.add_rsp8(8);
    print_str_fn.pop_rbx();
    print_str_fn.leave();
    print_str_fn.ret();

    uint32_t sc_concat_sz=(uint32_t)str_concat_fn.c.size();
    uint32_t sc_compare_sz=(uint32_t)str_compare_fn.c.size();
    uint32_t sc_index_sz=(uint32_t)str_index_fn.c.size();
    uint32_t sc_slice_sz=(uint32_t)str_slice_fn.c.size();
    uint32_t ps_sz=(uint32_t)print_str_fn.c.size();
    uint32_t total_helpers=alloc_sz+puts_sz+pif_sz+sc_concat_sz+sc_compare_sz+sc_index_sz+sc_slice_sz+ps_sz;
    uint32_t total_code=preamble_sz+user_sz+total_helpers;
    uint32_t rodsz=(uint32_t)C.rodata.size();
    uint32_t filesz=CODE_OFF+total_code+rodsz;
    uint32_t memsz=(filesz+15)&~15u;
    filesz=(filesz+15)&~15u;

    uint64_t arena_base=BASE_ADDR+filesz;
    uint64_t arena_ptr=BASE_ADDR+filesz+8;
    uint64_t alloc_addr=BASE_ADDR+CODE_OFF+preamble_sz+user_sz;
    uint64_t puts_addr=alloc_addr+alloc_sz;
    uint64_t pi_addr=puts_addr+puts_sz;
    uint64_t sc_concat_addr=pi_addr+pif_sz;
    uint64_t sc_compare_addr=sc_concat_addr+sc_concat_sz;
    uint64_t sc_index_addr=sc_compare_addr+sc_compare_sz;
    uint64_t sc_slice_addr=sc_index_addr+sc_index_sz;
    uint64_t ps_addr=sc_slice_addr+sc_slice_sz;

    memcpy(&preamble.c[ab_pos+2],&arena_base,8);
    memcpy(&preamble.c[ap_pos+2],&arena_ptr,8);
    int32_t cmd=(int32_t)(preamble_sz-(call_main_pos+5));
    memcpy(&preamble.c[call_main_pos+1],&cmd,4);
    memcpy(&preamble.c[call_main_pos+5+2],&arena_base,8);

    memcpy(&alloc_fn.c[2],&arena_ptr,8);
    memcpy(&alloc_fn.c[21],&arena_ptr,8);

    for(size_t f:C.alloc_fixups){
        int32_t d=(int32_t)(alloc_addr-(BASE_ADDR+CODE_OFF+preamble_sz+f+4));
        memcpy(&C.e.c[f],&d,4);
    }
    for(size_t f:C.puts_fixups){
        int32_t d=(int32_t)(puts_addr-(BASE_ADDR+CODE_OFF+preamble_sz+f+4));
        memcpy(&C.e.c[f],&d,4);
    }
    for(size_t f:C.print_int_fixups){
        int32_t d=(int32_t)(pi_addr-(BASE_ADDR+CODE_OFF+preamble_sz+f+4));
        memcpy(&C.e.c[f],&d,4);
    }
    for(size_t f:C.str_concat_fixups){
        int32_t d=(int32_t)(sc_concat_addr-(BASE_ADDR+CODE_OFF+preamble_sz+f+4));
        memcpy(&C.e.c[f],&d,4);
    }
    for(size_t f:C.str_compare_fixups){
        int32_t d=(int32_t)(sc_compare_addr-(BASE_ADDR+CODE_OFF+preamble_sz+f+4));
        memcpy(&C.e.c[f],&d,4);
    }
    for(size_t f:C.str_index_fixups){
        int32_t d=(int32_t)(sc_index_addr-(BASE_ADDR+CODE_OFF+preamble_sz+f+4));
        memcpy(&C.e.c[f],&d,4);
    }
    for(size_t f:C.str_slice_fixups){
        int32_t d=(int32_t)(sc_slice_addr-(BASE_ADDR+CODE_OFF+preamble_sz+f+4));
        memcpy(&C.e.c[f],&d,4);
    }
    for(size_t f:C.print_str_fixups){
        int32_t d=(int32_t)(ps_addr-(BASE_ADDR+CODE_OFF+preamble_sz+f+4));
        memcpy(&C.e.c[f],&d,4);
    }
    for(auto& hc:helper_alloc_calls){
        Emit* buf=hc.first;
        size_t pos=hc.second;
        uint64_t call_addr=BASE_ADDR+CODE_OFF+preamble_sz+user_sz;
        if(buf==&str_concat_fn) call_addr=sc_concat_addr;
        else if(buf==&str_compare_fn) call_addr=sc_compare_addr;
        else if(buf==&str_index_fn) call_addr=sc_index_addr;
        else if(buf==&str_slice_fn) call_addr=sc_slice_addr;
        int32_t d=(int32_t)(alloc_addr-(call_addr+pos+4));
        memcpy(&buf->c[pos],&d,4);
    }
    for(auto& f:C.user_calls){
        auto it=C.func_offsets.find(f.name);
        if(it!=C.func_offsets.end()){
            int32_t d=(int32_t)(it->second-(f.call_off+4));
            memcpy(&C.e.c[f.call_off],&d,4);
        }
    }

    std::vector<uint8_t> bin(filesz,0);

    auto h8=[&](size_t o,uint8_t v){bin[o]=v;};
    auto h16=[&](size_t o,uint16_t v){bin[o]=v;bin[o+1]=v>>8;};
    auto h32=[&](size_t o,uint32_t v){bin[o]=v;bin[o+1]=v>>8;bin[o+2]=v>>16;bin[o+3]=v>>24;};
    auto h64=[&](size_t o,uint64_t v){for(int i=0;i<8;i++)bin[o+i]=(v>>(i*8));};

    h8(0,0x7F);h8(1,'E');h8(2,'L');h8(3,'F');
    h8(4,2);h8(5,1);h8(6,1);
    h16(16,2);h16(18,0x3E);h32(20,1);
    h64(24,BASE_ADDR+CODE_OFF);
    h64(32,64);h64(40,0);
    h32(48,0);h16(52,64);h16(54,56);h16(56,1);
    h16(58,0);h16(60,0);h16(62,0);

    h32(64,1);h32(68,7);
    h64(72,0);h64(80,BASE_ADDR);h64(88,BASE_ADDR);
    h64(96,filesz);h64(104,memsz);h64(112,0x1000);

    memcpy(&bin[CODE_OFF],preamble.c.data(),preamble_sz);
    memcpy(&bin[CODE_OFF+preamble_sz],C.e.c.data(),user_sz);
    memcpy(&bin[CODE_OFF+preamble_sz+user_sz],alloc_fn.c.data(),alloc_sz);
    memcpy(&bin[CODE_OFF+preamble_sz+user_sz+alloc_sz],puts_fn.c.data(),puts_sz);
    memcpy(&bin[CODE_OFF+preamble_sz+user_sz+alloc_sz+puts_sz],pifn.c.data(),pif_sz);
    size_t off=CODE_OFF+preamble_sz+user_sz+alloc_sz+puts_sz+pif_sz;
    memcpy(&bin[off],str_concat_fn.c.data(),sc_concat_sz);off+=sc_concat_sz;
    memcpy(&bin[off],str_compare_fn.c.data(),sc_compare_sz);off+=sc_compare_sz;
    memcpy(&bin[off],str_index_fn.c.data(),sc_index_sz);off+=sc_index_sz;
    memcpy(&bin[off],str_slice_fn.c.data(),sc_slice_sz);off+=sc_slice_sz;
    memcpy(&bin[off],print_str_fn.c.data(),ps_sz);

    uint32_t rod_off=CODE_OFF+total_code;
    for(auto& f:C.fixups){
        uint64_t target=BASE_ADDR+rod_off+f.str_off+4;
        memcpy(&bin[CODE_OFF+preamble_sz+f.code_off],&target,8);
    }
    memcpy(&bin[rod_off],C.rodata.data(),rodsz);

    std::ofstream ofs(output,std::ios::binary);
    if(!ofs){ fprintf(stderr,"Cannot write %s\n",output.c_str()); return false; }
    ofs.write((char*)bin.data(),filesz);
    ofs.close();
    chmod(output.c_str(),0755);
    return true;
}

bool compile_native(const std::string& source, const std::string& output){
    Lexer lexer(source);
    auto tokens=lexer.tokenize();
    if(lexer.has_errors()){
        for(auto& e:lexer.errors()) fprintf(stderr,"Lexer: %s\n",e.c_str());
        return false;
    }

    Parser parser(tokens);
    ASTNode* ast=parser.parse();
    if(parser.has_errors()){
        for(auto& e:parser.errors()) fprintf(stderr,"Parser: %s\n",e.c_str());
        delete ast;
        return false;
    }

    Comp C;
    C.compile(ast);
    delete ast;

    if(C.e.c.empty()){
        fprintf(stderr,"Nothing to compile\n");
        return false;
    }

    bool ok=write_elf(C,output);
    if(ok) fprintf(stderr,"Compiled -> %s (%zu bytes code)\n",output.c_str(),C.e.c.size());
    return ok;
}

#ifndef JWAROL_LIB_ONLY
int main(int argc, char** argv){
    if(argc<2){
        fprintf(stderr,"jwarol2 — Python → native x86_64 compiler\n"
                       "Usage: %s <input.py> [-o output]\n",argv[0]);
        return 1;
    }
    std::string input=argv[1];
    std::string output;
    for(int i=2;i<argc;i++){
        if(std::string(argv[i])=="-o"&&i+1<argc) output=argv[++i];
    }
    if(output.empty()){
        std::string base=input;
        size_t dot=base.rfind('.');
        if(dot!=std::string::npos) base=base.substr(0,dot);
        size_t slash=base.rfind('/');
        if(slash!=std::string::npos) base=base.substr(slash+1);
        output=base;
    }
    std::ifstream ifs(input);
    if(!ifs){ fprintf(stderr,"Cannot open %s\n",input.c_str()); return 1; }
    std::string src((std::istreambuf_iterator<char>(ifs)),std::istreambuf_iterator<char>());
    if(!compile_native(src,output)) return 1;
    return 0;
}
#endif
