cmake_minimum_required(VERSION 3.16)
project(FluorescenceQuant LANGUAGES CXX)

# -----------------------------------------------
# 基础配置
# -----------------------------------------------
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)     # 自动处理 qrc
set(CMAKE_AUTOUIC OFF)

# 如果 Qt 没有自动找到路径，可以手动指定
list(APPEND CMAKE_PREFIX_PATH "${Qt5_DIR}")
find_package(Qt5 REQUIRED COMPONENTS Core Gui Qml Quick Charts)

# -----------------------------------------------
# 包含路径
# -----------------------------------------------
include_directories(APP/ADS1115/inc)

# -----------------------------------------------
# 源文件与头文件
# -----------------------------------------------
set(SOURCES
    main.cpp
    APP/ADS1115/src/MainViewModel.cpp
    APP/ADS1115/src/IIODeviceController.cpp
    APP/ADS1115/src/IIOReaderThread.cpp
)

set(HEADERS
    APP/ADS1115/inc/MainViewModel.h
    APP/ADS1115/inc/IIODeviceController.h
    APP/ADS1115/inc/IIOReaderThread.h
)

# -----------------------------------------------
# 自动扫描 QML 文件并生成 qml.qrc
# -----------------------------------------------
file(GLOB_RECURSE QML_FILES "${CMAKE_SOURCE_DIR}/qml/*.qml")

# 把 qml.qrc 生成到源代码目录（不是 build 目录）
set(QRC_FILE "${CMAKE_SOURCE_DIR}/qml.qrc")

# 重新写入 qml.qrc 文件
file(WRITE ${QRC_FILE} "<RCC>\n  <qresource prefix=\"/\">\n")
foreach(qml ${QML_FILES})
    # 使用绝对路径，避免 Ninja 路径错误
    file(APPEND ${QRC_FILE} "    <file>${qml}</file>\n")
endforeach()
file(APPEND ${QRC_FILE} "  </qresource>\n</RCC>\n")

# 当 QML 文件变化时，CMake 自动重新生成 qml.qrc
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${QML_FILES})

# -----------------------------------------------
# 生成可执行文件
# -----------------------------------------------
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${QRC_FILE}
)

# -----------------------------------------------
# 链接 Qt 库
# -----------------------------------------------
target_link_libraries(FluorescenceQuant PRIVATE
  Qt5::Core
  Qt5::Gui
  Qt5::Qml
  Qt5::Quick
  Qt5::Charts
)
