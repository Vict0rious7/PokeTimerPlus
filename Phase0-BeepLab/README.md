# BeepLab — PokeTimerPlus Phase 0

Measures **when beeps really come out of the speaker**, using the Mac's microphone, for three ways of making beeps:

| Engine | What it is |
|---|---|
| **A: EonTimer way** | Copy of EonTimer 3.0.0-rc.8: a thread waits (8 ms sleeps + 2 ms spin), then the main thread starts a `QSoundEffect`. |
| **B: Qt stream** | One `QAudioSink` stream kept open; beeps are written at exact sample positions (FlowTimer's idea). |
| **C: miniaudio stream** | Same idea, CoreAudio driven directly by miniaudio with a 10 ms period. |
| **Listen only** | Doesn't play anything: times the beeps of another app (the real EonTimer) from the microphone. |

The decisive question (plan, Phase 0): can B or C place every beep within ~1 ms, including in the conditions where EonTimer goes irregular?

## Build on the Mac

Same toolchain as PokeFinderPlus (Homebrew Qt includes Qt Multimedia):

```sh
cd ~/Documents/Personnal/Games/Pokemon/Dev/PokeTimerPlus/Phase0-BeepLab
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" -DCMAKE_OSX_ARCHITECTURES=$(uname -m)
cmake --build build
(cd build && ctest)        # analysis tests on synthetic recordings
open build/BeepLab.app
```

The first launch asks for the microphone. If you said no: System Settings → Privacy & Security → Microphone → BeepLab.

## Before the runs

- Built-in speakers at about half volume, built-in mic, quiet room. Don't touch the Mac during a run.
- Press **Test beep** once per engine: it should say "heard 3/3". If not, raise the volume.
- Results go to `results/` next to this file (one CSV per run, the WAV recording, and `summary.csv`).

## The runs (about 45 min; the first block alone already answers most of it)

Defaults: 6 countdowns of 5 beeps, 500 ms apart, one countdown every 8 s (≈ 1 min per run). Set **Condition** to match, so each row says what was going on.

| # | Engine | Condition | How |
|---|---|---|---|
| 1–3 | A, B, C | Idle | Nothing else open. "Keep awake" off. |
| 4–6 | A, B, C | YouTube playing during the run | Video playing quietly the whole run. |
| 7–9 | A, B, C | YouTube played just before | Play a video ~10 s, pause it, start the run right away. |
| 10–12 | A, B, C | Window hidden | Start the run, then cover BeepLab with another full window (or minimise it). |
| 13–15 | A, B, C | Long silence (30 s gaps) | "Countdown every" = 30 s, 4 countdowns. |
| 16–18 | A, B, C | Battery + Low Power Mode | Unplugged, Low Power Mode on. For A, do it twice: Keep awake off, then on. |
| 19–21 | A, B, C | Bluetooth output | AirPods (or a speaker) as output, placed right next to the Mac's mic. |
| 22–23 | Listen | Idle, then YouTube playing | In EonTimer, a timer with 5 beeps 500 ms apart (e.g. Custom); BeepLab listens 60 s while you start it a few times. Set "Sound" to EonTimer's sound. |

Then tell Claude "Phase 0 results are in". Everything needed is in `results/`.

## Reading the table

All values in ms.

- **Jitter σ / Max |err|**: how much the beeps wander around their own average timing. This is what calibration can't fix. Green ≤ 1, orange ≤ 4, red above.
- **1st beep Δ**: how much later the first beep of each countdown comes than the others (the "irregular at first" symptom).
- **Interval σ / max**: the beep-to-beep spacing inside a countdown, compared with the schedule.
- **Latency**: the constant part (speaker + mic + distance). Calibration absorbs it, so it doesn't matter.
- **Drift ppm**: output clock vs microphone clock. A straight-line trend, not jitter.
- **Fired late** (A only): how late EonTimer's thread fired compared with the target, before the sound system adds its own delay.
- **Events**: device changes, underruns, stream slips (the stream stalled, so every later beep is late).

## Files

- `src/Analysis.*`: beep detection (band-pass + envelope correlation) and the metrics. No Qt.
- `src/BeepMixer.*`: the stream engine core (beeps at sample positions, slip detection). No Qt. This becomes PokeTimerPlus's audio engine.
- `src/*Engine.cpp`, `src/Recorder.*`, `src/MainWindow.*`: the app.
- `tests/analysis_test.cpp`: synthetic recordings (room echo, noise, music, Bluetooth-like latency, swallowed beeps, late first beeps) checked against the known truth.
- `tests/reanalyze.cpp`: `build/reanalyze results/<run>` re-runs the analysis on a saved recording.
- `resources/sounds/`: EonTimer's sounds (MIT, see LICENSE-EonTimer.md). `third_party/miniaudio/` (public domain / MIT-0).

Development aids: `BEEPLAB_AUTORUN=0,1,2` runs short tests with those engines and quits (`BEEPLAB_RESULTS=<dir>` for the output); `BEEPLAB_SCREENSHOT=<png>` saves the window and quits.
