// Engine B: one QAudioSink in pull mode on its own thread, fed by BeepMixer.
#include "Engines.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
// Infinite stream: every read returns exactly what was asked, rendered by the mixer.
class MixerDevice : public QIODevice
{
public:
    MixerDevice(beeplab::BeepMixer *mixer, QAudioFormat format) : mixer_(mixer), format_(format)
    {
    }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return std::numeric_limits<qint32>::max(); }

protected:
    qint64 readData(char *data, qint64 maxlen) override
    {
        const int channels = format_.channelCount();
        const int bytesPerFrame = format_.bytesPerFrame();
        const int frames = int(maxlen / bytesPerFrame);
        if (frames <= 0)
        {
            return 0;
        }
        buffer_.resize(size_t(frames) * channels);
        mixer_->render(buffer_.data(), frames);
        const size_t n = buffer_.size();
        switch (format_.sampleFormat())
        {
        case QAudioFormat::Float:
            std::memcpy(data, buffer_.data(), n * sizeof(float));
            break;
        case QAudioFormat::Int16:
        {
            auto *o = reinterpret_cast<qint16 *>(data);
            for (size_t i = 0; i < n; i++)
            {
                o[i] = qint16(std::clamp(buffer_[i], -1.0f, 1.0f) * 32767.0f);
            }
            break;
        }
        case QAudioFormat::Int32:
        {
            auto *o = reinterpret_cast<qint32 *>(data);
            for (size_t i = 0; i < n; i++)
            {
                o[i] = qint32(double(std::clamp(buffer_[i], -1.0f, 1.0f)) * 2147483647.0);
            }
            break;
        }
        case QAudioFormat::UInt8:
        {
            auto *o = reinterpret_cast<quint8 *>(data);
            for (size_t i = 0; i < n; i++)
            {
                o[i] = quint8(128 + std::clamp(buffer_[i], -1.0f, 1.0f) * 127.0f);
            }
            break;
        }
        default:
            std::memset(data, 0, size_t(frames) * bytesPerFrame);
        }
        return qint64(frames) * bytesPerFrame;
    }
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    beeplab::BeepMixer *mixer_;
    QAudioFormat format_;
    std::vector<float> buffer_;
};
}

// Lives on the engine's thread so the sink is serviced by that thread's event loop, not the GUI's.
class QtStreamWorker : public QObject
{
    Q_OBJECT
public:
    QtStreamWorker(beeplab::BeepMixer *mixer) : mixer_(mixer)
    {
    }

    // Returns an error message, or empty on success. Runs on the worker thread.
    QString start(const QAudioDevice &output, const beeplab::Sound &sound, QString *details)
    {
        QAudioFormat format = output.preferredFormat();
        QAudioFormat floatFormat = format;
        floatFormat.setSampleFormat(QAudioFormat::Float);
        if (output.isFormatSupported(floatFormat))
        {
            format = floatFormat;
        }
        if (!format.isValid())
        {
            return QStringLiteral("no valid output format");
        }
        sink_ = new QAudioSink(output, format, this);
        // Ask for ~20 ms. CoreAudio may round it; the real size is reported in details.
        sink_->setBufferSize(format.bytesForDuration(20000));
        connect(sink_, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
            if (state == QAudio::IdleState)
            {
                emit notice(QStringLiteral("QAudioSink: idle (underrun)"));
            }
            else if (state == QAudio::StoppedState && sink_->error() != QAudio::NoError)
            {
                emit notice(QStringLiteral("QAudioSink stopped, error %1").arg(int(sink_->error())));
            }
        });

        device_ = new MixerDevice(mixer_, format);
        device_->open(QIODevice::ReadOnly);

        // Latency estimate for the mixer's anchor: one buffer. Any constant error is calibrated away.
        double bufferMs = format.durationForBytes(sink_->bufferSize()) / 1000.0;
        mixer_->configure(format.sampleRate(), format.channelCount(), sound.samples, sound.sampleRate, bufferMs);
        sink_->start(device_);
        if (sink_->error() != QAudio::NoError)
        {
            return QStringLiteral("QAudioSink error %1").arg(int(sink_->error()));
        }
        bufferMs = format.durationForBytes(sink_->bufferSize()) / 1000.0;
        *details = QStringLiteral("%1 Hz, %2 ch, %3, buffer %4 ms")
                       .arg(format.sampleRate())
                       .arg(format.channelCount())
                       .arg(format.sampleFormat() == QAudioFormat::Float ? QStringLiteral("float")
                                                                          : QStringLiteral("int"))
                       .arg(bufferMs, 0, 'f', 1);
        return {};
    }

    void stop()
    {
        if (sink_)
        {
            sink_->stop();
            delete sink_;
            sink_ = nullptr;
        }
        delete device_;
        device_ = nullptr;
    }

signals:
    void notice(const QString &text);

private:
    beeplab::BeepMixer *mixer_;
    QAudioSink *sink_ = nullptr;
    QIODevice *device_ = nullptr;
};

QtStreamEngine::QtStreamEngine(QObject *parent) : Engine(parent)
{
}

QtStreamEngine::~QtStreamEngine()
{
    close();
}

bool QtStreamEngine::open(const QAudioDevice &output, const beeplab::Sound &sound, const QString &, QString *error)
{
    close();
    thread_ = new QThread(this);
    thread_->setObjectName(QStringLiteral("QtStreamEngine"));
    worker_ = new QtStreamWorker(&mixer_);
    worker_->moveToThread(thread_);
    connect(worker_, &QtStreamWorker::notice, this, &Engine::notice);
    thread_->start(QThread::TimeCriticalPriority);

    QString err, details;
    QMetaObject::invokeMethod(
        worker_, [&] { err = worker_->start(output, sound, &details); }, Qt::BlockingQueuedConnection);
    details_ = details;
    if (!err.isEmpty())
    {
        if (error)
        {
            *error = err;
        }
        close();
        return false;
    }
    return true;
}

void QtStreamEngine::play(const std::vector<beeplab::ScheduledBeep> &schedule)
{
    std::vector<double> ms;
    for (const auto &s : schedule)
    {
        ms.push_back(s.ms);
    }
    mixer_.setSchedule(ms);
    if (!mixer_.anchored())
    {
        emit notice(QStringLiteral("stream not running yet when the schedule was set"));
    }
}

void QtStreamEngine::close()
{
    if (worker_)
    {
        QMetaObject::invokeMethod(worker_, [this] { worker_->stop(); }, Qt::BlockingQueuedConnection);
        worker_->deleteLater();
        worker_ = nullptr;
    }
    if (thread_)
    {
        thread_->quit();
        thread_->wait();
        delete thread_;
        thread_ = nullptr;
    }
}

#include "QtStreamEngine.moc"
