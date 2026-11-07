/*
 * This file is part of PokeTimerPlus
 */

#ifndef STORETEST_HPP
#define STORETEST_HPP

#include <QObject>

class StoreTest : public QObject
{
    Q_OBJECT
private slots:
    void catalog();
    void roundTrip();
    void tolerantParsing();
    void saveAndLoad();
    void continuousTargets();
};

#endif // STORETEST_HPP
