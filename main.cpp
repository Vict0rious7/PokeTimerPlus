/*
 * This file is part of PokeTimerPlus
 * All-in-one RNG timer: EonTimer's timers, FlowTimer's flowing targets, beeps written straight into
 * an always-open audio stream.
 */

#include <Form/MainWindow.hpp>
#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QPushButton>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("PokeTimerPlus"));
    app.setOrganizationName(QStringLiteral("PokeTimerPlus"));
    app.setStyle(QStringLiteral("fusion")); // PokeFinderPlus look

    // Window / taskbar icon (the .app and .exe icons are set in CMakeLists.txt).
    QIcon icon;
    for (int size : { 16, 24, 32, 48, 256 })
    {
        icon.addFile(QStringLiteral(":/icons/poketimerplus_%1x%1.png").arg(size));
    }
    app.setWindowIcon(icon);

    // Decimals with a dot whatever the system region (French uses a comma), no thousands separator.
    QLocale numbers = QLocale::c();
    numbers.setNumberOptions(QLocale::OmitGroupSeparator | QLocale::RejectGroupSeparator);
    QLocale::setDefault(numbers);

    MainWindow window;
    window.show();

    // Development aid: POKETIMERPLUS_SCREENSHOT=path.png saves the window after start-up and quits.
    const QString shot = qEnvironmentVariable("POKETIMERPLUS_SCREENSHOT");
    if (!shot.isEmpty())
    {
        QTimer::singleShot(1200, &window, [&window, shot] {
            window.grab().save(shot);
            window.close();
        });
    }
    // Development aid: POKETIMERPLUS_AUTOSTART=<ms> clicks Start after that delay (end-to-end audio test).
    const int autostart = qEnvironmentVariableIntValue("POKETIMERPLUS_AUTOSTART");
    if (autostart > 0)
    {
        QTimer::singleShot(autostart, &window, [&window] {
            for (auto *b : window.findChildren<QPushButton *>())
            {
                if (b->text() == QObject::tr("Start"))
                {
                    b->click();
                }
            }
        });
    }
    return app.exec();
}
