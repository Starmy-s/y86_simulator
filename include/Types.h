#ifndef TYPES_H
#define TYPES_H

// 带有前缀，便于区分，以防重名，以及被系统宏定义修改

// 状态码
// Stat
enum Stat {
    S_AOK = 1,
    S_HLT = 2,
    S_ADR = 3,
    S_INS = 4
};

// 指令指示符————高 4 位：代码
// icode
enum OPcode {
    I_HALT = 0x0, I_NOP = 0x1, I_RRMOVQ = 0x2, I_IRMOVQ = 0x3,
    I_RMMOVQ = 0x4, I_MRMOVQ = 0x5, I_OPQ = 0x6, I_JXX = 0x7, 
    I_CALL = 0x8, I_RET = 0x9, I_PUSHQ = 0xA, I_POPQ = 0xB
};

// 指令指示符————低 4 位：整数操作功能码
// ifun
enum ALU_Fun{
    ALU_ADD = 0x0,
    ALU_SUB = 0x1,
    ALU_AND = 0x2,
    ALU_XOR = 0x3
};

// 指令指示符————低 4 位：分支与传送功能码
// ifun
enum Jump_Fun{
    F_JMP = 0x0,
    F_JLE = 0x1,
    F_JL = 0x2,
    F_JE = 0x3,
    F_JNE = 0x4,
    F_JGE = 0x5,
    F_JG = 0x6
};

// 寄存器标识符
// Register ID
enum Register{
    R_RAX = 0, R_RCX = 1, R_RDX = 2, R_RBX = 3,
    R_RSP = 4, R_RBP = 5, R_RSI = 6, R_RDI = 7,
    R_R8 = 8, R_R9 = 9, R_R10 = 10, R_R11 = 11,
    R_R12 = 12, R_R13 = 13, R_R14 = 14, R_NONE = 15 // 用于不需要寄存器操作数的寄存器指示符字节中占位
};


#endif // !TYPES_H