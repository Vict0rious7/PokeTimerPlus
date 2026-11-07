"""Reference data for PokeTimerPlus' port of EonTimer's timer maths.

Runs the real EonTimer 3.x desktop code (Python, MIT, DasAmpharos) on many inputs and writes what
it returns, so the C++ port can be checked number for number (Test/Resources/eontimer_golden.json).

Usage (needs PySide6-Essentials, platformdirs, pillow, requests, libsass in the venv):
    QT_QPA_PLATFORM=offscreen python eontimer_golden.py <EonTimer 3.x-python source dir> <out.json>

One deliberate difference from EonTimer: its SecondTimer always pads to the default 14 s minimum
length, while the delay timers use the configurable one (the web rewrite fixed this). The port uses
the configurable minimum everywhere, so the second timer is wrapped the same way here.
"""

import json
import os
import random
import sys
import tempfile

sys.path.insert(0, sys.argv[1])

from PySide6.QtCore import QCoreApplication, QSettings  # noqa: E402

from eon_timer import timers  # noqa: E402
from eon_timer.settings.timer.model import Console, TimerSettingsModel  # noqa: E402
from eon_timer.timers import Calibrator, DelayTimer, EnhancedEntralinkTimer, EntralinkTimer, SecondTimer  # noqa: E402
from eon_timer.timers.custom.custom_phase import CustomPhase  # noqa: E402
from eon_timer.timers.custom.model import CustomTimerModel  # noqa: E402
from eon_timer.timers.custom.timer import CustomTimer  # noqa: E402
from eon_timer.timers.frame_timer import FrameTimer, VariableFrameTimer  # noqa: E402
from eon_timer.timers.gen3.model import Gen3Mode, Gen3Model  # noqa: E402
from eon_timer.timers.gen3.timer import Gen3Timer  # noqa: E402
from eon_timer.timers.gen4.model import Gen4Model  # noqa: E402
from eon_timer.timers.gen4.timer import Gen4Timer  # noqa: E402
from eon_timer.timers.gen5.model import Gen5Mode, Gen5Model  # noqa: E402
from eon_timer.timers.gen5.timer import Gen5Timer  # noqa: E402

app = QCoreApplication([])
rng = random.Random(20261104)


class ConfigurableSecondTimer(SecondTimer):
    def __init__(self, calibrator):
        self.calibrator = calibrator

    def create(self, target_second, calibration):
        return [timers.to_minimum_length(target_second * 1000 + calibration + 200, self.calibrator.minimum_length)]


def fresh_settings():
    path = tempfile.NamedTemporaryFile(suffix='.ini', delete=False).name
    os.unlink(path)
    return QSettings(path, QSettings.Format.IniFormat)


def num(x):
    if x == float('inf'):
        return 'inf'
    return float(x) if isinstance(x, float) else x


def phases(ps):
    return [num(p) for p in ps]


CONFIGS = [
    # console, custom fps (only for Custom), precision calibration, minimum length (s)
    (Console.GBA, 60.0, False, 14),
    (Console.NDS_SLOT1, 60.0, False, 14),
    (Console.NDS_SLOT2, 60.0, False, 14),
    (Console.NDS_SLOT1, 60.0, True, 14),
    (Console.GBA, 60.0, False, 20),
    (Console.CUSTOM, 59.94, False, 14),
    (Console.CUSTOM, 119.445, True, 14),
    # 0.5 and 2 ms per frame: exact .5 ties, so round-half-to-even is actually exercised
    (Console.CUSTOM, 2000.0, False, 14),
    (Console.CUSTOM, 500.0, True, 14),
]
CONSOLE_FPS = {Console.GBA: 59.7275, Console.NDS_SLOT1: 59.8261, Console.NDS_SLOT2: 59.6555}

out = {'source': 'EonTimer 3.x-python', 'configs': []}

for console, custom_fps, precision, min_len in CONFIGS:
    s = fresh_settings()
    ts = TimerSettingsModel(s)
    ts.console.set(console)
    ts.custom_framerate.set(custom_fps)
    ts.precision_calibration.set(precision)
    ts.minimum_length.set(min_len)
    cal = Calibrator(ts)
    second = ConfigurableSecondTimer(cal)
    delay = DelayTimer(cal, second)
    frame = FrameTimer(cal)
    variable = VariableFrameTimer(frame)
    entralink = EntralinkTimer(delay)
    enhanced = EnhancedEntralinkTimer(entralink)
    gen3 = Gen3Timer(frame, variable)
    gen4 = Gen4Timer(cal, delay)
    gen5 = Gen5Timer(cal, delay, second, entralink, enhanced)
    custom = CustomTimer(cal)

    cfg = {
        'fps': CONSOLE_FPS.get(console, custom_fps),
        'precision': precision,
        'minimumLength': min_len,
        'calibrator': [],
        'second': [],
        'delay': [],
        'frame': [],
        'entralink': [],
        'enhanced': [],
        'gen3': [],
        'gen4': [],
        'gen5': [],
        'custom': [],
    }

    for _ in range(60):
        ms = rng.choice([rng.uniform(-5000, 60000), rng.randint(-2000, 2000) + 0.5, rng.randint(0, 100000), rng.randint(-50, 50)])
        delays = rng.randint(-3000, 20000)
        seconds = rng.randint(0, 59) + rng.choice([0, 0.5, rng.random()])
        cfg['calibrator'].append({
            'ms': ms, 'delays': delays, 'seconds': seconds,
            'toDelays': cal.to_delays(ms),
            'toMilliseconds': num(cal.to_milliseconds(delays)),
            'calibrateToDelays': cal.calibrate_to_delays(ms),
            'calibrateToMilliseconds': num(cal.calibrate_to_milliseconds(delays)),
            'createCalibration': num(cal.create_calibration(delays, seconds)),
        })

    for _ in range(40):
        t, h = rng.randint(0, 59), rng.randint(0, 59)
        c = rng.choice([0.0, rng.uniform(-3000, 3000), float(rng.randint(-200, 200))])
        cfg['second'].append({'target': t, 'hit': h, 'calibration': c,
                              'phases': phases(second.create(t, c)), 'calibrate': num(second.calibrate(t, h))})

    for _ in range(40):
        d, sec = rng.randint(0, 20000), rng.randint(0, 59)
        hit = d + rng.choice([0, rng.randint(-10, 10), rng.randint(-400, 400)])
        c = rng.choice([0.0, rng.uniform(-3000, 3000)])
        cfg['delay'].append({'target': d, 'second': sec, 'hit': hit, 'calibration': c,
                             'phases': phases(delay.create(d, sec, c)), 'calibrate': num(delay.calibrate(d, hit))})

    for _ in range(40):
        pre, f = rng.randint(0, 20000), rng.randint(0, 200000)
        hit = f + rng.randint(-300, 300)
        c = rng.choice([0.0, rng.uniform(-500, 500)])
        cfg['frame'].append({'preTimer': pre, 'target': f, 'hit': hit, 'calibration': c,
                             'phases': phases(frame.create(pre, f, c)), 'calibrate': num(frame.calibrate(f, hit)),
                             'variable': phases(variable.create(pre))})

    for _ in range(30):
        d, sec = rng.randint(0, 20000), rng.randint(0, 59)
        hit = d + rng.randint(-300, 300)
        c, ec = rng.uniform(-2000, 2000), rng.uniform(-500, 500)
        adv, advHit = rng.randint(0, 2000), rng.randint(0, 2000)
        fc = rng.randint(-500, 500)
        cfg['entralink'].append({'target': d, 'second': sec, 'hit': hit, 'calibration': c, 'entralinkCalibration': ec,
                                 'phases': phases(entralink.create(d, sec, c, ec)),
                                 'calibrate': num(entralink.calibrate(d, hit))})
        cfg['enhanced'].append({'target': d, 'second': sec, 'advances': adv, 'advancesHit': advHit, 'calibration': c,
                                'entralinkCalibration': ec, 'frameCalibration': fc,
                                'phases': phases(enhanced.create(d, sec, adv, c, ec, fc)),
                                'calibrate': num(enhanced.calibrate(adv, advHit))})

    for _ in range(30):
        m = Gen3Model(fresh_settings())
        m.mode.set(rng.choice([Gen3Mode.STANDARD, Gen3Mode.STANDARD, Gen3Mode.VARIABLE_TARGET]))
        m.pre_timer.set(rng.randint(0, 20000))
        m.target_frame.set(rng.randint(0, 100000))
        m.calibration.set(rng.choice([0.0, rng.uniform(-300, 300)]))
        hit = m.target_frame.get() + rng.randint(-200, 200)
        case = {'mode': str(m.mode.get()), 'preTimer': m.pre_timer.get(), 'targetFrame': m.target_frame.get(),
                'calibration': m.calibration.get(), 'frameHit': hit, 'phases': phases(gen3.create(m))}
        m.frame_hit.set(hit)
        gen3.calibrate(m)
        case['calibrationAfter'] = num(m.calibration.get())
        cfg['gen3'].append(case)

    for _ in range(30):
        m = Gen4Model(fresh_settings())
        m.target_delay.set(rng.randint(400, 20000))
        m.target_second.set(rng.randint(0, 59))
        m.calibrated_delay.set(rng.randint(300, 900))
        m.calibrated_second.set(rng.randint(10, 20))
        hit = rng.choice([0, m.target_delay.get() + rng.randint(-400, 400)])
        case = {'targetDelay': m.target_delay.get(), 'targetSecond': m.target_second.get(),
                'calibratedDelay': m.calibrated_delay.get(), 'calibratedSecond': m.calibrated_second.get(),
                'delayHit': hit, 'calibration': num(gen4.get_calibration(m)), 'phases': phases(gen4.create(m)),
                'minutesBefore': gen4.minutes_before_target(m)}
        m.delay_hit.set(hit)
        gen4.calibrate(m)
        case['calibratedDelayAfter'] = m.calibrated_delay.get()
        cfg['gen4'].append(case)

    for _ in range(60):
        m = Gen5Model(fresh_settings())
        m.mode.set(rng.choice(list(Gen5Mode)))
        m.calibration.set(rng.randint(-300, 300))
        m.frame_calibration.set(rng.randint(-300, 300))
        m.entralink_calibration.set(rng.randint(0, 600))
        m.target_delay.set(rng.randint(400, 20000))
        m.target_second.set(rng.randint(0, 59))
        m.target_advances.set(rng.randint(0, 2000))
        dh = rng.choice([None, m.target_delay.get(), m.target_delay.get() + rng.randint(-300, 300)])
        sh = rng.choice([None, m.target_second.get(), rng.randint(0, 59)])
        ah = rng.choice([None, m.target_advances.get(), m.target_advances.get() + rng.randint(-30, 30)])
        case = {'mode': str(m.mode.get()), 'calibration': m.calibration.get(),
                'frameCalibration': m.frame_calibration.get(), 'entralinkCalibration': m.entralink_calibration.get(),
                'targetDelay': m.target_delay.get(), 'targetSecond': m.target_second.get(),
                'targetAdvances': m.target_advances.get(), 'delayHit': dh, 'secondHit': sh, 'advancesHit': ah,
                'phases': phases(gen5.create(m)), 'minutesBefore': gen5.minutes_before_target(m)}
        m.delay_hit.set(dh)
        m.second_hit.set(sh)
        m.advances_hit.set(ah)
        gen5.calibrate(m)
        case['calibrationAfter'] = num(m.calibration.get())
        case['entralinkCalibrationAfter'] = num(m.entralink_calibration.get())
        case['frameCalibrationAfter'] = num(m.frame_calibration.get())
        cfg['gen5'].append(case)

    for _ in range(20):
        m = CustomTimerModel(fresh_settings())
        m.reset()
        items = []
        for _ in range(rng.randint(1, 4)):
            unit = rng.choice(list(CustomPhase.Unit))
            value = rng.randint(0, 50000)
            c = rng.choice([0.0, rng.uniform(-200, 200)])
            m.append(CustomPhase(unit, value, c))
            items.append({'unit': str(unit), 'value': value, 'calibration': c})
        cfg['custom'].append({'phases': items, 'result': phases(custom.create(m))})

    out['configs'].append(cfg)

with open(sys.argv[2], 'w') as f:
    json.dump(out, f, separators=(',', ':'))
print('wrote', sys.argv[2], sum(len(v) for c in out['configs'] for v in c.values() if isinstance(v, list)), 'cases')
