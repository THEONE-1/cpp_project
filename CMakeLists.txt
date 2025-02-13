# 指定CMake的最低版本要求
cmake_minimum_required(VERSION 3.10)
# 设置项目名称为mymuduo
project(mymuduo)

# 设置库文件的输出目录为项目源代码目录下的lib文件夹
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置调试信息以及启动C++11标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

# 定义头文件路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# 定义muduo库的源文件
aux_source_directory(./src SRC_LIST)

# 编译生成动态库
add_library(mymuduo SHARED ${SRC_LIST})