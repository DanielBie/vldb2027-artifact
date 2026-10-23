#pragma once
#include <stdint.h>

#define RAX 0
#define RCX 1
#define RDX 2
#define RBX 3
#define RSP 4
#define RBP 5
#define RSI 6
#define RDI 7
#define R8 8
#define R9 9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define R14 14
#define R15 15

#define JA 0x77
#define JAE 0x73
#define JB 0x72
#define JBE 0x76
#define JC 0x72
#define JECXZ 0xe3
#define JRCXZ 0xe3
#define JE 0x74
#define JG 0x7f
#define JGE 0x7d
#define JL 0x7c
#define JLE 0x7e
#define JNA 0x76
#define JNAE 0x72
#define JNB 0x73
#define JNBE 0x77
#define JNC 0x73
#define JNE 0x75
#define JNG 0x7e
#define JNGE 0x7c
#define JNL 0x7d
#define JNLE 0x7f
#define JNO 0x71
#define JNP 0x7b
#define JNS 0x79
#define JNZ 0x75
#define JO 0x70
#define JP 0x7a
#define JPE 0x7a
#define JPO 0x7b
#define JS 0x78
#define JZ 0x74

// least significan bits in modrm are r/m (r), more significant bits are reg (r)

#pragma pack(1)                                         
// struct x86_inst {
//   virtual uint16_t get_size() = 0;
// };

struct x86_add_r64_imm32 {
  uint32_t rex : 8;
  uint32_t op : 8;
  uint32_t modrm : 8;
  int32_t imm : 32;
  x86_add_r64_imm32() = default;
  x86_add_r64_imm32(const x86_add_r64_imm32 &other){
    this->rex = other.rex;
    this->op = other.op;
    this->modrm = other.modrm;
    this->imm = other.imm;
  }
  x86_add_r64_imm32(uint32_t reg, int32_t imm){
    this->rex = 0b01001000 + (0b1 * (reg > 7));
    this->op = 0x81;
    this->modrm = 0b11000000 + (reg & 0b111);
    this->imm = imm;
  }
  uint16_t get_size(){
    return sizeof(x86_add_r64_imm32);
  }
};

struct x86_call_r64 {
  uint32_t op : 8;
  uint32_t modrm : 8;
  x86_call_r64() = default;
  x86_call_r64(const x86_call_r64 &other){
    this->op = other.op;
    this->modrm = other.modrm;
  }
  x86_call_r64(uint32_t reg){
    this->op = 0xff;
    this->modrm = 0b11010000 + (reg & 0b111);
  }
  uint16_t get_size(){
    return sizeof(x86_call_r64);
  }
};

struct x86_cmp_r32_m32 {
  uint32_t rex : 8;
  uint32_t op : 8;
  uint32_t modrm : 8;
  x86_cmp_r32_m32() = default;
  x86_cmp_r32_m32(const x86_cmp_r32_m32 &other){
    this->rex = other.rex;
    this->op = other.op;
    this->modrm = other.modrm;
  }
  x86_cmp_r32_m32(uint32_t reg, uint32_t mem_reg){
    this->rex = 0b01000000 + (0b1 * (mem_reg > 7)) + (0b100 * (reg > 7));
    this->op = 0x3b;
    this->modrm = 0b00000000 + ((reg & 0b111) << 3) + ((mem_reg & 0b111));
  }
  uint16_t get_size(){
    return sizeof(x86_cmp_r32_m32);
  }
};

struct x86_cmp_r64_r64 {
  uint32_t rex : 8;
  uint32_t op : 8;
  uint32_t modrm : 8;
  x86_cmp_r64_r64() = default;
  x86_cmp_r64_r64(const x86_cmp_r64_r64 &other){
    this->rex = other.rex;
    this->op = other.op;
    this->modrm = other.modrm;
  }
  x86_cmp_r64_r64(uint32_t reg_1, uint32_t reg_2){
    this->rex = 0b01001000 + (0b1 * (reg_1 > 7)) + (0b100 * (reg_2 > 7));
    this->op = 0x39;
    this->modrm = 0b11000000 + (reg_1 & 0b111) + ((reg_2 & 0b111) << 3);
  }
  uint16_t get_size(){
    return sizeof(x86_cmp_r64_r64);
  }
};

struct x86_cmp_r64_imm32 {
  uint32_t rex : 8;
  uint32_t op : 8;
  uint32_t modrm : 8;
  int32_t imm : 32;
  x86_cmp_r64_imm32() = default;
  x86_cmp_r64_imm32(const x86_cmp_r64_imm32 &other){
    this->rex = other.rex;
    this->op = other.op;
    this->modrm = other.modrm;
    this->imm = other.imm;
  }
  x86_cmp_r64_imm32(uint32_t reg, int32_t imm){
    this->rex = 0b01001000 + (0b1 * (reg > 7));
    this->op = 0x81;
    this->modrm = 0b11000000 + (reg & 0b111) + ((0b111) << 3);
    this->imm = imm;
  }
  uint16_t get_size(){
    return sizeof(x86_cmp_r64_imm32);
  }
};

struct x86_jmp_rel8 {
  uint32_t op : 8;
  int32_t offset : 8;
  x86_jmp_rel8() = default;
  x86_jmp_rel8(const x86_jmp_rel8 &other){
    this->op = other.op;
    this->offset = other.offset;
  }
  x86_jmp_rel8(int32_t offset){
    this->op = 0xeb;
    this->offset = offset;
  }
  x86_jmp_rel8(const void* address){
    this->op = 0xeb;
    this->set_address(address);
  }
  uint16_t get_size(){
    return sizeof(x86_jmp_rel8);
  }
  void set_address(const void* address){
    int64_t adr = (uint64_t) address - (uint64_t) this - this->get_size();
    assert(adr <= INT8_MAX && adr >= INT8_MIN);
    this->offset = adr;
  }
  void* get_address(){
    return (void*) ((uint64_t) this + this->offset + this->get_size());
  }
};

struct x86_jmp_rel32 {
  uint32_t op : 8;
  int32_t offset : 32;
  x86_jmp_rel32() = default;
  x86_jmp_rel32(const x86_jmp_rel32 &other){
    this->op = other.op;
    this->offset = other.offset;
  }
  x86_jmp_rel32(int32_t offset){
    this->op = 0xe9;
    this->offset = offset;
  }
  x86_jmp_rel32(const void* address){
    this->op = 0xe9;
    this->set_address(address);
  }
  uint16_t get_size(){
    return sizeof(x86_jmp_rel32);
  }
  void set_address(const void* address){
    int64_t adr = (uint64_t) address - (uint64_t) this - this->get_size();
    assert(adr <= INT32_MAX && adr >= INT32_MIN);
    this->offset = adr;
  }
  void* get_address(){
    return (void*) ((uint64_t) this + this->offset + this->get_size());
  }
};

// doesn't work with registers r8 - r15
struct x86_jmp_r64 {
  uint32_t op : 8;
  uint32_t modrm : 8;
  x86_jmp_r64() = default;
  x86_jmp_r64(const x86_jmp_r64 &other){
    this->op = other.op;
    this->modrm = other.modrm;
  }
  x86_jmp_r64(uint32_t reg){
    this->op = 0xff;
    this->modrm = 0b11100000 + (reg & 0b111);
  }
  uint16_t get_size(){
    return sizeof(x86_jmp_r64);
  }
};

// also works with registers r8 - r15
struct x86_jmp_r64_full {
  uint32_t rex : 8;
  uint32_t op : 8;
  uint32_t modrm : 8;
  x86_jmp_r64_full() = default;
  x86_jmp_r64_full(const x86_jmp_r64_full &other){
    this->rex = other.rex;
    this->op = other.op;
    this->modrm = other.modrm;
  }
  x86_jmp_r64_full(uint32_t reg){
    this->rex = 0b01000000 + (0b1 * (reg > 7));
    this->op = 0xff;
    this->modrm = 0b11100000 + (reg & 0b111);
  }
  uint16_t get_size(){
    return sizeof(x86_jmp_r64_full);
  }
};

struct x86_jcc_rel8 {
  uint32_t op : 8;
  int32_t offset : 8;
  x86_jcc_rel8() = default;
  x86_jcc_rel8(const x86_jcc_rel8 &other){
    this->op = other.op;
    this->offset = other.offset;
  }
  x86_jcc_rel8(uint16_t cond, int32_t offset){
    this->op = cond;
    this->offset = offset;
  }
  x86_jcc_rel8(const uint16_t cond, const void* address){
    this->op = cond;
    this->set_address(address);
  }
  uint16_t get_size(){
    return sizeof(x86_jcc_rel8);
  }
  void set_address(const void* address){
    int64_t adr = (uint64_t) address - (uint64_t) this - this->get_size();
    assert(adr <= INT8_MAX && adr >= INT8_MIN);
    this->offset = adr;
  }
  void* get_address(){
    return (void*) ((uint64_t) this + this->offset + this->get_size());
  }
};

struct x86_jcc_rel32 {
  uint32_t op : 16;
  int32_t offset : 32;
  x86_jcc_rel32() = default;
  x86_jcc_rel32(const x86_jcc_rel32 &other){
    this->op = other.op;
    this->offset = other.offset;
  }
  x86_jcc_rel32(uint16_t cond, int32_t offset){
    this->op = 0x000f + ((cond + 0x10) << 8);
    this->offset = offset;
  }
  x86_jcc_rel32(const uint16_t cond, const void* address){
    this->op = 0x000f + ((cond + 0x10) << 8);
    this->set_address(address);
  }
  uint16_t get_size(){
    return sizeof(x86_jcc_rel32);
  }
  void set_address(const void* address){
    int64_t adr = (uint64_t) address - (uint64_t) this - this->get_size();
    assert(adr <= INT32_MAX && adr >= INT32_MIN);
    this->offset = adr;
  }
  void* get_address(){
    return (void*) ((uint64_t) this + this->offset + this->get_size());
  }
};

struct x86_mov_r8_imm8 {
  uint32_t rex : 8;
  uint32_t op : 8;
  int64_t imm : 8;
  x86_mov_r8_imm8() = default;
  x86_mov_r8_imm8(const x86_mov_r8_imm8 &other){
    this->rex = other.rex;
    this->op = other.op;
    this->imm = other.imm;
  }
  x86_mov_r8_imm8(uint32_t dest_reg, int8_t imm){
    this->rex = 0b01001000 + (0b1 * (dest_reg > 7));
    this->op = 0xb0 + (dest_reg & 0b111);
    this->imm = imm;
  }
  uint16_t get_size(){
    return sizeof(x86_mov_r8_imm8);
  }
};

struct x86_mov_m8_imm8 {
  uint32_t op : 8;
  uint32_t modrm : 8;
  int64_t imm : 8;
  x86_mov_m8_imm8() = default;
  x86_mov_m8_imm8(const x86_mov_m8_imm8 &other){
    this->op = other.op;
    this->modrm = other.modrm;
    this->imm = other.imm;
  }
  x86_mov_m8_imm8(uint32_t mem_reg, int8_t imm){
    this->op = 0xc6;
    this->modrm = 0b00000000 + (mem_reg & 0b111);
    this->imm = imm;
  }
  uint16_t get_size(){
    return sizeof(x86_mov_m8_imm8);
  }
};

struct x86_mov_r16_r16 {
  uint32_t op : 8;
  uint32_t modrm : 8;
  x86_mov_r16_r16() = default;
  x86_mov_r16_r16(const x86_mov_r16_r16 &other){
    this->op = other.op;
    this->modrm = other.modrm;
  }
  x86_mov_r16_r16(uint32_t dest_reg, uint32_t source_reg){
    this->op = 0x89;
    this->modrm = 0b11000000 + (dest_reg & 0b111) + ((source_reg & 0b111) << 3);
  }
  uint16_t get_size(){
    return sizeof(x86_mov_r16_r16);
  }
};

struct x86_mov_r16_imm16 {
  uint32_t flip : 8;
  uint32_t op : 8;
  int64_t imm : 16;
  x86_mov_r16_imm16() = default;
  x86_mov_r16_imm16(const x86_mov_r16_imm16 &other){
    this->flip = other.flip;
    this->op = other.op;
    this->imm = other.imm;
  }
  x86_mov_r16_imm16(uint32_t dest_reg, int64_t imm){
    this->flip = 0x66;
    this->op = 0xb8 + (dest_reg & 0b111);
    this->imm = imm;
  }
  uint16_t get_size(){
    return sizeof(x86_mov_r16_imm16);
  }
};

struct x86_mov_r32_r32 {
  uint32_t op : 8;
  uint32_t modrm : 8;
  x86_mov_r32_r32() = default;
  x86_mov_r32_r32(const x86_mov_r32_r32 &other){
    this->op = other.op;
    this->modrm = other.modrm;
  }
  x86_mov_r32_r32(uint32_t dest_reg, uint32_t source_reg){
    this->op = 0x89;
    this->modrm = 0b11000000 + (dest_reg & 0b111) + ((source_reg & 0b111) << 3);
  }
  uint16_t get_size(){
    return sizeof(x86_mov_r32_r32);
  }
};

struct x86_mov_r32_imm32 {
  uint32_t op : 8;
  int64_t imm : 32;
  x86_mov_r32_imm32() = default;
  x86_mov_r32_imm32(const x86_mov_r32_imm32 &other){
    this->op = other.op;
    this->imm = other.imm;
  }
  x86_mov_r32_imm32(uint32_t dest_reg, int64_t imm){
    this->op = 0xb8 + (dest_reg & 0b111);
    this->imm = imm;
  }
  uint16_t get_size(){
    return sizeof(x86_mov_r32_imm32);
  }
};

struct x86_mov_r32_m32 {
  uint32_t rex : 8;
  uint32_t op : 8;
  int64_t modrm : 8;
  x86_mov_r32_m32() = default;
  x86_mov_r32_m32(const x86_mov_r32_m32 &other){
    this->rex = other.rex;
    this->op = other.op;
    this->modrm = other.modrm;
  }
  x86_mov_r32_m32(uint32_t dest_reg, uint32_t mem_reg){
    this->rex = 0b01000000 + ((0b1 * (dest_reg > 7)) << 2) + (0b1 * (mem_reg > 7));
    this->op = 0x8b;
    this->modrm = 0b00000000 + (mem_reg & 0b111) + ((dest_reg & 0b111) << 3);
  }
  uint16_t get_size(){
    return sizeof(x86_mov_r32_m32);
  }
};



struct x86_mov_r64_r64 {
  uint32_t rex : 8;
  uint32_t op : 8;
  uint32_t modrm : 8;
  x86_mov_r64_r64() = default;
  x86_mov_r64_r64(const x86_mov_r64_r64 &other){
    this->rex = other.rex;
    this->op = other.op;
    this->modrm = other.modrm;
  }
  x86_mov_r64_r64(uint32_t dest_reg, uint32_t source_reg){
    this->rex = 0b01001000 + ((0b1 * (dest_reg > 7)) << 2) + (0b1 * (source_reg > 7));
    this->op = 0x89;
    this->modrm = 0b11000000 + (dest_reg & 0b111) + ((source_reg & 0b111) << 3);
  }
  uint16_t get_size(){
    return sizeof(x86_mov_r64_r64);
  }
};

struct x86_mov_r64_imm64 {
  uint32_t rex : 8;
  uint32_t op : 8;
  int64_t imm : 64;
  x86_mov_r64_imm64() = default;
  x86_mov_r64_imm64(const x86_mov_r64_imm64 &other){
    this->rex = other.rex;
    this->op = other.op;
    this->imm = other.imm;
  }
  x86_mov_r64_imm64(uint32_t dest_reg, int64_t imm){
    this->rex = 0b01001000 + (0b1 * (dest_reg > 7));
    this->op = 0xb8 + (dest_reg & 0b111);
    this->imm = imm;
  }
  uint16_t get_size(){
    return sizeof(x86_mov_r64_imm64);
  }
};

struct x86_mov_r64_m64 {
  uint32_t rex : 8;
  uint32_t op : 8;
  int64_t modrm : 8;
  x86_mov_r64_m64() = default;
  x86_mov_r64_m64(const x86_mov_r64_m64 &other){
    this->rex = other.rex;
    this->op = other.op;
    this->modrm = other.modrm;
  }
  x86_mov_r64_m64(uint32_t dest_reg, uint32_t mem_reg){
    this->rex = 0b01001000 + ((0b1 * (dest_reg > 7)) << 2) + (0b1 * (mem_reg > 7));
    this->op = 0x8b;
    this->modrm = 0b00000000 + (mem_reg & 0b111) + ((dest_reg & 0b111) << 3);
  }
  uint16_t get_size(){
    return sizeof(x86_mov_r64_imm64);
  }
};


struct x86_xor_r32_r32 {
  uint32_t op : 8;
  uint32_t modrm : 8;
  x86_xor_r32_r32() = default;
  x86_xor_r32_r32(const x86_xor_r32_r32 &other){
    this->op = other.op;
    this->modrm = other.modrm;
  }
  x86_xor_r32_r32(uint32_t dest_reg, uint32_t source_reg){
    this->op = 0x33;
    this->modrm = 0b11000000 + (dest_reg & 0b111) + ((source_reg & 0b111) << 3);
  }
  uint16_t get_size(){
    return sizeof(x86_xor_r32_r32);
  }
};

struct x86_xor_r64_r64 {
  uint32_t rex : 8;
  uint32_t op : 8;
  uint32_t modrm : 8;
  x86_xor_r64_r64() = default;
  x86_xor_r64_r64(const x86_xor_r64_r64 &other){
    this->rex = other.rex;
    this->op = other.op;
    this->modrm = other.modrm;
  }
  x86_xor_r64_r64(uint32_t dest_reg, uint32_t source_reg){
    this->rex = 0b01001000 + (0b1 * (source_reg > 7)) + (0b100 * (dest_reg > 7));
    this->op = 0x31;
    this->modrm = 0b11000000 + (dest_reg & 0b111) + ((source_reg & 0b111) << 3);
  }
  uint16_t get_size(){
    return sizeof(x86_mov_r64_r64);
  }
};

struct x86_ret_near {
  uint32_t op:8;
  x86_ret_near(){
    this->op = 0xc3;
  }
  x86_ret_near(const x86_ret_near &other){
    this->op = other.op;
  }
  uint16_t get_size(){
    return sizeof(x86_ret_near);
  }
};

struct x86_optional_success {
  x86_mov_r8_imm8 mov_rdx;
  x86_ret_near ret;
  
  x86_optional_success(){
    mov_rdx = x86_mov_r8_imm8(RDX, 1);
  }
  x86_optional_success(const x86_optional_success &other){
    this->mov_rdx = x86_mov_r8_imm8(other.mov_rdx);
    this->ret = x86_ret_near(other.ret);
  }
  uint16_t get_size(){
    return sizeof(x86_optional_success);
  }
};

struct x86_optional_success_full {
  x86_mov_r64_r64 mov_rax_rdx;
  x86_optional_success ret_success;
  x86_optional_success_full(){
    mov_rax_rdx = x86_mov_r64_r64(RAX, RDX);
  }
  x86_optional_success_full(const x86_optional_success_full &other){
    this->mov_rax_rdx = x86_mov_r64_r64(other.mov_rax_rdx);
    this->ret_success = x86_optional_success(other.ret_success);
  }
  uint16_t get_size(){
    return sizeof(x86_optional_success_full);
  }
};

struct x86_optional_failure {
  x86_xor_r32_r32 xor_rdx;
  x86_ret_near ret;
  x86_optional_failure(){
    xor_rdx = x86_xor_r32_r32(RDX, RDX);
  }
  x86_optional_failure(const x86_optional_failure &other){
    this->xor_rdx = x86_xor_r32_r32(other.xor_rdx);
    this->ret = x86_ret_near(other.ret);
  }
  uint16_t get_size(){
    return sizeof(x86_optional_failure);
  }
};

struct x86_optional_failure_full {
  x86_xor_r32_r32 xor_rax;
  x86_optional_failure ret_fail;
  x86_optional_failure_full(){
    xor_rax = x86_xor_r32_r32(RAX, RAX);
  }
  x86_optional_failure_full(const x86_optional_failure_full &other){
    this->xor_rax = x86_xor_r32_r32(other.xor_rax);
    this->ret_fail = x86_optional_failure(other.ret_fail);
  }
  uint16_t get_size(){
    return sizeof(x86_optional_failure_full);
  }
};


struct x86_full_bplus_ret {
  // uint8_t bytes[21] = {0x48, 0x89, 0xd0, 0x48, 0xc7, 0xc2, 0x01, 0x00, 0x00, 0x00, 0xc3};
  x86_mov_m8_imm8 mov_success;
  x86_optional_success_full ret;
  x86_full_bplus_ret(){
    this->mov_success = x86_mov_m8_imm8(RSI, 1);
  }
  x86_full_bplus_ret(const x86_full_bplus_ret &other){
    this->mov_success = x86_mov_m8_imm8(other.mov_success);
    this->ret = x86_optional_success_full(other.ret);
  }
  uint16_t get_size(){
    return sizeof(x86_full_bplus_ret);
  }
};

struct x86_debug {
  uint8_t bytes[1] = {0xcc};
  x86_debug(){}
  x86_debug(const x86_debug &other){}
  uint16_t get_size(){
    return sizeof(x86_debug);
  }
};



