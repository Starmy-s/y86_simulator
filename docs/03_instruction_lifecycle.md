# ⚡ 03. 全指令生命周期与物理电平流转

在单周期（SEQ）微架构中，**每一个时钟周期，CPU 都将雷打不动地走完完整的六个阶段**。所有的物理组合逻辑电平在极短的纳秒级时间内，从取指端一路激荡、分流、运算并传递到存储端的锁存引脚前，等待时钟上升沿的最终拍板。

本手册将为你逐一拆解 Y86-64 指令集中所有 13 条核心指令在芯片内的全景流转过程，带你彻底看清电流在数字化主板上的运动轨迹。

---

## 🛠️ SEQ 核心微架构六阶段定义

为了方便对照后续的指令拆解，请先牢记这六个物理阶段的严密定义：

1. **取指 (Fetch)**：根据当前程序计数器 `pc`，从主存中搂出变长的机器码字节。通过拆分器得到 `icode:ifun` 和可能存在的 `rA:rB` 及 8 字节立即数 `valC`。计算出顺序推进地址 `valP = pc + 指令长度`。
2. **译码 (Decode)**：寄存器堆（Register File）根据控制逻辑给出的 `srcA` 和 `srcB` 地址引脚，自发引出对应的电平数据 `valA` 和 `valB`。
3. **执行 (Execute)**：算术逻辑单元（ALU）根据控制线 `alu_fun` 切换通路，对输入端数据进行硬件级运算，吐出计算成果 `valE`。同时判定是否拉高 `set_cc` 锁存标志位，或计算分支选通信号 `cnd`。
4. **访存 (Memory)**：内存控制块根据使能信号，接通数据总线，执行读内存（从物理地址读出 8 字节存入 `valM`）或写内存（将数据灌入物理地址）。
5. **写回 (Writeback)**：寄存器堆的写使能打开，根据 `dstE` 和 `dstM` 的引脚编号，将当前周期算出来的 `valE` 或从内存捞出来的 `valM` 送到对应寄存器的预写端。
6. **时钟更新 (PC Update)**：时钟沿发生跳变，合闸锁存！下一周期的 `pc` 寄存器根据当前的多路选择器（MUX）状态，更新为 `next_pc`。

---

## 🗺️ 13条指令硬核流转步调逐一解剖

### 1. `halt` (停机指令)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (读出 `00`)；状态控制块检测到 `icode == 0`，下一周期预写状态设为 `S_HLT`；顺序推进地址 `valP <- pc + 1`。
* **Decode (译码)**: 无任何寄存器读操作，`srcA <- R_NONE`, `srcB <- R_NONE`。
* **Execute (执行)**: 无。
* **Memory (访存)**: 无。
* **Writeback (写回)**: 无。
* **PC Update (时钟更新)**:
状态寄存器 `Stat` 发生同步合闸，被强刷为 **`S_HLT`**。外部主板的晶振检测到该电平，**晶振循环当场休克，全机安全断电保护**。

---

### 2. `nop` (空操作指令)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (读出 `10`)；`valP <- pc + 1`。
* **Decode/Execute/Memory/Writeback**:
控制逻辑发出全局静默信号，所有硬件实体（ALU, 寄存器写通道, 内存总线）的使能信号线全部拉低为 `0`。电信号在门电路里空转，没有任何状态被改写。
* **PC Update (时钟更新)**:
`pc <- valP`。时钟沿跳变，程序计数器前进一步，指向 `pc + 1`。

---

### 3. `rrmovq rA, rB` (寄存器-寄存器传送)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (`20`)；`rA:rB <- M[pc + 1]`；`valP <- pc + 2`。
* **Decode (译码)**:
`srcA <- rA` ➡️ 寄存器堆读端口 A 接通，自发引出 `valA <- R[rA]`。
* **Execute (执行)**:
`valE <- 0 + valA`。ALU 的加法通道开门，但输入端 B 被灌入 0 电平，直接透传 `valA`。
* **Memory (访存)**: 无。
* **Writeback (写回)**:
`dstE <- rB` ➡️ 将透传出来的 `valE` 数据送往目标寄存器 `rB` 的写回入口。
* **PC Update (时钟更新)**:
`R[rB] <- valE` 且 `pc <- valP`。时钟沿合闸，`rB` 抽屉被重写，PC 推进 2 字节。

---

### 4. `cmovXX rA, rB` (条件传送)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (`2X`)；`rA:rB <- M[pc + 1]`；`valP <- pc + 2`。
* **Decode (译码)**:
`srcA <- rA` ➡️ 引出 `valA <- R[rA]`。
* **Execute (执行)**:
`valE <- 0 + valA`。同时，**物理条件判定电路通电**：并联条件码寄存器，算出 `cnd <- alu_cond(ifun, CC)`（输出 1 或 0）。
* **Memory (访存)**: 无。
* **Writeback (写回)**:
`dstE <- cnd ? rB : R_NONE`。
* 💡 **硬核控制线**：如果条件不满足（`cnd == 0`），多路选择器（MUX）直接将写回目标引脚强行切换为断路状态 `R_NONE`。


* **PC Update (时钟更新)**:
`pc <- valP`。若 `cnd == 1`，数据在时钟沿被锁入 `rB`；若 `cnd == 0`，由于写引脚断路，写使能关闭，数据锁存失败，寄存器完美保持原值。

---

### 5. `irmovq V, rB` (立即数-寄存器传送)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (`30`)；`rA:rB <- M[pc + 1]` (读出 `f rB`)；
`valC <- M[pc + 2]` (数据总线连捞 8 字节立即数)；`valP <- pc + 10`。
* **Decode (译码)**: 无源寄存器，`srcA <- R_NONE`。
* **Execute (执行)**:
`valE <- 0 + valC`。立即数总线连接到 ALU 并透传。
* **Memory (访存)**: 无。
* **Writeback (写回)**:
`dstE <- rB` ➡️ 将立即数成果 `valE` 投递给目标寄存器 `rB` 的输入端。
* **PC Update (时钟更新)**:
`R[rB] <- valE` 且 `pc <- valP`。时钟上升沿同步锁存，PC 狂飙 10 字节。

---

### 6. `rmmovq rA, D(rB)` (寄存器入物理内存/写内存)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (`40`)；`rA:rB <- M[pc + 1]`；`valC <- M[pc + 2]` (8字节物理偏移量 $D$)；`valP <- pc + 10`。
* **Decode (译码)**:
`srcA <- rA` ➡️ `valA <- R[rA]` (提取要存进内存的数据)；
`srcB <- rB` ➡️ `valB <- R[rB]` (提取基地址)。
* **Execute (执行)**:
`valE <- valB + valC`。ALU 执行补码加法，算出**精准的物理内存目标地址**。
* **Memory (访存)**:
激活内存写总线使能信号。将寄存器里抽出来的 `valA` 强行灌入内存槽位 `M[valE]`。
* **Writeback (写回)**: 无寄存器写回。
* **PC Update (时钟更新)**:
`pc <- valP`。时钟沿更新，内存状态被永久改写。

---

### 7. `mrmovq D(rB), rA` (内存出栈入寄存器/读内存)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (`50`)；`rA:rB <- M[pc + 1]`；`valC <- M[pc + 2]`；`valP <- pc + 10`。
* **Decode (译码)**:
`srcB <- rB` ➡️ `valB <- R[rB]` (提取基地址)。
* **Execute (执行)**:
`valE <- valB + valC`。ALU 计算出物理内存目标地址。
* **Memory (访存)**:
激活内存读总线使能信号。从物理地址 `M[valE]` 中抽取 8 字节电平，存入局部总线变量 `valM`。
* **Writeback (写回)**:
`dstM <- rA` ➡️ 接通寄存器 `rA` 的写入通道，将内存捞出来的 `valM` 送过去。
* **PC Update (时钟更新)**:
`R[rA] <- valM` 且 `pc <- valP`。时钟沿合闸，数据安全进驻寄存器。

---

### 8. `opq rA, rB` (算术逻辑核心运算)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (`6X`)；`rA:rB <- M[pc + 1]`；`valP <- pc + 2`。
* **Decode (译码)**:
`srcA <- rA` ➡️ `valA <- R[rA]`；
`srcB <- rB` ➡️ `valB <- R[rB]`。
* **Execute (执行)**:
`valE <- valB OP valA`（根据 `ifun` 真值表，硬件瞬间选择加、减、与、异或运算电路）。
🔥 **硬核微操作**：控制信号拉高 `set_cc` 导线。条件码判定电路根据 `valE` 的高位和零状态，实时计算出下一周期 CC 的预写值。
* **Memory (访存)**: 无。
* **Writeback (写回)**:
`dstE <- rB` ➡️ 将计算成果 `valE` 送回目标寄存器 `rB`。
* **PC Update (时钟更新)**:
`R[rB] <- valE`，条件码寄存器锁存 `CC <- next_cc`，`pc <- valP`。三处物理状态在时钟跳变沿同时完成交割。

---

### 9. `jXX Dest` (核心条件跳转)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (`7X`)；`valC <- M[pc + 1]` (8字节绝对跳转地址)；`valP <- pc + 9`。
* **Decode (译码)**: 无。
* **Execute (执行)**:
条件判定多路选择器通电：`Cnd <- alu_cond(ifun, CC)`（结合当前锁存的红绿灯状态，输出 1 或 0）。
* **Memory (访存)**: 无。
* **Writeback (写回)**: 无。
* **PC Update (时钟更新)**:
**硬件 MUX 轨道切换分流**：
`next_pc = Cnd ? valC : valP`
* 💡 **电路精髓**：如果条件成立（`Cnd == 1`），下一周期的 PC 触发器在合闸瞬间直接被灌入立即数地址 `valC`（实现大跨度空间跳转）；如果失败，则老老实实刷入顺序推进地址 `valP`。



---

### 10. `call Dest` (硬件级函数调用)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (`80`)；`valC <- M[pc + 1]` (函数入口地址)；`valP <- pc + 9`。
* **Decode (译码)**:
`srcB <- R_RSP` ➡️ 从寄存器堆中抽出当前的物理栈顶指针 `valB <- R[%rsp]`。
* **Execute (执行)**:
`valE <- valB + (-8)`。ALU 的减法通路开启，将栈指针向下猛推 8 个字节，提前算出**压栈后的新物理栈顶**。
* **Memory (访存)**:
激活内存写总线。**将当前计算出的顺序返回地址 `valP`（即 call 指令后面紧跟的那条指令的物理地址），强行压入刚才算出的新栈顶 `M[valE]**`。
* **Writeback (写回)**:
`dstE <- R_RSP` ➡️ 将扣减后的新栈顶地址 `valE` 投递给 `%rsp` 的预写通道。
* **PC Update (时钟更新)**:
时钟沿跳变合闸！`R[%rsp] <- valE`（栈顶指针正式更新），同时 **PC 寄存器被强行绑架、刷写为函数入口物理地址 `valC**`。

---

### 11. `ret` (硬件级函数弹回)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (`90`)；`valP <- pc + 1`。
* **Decode (译码)**:
`srcA <- R_RSP; srcB <- R_RSP` ➡️ 双通道并联，同时读出当前栈指针地址 `valA` 和 `valB`。
* **Execute (执行)**:
`valE <- valB + 8`。ALU 执行加法，算出函数退出后、栈指针回弹弹开 8 字节后的新地址。
* **Memory (访存)**:
激活内存读总线。**从当前 `%rsp` 指向的物理旧栈顶槽位 `M[valA]` 中，捞出之前暂存的返回地址，送入 `valM` 总线**。
* **Writeback (写回)**:
`dstE <- R_RSP` ➡️ 将回弹后的栈指针 `valE` 送回 `%rsp` 的锁存端。
* **PC Update (时钟更新)**:
合闸！栈指针正式恢复 `R[%rsp] <- valE`。同时，**PC 寄存器拒绝了顺序地址 `valP`，而是直接强刷了从物理内存栈顶里抢救出来的返回地址 `valM**`！程序完美弹回函数调用点。

---

### 12. `pushq rA` (寄存器数据压入栈)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (`a0`)；`rA:rB <- M[pc + 1]` (`rA f`)；`valP <- pc + 2`。
* **Decode (译码)**:
`srcA <- rA` ➡️ `valA <- R[rA]` (取出要打入冷宫的数据)；
`srcB <- R_RSP` ➡️ `valB <- R[%rsp]` (读出当前栈顶位置)。
* **Execute (执行)**:
`valE <- valB + (-8)`。ALU 算出压栈后的新栈顶物理地址。
* **Memory (访存)**:
激活内存写总线。将数据 `valA` 牢牢写入新栈顶地址 `M[valE]`。
* **Writeback (写回)**:
`dstE <- R_RSP` ➡️ 将移动后的新栈指针 `valE` 投递给 `%rsp` 的写入口。
* **PC Update (时钟更新)**:
`R[%rsp] <- valE` 且 `pc <- valP`。

---

### 13. `popq rA` (栈顶数据弹出至寄存器)

* **Fetch (取指)**:
`icode:ifun <- M[pc]` (`b0`)；`rA:rB <- M[pc + 1]` (`rA f`)；`valP <- pc + 2`。
* **Decode (译码)**:
`srcA <- R_RSP; srcB <- R_RSP` ➡️ 抽出栈指针电平送往两条内部总线。
* **Execute (执行)**:
`valE <- valB + 8`。ALU 执行加法，算出数据出栈后、指针释放回弹的物理地址。
* **Memory (访存)**:
激活内存读总线。从当前未移动的旧栈顶 `M[valA]` 中抽取 8 字节物理数据，存入 `valM`。
* **Writeback (写回)**:
🔥 **双通道写回使能开启**：
`dstE <- R_RSP` ➡️ 负责把回弹地址 `valE` 锁回 `%rsp`；
`dstM <- rA`   ➡️ 负责把从栈底捞出来的内存物理数据 `valM` 锁入目标寄存器 `rA`。
* **PC Update (时钟更新)**:
时钟沿跳变。寄存器堆（Register File）的两路写端口在同个纳秒瞬间一齐锁存，`%rsp` 和 `rA` 瞬间同时换血刷新！`pc <- valP`。

---

## 🧭 给初学者的总结：如何在代码中审视单周期的“等价性”

当你在写 C++ 代码的各个时阶段函数时，请闭上眼睛想象：

> “在真实芯片中，上面的每一步操作，都代表着主板上有特定的使能信号铜导线被拉到了高电平。每一条指令对硬件电路的开闭，都完美对应着我们在 C++ 里写下的那些 `if` 和 `switch` 分支。”

这就是体系结构的魅力。掌握了这一章，你已经彻底看透了 Y86-64 架构下所有代码的生命轨迹。接下来，如果你在仿真实现中碰到了诡异的 Bug，请立刻翻阅最后的 **`[🛠️ 04. 功勋墙与仿真避坑指南]`**，看看那些由于软件语言的隐式时序与硬件逻辑冲突带来的惨烈事故与精妙解法！