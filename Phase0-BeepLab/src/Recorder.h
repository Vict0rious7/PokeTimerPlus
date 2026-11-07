// Microphone capture with arrival timestamps, so recording frames can be mapped to system time.
#pragma once

#include "Analysis.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QObject>

class QAudioSource;
class QIODevice;
class QTimer;

class Recorder : public QObject
{
    Q_OBJECT
public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder() override;

    bool start(const QAudioDevice &input, QString *error);
    beeplab::Recording stop();
    bool running() const { return source_ != nullptr; }
    float level() const { return level_; } // peak of the last chunk, 0..1
    QString details() const { return details_; }

signals:
    void levelChanged(float peak);

private:
    void drain();

    QAudioSource *source_ = nullptr;
    QIODevice *io_ = nullptr;
    QTimer *poll_ = nullptr;
    QAudioFormat format_;
    beeplab::Recording rec_;
    QByteArray leftover_;
    float level_ = 0;
    QString details_;
};
