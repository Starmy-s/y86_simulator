#ifndef CPU_H
#define CPU_H

#include "Types.h"
#include "Memory.h"
#include "RegisterFile.h"

#include <vector>
#include <cstdint>

class Y86Core{
private:
    RegisterFile reg_file;
    uint64_t pc = 0;
    uint8_t cc_register;
    Memory* mem_ptr;
    Stat stat = S_AOK;
    Stat next_stat = S_AOK;

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
    Y86Core(Memory* memory);
    void clock_tick();
};

#endif // !CPU_H