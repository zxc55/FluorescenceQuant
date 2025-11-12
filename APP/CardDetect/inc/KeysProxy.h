#ifndef KEYSPROXY_H_
#define KEYSPROXY_H_
#include <QObject>

class KeysProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool inserted READ inserted WRITE setInserted NOTIFY insertedChanged)
public:
    explicit KeysProxy(QObject* p = nullptr) : QObject(p) {}
    bool inserted() const { return m_inserted; }
    void setInserted(bool on) {
        if (m_inserted == on)
            return;
        m_inserted = on;
        emit insertedChanged(m_inserted);
        if (m_inserted)
            emit cardInserted();
        else
            emit cardRemoved();
    }

signals:
    void cardInserted();
    void cardRemoved();
    void insertedChanged(bool on);

private:
    bool m_inserted = false;
};
#endif  // KEYSPROXY_H_
