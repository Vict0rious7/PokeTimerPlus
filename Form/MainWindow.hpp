/*
 * This file is part of PokeTimerPlus
 * Console tabs (GB/GBA | DS | 3DS | Switch) -> RNG types -> that type's fields, with its saved
 * calibration pre-filled; the timer on the right.
 */

#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include "SettingsDialog.hpp"
#include <Core/Profile/CalibrationStore.hpp>
#include <Core/Timer/Calibrator.hpp>
#include <Core/Timer/RngTypes.hpp>
#include <QMainWindow>
#include <map>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTabBar;
class TimerController;
class TimerDisplay;
class TimerPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    static QString storePath();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void selectConsole(int index);
    void selectType(const std::string &id);
    void hardwareChanged();
    TimerPanel *panelFor(const RngType &type);
    void prefill(const RngType &type, TimerPanel *panel);
    Calibrator calibrator() const;
    std::string currentKey() const;
    void saveCalibration();
    void updateSaveState();
    void updateNote();
    void updatePreview();
    void startStop();
    void calibrate();
    void setTarget();
    void finished();
    void applySettings();
    void openSettings();
    void outputChanged();
    void saveStore();

    const Console &console() const;
    const RngType *type() const;
    TimerPanel *panel() const;

    CalibrationStore store_;
    AppSettings settings_;
    int consoleIndex_ = 0;
    std::string typeId_;
    std::map<std::string, TimerPanel *> panels_;

    TimerController *controller_;
    QTabBar *consoles_;
    QComboBox *hardware_;
    QDoubleSpinBox *customFps_;
    QListWidget *types_;
    QLabel *title_;
    QStackedWidget *stack_;
    QLabel *note_;
    QPushButton *saveButton_;
    QLabel *saveState_;
    TimerDisplay *display_;
    QPushButton *updateButton_;
    QPushButton *startButton_;
    QLabel *outputLabel_;
};

#endif // MAINWINDOW_HPP
