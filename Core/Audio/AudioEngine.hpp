/*
 * This file is part of PokeTimerPlus
 * The output stream (engine C of Phase 0): miniaudio, low-latency profile, 10 ms period, opened when
 * the app starts and kept open, so the device never idles between countdowns (EonTimer's
 * "irregular" bug). No Qt here.
 */

#ifndef AUDIOENGINE_HPP
#define AUDIOENGINE_HPP

#include "BeepMixer.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct OutputDevice
{
    std::string name;
    bool isDefault;
};

class AudioEngine
{
public:
    enum class Event
    {
        Started,
        Stopped,
        Rerouted, // the system default output changed under us
        InterruptionBegan,
        InterruptionEnded,
    };
    using EventFn = std::function<void(Event)>; // called from the audio thread

    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine &) = delete;
    AudioEngine &operator=(const AudioEngine &) = delete;

    static std::vector<OutputDevice> outputs();

    /**
     * @brief Opens (or reopens) the output and starts the stream
     *
     * @param deviceName Output name; empty = system default (follows the default when it changes)
     */
    bool open(const std::string &deviceName, std::string *error = nullptr);
    void close();
    bool isOpen() const;

    std::string deviceName() const; // the device actually playing (resolved for the default)
    bool followsDefault() const;
    std::string details() const; // backend, rate, period
    double bufferMs() const;

    void setEventCallback(EventFn fn);

    BeepMixer &mixer();
    const BeepMixer &mixer() const;

    // Internal: called by the audio backend's notification callback.
    struct State;
    void notify(Event e);

private:
    std::unique_ptr<State> state_;
    BeepMixer mixer_;
    EventFn events_;
};

#endif // AUDIOENGINE_HPP
