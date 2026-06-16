# 🗺️ 02. 指令集架构 (ISA) 物理字典

在计算机体系结构中，指令集架构（ISA）是软硬件之间最核心的接口协议。对于硬件工程师而言，ISA 就是设计**译码器（Instruction Decoder）组合逻辑电路**时的硬核真值表；对于软件工程师而言，它决定了编译器输出的二进制字节流如何被 CPU 物理识别。

Y86-64 指令集最大的特征之一是**变长编码（Variable-length Encoding）**（指令长度从 1 字节到 10 字节不等）。本字典完整汇总了 Y86-64 的机器码拓扑结构、功能位真值表以及寄存器物理引脚编码。

---

## 一、 Y86-64 指令拓扑结构总表

机器码的第 0 个字节（Byte 0）永远被对半平分：高 4 位为指令代码（`icode`），低 4 位为指令功能（`ifun`）。指令内部的每一个字节都严格对应着特定的硬件分流导线。

| 汇编指令语法 (Assembly) | 机器码拓扑结构 (Hex Layout) | 长度 (Bytes) | 硬件核心行为与数据流向 |
| --- | --- | --- | --- |
| `halt` | `00` | 1 | 状态控制块激活，`Stat` 强制刷写为 `S_HLT`，晶振熔断断电。 |
| `nop` | `10` | 1 | 空操作，全机控制线保持静默，PC 顺延加 1。 |
| `rrmovq rA, rB` | `20 rArB` | 2 | 寄存器堆读端口 A 接通 `rA`，预写通道接通 `rB`：$valA \rightarrow dstE$。 |
| `cmovXX rA, rB` | `2X rArB` ($X \in [1, 6]$) | 2 | 条件传送：根据条件判定电路 $cnd$ 的输出决定是否关闭写回使能。 |
| `irmovq V, rB` | `30 f rB [8-Byte V]` | 10 | 8 字节立即数经总线送入 ALU，写回通道锁存进 $rB$。$rA$ 引脚置为 `f`（断路）。 |
| `rmmovq rA, D(rB)` | `40 rArB [8-Byte D]` | 10 | **访存写**：ALU 算出物理地址 $valB + valC$，激活内存写总线强灌 $valA$。 |
| `mrmovq D(rB), rA` | `50 rArB [8-Byte D]` | 10 | **访存读**：ALU 算出物理地址 $valB + valC$，激活内存读总线，将 $valM$ 锁入 $rA$。 |
| `opq rA, rB` | `6X rArB` ($X \in [0, 3]$) | 2 | **ALU核心计算**：选通对应运算器，**拉高 `set_cc` 信号线**更新条件码。 |
| `jXX Dest` | `7X [8-Byte Dest]` ($X \in [0, 6]$) | 9 | **条件跳转**：根据条件判断电路输出，`next_pc` 选通 $valC$（跳）或 $valP$（不跳）。 |
| `call Dest` | `80 [8-Byte Dest]` | 9 | 栈指针引脚选通 `%rsp`，ALU 自动执行减 8，将返回地址 $valP$ 压入物理内存栈。 |
| `ret` | `90` | 1 | 栈指针引脚选通 `%rsp`，从物理栈顶读出返回地址，直接强刷给 `next_pc`，栈指针加 8。 |
| `pushq rA` | `a0 rA f` | 2 | 栈操作：`%rsp` 经 ALU 减 8 算出新栈顶，将寄存器 $rA$ 的电平数据写入该内存地址。 |
| `popq rA` | `b0 rA f` | 2 | 栈操作：从当前 `%rsp` 栈顶读出 8 字节写入 $rA$，同时 `%rsp` 经 ALU 加 8 锁回。 |

---

## 二、 功能位（`ifun`）硬件译码真值表

当控制逻辑块（Control Logic）读取到 `icode` 后，若涉及到算术逻辑计算（`I_OPQ`）、条件跳转（`I_JXX`）或条件传送（`I_RRMOVQ` 衍生），会进一步打开低 4 位的 `ifun` 电子开关。它们在微架构层面建立了严密的布尔逻辑对齐：

### 1. 算术逻辑控制线 (`icode = 6: opq`)

通过 `ifun` 直接驱动 ALU 内部的数据选择器，物理切换不同的运算器通路：

* `60` ➡️ **`addq`** (激活通用加法器补码电路)
* `61` ➡️ **`subq`** (激活减法器电路，执行 $valB - valA$)
* `62` ➡️ **`andq`** (接通物理与门阵列)
* `63` ➡️ **`xorq`** (接通物理异或门阵列)

### 2. 条件控制线 (`icode = 7: jXX` 或 `icode = 2: cmovXX`)

通过 `ifun` 选通不同的条件判定组合逻辑电路（对应的 C++ 行为级判断函数），与条件码（ZF-零标志、SF-符号标志、OF-溢出标志）实时并联：

* `X0` ➡️ **`jmp` / `rrmovq**` (无条件激活：信号恒为 1)
* `X1` ➡️ **`jle` / `cmovle**` (小于等于：$(SF \oplus OF) \lor ZF$)
* `X2` ➡️ **`jl` / `cmovl**` (小于：$SF \oplus OF$)
* `X3` ➡️ **`je` / `cmove**` (等于：$ZF$)
* `X4` ➡️ **`jne` / `cmovne**` (不等于：$\neg ZF$)
* `X5` ➡️ **`jge` / `cmovge**` (大于等于：$\neg(SF \oplus OF)$)
* `X6` ➡️ **`jg` / `cmovg**` (大于：$\neg(SF \oplus OF) \land \neg ZF$)

---

## 三、 通用寄存器堆（Register File）物理引脚编码

在机器码中，寄存器是用 **4 个 Bit（半字节/十六进制单数）** 来表示的。这 4 位二进制数直接作为寄存器堆硬件阵列的**物理选通地址引脚**。控制逻辑块通过将这些编码导向读端口（`srcA`/`srcB`）或写端口（`dstE`/`dstM`）来实现精准的数据存取。

| 寄存器十六进制编码 (Hex) | 映射的通用寄存器 (Register) | 硬件微架构特殊用途 (Architectural Role) |
| --- | --- | --- |
| `0` | **`%rax`** | 常用于暂存返回值 / 算术一号累加器 |
| `1` | **`%rcx`** | 通用寄存器 / 算术二号累加器 |
| `2` | **`%rdx`** | 通用寄存器 |
| `3` | **`%rbx`** | 通用寄存器 |
| `4` | **`%rsp`** | **运行时堆栈指针（Stack Pointer）**，被 `call/ret/push/pop` 硬件硬连线强行控制 |
| `5` | **`%rbp`** | 栈帧基址指针（Frame Pointer） |
| `6` | **`%rsi`** | 通用寄存器 |
| `7` | **`%rdi`** | 通用寄存器 |
| `8` - `e` | **`%r8` - `%r14**` | 通用扩展寄存器 |
| `f` | **`R_NONE`** | **物理断路状态**。代表本周期该引脚不连接任何寄存器（如 `irmovq` 的源寄存器端口） |

---

## 🛠️ 规范映射：C++ 开发中的硬核类型对齐

为了让仿真器项目的代码具备纯正的“体系结构味”，我们必须根据本字典，在 `Types.h` 中将这些物理十六进制编码转化为强类型枚举（Strongly Typed Enums）。这不仅极大地提升了项目的可读性，还能借助编译器直接拦截非法引脚编码：

```cpp
#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

// 1. 指令大闸代码 (icode) 真值定义
enum InstructionCode : uint8_t {
    I_HALT   = 0x0, // 0
    I_NOP    = 0x1, // 1
    I_RRMOVQ = 0x2, // 2 (包含 cmovXX)
    I_IRMOVQ = 0x3, // 3
    I_RMMOVQ = 0x4, // 4
    I_MRMOVQ = 0x5, // 5
    I_OPQ    = 0x6, // 6
    I_JXX    = 0x7, // 7
    I_CALL   = 0x8, // 8
    I_RET    = 0x9, // 9
    I_PUSHQ  = 0xA, // 10
    I_POPQ   = 0xB  // 11
};

// 2. ALU 功能模式控制线 (ifun) 真值定义
enum AluFunction : uint8_t {
    A_ADD = 0x0,
    A_SUB = 0x1,
    A_AND = 0x2,
    A_XOR = 0x3
};

// 3. 通用寄存器堆物理引脚选通编码
enum RegisterID : uint8_t {
    R_RAX  = 0x0, R_RCX  = 0x1, R_RDX  = 0x2, R_RBX  = 0x3,
    R_RSP  = 0x4, R_RBP  = 0x5, R_RSI  = 0x6, R_RDI  = 0x7,
    R_R8   = 0x8, R_R9   = 0x9, R_R10  = 0xA, R_R11  = 0xB,
    R_R12  = 0xC, R_R13  = 0xD, R_R14  = 0xE, 
    R_NONE = 0xF  // 1111 物理断路状态
};

// 4. 处理器运行状态编码状态线
enum StatusCore : uint8_t {
    S_AOK = 0x1,  // 正常运行
    S_HLT = 0x2,  // 捕获 halt，正常断电停机
    S_ADR = 0x3,  // 访存或取指物理地址越界异常
    S_INS = 0x4   // 捕获非法指令机器码（未定义的 icode）
};

#endif // TYPES_H

```

有了这份全完备的 ISA 字典，无论是当前单周期（SEQ）译码级代码的编写，还是后续向更高级的 5 级流水线（PIPE）架构重构，你都能通过该字典在零点几秒内完成任意字节流的电路级对齐。

接下来，请翻阅 **`[⚡ 03. 全指令生命周期与物理电平流转]`**，看这套数字化主板是如何驱动电流，将每一条具体的指令在 6 大微架构阶段中彻底跑通的！