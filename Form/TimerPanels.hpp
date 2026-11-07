/*
 * This file is part of PokeTimerPlus
 * One panel per timer template. A panel shows the targets (empty or last used), the calibration
 * (pre-filled from what was saved for that RNG type) and "hit" fields for Update.
 */

#ifndef TIMERPANELS_HPP
#define TIMERPANELS_HPP

#include <Core/Timer/Calibrator.hpp>
#include <Core/Timer/RngTypes.hpp>
#include <Core/Timer/Timers.hpp>
#include <QWidget>
#include <map>
#include <optional>
#include <string>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTabBar;
class QTableWidget;

using Fields = std::map<std::string, double>;

class TimerPanel : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;

    // Sections (egg types have Held / Pickup); a plain panel is its own single section.
    virtual int sectionCount() const
    {
        return 1;
    }
    virtual TimerPanel *section(int)
    {
        return this;
    }
    virtual TimerPanel *current()
    {
        return this;
    }
    virtual int currentSection() const
    {
        return 0;
    }

    virtual Fields calibration() const = 0;
    virtual void setCalibration(const Fields &fields) = 0; // missing fields take the defaults
    virtual std::vector<double> phases(const Calibrator &calibrator) const = 0;
    virtual bool calibrate(const Calibrator &calibrator) = 0; // from the hit fields, then clears them
    virtual int minutesBefore(const Calibrator &) const
    {
        return -1;
    }
    virtual bool isContinuous() const
    {
        return false;
    }
    virtual void setRunning(bool)
    {
    }

signals:
    void changed(); // anything that changes the preview
    void calibrationEdited();
    void sectionChanged();
    void setTargetRequested(); // Gen 3 Variable Target, while running
    void addTargetRequested(double value, bool frames); // continuous custom timer
    void nudgeRequested(int frames);
    void removeLastRequested();

protected:
    static QSpinBox *spin(int min, int max, const QString &suffix = {});
    static QSpinBox *targetSpin(int max, const QString &suffix = {}); // shows empty at 0
    static QDoubleSpinBox *doubleSpin(const QString &suffix = {});
    static QLineEdit *hitEdit(const QString &placeholder);
    static std::optional<double> hit(const QLineEdit *edit);
    static double field(const Fields &f, const char *key, double fallback);
    void watch(QSpinBox *s, bool isCalibration);
    void watch(QDoubleSpinBox *s, bool isCalibration);
    bool loading_ = false;
};

// EonTimer Gen 3: pre-timer + target frame, calibration in ms. Fields: ms
class Gen3FramesPanel : public TimerPanel
{
    Q_OBJECT
public:
    explicit Gen3FramesPanel(QWidget *parent = nullptr);
    Fields calibration() const override;
    void setCalibration(const Fields &fields) override;
    std::vector<double> phases(const Calibrator &calibrator) const override;
    bool calibrate(const Calibrator &calibrator) override;
    void setRunning(bool running) override;
    double targetPhase(const Calibrator &calibrator) const;

private:
    void updateRows();
    QSpinBox *preTimer_, *targetFrame_;
    QCheckBox *variable_;
    QPushButton *setTarget_;
    QDoubleSpinBox *calibration_;
    QLineEdit *frameHit_;
    bool running_ = false;
};

// FRLG: initial seed time (ms) + advances, each with a calibration. Fields: seedMs, advances
class SeedAdvancesPanel : public TimerPanel
{
    Q_OBJECT
public:
    explicit SeedAdvancesPanel(QWidget *parent = nullptr);
    Fields calibration() const override;
    void setCalibration(const Fields &fields) override;
    std::vector<double> phases(const Calibrator &calibrator) const override;
    bool calibrate(const Calibrator &calibrator) override;

private:
    QSpinBox *seed_, *advances_, *advancesCalibration_;
    QDoubleSpinBox *seedCalibration_;
    QLineEdit *seedHit_, *advanceHit_;
};

// EonTimer Gen 4. Fields: delay, second
class Gen4DelayPanel : public TimerPanel
{
    Q_OBJECT
public:
    explicit Gen4DelayPanel(QWidget *parent = nullptr);
    Fields calibration() const override;
    void setCalibration(const Fields &fields) override;
    std::vector<double> phases(const Calibrator &calibrator) const override;
    bool calibrate(const Calibrator &calibrator) override;
    int minutesBefore(const Calibrator &calibrator) const override;

private:
    Gen4Targets targets() const;
    QSpinBox *targetDelay_, *targetSecond_, *calibratedDelay_, *calibratedSecond_;
    QLineEdit *delayHit_;
};

// EonTimer Gen 5. Fields: calibration, entralink, frame
class Gen5Panel : public TimerPanel
{
    Q_OBJECT
public:
    explicit Gen5Panel(QWidget *parent = nullptr);
    Fields calibration() const override;
    void setCalibration(const Fields &fields) override;
    std::vector<double> phases(const Calibrator &calibrator) const override;
    bool calibrate(const Calibrator &calibrator) override;
    int minutesBefore(const Calibrator &calibrator) const override;

private:
    Gen5Targets targets() const;
    Gen5Calibration cal() const;
    void updateRows();
    QFormLayout *form_;
    QComboBox *mode_;
    QSpinBox *targetDelay_, *targetSecond_, *targetAdvances_, *calibration_, *entralink_;
    QDoubleSpinBox *frame_;
    QLineEdit *delayHit_, *secondHit_, *advancesHit_;
};

// Free phases, or continuous (the clock runs, targets are added on the fly). Fields: continuous (ms)
class CustomPanel : public TimerPanel
{
    Q_OBJECT
public:
    explicit CustomPanel(QWidget *parent = nullptr);
    Fields calibration() const override;
    void setCalibration(const Fields &fields) override;
    std::vector<double> phases(const Calibrator &calibrator) const override;
    bool calibrate(const Calibrator &calibrator) override;
    bool isContinuous() const override;
    void setRunning(bool running) override;

    std::vector<CustomPhase> layout() const;
    void setLayout(const std::vector<CustomPhase> &phases);
    // Continuous mode
    double resolveTarget(const Calibrator &calibrator, double value, bool frames) const; // ms from start
    void showTargets(const std::vector<double> &targetsMs, const Calibrator &calibrator);

private:
    void addRow(const CustomPhase &phase);
    void updateMode();
    QCheckBox *continuous_;
    QStackedWidget *stack_;
    QTableWidget *table_;
    // continuous
    QSpinBox *target_;
    QComboBox *unit_;
    QDoubleSpinBox *continuousCalibration_;
    QPushButton *add_;
    QListWidget *targets_;
    QPushButton *minus_, *plus_, *removeLast_;
    QLineEdit *continuousHit_;
    std::vector<std::pair<double, bool>> typed_; // targets as typed, for Update
    bool running_ = false;
};

// Two sections (Held / Pickup) of the same template, each with its own calibration.
class SectionedPanel : public TimerPanel
{
    Q_OBJECT
public:
    SectionedPanel(const std::vector<RngSection> &sections, TimerTemplate timer, QWidget *parent = nullptr);
    int sectionCount() const override;
    TimerPanel *section(int index) override;
    TimerPanel *current() override;
    int currentSection() const override;

    Fields calibration() const override;
    void setCalibration(const Fields &fields) override;
    std::vector<double> phases(const Calibrator &calibrator) const override;
    bool calibrate(const Calibrator &calibrator) override;
    void setRunning(bool running) override;

private:
    QTabBar *bar_;
    QStackedWidget *stack_;
    std::vector<TimerPanel *> panels_;
};

TimerPanel *createPanel(const RngType &type);

#endif // TIMERPANELS_HPP
