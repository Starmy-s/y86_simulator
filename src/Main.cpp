#include "CPU.h"
#include "Memory.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

/**
 * @brief 工业级 Loader：直接解析 .yo 文件并强灌进仿真器的物理内存对象中
 * @param yo_path .yo 文件的物理路径
 * @param mem 传入你的 Memory 实体引用
 */
bool load_yo_to_shared_memory(const std::string& yo_path, Memory& mem) {
    std::ifstream file(yo_path);
    if (!file.is_open()) {
        std::cerr << "[ Loader 错误 ] 无法打开物理文件: " << yo_path << std::endl;
        return false;
    }

    std::string line;
    bool write_error = false;
    
    while (std::getline(file, line)) {
        // 1. 过滤无关行：必须包含 0x 和 :
        if (line.find("0x") == std::string::npos || line.find(":") == std::string::npos) continue;
        
        // 2. 提取物理绝对地址 (例如 "  0x000a:" -> 0x000a)
        size_t addr_start = line.find("0x") + 2;
        size_t addr_end = line.find(":");
        uint64_t phys_addr = std::stoull(line.substr(addr_start, addr_end - addr_start), nullptr, 16);
        
        // 3. 提取机器码子串 (找到 ":" 和 "|" 之间的部分)
        size_t pipe_pos = line.find("|");
        if (pipe_pos == std::string::npos) continue;
        std::string hex_code = line.substr(addr_end + 1, pipe_pos - (addr_end + 1));
        
        // 剔除空格
        hex_code.erase(std::remove_if(hex_code.begin(), hex_code.end(), ::isspace), hex_code.end());
        if (hex_code.empty()) continue; // 纯标号行，无实质机器码，跳过

        // 4. 将双字符 Hex 转换为 uint8_t 缓冲阵列
        std::vector<uint8_t> line_bytes;
        for (size_t i = 0; i < hex_code.length(); i += 2) {
            uint8_t byte = static_cast<uint8_t>(std::stoul(hex_code.substr(i, 2), nullptr, 16));
            line_bytes.push_back(byte);
        }

        // 5. 调用你封装好的 Memory 组件，将这行机器码物理写入总线
        mem.write_bytes(phys_addr, line_bytes.data(), line_bytes.size(), write_error);
        if (write_error) {
            std::cerr << "[ 物理错误 ] 内存写入越界！地址: 0x" << std::hex << phys_addr << std::endl;
            return false;
        }
    }
    
    std::cout << "▶️ [Loader] 汇编机器码成功注入系统主板物理内存。" << std::endl;
    return true;
}


int main(int argc, char* argv[]) {
    std::cout << "[硬件主板] 正在通电并初始化物理电路..." << std::endl;
    
    // 1. 初始化 64KB 物理内存与 CPU 核心
    Memory shared_memory(64 * 1024);
    Y86Core cpu(&shared_memory);

    // 2. 动态获取要运行的 .yo 路径（支持命令行传参，默认读取本地的 sum.yo）
    std::string yo_file = "machine_code/sum.yo";
    if (argc > 1) {
        yo_file = argv[1];
    }

    std::cout << "[硬件主板] 正在从目标路径引导固件: " << yo_file << " ..." << std::endl;

    // 3. 放弃之前的硬编码写死数组，直接通过 Loader 强灌物理总线
    if (!load_yo_to_shared_memory(yo_file, shared_memory)) {
        std::cerr << "[物理错误] 固件引导（机器码加载）失败！断电熔断。" << std::endl;
        return -1;
    }

    // 4. 重置/确保 CPU 的初始 PC 指向 0x0000（或者是你在汇编里用 .pos 指定的入口）
    // 如果你的 Y86Core 默认 PC 就是 0，这行可以不写，或者在这里显式指定：
    // cpu.set_pc(0x0000); 

    std::cout << "[硬件主板] 初始化完毕。当前 PC = 0x0000，CPU 处于 AOK 状态。" << std::endl;
    std::cout << "=================== 晶振开始工作 ===================" << std::endl;

    uint64_t cycle_count = 0;

    // 5. 主频时钟震荡死循环
    while (true) {
        cycle_count++;
        
        // 触发单周期时钟沿，内部执行 Fetch -> Decode -> Execute -> Memory -> Writeback
        cpu.clock_tick();

        // 检查状态寄存器（一旦发生 S_HLT 或错误状态，立马熔断断电）
        if (cpu.get_stat() != S_AOK) {
            std::cout << "\n=================== 晶振停止震荡 ===================" << std::endl;
            std::cout << "[时钟熔断] 在第 " << std::dec << cycle_count << " 个周期检测到非 AOK 状态！" << std::endl;
            break;
        }
    }

    // 6. 打印最终寄存器和状态快照，用于对账
    cpu.dump_cpu_state();

    return 0;
}