#ifndef REGISTER_FILE_H
#define REGISTER_FILE_H

#include "Types.h"

#include <cstdint>
#include <array>

// 理论上也需要锁存，和时钟寄存器用一个时钟，先不考虑了
class RegisterFile{
private:
    std::array<uint64_t, 15> regs = {0};
public:
    uint64_t read_port_A(uint64_t srcA) const {
        if(srcA == R_NONE) return 0; // 档位 15 接地
        return regs[srcA];
    }

    uint64_t read_port_B(uint64_t srcB) const {
        if(srcB == R_NONE) return 0; // 档位 15 接地
        return regs[srcB];
    }

    void write_ports(uint8_t dstE, uint64_t valE, uint8_t dstM, uint64_t valM){
        if(dstE != R_NONE)
            regs[dstE] = valE;

        // pushq 和 popq 可能会同时写 dstE 和 dstM
        // 写 valE 之后再写 valM 会覆盖掉 valE 的值，所以先写 valE 再写 valM
        // 实际电路是如何呢？暂时不考虑
        if(dstM != R_NONE)
            regs[dstM] = valM;
    }
};

#endif // !REGISTER_FILE_H 