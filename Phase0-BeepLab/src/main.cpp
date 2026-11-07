// PokeTimerPlus BeepLab — Phase 0 of PokeTimerPlus.
// Measures, with the microphone, when beeps actually come out of the speaker for three engines
// (EonTimer's way, a Qt stream, a miniaudio stream), and can time a running EonTimer.
#include "MainWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QPermissions>
#include <QStyleHints>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("BeepLab"));
    app.setOrganizationName(QStringLiteral("PokeTimerPlus"));
    app.setStyle(QStringLiteral("fusion")); // same look as PokeFinderPlus

    MainWindow window;
    window.show();

    // Development aid: BEEPLAB_AUTORUN=0,1,2 runs short tests with those engines and quits
    // (results in BEEPLAB_RESULTS if set). Used to test the whole chain on a virtual audio device.
    const QString autorun = qEnvironmentVariable("BEEPLAB_AUTORUN");
    if (!autorun.isEmpty())
    {
        QList<int> modes;
        for (const QString &m : autorun.split(',', Qt::SkipEmptyParts))
        {
            modes << m.trimmed().toInt();
        }
        window.autoRun(modes, qEnvironmentVariable("BEEPLAB_RESULTS"));
        return app.exec();
    }

    // Development aid: BEEPLAB_SCREENSHOT=path.png saves the window after start-up and quits.
    const QString shot = qEnvironmentVariable("BEEPLAB_SCREENSHOT");
    if (!shot.isEmpty())
    {
        QTimer::singleShot(1500, &window, [&window, shot] {
            window.grab().save(shot);
            QApplication::quit();
        });
        return app.exec();
    }

#if QT_CONFIG(permissions)
    QMicrophonePermission mic;
    switch (app.checkPermission(mic))
    {
    case Qt::PermissionStatus::Undetermined:
        app.requestPermission(mic, &window, [&window](const QPermission &p) {
            if (p.status() != Qt::PermissionStatus::Granted)
            {
                QMessageBox::warning(&window, window.windowTitle(),
                                     QStringLiteral("BeepLab needs the microphone to hear the beeps.\n"
                                                    "System Settings → Privacy & Security → Microphone."));
            }
        });
        break;
    case Qt::PermissionStatus::Denied:
        QMessageBox::warning(&window, window.windowTitle(),
                             QStringLiteral("Microphone access is off for BeepLab.\n"
                                            "System Settings → Privacy & Security → Microphone."));
        break;
    case Qt::PermissionStatus::Granted:
        break;
    }
#endif

    return app.exec();
}
