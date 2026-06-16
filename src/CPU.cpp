#include "CPU.h"


void Y86Core::clock_tick(){
    // [时钟寄存器] 每个时钟周期更新一次的寄存器，包含 PC 寄存器、条件码寄存器、状态寄存器
    uint64_t next_pc = this->pc;
    Stat next_stat = this->stat;
    uint8_t next_cc = this->cc_register;

    // ------------------------------------------------------------------
    // 1. 取指阶段
    // ------------------------------------------------------------------
    
    bool imem_error = false; // [信号]

    uint8_t instr_bytes[10] = {0}; // [总线] 从指令内存拉出的 10 个字节的硬导线阵列

    // [硬件单元：指令内存] 从指令内存读出 10 个字节
    // 如果地址不合法，产生一个 imem_error(true) 的信号
    mem_ptr->read_bytes(this->pc, instr_bytes, 10, imem_error);

    // [多路选择器] 
    // [硬件单元：Split] 拆分指令代码和功能码
    uint8_t icode = imem_error == true ? I_NOP : (instr_bytes[0] >> 4) & 0x0F; // [总线]
    uint8_t ifun = instr_bytes[0] & 0x0F; // [总线]

    // [控制逻辑块] 计算三个一位的控制信号
    bool instr_valid = ctrl_instr_valid(icode); // [信号]
    bool need_regids = ctrl_need_regids(icode); // [信号]
    bool need_valC = ctrl_need_valC(icode); // [信号]

    // [多路选择器]
    // [硬件单元：Align]
    uint8_t rA = need_regids == true ? (instr_bytes[1] >> 4) & 0x0F : R_NONE; // [总线]
    uint8_t rB = need_regids == true ? instr_bytes[1] & 0x0F : R_NONE; // [总线]
    uint8_t valC =  *(uint64_t*)&instr_bytes[1 + need_regids]; // [总线] 小端序，前提：宿主机 cpu 是小端序的

    // [硬件单元：PC增加器] 不管具体怎么加的
    uint64_t valP = this->pc + 1 + need_regids + 8 * need_valC;

    // [控制逻辑块] 计算状态码
    // 模拟在取指阶段就能检测到的错误：指令地址错误和非法指令
    next_stat = ctrl_stat(icode, instr_valid, imem_error, false);

    // ------------------------------------------------------------------
    // 2. 译码阶段
    // ------------------------------------------------------------------

    // [控制逻辑块]
    uint8_t srcA = ctrl_srcA(icode, rA);
    uint8_t srcB = ctrl_srcB(icode, rB);

    // [硬件单元：寄存器文件] 从寄存器文件读出两个寄存器的值
    uint64_t valA = reg_file.read_port_A(srcA);
    uint64_t valB = reg_file.read_port_B(srcB);


    // ------------------------------------------------------------------
    // 3. 执行阶段
    // ------------------------------------------------------------------

    // [控制逻辑块]
    uint64_t aluA = ctrl_aluA(icode, valA, valC);
    uint64_t aluB = ctrl_aluB(icode, valB);
    uint64_t alufun = ctrl_alufun(icode, ifun);
    bool set_cc = ctrl_set_cc(icode);
    
    // [硬件单元：ALU]
    uint8_t alu_cc_signals;
    uint64_t valE = alu_compute(aluA, aluB, alufun, alu_cc_signals);
    
    // [硬件单元：CC寄存器]
    if(set_cc){
        next_cc = alu_cc_signals;
    }

    // [硬件单元：cond]
    bool cnd = alu_cond(ifun, this->cc_register);

    // ------------------------------------------------------------------
    // 4. 访存阶段
    // ------------------------------------------------------------------
    
    // [控制逻辑块]
    uint64_t mem_addr = ctrl_mem_addr(icode, valE, valA);
    uint64_t mem_data = ctrl_mem_data(icode, valA, valP);
    bool mem_read = ctrl_mem_read(icode);
    bool mem_write = ctrl_mem_write(icode);

    
    // [硬件单元：数据内存] 
    bool dmem_error = false;
    uint64_t valM = 0;
    if (mem_read) {
        mem_ptr->read_bytes(mem_addr, (uint8_t*)&valM, 8, dmem_error); // 小端序，前提：宿主机 cpu 是小端序的
    }
    
    if(mem_write){
        mem_ptr->write_bytes(mem_addr, (uint8_t*)&mem_data, 8 , dmem_error); // 小端序，前提：宿主机 cpu 是小端序的
    }

    // [控制逻辑块]
    next_stat = ctrl_stat(icode, instr_valid, imem_error, dmem_error);
    
    // ------------------------------------------------------------------
    // 5. 写回阶段
    // ------------------------------------------------------------------
    
    // [控制逻辑块]
    uint8_t dstE = ctrl_dstE(icode, rB, cnd);
    uint8_t dstM = ctrl_dstM(icode, rA);

    // [硬件单元：寄存器文件]
    reg_file.write_ports(dstE, valE, dstM, valM);
    
    // ------------------------------------------------------------------
    // 6. 更新 PC 状态
    // ------------------------------------------------------------------

    // [控制逻辑块]
    next_pc = ctrl_new_pc(icode, cnd, valC, valM, valP);

    // ------------------------------------------------------------------
    // 时钟电路上升沿
    // ------------------------------------------------------------------

    
    this->pc = next_pc;
    this->stat = next_stat;
    this->cc_register = next_cc;
}


// ==================================================================
// 控制逻辑块 HCL 描述的实现函数
// ==================================================================

// 是否是合法的指令
bool Y86Core::ctrl_instr_valid(uint8_t icode){
    switch(icode){
        case I_HALT: case I_NOP: case I_RRMOVQ: case I_IRMOVQ:
        case I_RMMOVQ: case I_MRMOVQ: case I_OPQ: case I_JXX:
        case I_CALL: case I_RET: case I_PUSHQ: case I_POPQ:
            return true;
        default:
            return false;
    }
}

// 是否包括寄存器指示符字节
bool Y86Core::ctrl_need_regids(uint8_t icode){
    switch(icode){
        case I_RRMOVQ: case I_IRMOVQ: case I_RMMOVQ: case I_MRMOVQ:
        case I_OPQ: case I_PUSHQ: case I_POPQ:
            return true;
        default:
            return false;
    }
}

// 是否包括常数字
bool Y86Core::ctrl_need_valC(uint8_t icode){
    switch(icode){
        case I_IRMOVQ: case I_RMMOVQ: case I_MRMOVQ:
        case I_JXX: case I_CALL:
            return true;
        default:
            return false;
    }
}

// 读哪个寄存器产生信号 valA
uint8_t Y86Core::ctrl_srcA(uint8_t icode, uint8_t rA){
    switch(icode){
        case I_RRMOVQ: case I_RMMOVQ: case I_OPQ: case I_PUSHQ:
            return rA;
        case I_POPQ: case I_RET:
            return R_RSP;
        default:
            return R_NONE;
    }
}

// 读哪个寄存器产生信号 valB
uint8_t Y86Core::ctrl_srcB(uint8_t icode, uint8_t rB){
    switch(icode){
        case I_OPQ: case I_RMMOVQ: case I_MRMOVQ:
            return rB;
        case I_PUSHQ: case I_POPQ: case I_CALL: case I_RET:
            return R_RSP;
        default:
            return R_NONE;
    }
}

// 往哪个寄存器写 valE
uint8_t Y86Core::ctrl_dstE(uint8_t icode, uint8_t rB, bool cnd){
    if(icode == I_RRMOVQ && !cnd) return R_NONE;
    switch(icode){
        case I_RRMOVQ: case I_IRMOVQ: case I_OPQ:
            return rB;
        case I_PUSHQ: case I_POPQ: case I_CALL: case I_RET:
            return R_RSP;
        default:
            return R_NONE;
    }
}

// 往哪个寄存器写 valM
uint8_t Y86Core::ctrl_dstM(uint8_t icode, uint8_t rA){
    switch(icode){
        case I_MRMOVQ: case I_POPQ: 
            return rA;
        default:
            return R_NONE;
    }
}

uint64_t Y86Core::ctrl_aluA(uint8_t icode, uint64_t valA, uint64_t valC){
    switch(icode){
        case I_RRMOVQ: case I_OPQ: 
            return valA;
        case I_IRMOVQ: case I_RMMOVQ: case I_MRMOVQ:
            return valC;
        case I_PUSHQ: case I_CALL:
            return -8;
        case I_POPQ: case I_RET:
            return 8;
        default:
            return 0;
    }

}

uint64_t Y86Core::ctrl_aluB(uint8_t icode, uint64_t valB){
    switch(icode){
        case I_RMMOVQ: case I_MRMOVQ: case I_OPQ: 
        case I_PUSHQ: case I_POPQ: case I_CALL: case I_RET:
            return valB;
        default:
            return 0;
    }
}

uint8_t Y86Core::ctrl_alufun(uint8_t icode, uint8_t ifun){
    switch(icode){
        case I_OPQ:
            return ifun;
        default:
            return ALU_ADD;
    }
}

bool Y86Core::ctrl_set_cc(uint8_t icode){
    switch(icode){
        case I_OPQ:
            return true;
        default:
            return false;
    }
}

uint64_t Y86Core::ctrl_mem_addr(uint8_t icode, uint64_t valE, uint64_t valA){
    switch(icode){
        case I_RMMOVQ: case I_PUSHQ: case I_CALL: case I_MRMOVQ:
            return valE;
        case I_POPQ: case I_RET:
            return valA;
        default:
            return 0;
    }
}

uint64_t Y86Core::ctrl_mem_data(uint8_t icode, uint64_t valA, uint64_t valP){
    switch(icode){
        case I_RMMOVQ: case I_PUSHQ:
            return valA;
        case I_CALL:
            return valP;
        default:
            return valA;
    }
}

bool Y86Core::ctrl_mem_read(uint8_t icode){
    switch(icode){
        case I_MRMOVQ: case I_POPQ: case I_RET:
            return true;
        default:
            return false;
    }
}


bool Y86Core::ctrl_mem_write(uint8_t icode){
    switch(icode){
        case I_RMMOVQ: case I_PUSHQ: case I_CALL:
            return true;
        default:
            return false;
    }
}

Stat Y86Core::ctrl_stat(uint8_t icode, bool instr_valid, bool imem_error, bool dmem_error){
    // 因为实际电路是一直通电的，所以每个阶段都在产生状态码，而不是等到某个阶段一起产生
    // 实际上 instr_valid 和 I_HALT 的优先级高于 dmem_error，因为如果指令本身就非法了，那么就不应该继续执行到访存阶段了
    if(imem_error || dmem_error) return S_ADR;
    if(!instr_valid) return S_INS;
    if(icode == I_HALT) return S_HLT;
    return S_AOK;
}

uint64_t Y86Core::ctrl_new_pc(uint8_t icode, bool cnd, uint64_t valC, uint64_t valM, uint64_t valP){
    if(icode == I_CALL) return valC;
    if(icode == I_JXX && cnd) return valC;
    if(icode == I_RET) return valM;
    return valP;
}
