#include "Recorder.h"

#include "Clock.h"

#include <QAudioSource>
#include <QIODevice>
#include <QTimer>

#include <cmath>
#include <cstring>
#include <algorithm>

Recorder::Recorder(QObject *parent) : QObject(parent)
{
}

Recorder::~Recorder()
{
    stop();
}

bool Recorder::start(const QAudioDevice &input, QString *error)
{
    stop();
    format_ = input.preferredFormat();
    QAudioFormat f = format_;
    f.setSampleFormat(QAudioFormat::Float);
    if (input.isFormatSupported(f))
    {
        format_ = f;
    }
    if (!format_.isValid())
    {
        if (error)
        {
            *error = QStringLiteral("no valid input format");
        }
        return false;
    }

    rec_ = {};
    rec_.sampleRate = format_.sampleRate();
    rec_.samples.reserve(size_t(format_.sampleRate()) * 180);
    leftover_.clear();

    source_ = new QAudioSource(input, format_, this);
    source_->setBufferSize(format_.bytesForDuration(250000)); // room for a 250 ms GUI stall; larger can add latency
    io_ = source_->start();
    if (!io_ || source_->error() != QAudio::NoError)
    {
        if (error)
        {
            *error = QStringLiteral("cannot open the microphone (permission?)");
        }
        delete source_;
        source_ = nullptr;
        io_ = nullptr;
        return false;
    }
    connect(io_, &QIODevice::readyRead, this, &Recorder::drain);
    poll_ = new QTimer(this);
    poll_->setTimerType(Qt::PreciseTimer);
    connect(poll_, &QTimer::timeout, this, &Recorder::drain);
    poll_->start(5);
    details_ = QStringLiteral("%1 Hz, %2 ch").arg(format_.sampleRate()).arg(format_.channelCount());
    return true;
}

void Recorder::drain()
{
    if (!io_)
    {
        return;
    }
    QByteArray bytes = io_->readAll();
    if (bytes.isEmpty())
    {
        return;
    }
    const double arrived = beeplab::nowMs();
    bytes.prepend(leftover_);
    const int bpf = format_.bytesPerFrame();
    const int bps = format_.bytesPerSample();
    const int ch = format_.channelCount();
    const int frames = int(bytes.size() / bpf);
    leftover_ = bytes.mid(frames * bpf);

    float peak = 0;
    const char *p = bytes.constData();
    for (int i = 0; i < frames; i++)
    {
        double sum = 0;
        for (int c = 0; c < ch; c++)
        {
            const char *s = p + i * bpf + c * bps;
            double v = 0;
            switch (format_.sampleFormat())
            {
            case QAudioFormat::Float:
            {
                float x;
                std::memcpy(&x, s, 4);
                v = x;
                break;
            }
            case QAudioFormat::Int16:
            {
                qint16 x;
                std::memcpy(&x, s, 2);
                v = x / 32768.0;
                break;
            }
            case QAudioFormat::Int32:
            {
                qint32 x;
                std::memcpy(&x, s, 4);
                v = x / 2147483648.0;
                break;
            }
            case QAudioFormat::UInt8:
                v = (quint8(*s) - 128) / 128.0;
                break;
            default:
                break;
            }
            sum += v;
        }
        float m = float(sum / ch);
        rec_.samples.push_back(m);
        peak = std::max(peak, std::abs(m));
    }
    rec_.stamps.push_back({ int64_t(rec_.samples.size()), arrived });
    level_ = peak;
    emit levelChanged(peak);
}

beeplab::Recording Recorder::stop()
{
    if (source_)
    {
        drain();
        if (poll_)
        {
            poll_->stop();
            delete poll_;
            poll_ = nullptr;
        }
        source_->stop();
        delete source_;
        source_ = nullptr;
        io_ = nullptr;
    }
    beeplab::Recording out = std::move(rec_);
    rec_ = {};
    return out;
}
