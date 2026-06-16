#include "CPU.h"
#include "Memory.h"

#include <iostream>
#include <vector>


int main(){

    std::cout << "[硬件主板] 正在通电并初始化物理电路..." << std::endl;
    Memory shared_memory(64 * 1024);

    Y86Core cpu(&shared_memory);

    std::vector<uint8_t> machine_code = {
        0x30, 0xf0, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // irmovq $0x1234, %rax
        0x30, 0xf1, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // irmovq $0x7856, %rcx
        0x60, 0x01,                                                 // addq %rax, %rcx
        0x00                                                        // halt
    };

    bool load_error = false;
    shared_memory.write_bytes(0x0000, machine_code.data(), machine_code.size(), load_error);

    if (load_error) {
        std::cerr << "[物理错误] 机器码加载到主板内存失败！" << std::endl;
        return -1;
    }

    std::cout << "[硬件主板] 初始化完毕。当前 PC = 0x0000，CPU 处于 AOK 状态。" << std::endl;
    std::cout << "=================== 晶振开始工作 ===================" << std::endl;

    uint64_t cycle_count = 0;

    while (true) {
        cycle_count++;
        
        cpu.clock_tick();

        if (cpu.get_stat() != S_AOK) {
            std::cout << "\n=================== 晶振停止震荡 ===================" << std::endl;
            std::cout << "[时钟熔断] 在第 " << cycle_count << " 个周期检测到非 AOK 状态！" << std::endl;
            break;
        }
    }

    cpu.dump_cpu_state();

    return 0;
}
