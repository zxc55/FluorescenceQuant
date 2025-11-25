cmake_minimum_required(VERSION 3.16)
project(FluorescenceQuant LANGUAGES CXX C)
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
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmakes/GenerateRcc.cmake)

# 如果 Qt 没有自动找到路径，可以手动指定
list(APPEND CMAKE_PREFIX_PATH "${Qt5_DIR}")
find_package(Qt5 REQUIRED COMPONENTS Core Gui Qml Quick Charts QuickControls2 Widgets QuickControls2 VirtualKeyboard Sql)

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
# --- 仅生成 icons.qrc，匹配 QML 里的 qrc:/resources/icons/xxx ---
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated_resources")
set(ICONS_DIR  "${CMAKE_SOURCE_DIR}/resources/icons")
set(ICONS_QRC "${CMAKE_BINARY_DIR}/generated_resources/icons.qrc")

# 收集常见大小写后缀（避免 .PNG/.Svg 漏掉）
file(GLOB_RECURSE ICON_FILES
    LIST_DIRECTORIES false
    "${ICONS_DIR}/*.png" "${ICONS_DIR}/*.PNG"
    "${ICONS_DIR}/*.svg" "${ICONS_DIR}/*.SVG"
    "${ICONS_DIR}/*.ico" "${ICONS_DIR}/*.ICO"
    "${ICONS_DIR}/*.jpg" "${ICONS_DIR}/*.JPG"
    "${ICONS_DIR}/*.jpeg" "${ICONS_DIR}/*.JPEG"
)

file(WRITE "${ICONS_QRC}" "<RCC>\n  <qresource prefix=\"/resources/icons\">\n")
foreach(f IN LISTS ICON_FILES)
    # alias = 相对 icons 目录的路径（如 start.png / sub/refresh.png）
    file(RELATIVE_PATH alias_path "${ICONS_DIR}" "${f}")
    file(APPEND "${ICONS_QRC}" "    <file alias=\"${alias_path}\">${f}</file>\n")
endforeach()
file(APPEND "${ICONS_QRC}" "  </qresource>\n</RCC>\n")

# 把 application.qrc + icons.qrc 一起编译
if(compilerType STREQUAL "QuanZhi")
    qt5_add_resources(APP_RCS "${QRC_OUT}" "${ICONS_QRC}")
else()
    qt_add_resources(APP_RCS "${QRC_OUT}" "${ICONS_QRC}")
endif()




# 自动收集 APP/Recognition 下所有 .cpp / .h
file(GLOB_RECURSE RECOGNITION_SOURCES
    ${CMAKE_SOURCE_DIR}/APP/Recognition/*.cpp
    ${CMAKE_SOURCE_DIR}/APP/QUIRC/*.cpp
    ${CMAKE_SOURCE_DIR}/APP/QUIRC/*.c
    ${CMAKE_SOURCE_DIR}/APP/Scanner/*.cpp
)

file(GLOB_RECURSE RECOGNITION_HEADERS
    ${CMAKE_SOURCE_DIR}/APP/Recognition/*.h
    ${CMAKE_SOURCE_DIR}/APP/QUIRC/*.h
    ${CMAKE_SOURCE_DIR}/APP/Scanner/*.h
)
# -----------------------------------------------
# 包含路径
# -----------------------------------------------
include_directories(${CMAKE_SOURCE_DIR}/APP/ADS1115/inc)
include_directories(${CMAKE_SOURCE_DIR}/APP/EM5820H/inc)
include_directories(${CMAKE_SOURCE_DIR}/APP/libmodbus/include)
include_directories(${CMAKE_SOURCE_DIR}/APP/modbus_rtu/inc)
include_directories(${CMAKE_SOURCE_DIR}/APP/Recognition)
include_directories(${CMAKE_SOURCE_DIR}/APP/QUIRC)
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
    APP/modbus_rtu/src/ModbusWorkerThread.cpp
    APP/modbus_rtu/src/MotorController.cpp
    APP/CardDetect/src/CardWatcherStd.cpp
    APP/sqlite/DB/src/DBWorker.cpp
    APP/sqlite/DB/src/Migrations.cpp
    APP/sqlite/DB/src/SqlUtil.cpp
    APP/sqlite/Repo/src/SettingsRepo.cpp
    APP/sqlite/Repo/src/UsersRepo.cpp
    APP/sqlite/VM/src/SettingsViewModel.cpp
    APP/sqlite/VM/src/UserViewModel.cpp
    APP/sqlite/Repo/src/ProjectsRepo.cpp
    APP/sqlite/VM/src/ProjectsViewModel.cpp
    APP/sqlite/VM/src/HistoryViewModel.cpp
    APP/sqlite/Repo/src/HistoryRepo.cpp
    APP/MyQmlComponents/MyLineSeries/MyLineSeries.cpp
    APP/MyQmlComponents/MyCurveLoader/CurveLoader.cpp
   # APP/MyQmlComponents/MyPlotView/PlotView.cpp
    APP/MyQmlComponents/MySeriesFeeder/SeriesFeeder.cpp
 
)

set(HEADERS
    APP/MainViewModel.h
    APP/ADS1115/inc/IIODeviceController.h
    APP/ADS1115/inc/IIOReaderThread.h
    APP/EM5820H/inc/PrinterManager.h
    APP/EM5820H/inc/printer_lib.h
    APP/EM5820H/inc/printer_type.h
    APP/EM5820H/inc/printerDeviceController.h
    APP/libmodbus/include/modbus/modbus.h
    APP/modbus_rtu/inc/ModbusWorkerThread.h
    APP/modbus_rtu/inc/MotorController.h
    APP/CardDetect/inc/CardWatcherStd.h
    APP/CardDetect/inc/KeysProxy.h
    APP/sqlite/DB/inc/DBWorker.h
    APP/sqlite/DB/inc/DTO.h
    APP/sqlite/DB/inc/DBTasks.h
    APP/sqlite/DB/inc/Migrations.h
    APP/sqlite/DB/inc/SqlUtil.h
    APP/sqlite/Repo/inc/SettingsRepo.h
    APP/sqlite/Repo/inc/UsersRepo.h
    APP/sqlite/VM/inc/SettingsViewModel.h
    APP/sqlite/VM/inc/UserViewModel.h
    APP/sqlite/Repo/inc/ProjectsRepo.h
    APP/sqlite/VM/inc/ProjectsViewModel.h
    APP/sqlite/VM/inc/HistoryViewModel.h
    APP/sqlite/Repo/inc/HistoryRepo.h
    APP/MyQmlComponents/MyLineSeries/MyLineSeries.h
    APP/MyQmlComponents/MyCurveLoader/CurveLoader.h
   # APP/MyQmlComponents/MyPlotView/PlotView.h
    APP/MyQmlComponents/MySeriesFeeder/SeriesFeeder.h

)
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${QRC_FILE}
    ${APP_RCS}
    ${RECOGNITION_SOURCES}
    ${RECOGNITION_HEADERS}

)
link_directories(${CMAKE_SOURCE_DIR}/APP/libmodbus/lib)
target_include_directories(FluorescenceQuant PRIVATE
    ${CMAKE_SOURCE_DIR}/APP/CardDetect/inc
    ${CMAKE_SOURCE_DIR}/APP/sqlite/DB/inc
    ${CMAKE_SOURCE_DIR}/APP/sqlite/VM/inc
    ${CMAKE_SOURCE_DIR}/APP/sqlite/Repo/inc
    ${CMAKE_SOURCE_DIR}/APP
)

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
  Qt5::QuickControls2
  Qt5::VirtualKeyboard
  Qt5::Sql
)
