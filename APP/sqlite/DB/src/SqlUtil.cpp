#include "SqlUtil.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QString>

static QString genSaltHexInternal(int bytes) {
    if (bytes <= 0)
        bytes = 16;
    QByteArray raw;
    raw.resize(bytes);

    int i = 0;
    while (i < bytes) {
        quint32 r = QRandomGenerator::system()->generate();
        for (int k = 0; k < 4 && i < bytes; ++k) {
            raw[i++] = static_cast<char>((r >> (k * 8)) & 0xFF);
        }
    }
    return raw.toHex();
}

QString genSaltHex() {
    return genSaltHexInternal(16);
}

QString hashPassword(const QString& saltHex, const QString& pass) {
    const QByteArray salt = QByteArray::fromHex(saltHex.toUtf8());
    QByteArray in;
    in.reserve(salt.size() + pass.size());
    in.append(salt);
    in.append(pass.toUtf8());
    const QByteArray digest = QCryptographicHash::hash(in, QCryptographicHash::Sha256);
    return digest.toHex();
}
