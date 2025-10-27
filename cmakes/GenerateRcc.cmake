# 文件: cmake/GenerateRcc.cmake

# 定义一个函数：generate_rcc
# 参数：
#   RESOURCE_DIR   - 源资源目录（绝对路径）
#   OUT_QRC_PATH   - 输出 .qrc 文件路径（绝对路径）
function(generate_rcc RESOURCE_DIR OUT_QRC_PATH )
    # 1. 递归收集资源
    file(GLOB_RECURSE _r_files
        RELATIVE "${RESOURCE_DIR}"
        "${RESOURCE_DIR}/*"
    )

    # 2. 创建输出目录
    get_filename_component(_out_dir "${OUT_QRC_PATH}" DIRECTORY)
    file(MAKE_DIRECTORY "${_out_dir}")

    # 3. 写入 qrc 头部
    file(WRITE "${OUT_QRC_PATH}"
        "<!DOCTYPE RCC><RCC version=\"1.0\">\n"
        "  <qresource prefix=\"/\">\n"
    )

    # 4. 遍历并追加 <file> 节点（带 alias 保留子目录结构）
    foreach(_file IN LISTS _r_files)
        file(APPEND "${OUT_QRC_PATH}"
            "    <file alias=\"${_file}\">${RESOURCE_DIR}/${_file}</file>\n"
        )
    endforeach()

    # 5. 写入 qrc 尾部
    file(APPEND "${OUT_QRC_PATH}"
        "  </qresource>\n"
        "</RCC>\n"
    )
endfunction()
