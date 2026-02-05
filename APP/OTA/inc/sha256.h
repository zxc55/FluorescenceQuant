#ifndef _SHA256_H_
#define _SHA256_H_
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

class SHA256 {
public:
    SHA256();

    void update(const unsigned char* data, size_t length);

    void update(const std::string& data);
    std::string finalize();
    // 读取文件并计算其 SHA-256
    std::string calculateSHA256FromFile(const std::string& filePath);
    std::string calculateSHA256FromString(const std::string& input);

private:
    void reset();

    void processBlock(const unsigned char* block);

    uint32_t rotr(uint32_t x, uint32_t n);

    unsigned char m_buffer[64];
    uint32_t m_hash[8];
    size_t m_numBytes;
    uint64_t m_numBits;
};

#endif  // _SHA256_H_