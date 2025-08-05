cmake_minimum_required(VERSION 3.10)
project(MyCppProject VERSION 1.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/utils.cpp
)

# 包含目录
target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
