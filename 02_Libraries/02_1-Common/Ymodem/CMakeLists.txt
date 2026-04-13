cmake_minimum_required(VERSION 3.10)

# 定义 IIC 驱动库
add_library(Ymodem STATIC)

# 添加 IIC 模块的源文件
target_sources(Ymodem PRIVATE
        i2c_bus.c
)

# 暴露 IIC 模块的头文件路径
target_include_directories(Ymodem PUBLIC
        .
)

# 链接依赖库（根据实际需求添加，例如硬件驱动库）
target_link_libraries(Ymodem PRIVATE
        STM32_Drivers
        bsp
        elog
)