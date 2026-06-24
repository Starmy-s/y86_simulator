# ============================================================
# Y86Simulator 构建配置文件
# ============================================================

# 强制要求 CMake 版本不低于 3.10
cmake_minimum_required(VERSION 3.10)

# 定义项目名称为 Y86Simulator，版本号 1.0，使用的是 C++ 语言
project(Y86Simulator VERSION 1.0 LANGUAGES CXX)

# 强制编译器使用 C++17 标准
set(CMAKE_CXX_STANDARD 17)
# 如果编译器不支持 C++17 直接报错
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------
# 1. 自动化汇编
# ------------------------------------------------------------

# 搜索并加载系统中的 Python3 解释器
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(ASM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asm_src")
set(BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/machine_code")

# 创建文件夹
file(MAKE_DIRECTORY ${BIN_DIR})

# GLOB: 使用通配符匹配文件名
file(GLOB ALL_YS_FILES "${ASM_DIR}/*.ys")

# 初始化一个空列表
set(ALL_YO_OUTPUTS "")

# 循环遍历列表中的每一个 .ys 文件路径
foreach(YS_FILE_PATH ${ALL_YS_FILES})
    # 从完整路径中提取文件名（不含扩展名）
    get_filename_component(YS_RAW_NAME ${YS_FILE_PATH} NAME_WE)
    set(YO_FILE_OUTPUT "${BIN_DIR}/${YS_RAW_NAME}.yo")

    # add_custom_command: 定义一个自定义构建规则
    # OUTPUT: 指定这条规则生成的产物
    # COMMAND: 定义执行的 shell 指令（此处调用 python 脚本将 .ys 转译为 .yo）
    # DEPENDS: 建立依赖关系，当 .ys 源码或编译器脚本变化时，强制触发该规则
    # COMMENT: 在控制台输出友好的构建提示信息
    add_custom_command(
        OUTPUT ${YO_FILE_OUTPUT}
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tools/yas.py" "${YS_FILE_PATH}" > "${YO_FILE_OUTPUT}"
        DEPENDS "${YS_FILE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/yas.py"
        COMMENT "Automatically assembling ${YS_RAW_NAME}.ys -> ${YS_RAW_NAME}.yo"
    )
    # list(APPEND ...): 将新生成的 .yo 路径加入总列表
    list(APPEND ALL_YO_OUTPUTS ${YO_FILE_OUTPUT})
endforeach()

# add_custom_target: 定义一个名为 assemble 的伪目标，执行此目标时会强制生成列表中的所有 .yo 文件
add_custom_target(assemble DEPENDS ${ALL_YO_OUTPUTS})

# set_directory_properties: 设置属性，确保执行 make clean 时，自动清理这些生成的机器码
set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES "${BIN_DIR}")

# ------------------------------------------------------------
# 2. C++ 程序构建
# ------------------------------------------------------------

# 将 src 目录下所有的 .cpp 文件，存入 SOURCES 变量
file(GLOB SOURCES "src/*.cpp")

# 定义编译目标：把 SOURCES 里的 cpp 源码编译成一个名为 y86_sim 的可执行二进制程序
add_executable(y86_sim ${SOURCES})

# add_dependencies: 强制在编译 y86_sim 之前，先完成 assemble 目标的任务
add_dependencies(y86_sim assemble)

# 绑定头文件路径：PRIVATE 只有 y86_sim 编译时会使用这些头文件
target_include_directories(y86_sim PRIVATE include)
