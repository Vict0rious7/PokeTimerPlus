/*
 * This file is part of PokeTimerPlus
 * App settings: beeps, timer options, output, appearance (QSettings). Calibrations live elsewhere
 * (CalibrationStore, "Save to profile").
 */

#ifndef SETTINGSDIALOG_HPP
#define SETTINGSDIALOG_HPP

#include <Core/Audio/Wav.hpp>
#include <QDialog>

class QCheckBox;
class QComboBox;
class QSpinBox;
class TimerController;

Audio::Sound loadSound(const QString &name); // beep, ding, pop, tick (EonTimer's sounds, in resources)

struct AppSettings
{
    int beepCount = 6; // EonTimer defaults
    int beepIntervalMs = 500;
    QString sound = QStringLiteral("beep");
    bool audio = true;
    bool visual = true;
    bool precisionCalibration = false;
    int minimumLengthSeconds = 14;
    QString output; // empty = system default
    QString theme = QStringLiteral("auto");

    static AppSettings load();
    void save() const;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    SettingsDialog(const AppSettings &settings, TimerController *controller, QWidget *parent = nullptr);
    AppSettings settings() const;

private:
    AppSettings base_;
    TimerController *controller_;
    QSpinBox *count_, *interval_, *minimumLength_;
    QComboBox *sound_, *output_, *theme_;
    QCheckBox *audio_, *visual_, *precision_;
};

#endif // SETTINGSDIALOG_HPP
