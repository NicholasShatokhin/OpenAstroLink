#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QIODevice>
#include <QMetaObject>
#include <QObject>
#include <QSerialPort>
#include <QThread>
#include <QString>

#include <algorithm>
#include <memory>
#include <utility>

// Small blocking RPC wrapper around QSerialPort with a dedicated worker thread.
//
// Native OAL operations run on arbitrary operation-worker threads. Keeping a
// QSerialPort directly inside a device object would therefore violate QObject
// thread affinity as subsequent calls may arrive from a different worker.
// Reopening a USB serial port for every command is also undesirable for
// microcontroller-based focusers because DTR/open transitions can reset some
// controllers. This class keeps one physical port open and executes all I/O on
// one dedicated thread while exposing a synchronous call surface to ABI-v2
// drivers.
class OalBlockingSerialSession final {
public:
    OalBlockingSerialSession() {
        worker_.moveToThread(&thread_);
        thread_.setObjectName(QStringLiteral("oal-native-serial"));
        thread_.start();
    }

    ~OalBlockingSerialSession() {
        close();
        if (thread_.isRunning()) {
            QThread *ownerThread = QThread::currentThread();
            if (ownerThread != &thread_) {
                run([this, ownerThread] { worker_.moveToThread(ownerThread); });
                thread_.quit();
                thread_.wait(3000);
            } else {
                thread_.quit();
            }
        }
    }

    OalBlockingSerialSession(const OalBlockingSerialSession &) = delete;
    OalBlockingSerialSession &operator=(const OalBlockingSerialSession &) = delete;

    bool open(const QString &portName, qint32 baudRate, int settleMs,
              QString *error = nullptr) {
        bool result = false;
        QString localError;
        run([&] {
            if (port_ && port_->isOpen()) {
                if (port_->portName() == portName && port_->baudRate() == baudRate) {
                    result = true;
                    return;
                }
                port_->close();
                port_.reset();
            }
            port_ = std::make_unique<QSerialPort>();
            port_->setPortName(portName);
            port_->setBaudRate(baudRate);
            port_->setDataBits(QSerialPort::Data8);
            port_->setParity(QSerialPort::NoParity);
            port_->setStopBits(QSerialPort::OneStop);
            port_->setFlowControl(QSerialPort::NoFlowControl);
            if (!port_->open(QIODevice::ReadWrite)) {
                localError = port_->errorString();
                port_.reset();
                return;
            }
            // Do not intentionally assert modem-control lines. Many astronomy
            // controllers ignore them; some Arduino-derived devices reset on
            // DTR transitions. Keeping the port open avoids repeated toggles.
            port_->setDataTerminalReady(false);
            port_->setRequestToSend(false);
            if (settleMs > 0) QThread::msleep(unsigned(settleMs));
            result = true;
        });
        if (!result && error) *error = localError;
        return result;
    }

    void close() {
        if (!thread_.isRunning()) return;
        run([&] {
            if (port_) {
                if (port_->isOpen()) port_->close();
                port_.reset();
            }
        });
    }

    bool isOpen() const {
        bool result = false;
        run([&] { result = port_ && port_->isOpen(); });
        return result;
    }

    QString errorString() const {
        QString result;
        run([&] { if (port_) result = port_->errorString(); });
        return result;
    }

    QString lastExchangeDiagnostic() const {
        QString result;
        run([&] { result = lastExchangeDiagnostic_; });
        return result;
    }

    bool exchange(const QByteArray &command, QByteArray *reply, int timeoutMs,
                  bool expectReply = true, char terminator = '#') {
        bool result = false;
        QByteArray data;
        run([&] {
            lastExchangeDiagnostic_.clear();
            if (!port_ || !port_->isOpen()) {
                lastExchangeDiagnostic_ = QStringLiteral("serial port is not open");
                return;
            }
            // Discard only stale input. Never clear output after a command was
            // issued because that can truncate bytes still in the OS buffer.
            port_->clear(QSerialPort::Input);
            const auto written = port_->write(command);
            if (written != command.size()) {
                lastExchangeDiagnostic_ = QStringLiteral("write accepted %1/%2 byte(s): %3")
                                              .arg(written).arg(command.size()).arg(port_->errorString());
                return;
            }

            // A tiny serial write can already have left Qt's buffer before the
            // blocking wait begins on some Windows USB-UART drivers. In that
            // case bytesToWrite()==0 is success, not a transport failure. If a
            // wait times out, re-check the pending byte count before rejecting
            // the exchange. We still wait when data is actually queued.
            if (port_->bytesToWrite() > 0) {
                const bool writeSignal =
                    port_->waitForBytesWritten(std::max(1, std::min(timeoutMs, 1500)));
                if (!writeSignal && port_->bytesToWrite() > 0) {
                    lastExchangeDiagnostic_ = QStringLiteral("write timeout with %1 byte(s) pending: %2")
                                                  .arg(port_->bytesToWrite()).arg(port_->errorString());
                    return;
                }
            }
            if (!expectReply) {
                result = true;
                lastExchangeDiagnostic_ = QStringLiteral("write complete; no reply requested");
                return;
            }
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < timeoutMs) {
                const int remaining = std::max(1, timeoutMs - int(timer.elapsed()));
                if (!port_->waitForReadyRead(std::min(100, remaining))) continue;
                data += port_->readAll();
                while (port_->waitForReadyRead(5)) data += port_->readAll();
                if (data.contains(terminator)) {
                    result = true;
                    break;
                }
            }
            if (result)
                lastExchangeDiagnostic_ = QStringLiteral("reply %1 byte(s): %2")
                                              .arg(data.size())
                                              .arg(QString::fromLatin1(data.toHex(' ')));
            else
                lastExchangeDiagnostic_ = QStringLiteral("read timeout; received %1 byte(s): %2; serial=%3")
                                              .arg(data.size())
                                              .arg(QString::fromLatin1(data.toHex(' ')))
                                              .arg(port_->errorString());
        });
        if (reply) *reply = data;
        return result;
    }

private:
    template <class Fn>
    void run(Fn &&fn) const {
        if (QThread::currentThread() == worker_.thread()) {
            fn();
            return;
        }
        QMetaObject::invokeMethod(&worker_, std::forward<Fn>(fn),
                                  Qt::BlockingQueuedConnection);
    }

    mutable QThread thread_;
    mutable QObject worker_;
    mutable std::unique_ptr<QSerialPort> port_;
    mutable QString lastExchangeDiagnostic_;
};
