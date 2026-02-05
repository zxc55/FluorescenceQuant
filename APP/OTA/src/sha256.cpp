#include "sha256.h"

SHA256::SHA256() { reset(); }

void SHA256::update(const unsigned char* data, size_t length) {
    const size_t blockSize = 64;
    size_t index = m_numBytes % blockSize;
    m_numBytes += length;
    m_numBits += length * 8;
    size_t partLen = blockSize - index;
    size_t i = 0;

    if (length >= partLen) {
        if (index > 0) {
            memcpy(m_buffer + index, data, partLen);
            processBlock(m_buffer);
            i += partLen;
        }

        for (; i + blockSize <= length; i += blockSize)
            processBlock(data + i);

        index = 0;
    }

    memcpy(m_buffer + index, data + i, length - i);
}

void SHA256::update(const std::string& data) {
    update(reinterpret_cast<const unsigned char*>(data.c_str()), data.size());
}

std::string SHA256::finalize() {
    const unsigned char padding[64] = {0x80};
    unsigned char lengthBytes[8];

    for (int i = 0; i < 8; ++i) {
        lengthBytes[7 - i] = static_cast<unsigned char>((m_numBits >> (i * 8)) & 0xFF);
    }

    size_t index = m_numBytes % 64;
    size_t padLen = (index < 56) ? (56 - index) : (120 - index);

    update(padding, padLen);
    update(lengthBytes, 8);

    std::stringstream hash;
    for (int i = 0; i < 8; ++i) {
        hash << std::hex << std::setw(8) << std::setfill('0') << m_hash[i];
    }

    reset();
    return hash.str();
}
void SHA256::reset() {
    m_numBytes = 0;
    m_numBits = 0;
    m_hash[0] = 0x6A09E667;
    m_hash[1] = 0xBB67AE85;
    m_hash[2] = 0x3C6EF372;
    m_hash[3] = 0xA54FF53A;
    m_hash[4] = 0x510E527F;
    m_hash[5] = 0x9B05688C;
    m_hash[6] = 0x1F83D9AB;
    m_hash[7] = 0x5BE0CD19;
}

void SHA256::processBlock(const unsigned char* block) {
    static const uint32_t k[64] =
        {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
            0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
            0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
            0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
            0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (block[i * 4 + 0] << 24) | (block[i * 4 + 1] << 16) |
               (block[i * 4 + 2] << 8) | (block[i * 4 + 3] << 0);
    }

    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = m_hash[0];
    uint32_t b = m_hash[1];
    uint32_t c = m_hash[2];
    uint32_t d = m_hash[3];
    uint32_t e = m_hash[4];
    uint32_t f = m_hash[5];
    uint32_t g = m_hash[6];
    uint32_t h = m_hash[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + k[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    m_hash[0] += a;
    m_hash[1] += b;
    m_hash[2] += c;
    m_hash[3] += d;
    m_hash[4] += e;
    m_hash[5] += f;
    m_hash[6] += g;
    m_hash[7] += h;
}

uint32_t SHA256::rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

// 读取文件并计算其 SHA-256
std::string SHA256::calculateSHA256FromFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "无法打开文件: " << filePath << std::endl;
        return "";
    }

    char buffer[1024];

    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        update(reinterpret_cast<unsigned char*>(buffer), file.gcount());
    }

    return finalize();
}

// 计算字符串的 SHA-256
std::string SHA256::calculateSHA256FromString(const std::string& input) {
    update(reinterpret_cast<const unsigned char*>(input.c_str()), input.size());
    return finalize();
}