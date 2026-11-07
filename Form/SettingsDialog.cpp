/*
 * This file is part of PokeTimerPlus
 */

#include "SettingsDialog.hpp"

#include "TimerController.hpp"
#include <Core/Audio/AudioEngine.hpp>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

Audio::Sound loadSound(const QString &name)
{
    QFile f(QStringLiteral(":/sounds/%1.wav").arg(name.toLower()));
    if (!f.open(QIODevice::ReadOnly))
    {
        f.setFileName(QStringLiteral(":/sounds/beep.wav"));
        if (!f.open(QIODevice::ReadOnly))
        {
            return {};
        }
    }
    QByteArray b = f.readAll();
    return Audio::parseWav(std::vector<unsigned char>(b.begin(), b.end()));
}

AppSettings AppSettings::load()
{
    QSettings s;
    AppSettings a;
    a.beepCount = std::clamp(s.value("beeps/count", a.beepCount).toInt(), 1, 20);
    a.beepIntervalMs = std::clamp(s.value("beeps/interval", a.beepIntervalMs).toInt(), 50, 5000);
    a.sound = s.value("beeps/sound", a.sound).toString();
    a.audio = s.value("beeps/audio", a.audio).toBool();
    a.visual = s.value("beeps/visual", a.visual).toBool();
    a.precisionCalibration = s.value("timer/precisionCalibration", a.precisionCalibration).toBool();
    a.minimumLengthSeconds = std::clamp(s.value("timer/minimumLength", a.minimumLengthSeconds).toInt(), 1, 60);
    a.output = s.value("output", a.output).toString();
    a.theme = s.value("style", a.theme).toString(); // same key as PokeFinder
    return a;
}

void AppSettings::save() const
{
    QSettings s;
    s.setValue("beeps/count", beepCount);
    s.setValue("beeps/interval", beepIntervalMs);
    s.setValue("beeps/sound", sound);
    s.setValue("beeps/audio", audio);
    s.setValue("beeps/visual", visual);
    s.setValue("timer/precisionCalibration", precisionCalibration);
    s.setValue("timer/minimumLength", minimumLengthSeconds);
    s.setValue("output", output);
    s.setValue("style", theme);
}

SettingsDialog::SettingsDialog(const AppSettings &settings, TimerController *controller, QWidget *parent) :
    QDialog(parent), base_(settings), controller_(controller)
{
    setWindowTitle(tr("Settings"));
    auto *layout = new QVBoxLayout(this);

    auto *beepBox = new QGroupBox(tr("Beeps"));
    auto *beepForm = new QFormLayout(beepBox);
    count_ = new QSpinBox;
    count_->setRange(1, 20);
    count_->setValue(settings.beepCount);
    interval_ = new QSpinBox;
    interval_->setRange(50, 5000);
    interval_->setSuffix(tr(" ms"));
    interval_->setValue(settings.beepIntervalMs);
    sound_ = new QComboBox;
    for (const char *s : { "beep", "ding", "pop", "tick" })
    {
        QString n = QString::fromLatin1(s);
        sound_->addItem(n.left(1).toUpper() + n.mid(1), n);
    }
    sound_->setCurrentIndex(std::max(0, sound_->findData(settings.sound)));
    audio_ = new QCheckBox(tr("Sound"));
    audio_->setChecked(settings.audio);
    visual_ = new QCheckBox(tr("Flash"));
    visual_->setChecked(settings.visual);
    auto *cues = new QHBoxLayout;
    cues->addWidget(audio_);
    cues->addWidget(visual_);
    cues->addStretch();
    auto *test = new QPushButton(tr("Test"));
    beepForm->addRow(tr("Beeps per target"), count_);
    beepForm->addRow(tr("Interval"), interval_);
    beepForm->addRow(tr("Sound"), sound_);
    beepForm->addRow(tr("Cues"), cues);
    beepForm->addRow(QString(), test);
    layout->addWidget(beepBox);

    auto *timerBox = new QGroupBox(tr("Timer"));
    auto *timerForm = new QFormLayout(timerBox);
    precision_ = new QCheckBox(tr("Precision calibration (Gen 5 calibrations in ms instead of delays)"));
    precision_->setChecked(settings.precisionCalibration);
    minimumLength_ = new QSpinBox;
    minimumLength_->setRange(1, 60);
    minimumLength_->setSuffix(tr(" s"));
    minimumLength_->setValue(settings.minimumLengthSeconds);
    minimumLength_->setToolTip(tr("Gen 4/5: a first phase shorter than this gets a minute added, as in EonTimer."));
    timerForm->addRow(QString(), precision_);
    timerForm->addRow(tr("Minimum length"), minimumLength_);
    layout->addWidget(timerBox);

    auto *outputBox = new QGroupBox(tr("Output and appearance"));
    auto *outputForm = new QFormLayout(outputBox);
    output_ = new QComboBox;
    output_->addItem(tr("System default"), QString());
    for (const auto &d : AudioEngine::outputs())
    {
        output_->addItem(QString::fromStdString(d.name), QString::fromStdString(d.name));
    }
    int oi = output_->findData(settings.output);
    if (oi < 0 && !settings.output.isEmpty())
    {
        output_->addItem(tr("%1 (not connected)").arg(settings.output), settings.output);
        oi = output_->count() - 1;
    }
    output_->setCurrentIndex(std::max(0, oi));
    theme_ = new QComboBox;
    theme_->addItem(tr("Automatic"), QStringLiteral("auto"));
    theme_->addItem(tr("Dark"), QStringLiteral("dark"));
    theme_->addItem(tr("Light"), QStringLiteral("light"));
    theme_->setCurrentIndex(std::max(0, theme_->findData(settings.theme)));
    outputForm->addRow(tr("Output"), output_);
    outputForm->addRow(tr("Theme"), theme_);
    layout->addWidget(outputBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(test, &QPushButton::clicked, this, [this] {
        if (controller_)
        {
            controller_->setSound(loadSound(sound_->currentData().toString()));
            controller_->setBeeps(count_->value(), interval_->value());
            controller_->testBeeps();
        }
    });
}

AppSettings SettingsDialog::settings() const
{
    AppSettings a = base_;
    a.beepCount = count_->value();
    a.beepIntervalMs = interval_->value();
    a.sound = sound_->currentData().toString();
    a.audio = audio_->isChecked();
    a.visual = visual_->isChecked();
    a.precisionCalibration = precision_->isChecked();
    a.minimumLengthSeconds = minimumLength_->value();
    a.output = output_->currentData().toString();
    a.theme = theme_->currentData().toString();
    return a;
}
