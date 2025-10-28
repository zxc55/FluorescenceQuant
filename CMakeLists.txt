cmake_minimum_required(VERSION 3.16)
project(FluorescenceQuant LANGUAGES CXX)
if(${TARGET_BOARD} STREQUAL "全志T113-S3")
   set(compilerType "QuanZhi")
    message(STATUS "使用全志T113-S3编译")
else()
 set(compilerType "X86")
    add_definitions(-DLOCAL_BUILD)
    message(使用本地X86编译)
endif()

# -----------------------------------------------
# 基础配置
# -----------------------------------------------
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)     # 自动处理 qrc
set(CMAKE_AUTOUIC OFF)

include(cmakes/GenerateRcc.cmake)

# 如果 Qt 没有自动找到路径，可以手动指定
list(APPEND CMAKE_PREFIX_PATH "${Qt5_DIR}")
find_package(Qt5 REQUIRED COMPONENTS Core Gui Qml Quick Charts QuickControls2 Widgets)

# 添加资源文件路径
set(RESOURCE_DIR ${CMAKE_SOURCE_DIR}/resources)

# --------------- 1. 生成 resources/application.qrc  ---------------
set(RESOURCE_DIR "${CMAKE_SOURCE_DIR}/resources")
set(QRC_OUT     "${CMAKE_BINARY_DIR}/generated_resources/application.qrc")

# 调用函数
generate_rcc("${RESOURCE_DIR}" "${QRC_OUT}")

if(compilerType STREQUAL "QuanZhi")
    qt5_add_resources(APP_RCS "${QRC_OUT}")
else()
    qt_add_resources(APP_RCS "${QRC_OUT}")
endif()

# -----------------------------------------------
# 包含路径
# -----------------------------------------------
include_directories(${CMAKE_SOURCE_DIR}/APP/ADS1115/inc)
include_directories(${CMAKE_SOURCE_DIR}/APP/EM5820H/inc)
include_directories(${CMAKE_SOURCE_DIR}/APP/libmodbus/include)
# -----------------------------------------------
# 源文件与头文件
# -----------------------------------------------
set(SOURCES
    main.cpp
    APP/MainViewModel.cpp
    APP/ADS1115/src/IIODeviceController.cpp
    APP/ADS1115/src/IIOReaderThread.cpp
    APP/EM5820H/src/PrinterManager.cpp
    APP/EM5820H/src/printerDeviceController.cpp
)

set(HEADERS
    APP/MainViewModel.h
    APP/ADS1115/inc/IIODeviceController.h
    APP/ADS1115/inc/IIOReaderThread.h
    APP/EM5820H/inc/PrinterManager.h
    APP/EM5820H/inc/printer_lib.h
    APP/EM5820H/inc/printer_type.h
    APP/EM5820H/inc/printerDeviceController.h
)
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${QRC_FILE}
    ${APP_RCS}
)
link_directories(${CMAKE_SOURCE_DIR}/APP/libmodbus/lib)


if(${TARGET_BOARD} STREQUAL "全志T113-S3")
    add_library(printer SHARED IMPORTED)
    set_target_properties(printer PROPERTIES IMPORTED_LOCATION
        "${CMAKE_CURRENT_SOURCE_DIR}/libprinter_1.0.3.so"
    )   
target_link_libraries(FluorescenceQuant PRIVATE
    printer
    ${CMAKE_SOURCE_DIR}/APP/libmodbus/lib/libmodbus.so
)

else()  

endif()







# -----------------------------------------------
# 链接 Qt 库
# -----------------------------------------------
target_link_libraries(FluorescenceQuant PRIVATE
  Qt5::Core
  Qt5::Gui
  Qt5::Qml
  Qt5::Quick
  Qt5::Charts
  Qt5::QuickControls2
  Qt5::Widgets
)
