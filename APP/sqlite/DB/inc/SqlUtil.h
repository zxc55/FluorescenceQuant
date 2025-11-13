#pragma once
#include <QString>

// 生成随机盐（十六进制字符串，默认 16 字节 = 32 hex）
QString genSaltHex();

// 计算 SHA-256(saltHex + password) 的十六进制字符串
// 其中 saltHex 是十六进制盐（例如 "a1b2..."）
QString hashPassword(const QString& saltHex, const QString& pass);
