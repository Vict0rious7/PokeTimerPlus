/*
 * This file is part of PokeTimerPlus
 */

#include "AudioTest.hpp"
#include "StoreTest.hpp"
#include "ScheduleTest.hpp"
#include "TimersTest.hpp"
#include "XYTidTest.hpp"
#include <QCoreApplication>
#include <QTest>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int status = 0;
    {
        TimersTest test;
        status |= QTest::qExec(&test, argc, argv);
    }
    {
        ScheduleTest test;
        status |= QTest::qExec(&test, argc, argv);
    }
    {
        AudioTest test;
        status |= QTest::qExec(&test, argc, argv);
    }
    {
        StoreTest test;
        status |= QTest::qExec(&test, argc, argv);
    }
    {
        XYTidTest test;
        status |= QTest::qExec(&test, argc, argv);
    }
    return status;
}
