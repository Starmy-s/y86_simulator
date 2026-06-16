#ifndef MEMORY_H
#define MEMORY_H

#include "Types.h"

#include <vector>
#include <cstdint>
#include <cstring>

// 内存不太懂，先凑合随便实现一些功能，小端序的问题以后还要考虑
// 数据内存理论上也需要锁存，和时钟寄存器用一个时钟，先不考虑了
class Memory{
private:
    std::vector<uint8_t> storage;
    uint64_t size;

public:
    Memory(uint64_t bytes) : size(bytes){
        storage.resize(bytes, 0);
    }

    uint8_t read_byte(uint64_t addr, bool& mem_error) const {
        if(addr >= size){
            mem_error = true; 
            return 0;
        }
        mem_error = false;
        return storage[addr];
    }

    // 这个在取指的时候会有边缘指令不够10个字节取不出来的问题，以后再解决
    void read_bytes(uint64_t start_addr, uint8_t* dest_buffer, size_t num_bytes, bool& mem_error) const {
        // 有越界风险，先不管
        if(start_addr + num_bytes > size){
            mem_error = true;
            return;
        }
        mem_error = false;
        std::memcpy(dest_buffer, &storage[start_addr], num_bytes);
    }

    void write_bytes(uint64_t start_addr, const uint8_t* src_buffer, size_t num_bytes, bool& mem_error){
        // 有越界风险，先不管
        // num_bytes 在 cpp 层面有为0的可能，待思考
        if(start_addr + num_bytes > size){
            mem_error = true;
            return;
        }
        mem_error = false;
        std::memcpy(&storage[start_addr], src_buffer, num_bytes);
    }
};

#endif // !MEMORY_H