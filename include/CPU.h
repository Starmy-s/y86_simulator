#ifndef CPU_H
#define CPU_H

#include "Types.h"
#include "Memory.h"
#include "RegisterFile.h"

#include <iostream>
#include <vector>
#include <cstdint>

class Y86Core{
private:
    RegisterFile reg_file;
    uint64_t pc = 0;
    uint8_t cc_register = 0;
    Memory* mem_ptr = nullptr;
    Stat stat = S_AOK;

    uint64_t alu_compute(uint64_t aluA, uint64_t aluB, uint8_t alufun, uint8_t& alu_cc_signals) const {
        uint64_t valE = 0;

        switch(alufun) {
            case ALU_ADD: valE = aluB + aluA; break;
            case ALU_SUB: valE = aluB - aluA; break;
            case ALU_AND: valE = aluB & aluA; break;
            case ALU_XOR: valE = aluB ^ aluA; break;
        }

        uint8_t signA = (aluA >> 63) & 1;
        uint8_t signB = (aluB >> 63) & 1;
        uint8_t signE = (valE >> 63) & 1;

        bool zf = (valE == 0);
        bool sf = (signE == 1);
        bool of = false;

        if(alufun == ALU_ADD){
            of = (signA == signB) && (signE != signA);
        }else{
            of = (signB != signA) && (signE != signB);
        }
        alu_cc_signals = (zf << 2) | (sf << 1) | of;

        return valE;
    }

    bool alu_cond(uint8_t ifun, uint8_t cc){
        bool zf = (cc >> 2) & 1;
        bool sf = (cc >> 1) & 1;
        bool of = cc & 1;

        switch (ifun) {
            case F_JMP: return true;
            case F_JLE: return (sf ^ of) || zf;
            case F_JL: return sf ^ of;
            case F_JE: return zf;
            case F_JNE: return !zf;
            case F_JGE: return !(sf ^ of);
            case F_JG: return !(sf ^ of) && !zf;
            default: return false; // 不加不让 build
        }
    }

    // 控制逻辑块 HCL 描述的实现函数
    bool ctrl_instr_valid(uint8_t icode);
    bool ctrl_need_regids(uint8_t icode);
    bool ctrl_need_valC(uint8_t icode);
    uint8_t ctrl_srcA(uint8_t icode, uint8_t rA);
    uint8_t ctrl_srcB(uint8_t icode, uint8_t rB);
    uint8_t ctrl_dstE(uint8_t icode, uint8_t rB, bool cnd);
    uint8_t ctrl_dstM(uint8_t icode, uint8_t rA);
    uint64_t ctrl_aluA(uint8_t icode, uint64_t valA, uint64_t valC);
    uint64_t ctrl_aluB(uint8_t icode, uint64_t valB);
    uint8_t ctrl_alufun(uint8_t icode, uint8_t ifun);
    bool ctrl_set_cc(uint8_t icode);
    uint64_t ctrl_mem_addr(uint8_t icode, uint64_t valE, uint64_t valA);
    uint64_t ctrl_mem_data(uint8_t icode, uint64_t valA, uint64_t valP);
    bool ctrl_mem_read(uint8_t icode);
    bool ctrl_mem_write(uint8_t icode);
    Stat ctrl_stat(uint8_t icode, bool instr_valid, bool imem_error, bool dmem_error);
    uint64_t ctrl_new_pc(uint8_t icode, bool cnd, uint64_t valC, uint64_t valM, uint64_t valP);

public:
    Y86Core(Memory* memory) : mem_ptr(memory) {}
    void clock_tick();

    Stat get_stat() const {
        return this->stat;
    }

    void dump_cpu_state() const {
        std::cout << "\n=================== 处理器物理现场快照 ===================" << std::endl;
        
        switch (this->stat) {
            case S_HLT: std::cout << ">> 状态编码 (Stat): S_HLT (Halt 正常停机)" << std::endl; break;
            case S_ADR: std::cout << ">> 状态编码 (Stat): S_ADR (Fatal! 内存越界异常)" << std::endl; break;
            case S_INS: std::cout << ">> 状态编码 (Stat): S_INS (Fatal! 非法指令异常)" << std::endl; break;
            case S_AOK: std::cout << ">> 状态编码 (Stat): S_AOK (健康运行中)" << std::endl; break;
        }
        
        std::cout << ">> 程序计数器 (PC)  : 0x" << std::hex << this->pc << std::dec << std::endl;
        std::cout << ">> 条件码 (CC)      : ZF=" << ((cc_register >> 2) & 1) 
                  << ", SF=" << ((cc_register >> 1) & 1) 
                  << ", OF=" << (cc_register & 1) << std::endl;
        
        std::cout << "----------------- 通用寄存器文件 (RF) -----------------" << std::endl;
        std::cout << "   %rax: 0x" << std::hex << reg_file.read_port_A(0) << std::endl;
        std::cout << "   %rcx: 0x" << reg_file.read_port_A(1) << std::endl;
        std::cout << "   %rdx: 0x" << reg_file.read_port_A(2) << std::dec << std::endl;
        std::cout << "==========================================================" << std::endl;
    }
};

#endif // !CPU_H