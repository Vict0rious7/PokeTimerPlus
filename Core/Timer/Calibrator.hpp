/*
 * This file is part of PokeTimerPlus
 * Port of EonTimer's Calibrator (3.x desktop, MIT, DasAmpharos): frames <-> milliseconds.
 */

#ifndef CALIBRATOR_HPP
#define CALIBRATOR_HPP

/**
 * @brief Converts between frames ("delays") and milliseconds for one frame rate
 *
 * Rounding is round-half-to-even, like EonTimer (Python round, originally C# Math.Round).
 */
class Calibrator
{
public:
    /**
     * @param fps Frames per second
     * @param precisionCalibration Gen 5 calibrations are kept in ms instead of delays
     * @param minimumLengthSeconds Shortest first phase; shorter ones get whole minutes added
     */
    Calibrator(double fps, bool precisionCalibration = false, int minimumLengthSeconds = 14);

    double msPerFrame() const;
    bool precisionCalibration() const;
    double minimumLength() const; // ms

    int toDelays(double milliseconds) const;
    double toMilliseconds(int delays) const; // always a whole number of ms
    int calibrateToDelays(double milliseconds) const;
    double calibrateToMilliseconds(int delays) const;
    double createCalibration(int delays, double seconds) const;

private:
    double msPerFrame_;
    bool precision_;
    int minimumLengthSeconds_;
};

/**
 * @brief Round half to even, as Python's round() and C#'s Math.Round
 */
double roundHalfEven(double value);

#endif // CALIBRATOR_HPP
