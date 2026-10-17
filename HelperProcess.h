#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <functional>
#include <utility>

// Owns a long-lived helper: readiness, heartbeat, bounded retry and intentional stop.
// It never starts Wine, modifies a prefix or interprets SDK methods itself.
// Calls and callbacks run on the QObject's owning Qt thread.
class HelperProcess : public QObject
{
public:
    struct Policy {
        int retries = 5;
        int firstRetryMs = 1000, maxRetryMs = 16000;
        int startupMs = 45000, heartbeatMs = 10000, stableMs = 30000;
        int pollMs = 250, stopMs = 4000;
    };
    struct Config { QString program; QStringList arguments; QString statusFile, errorLog; };
    enum class State { Stopped, Starting, Ready, WaitingRetry, Recovering, Exhausted };
    struct Status {
        State state = State::Stopped;
        int retries = 0;
        qint64 pid = 0, retryInMs = 0;
        QString error;
        QJsonObject health;
    };
    HelperProcess(Config config, Policy policy, QObject* parent = nullptr);
    explicit HelperProcess(Config config, QObject* parent = nullptr) : HelperProcess(std::move(config), Policy{}, parent) {}
    ~HelperProcess() override;
    void setEnabled(bool enabled);
    Status status() const;
    std::function<void()> changed;
    std::function<void()> unavailable;
private:
    Config config;
    Policy policy;
    QProcess process;
    QTimer monitor, retry, killDeadline;
    QElapsedTimer clock;
    Status current;
    bool wanted = false, failing = false, stableReset = false;
    qint64 attemptAt = 0, attemptWallMs = 0, lastHealthy = 0, readyAt = 0, retryAt = 0;
    void start();
    void poll();
    void fail(const QString& reason);
    void scheduleRetry();
    void state(State value);
    bool readHealth(QJsonObject& value, qint64& ageMs) const;
};
