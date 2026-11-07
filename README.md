<p align="center"><img src="Form/Images/Icon/poketimerplus_256x256.png" width="128" alt="PokeTimerPlus logo"></p>

# PokeTimerPlus

**An all-in-one RNG timer for Pokémon on real hardware: [EonTimer](https://github.com/DasAmpharos/EonTimer)'s timers and calibration, [FlowTimer](https://github.com/stringflow/FlowTimer)'s flowing targets, and beeps that land when they should.** Made by Vict0rious7, with the look of [PokeFinderPlus](https://github.com/Vict0rious7/PokeFinderPlus). Work in progress; report bugs in [the issues](https://github.com/Vict0rious7/PokeTimerPlus/issues).

## What's different from EonTimer and FlowTimer

**Beeps on time**
- Beeps are written into an audio stream that stays open from start-up to quit (miniaudio), at exact sample positions, instead of starting a sound when each beep is due. Measured on a MacBook Air: every beep within 0.2 ms, including the first beep after a silence, on battery with Low Power Mode, with YouTube playing, and on a Bluetooth speaker. EonTimer's first beep of each countdown came 6–21 ms late there, and the Bluetooth speaker swallowed it 5 times out of 6.
- Output stalls and drift between the sound card's clock and the system clock are detected and corrected (only between countdowns, so spacing inside a countdown stays exact).
- Choose the audio output in Settings; the app tells you when the system output changes.
- No network access at all, so nothing can fail or crash when the computer is offline.

**Organised by console and RNG type**
- Tabs GB/GBA, DS, 3DS and Switch, each listing its RNG types, instead of Gen 3 / 4 / 5 tabs. Every tab has a Custom timer.
- GB/GBA: RSE Wild, RSE Static, RSE Fishing, Emerald Egg and RS Egg (Held and Pickup sections, each with its own calibration; Emerald notes say which part decides what), FRLG Static, FRLG Wild, FRLG Egg (Held / Pickup). DS: DPPt Static, DPPt Wild, BW / B2W2 (Standard, C-Gear, Entralink, Entralink+).
- **FRLG timer:** seed time (ms) plus target advance, each with its own calibration (ms and advances), matching how FRLG RNG is calibrated. Pairs with PokeFinderPlus' FRLG initial seed results.
- "Played on" per console: GBA, GBA game on DS (slot 2), open_agb_firm, Game Boy Interface, Switch FR/LG, DS, 3DS native, 60 / 30 fps, or any custom frame rate. Rates not yet confirmed on hardware are marked.

**Saved calibrations**
- **Save to profile** keeps the calibration of each RNG type (and egg section). It pre-fills the field next time, so fewer tries go into calibrating; the panel says whether the value is saved, changed since, or not saved yet.
- "Hit" fields on every timer, Custom included: type what you got, Update corrects the calibration.

**Flowing targets (FlowTimer's variable offset)**
- Custom, continuous mode: start the clock, then type each target (frames or ms) when you know it. Beeps end on it. −1 / +1 frame and Remove last while it runs, a calibration applied to every target, and Hit on the last one.
- Gen 3 Variable Target: type the frame while the timer runs.

**Comfort**
- Beep count, interval and sound, and an optional screen flash on each beep.
- PokeFinderPlus look (Fusion, Auto / Dark / Light).
- Decimals always use a dot, whatever the system region.

## Download

No release yet: builds of every push are on the [Actions page](https://github.com/Vict0rious7/PokeTimerPlus/actions) (open the latest green run → Artifacts, needs a GitHub login), or build it yourself.

## Build on a Mac

With [Homebrew](https://brew.sh) installed, from this folder:

```sh
./build-mac.sh           # installs Qt, CMake and Ninja if missing, builds, opens PokeTimerPlus.app
./build-mac.sh --test    # same, and runs the tests first
./build-mac.sh --clean   # start again from an empty build folder
```

Windows and Linux: Qt 6.8 or newer, CMake and Ninja, then `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` and `cmake --build build` (the CI workflow in `.github/workflows/ci.yaml` does exactly this on all three systems).

Saved calibrations: `~/Library/Application Support/PokeTimerPlus/PokeTimerPlus/calibrations.json`.

## Code layout (same as PokeFinder)

- `Core/` (no Qt)
  - `Timer/`: frame rates, EonTimer's timer maths, beep schedule (with flowing targets).
  - `Audio/`: the output stream (miniaudio, always open) and `BeepMixer` (beeps at exact sample positions, stalls and clock drift corrected).
  - `Timer/RngTypes`: the RNG types per console tab (GB/GBA, DS, 3DS, Switch) and the frame rates each console offers.
  - `Profile/`: saved calibrations ("Save to profile"), one per RNG type / egg section, JSON.
- `Form/`: the window (Qt Widgets, Fusion, like PokeFinderPlus): console tabs, RNG list, one panel per timer template, timer; EonTimer's beep sounds.
- `Test/`: Qt Test suite. `Resources/eontimer_golden.json` = what EonTimer itself returns on 3,420 cases (made by `Tools/eontimer_golden.py`).
- `Phase0-BeepLab/`: the Phase 0 measurement tool (separate CMake project, see its README). Its Listen mode can time PokeTimerPlus too.
- `LICENSES/`: third-party notices (EonTimer MIT; miniaudio's licence is in `Core/External/miniaudio`).

## Development aids

`POKETIMERPLUS_SCREENSHOT=<png>` saves the window and quits; `POKETIMERPLUS_AUTOSTART=<ms>` clicks Start after that delay.

## Licence and credits

GPL-3.0 (see `LICENSE`), like PokeFinderPlus.

- Timer maths: port of [EonTimer](https://github.com/DasAmpharos/EonTimer) by DasAmpharos (MIT, notice in `LICENSES/`), checked against EonTimer's own output on 3,420 cases. EonTimer's beep sounds.
- Flowing targets and the always-open audio stream: ideas from [FlowTimer](https://github.com/stringflow/FlowTimer) by stringflow (no code copied).
- Stall and drift correction: approach of [Mystic Timer](https://github.com/zaksabeast/PokemonRNGGuides) (pokemonrng.com).
- Audio: [miniaudio](https://miniaud.io) (licence in `Core/External/miniaudio/LICENSE`).
