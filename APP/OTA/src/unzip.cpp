#include "./unzip.h"

#include <minizip/mz.h>
#include <minizip/mz_strm.h>
#include <minizip/mz_zip.h>
#include <minizip/mz_zip_rw.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#ifdef MEMORY_LIMIT
static bool createDirectory(const std::string& path) {
    if (path.empty())
        return false;

    // 循环逐级创建子目录
    size_t pos = 0;
    do {
        // 从上一次的 pos 继续往后查找 '/'
        pos = path.find('/', pos + 1);
        std::string subPath = path.substr(0, pos);

        // 若路径以 / 开头, subPath 可能空
        if (subPath.empty()) {
            continue;
        }

        if (mkdir(subPath.c_str(), 0755) != 0 && errno != EEXIST) {
            std::cerr << "[createDirectory] Failed to create dir: " << subPath
                      << " error: " << strerror(errno) << std::endl;
            return false;
        }
    } while (pos != std::string::npos);

    return true;
}

bool Unzip::extractFile(const std::string& zipFilePath, const std::string& outputDir) {
    // 创建 ZIP reader
    void* reader = mz_zip_reader_create();
    if (reader == nullptr) {
        std::cerr << "Failed to create ZIP reader." << std::endl;
        return false;
    }

    // 打开 ZIP 文件
    int32_t err = mz_zip_reader_open_file(reader, zipFilePath.c_str());
    if (err != MZ_OK) {
        std::cerr << "Failed to open ZIP file: " << zipFilePath
                  << " (err=" << err << ")" << std::endl;
        mz_zip_reader_delete(&reader);
        return false;
    }

    // 确保输出目录已创建
    if (!createDirectory(outputDir)) {
        std::cerr << "Failed to create output directory: " << outputDir << std::endl;
        mz_zip_reader_close(reader);
        mz_zip_reader_delete(&reader);
        return false;
    }

    // 跳到第一个条目
    err = mz_zip_reader_goto_first_entry(reader);
    if (err != MZ_OK && err != MZ_END_OF_LIST) {
        std::cerr << "Failed to goto first entry (err=" << err << ")" << std::endl;
        mz_zip_reader_close(reader);
        mz_zip_reader_delete(&reader);
        return false;
    }

    // 统计解压文件数量
    int64_t fileCount = 0;

    // 遍历 ZIP 内所有条目
    while (err == MZ_OK) {
        mz_zip_file* fileInfo = nullptr;
        // 获取当前条目信息 (文件名等)
        err = mz_zip_reader_entry_get_info(reader, &fileInfo);
        if (err != MZ_OK) {
            std::cerr << "Failed to get entry info (err=" << err << ")" << std::endl;
            // 进入下一条目前先尝试 goto_next_entry
            err = mz_zip_reader_goto_next_entry(reader);
            continue;
        }

        // 文件名 (可能是相对路径，如 "folder/sub/file.txt")
        std::string filename = fileInfo->filename ? fileInfo->filename : "";
        // 拼接到输出目录的完整路径
        std::string fullPath = outputDir + "/" + filename;

        // 判断是否目录
        bool isDirectory = (mz_zip_reader_entry_is_dir(reader) == MZ_OK);
        if (isDirectory) {
            // 目录则创建对应文件夹
            if (!createDirectory(fullPath)) {
                std::cerr << "Failed to create directory: " << fullPath << std::endl;
            } else {
                std::cout << "[DIR ] " << filename << std::endl;
            }
            // 跳到下一个条目
            err = mz_zip_reader_goto_next_entry(reader);
            continue;
        }

        // ---------- 流式解压核心 ----------
        // 打开当前条目
        err = mz_zip_reader_entry_open(reader);
        if (err != MZ_OK) {
            std::cerr << "Failed to open entry: " << filename
                      << " (err=" << err << ")" << std::endl;
            // 去下一个条目
            err = mz_zip_reader_goto_next_entry(reader);
            continue;
        }

        // 确保父目录存在（若条目含子目录）
        size_t lastSlash = fullPath.rfind('/');
        if (lastSlash != std::string::npos) {
            std::string parentDir = fullPath.substr(0, lastSlash);
            if (!createDirectory(parentDir)) {
                std::cerr << "Failed to create parent dirs for " << fullPath << std::endl;
                mz_zip_reader_entry_close(reader);
                // 跳到下一个条目
                err = mz_zip_reader_goto_next_entry(reader);
                continue;
            }
        }

        // 打开输出文件
        FILE* fp = fopen(fullPath.c_str(), "wb");
        if (!fp) {
            std::cerr << "Failed to open output file for writing: " << fullPath << std::endl;
            mz_zip_reader_entry_close(reader);
            // 下一个条目
            err = mz_zip_reader_goto_next_entry(reader);
            continue;
        }

        static const size_t BUFFER_SIZE = 64 * 1024;  // 64KB，按需可调
        std::vector<uint8_t> buffer(BUFFER_SIZE);

        // 循环读出数据并写到文件
        while (true) {
            int32_t bytesRead = mz_zip_reader_entry_read(reader, buffer.data(), BUFFER_SIZE);
            if (bytesRead < 0) {
                std::cerr << "Error reading entry data (bytesRead=" << bytesRead
                          << ") for: " << filename << std::endl;
                break;
            }
            if (bytesRead == 0) {
                // 读到末尾
                break;
            }
            // 写到文件
            size_t written = fwrite(buffer.data(), 1, bytesRead, fp);
            if (written != static_cast<size_t>(bytesRead)) {
                std::cerr << "Error writing file: " << fullPath << std::endl;
                break;
            }
        }

        fclose(fp);
        // 关闭条目
        mz_zip_reader_entry_close(reader);

        ++fileCount;
        std::cout << "[FILE] " << filename << std::endl;

        // 跳到下一个条目
        err = mz_zip_reader_goto_next_entry(reader);
    }

    if (err != MZ_END_OF_LIST) {
        // 只要不是读到末尾(MZ_END_OF_LIST)，都是异常或中断
        std::cerr << "ZIP entry iteration ended with err=" << err << std::endl;
    }

    mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);

    std::cout << "Extraction finished. " << fileCount << " file(s) extracted." << std::endl;

    // 如果需要在解压后删除ZIP，解除下面注释即可：
    // if (std::remove(zipFilePath.c_str()) == 0)
    //     std::cout << "Deleted original ZIP: " << zipFilePath << std::endl;
    // else
    //     std::cerr << "Failed to delete ZIP: " << zipFilePath << std::endl;

    return true;
}
#else
bool createDirectory(const std::string& path) {
    if (path.empty())
        return false;
    size_t pos = 0;
    do {
        pos = path.find('/', pos + 1);
        std::string subPath = path.substr(0, pos);
        if (subPath.empty())
            continue;
        if (mkdir(subPath.c_str(), 0755) != 0 && errno != EEXIST) {
            std::cerr << "Failed to create directory: " << subPath << std::endl;
            return false;
        }
    } while (pos != std::string::npos);
    return true;
}

bool extractFromBuffer(const uint8_t* buf, int32_t size, const std::string& outputDir) {
    // 1. 创建ZIP reader
    void* reader = mz_zip_reader_create();
    if (!reader) {
        std::cerr << "Failed to create zip reader." << std::endl;
        return false;
    }

    // 2. 从内存缓冲区打开 ZIP
    // 第三个参数 copy=0 表示不再复制缓冲区（只读模式）
    // 如果你之后会释放buf 或 buf不是一直有效，可设置 copy=1 让minizip内部复制
    int32_t err = mz_zip_reader_open_buffer(reader, (uint8_t*)buf, size, 0);
    if (err != MZ_OK) {
        std::cerr << "Failed to open zip from memory buffer. err=" << err << std::endl;
        mz_zip_reader_delete(&reader);
        return false;
    }

    // 确保输出目录
    if (!createDirectory(outputDir)) {
        mz_zip_reader_close(reader);
        mz_zip_reader_delete(&reader);
        return false;
    }

    // 跳到第一个条目
    err = mz_zip_reader_goto_first_entry(reader);
    if (err != MZ_OK && err != MZ_END_OF_LIST) {
        std::cerr << "Failed to goto first entry. err=" << err << std::endl;
        mz_zip_reader_close(reader);
        mz_zip_reader_delete(&reader);
        return false;
    }

    while (err == MZ_OK) {
        // 获取当前文件信息
        mz_zip_file* fileInfo = nullptr;
        err = mz_zip_reader_entry_get_info(reader, &fileInfo);
        if (err != MZ_OK) {
            std::cerr << "Failed to get entry info. err=" << err << std::endl;
            err = mz_zip_reader_goto_next_entry(reader);
            continue;
        }
        std::string filename = fileInfo->filename ? fileInfo->filename : "";
        std::string fullPath = outputDir + "/" + filename;

        // 判断是否目录
        bool isDir = (mz_zip_reader_entry_is_dir(reader) == MZ_OK);
        if (isDir) {
            createDirectory(fullPath);
            std::cout << "[DIR]  " << filename << std::endl;
            err = mz_zip_reader_goto_next_entry(reader);
            continue;
        }

        // 打开条目
        int32_t openErr = mz_zip_reader_entry_open(reader);
        if (openErr != MZ_OK) {
            std::cerr << "Failed to open entry: " << filename << " err=" << openErr << std::endl;
            err = mz_zip_reader_goto_next_entry(reader);
            continue;
        }

        // 确保父目录存在
        size_t slashPos = fullPath.rfind('/');
        if (slashPos != std::string::npos) {
            createDirectory(fullPath.substr(0, slashPos));
        }

        // 写出文件
        FILE* fp = fopen(fullPath.c_str(), "wb");
        if (!fp) {
            std::cerr << "Failed to create file: " << fullPath << std::endl;
            mz_zip_reader_entry_close(reader);
            err = mz_zip_reader_goto_next_entry(reader);
            continue;
        }

        std::vector<uint8_t> buffer(64 * 1024);
        while (true) {
            int32_t bytesRead = mz_zip_reader_entry_read(reader, buffer.data(), (int32_t)buffer.size());
            if (bytesRead < 0) {
                std::cerr << "Read error: " << bytesRead << std::endl;
                break;
            }
            if (bytesRead == 0) {
                // EOF
                break;
            }
            size_t written = fwrite(buffer.data(), 1, bytesRead, fp);
            if (written != (size_t)bytesRead) {
                std::cerr << "Write error!" << std::endl;
                break;
            }
        }
        fclose(fp);
        mz_zip_reader_entry_close(reader);

        std::cout << "[FILE] " << filename << std::endl;
        // 下一个
        err = mz_zip_reader_goto_next_entry(reader);
    }

    if (err != MZ_END_OF_LIST) {
        std::cerr << "Finished with error code: " << err << std::endl;
    }

    mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);

    return true;
}

bool Unzip::extractFile(const std::string& zipFilePath, const std::string& outputDir) {
    // 1) 先把 ZIP 文件读入内存
    FILE* fp = fopen(zipFilePath.c_str(), "rb");
    if (!fp) {
        std::cerr << "Failed to open zip file." << std::endl;
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    rewind(fp);

    // 如果文件非常大，需要确认系统是否有足够内存
    std::vector<uint8_t> zipData(fileSize);
    size_t readSize = fread(zipData.data(), 1, fileSize, fp);
    fclose(fp);

    if (readSize != (size_t)fileSize) {
        std::cerr << "Failed to read entire file into memory." << std::endl;
        return false;
    }

    // 2) 现在可以删除本地文件
    if (std::remove(zipFilePath.c_str()) == 0) {
        std::cout << "Removed local zip file: " << zipFilePath << std::endl;
    } else {
        std::cerr << "Failed to remove file: " << zipFilePath << std::endl;
        // 如果删除失败，也可以继续解压，只是无法节省这部分存储
    }

    // 3) 从内存中解压
    if (!extractFromBuffer(zipData.data(), (int32_t)fileSize, outputDir)) {
        std::cerr << "Extraction failed." << std::endl;
        return false;
    }

    std::cout << "All done." << std::endl;
    return true;
}

#endif
