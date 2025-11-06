#ifndef KEYSPROXY_H_
#define KEYSPROXY_H_
#include <QObject>

class KeysProxy : public QObject {
    Q_OBJECT
public:
    explicit KeysProxy(QObject* p = nullptr) : QObject(p) {}

signals:
    void cardInserted();
    void cardRemoved();
    void insertedChanged(bool on);
};
#endif  // KEYSPROXY_H_
