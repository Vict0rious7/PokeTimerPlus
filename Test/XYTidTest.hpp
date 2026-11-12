/*
 * This file is part of PokeTimerPlus
 * XY TID timeline checked against 6genTidSearch's own code (Resources/xytid_6gentidsearch.json, made by PokeFinderPlus'
 * Test/Gen6/golden6tid tool from 6genTidSearch.exe).
 */

#ifndef XYTIDTEST_HPP
#define XYTIDTEST_HPP

#include <QObject>

class XYTidTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void tinyMT();
    void timeline();
    void nudges();
    void handoff();
};

#endif // XYTIDTEST_HPP
