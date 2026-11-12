/*
 * This file is part of PokeTimerPlus
 * XY TID: the blink timeline and the wait after the 4th step, for an ID picked in PokeFinderPlus (Gen 6 → IDs).
 * Method and values: BZL's 6genTidSearch (used with permission), https://blog.bzl-web.com/entry/2020/08/31/202306
 */

#ifndef XYTIDPANEL_HPP
#define XYTIDPANEL_HPP

#include "TimerPanels.hpp"
#include <Core/Gen6/XYTid.hpp>
#include <memory>
#include <optional>
#include <vector>

class QLabel;
class QPushButton;
class QRadioButton;
class QTableWidget;

class XYTidPanel : public TimerPanel
{
    Q_OBJECT
public:
    explicit XYTidPanel(QWidget *parent = nullptr);

    // Saved with "Save to profile": pressMs (added to the A press), blinkOffset (frames, applied at the start)
    Fields calibration() const override;
    void setCalibration(const Fields &fields) override;
    std::vector<double> phases(const Calibrator &) const override
    {
        return {};
    }
    bool calibrate(const Calibrator &calibrator) override; // from the TID you got
    void setRunning(bool running) override;

    /// Fills the Seed fields with what PokeFinderPlus sent (not while a session runs)
    bool load(const XYTid::Handoff &handoff);
    bool hasHandoff() const;
    bool isSessionRunning() const;

    /// Builds the timeline now: the time since the first tap places the blinks
    void startSession(double nowMs);
    void stopSession();
    /// Space with A at each of the 4 scenes
    void stepPressed(double nowMs);
    /// Runs the timeline up to now; called by the window's clock (about every 15 ms)
    void tick(double nowMs);
    void setCountdown(int count, int intervalMs);

    /// Frame rate of this method (the taps and the timeline): 59.8261
    static double msPerFrame()
    {
        return 1000.0 / XYTid::FPS;
    }

signals:
    void beepsChanged(const std::vector<double> &absoluteMs);
    void countdown(double remainingMs, const QString &label); // NaN: nothing to count down
    void pasteRequested();
    void message(const QString &text);

private:
    double frameTime(double frame) const; // system ms of a timeline frame
    XYTid::u32 frameAt(double ms) const;
    void refreshBeeps(double nowMs);
    void refreshList();
    void updateStepLabel();
    void chooseRow(int row);
    double aimMs(const XYTid::Row &row) const;
    std::optional<XYTid::Handoff> fields() const; // from the Seed fields

    // UI
    QLineEdit *state_, *firstTap_, *target_;
    QPushButton *paste_;
    QRadioButton *boy_, *girl_;
    QPushButton *faster_, *slower_;
    QLabel *offsetLabel_;
    QLabel *stepLabel_;
    QPushButton *step_;
    QTableWidget *table_;
    QDoubleSpinBox *pressMs_;
    QSpinBox *blinkOffset_;
    QLineEdit *tidHit_;

    std::optional<XYTid::Handoff> handoff_;
    std::unique_ptr<XYTid::Session> session_;
    double startMs_ = 0; // system ms of timeline frame 0
    double step4Ms_ = 0;
    std::vector<XYTid::Row> rows_;
    int chosen_ = -1;
    std::optional<XYTid::Row> lastAim_; // for Update
    std::vector<XYTid::Row> lastRows_;
    double lastRefresh_ = 0;
    int count_ = 6;
    int intervalMs_ = 500;
    int nudges_ = 0;
};

#endif // XYTIDPANEL_HPP
