import re
import sys

# 全套指令集的 (icode, ifun, 物理长度)
INSTRUCTION_TABLE = {
    "halt": (0x0, 0, 1), "nop": (0x1, 0, 1), 
    "rrmovq": (0x2, 0, 2), "cmovle": (0x2, 1, 2), "cmovl": (0x2, 2, 2), "cmove": (0x2, 3, 2), "cmovne": (0x2, 4, 2), "cmovge": (0x2, 5, 2), "cmovg": (0x2, 6, 2),
    "irmovq": (0x3, 0, 10), "rmmovq": (0x4, 0, 10), "mrmovq": (0x5, 0, 10), 
    "addq": (0x6, 0, 2), "subq": (0x6, 1, 2), "andq": (0x6, 2, 2), "xorq": (0x6, 3, 2), 
    "jmp": (0x7, 0, 9), "jle": (0x7, 1, 9), "jl": (0x7, 2, 9), "je": (0x7, 3, 9), "jne": (0x7, 4, 9), "jge": (0x7, 5, 9), "jg": (0x7, 6, 9),
    "call": (0x8, 0, 9), "ret": (0x9, 0, 1), "pushq": (0xA, 0, 2), "popq": (0xB, 0, 2)
}

REGISTER_TABLE = {
    "%rax": 0, "%rcx": 1, "%rdx": 2, "%rbx": 3, "%rsp": 4, "%rbp": 5, "%rsi": 6, "%rdi": 7,
    "%r8": 8, "%r9": 9, "%r10": 10, "%r11": 11, "%r12": 12, "%r13": 13, "%r14": 14, "none": 15
}

def pre_process(lines):
    """剔除注释，并把标号行统一拆分，消除行内标号不一致隐患"""
    processed = []
    for line in lines:
        line = line.split('#')[0].strip()
        if not line: continue
        
        if ":" in line:
            parts = line.split(":", 1)
            processed.append(parts[0].strip() + ":")
            if parts[1].strip():
                processed.append(parts[1].strip())
        else:
            processed.append(line)
    return processed

def parse_mem_args(arg):
    """硬核解析 D(rB) 寻址语法。例如 8(%rsp) -> D=8, rB=%rsp; (%rax) -> D=0, rB=%rax"""
    match = re.match(r'([-\w]*)\((%?\w+)\)', arg)
    if match:
        d_str, rb_str = match.groups()
        d = int(d_str, 0) if d_str else 0
        return d, rb_str
    return 0, "none"

def assemble(filename):
    with open(filename, 'r') as f:
        lines = pre_process(f.readlines())

    symbol_table = {}
    location_counter = 0

    # PASS 1: 计算精确物理地标
    for line in lines:
        if line.startswith(".pos"):
            location_counter = int(line.split()[1], 0)
            continue
        if line.endswith(":"):
            symbol_table[line[:-1]] = location_counter
            continue
        
        tokens = re.split(r'[,\s]+', line)
        op = tokens[0]
        if op in INSTRUCTION_TABLE:
            location_counter += INSTRUCTION_TABLE[op][2]
        elif op == ".quad":
            location_counter += 8

    # PASS 2: 实施全面的全口径二进制电平灌注
    location_counter = 0
    for line in lines:
        if line.startswith(".pos"):
            location_counter = int(line.split()[1], 0)
            continue
        if line.endswith(":"):
            continue

        tokens = re.split(r'[,\s]+', line)
        op = tokens[0]
        machine_code = bytearray()

        # 1. 静态常数伪指令处理
        if op == ".quad":
            val_str = tokens[1]
            val = symbol_table[val_str] if val_str in symbol_table else int(val_str, 0)
            machine_code.extend(val.to_bytes(8, byteorder='little', signed=True))
            
        # 2. 标准指令集处理
        elif op in INSTRUCTION_TABLE:
            icode, ifun, length = INSTRUCTION_TABLE[op]
            machine_code.append((icode << 4) | ifun)

            if op == "irmovq":  # 30 f rB V
                val_str = tokens[1].replace("$", "")
                val = symbol_table[val_str] if val_str in symbol_table else int(val_str, 0)
                rB = REGISTER_TABLE[tokens[2]]
                machine_code.append((15 << 4) | rB)
                machine_code.extend(val.to_bytes(8, byteorder='little', signed=True))

            elif op == "rmmovq": # 40 rA rB D
                rA = REGISTER_TABLE[tokens[1]]
                d, rB_str = parse_mem_args(tokens[2])
                rB = REGISTER_TABLE[rB_str]
                machine_code.append((rA << 4) | rB)
                machine_code.extend(d.to_bytes(8, byteorder='little', signed=True))

            elif op == "mrmovq": # 50 rA rB D
                d, rB_str = parse_mem_args(tokens[1])
                rB = REGISTER_TABLE[rB_str]
                rA = REGISTER_TABLE[tokens[2]]
                machine_code.append((rA << 4) | rB)
                machine_code.extend(d.to_bytes(8, byteorder='little', signed=True))

            elif op in ["addq", "subq", "andq", "xorq"] or op.startswith("cmov"): # 2 字节指令 (rArB)
                rA = REGISTER_TABLE[tokens[1]]
                rB = REGISTER_TABLE[tokens[2]]
                machine_code.append((rA << 4) | rB)

            elif op in ["pushq", "popq"]: # 2 字节单寄存器指令 (rAf)
                rA = REGISTER_TABLE[tokens[1]]
                machine_code.append((rA << 4) | 15)

            elif op.startswith("j") or op == "call": # 9 字节跳转指令 (icode+ifun, Dest)
                dest_str = tokens[1]
                dest = symbol_table[dest_str] if dest_str in symbol_table else int(dest_str, 0)
                machine_code.extend(dest.to_bytes(8, byteorder='little', signed=True))

        if machine_code:
            hex_str = machine_code.hex()
            print(f"  0x{location_counter:04x}:    {hex_str:<22} | {line}")
            location_counter += len(machine_code)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 yas.py <test.ys>")
    else:
        assemble(sys.argv[1])