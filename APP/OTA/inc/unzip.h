#ifndef _UNZIP_H_
#define _UNZIP_H_
#include <string>
class Unzip {
private:
    /* data */
public:
    ~Unzip() = default;
    static bool extractFile(const std::string &zipData, const std::string &outputDir);
};
#endif  // _UNZIP_H_