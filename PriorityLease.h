#pragma once
#include <QSharedMemory>
#include <QDateTime>
#include <cstring>

// Both renderers hold this lock while writing LEDs. Expiring ownership also
// restores the ordinary effect if the game receiver stops unexpectedly.
class PriorityLease
{
    QSharedMemory memory{qEnvironmentVariable("OPENRGB_GAME_SYNC_LEASE_KEY", "OpenRGB.GameSync.Priority.v1")};
    bool locked = false;
public:
    explicit PriorityLease(bool create = false)
    {
        if (create && memory.create(sizeof(qint64)))
        {
            if (memory.lock())
            {
                std::memset(memory.data(), 0, sizeof(qint64));
                memory.unlock();
            }
        }
    }
    bool lock()
    {
        if (!memory.isAttached() && !memory.attach()) return false;
        if (memory.size() != sizeof(qint64)) return false;
        return locked = memory.lock();
    }
    bool active() const
    {
        qint64 until = 0;
        if (locked) std::memcpy(&until, memory.constData(), sizeof(until));
        return until > QDateTime::currentMSecsSinceEpoch();
    }
    void renew()
    {
        if (!locked) return;
        qint64 until = QDateTime::currentMSecsSinceEpoch() + 2000;
        std::memcpy(memory.data(), &until, sizeof(until));
    }
    void release()
    {
        if (locked) std::memset(memory.data(), 0, sizeof(qint64));
    }
    void unlock()
    {
        if (locked) { memory.unlock(); locked = false; }
    }
    ~PriorityLease() { unlock(); }
};

class PriorityLock
{
    PriorityLease& lease;
public:
    bool acquired;
    explicit PriorityLock(PriorityLease& lease) : lease(lease), acquired(lease.lock()) {}
    ~PriorityLock() { lease.unlock(); }
};
