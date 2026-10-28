"""Assess bounded native-process growth; this is not an allocation leak detector."""
import json
import statistics
import sys
from pathlib import Path


def analyze(samples):
    if len(samples) != 40 or [s['cycle'] for s in samples] != list(range(40)):
        raise ValueError('Expected 40 complete, ordered cycles')
    if len({s['pid'] for s in samples}) != 1:
        raise ValueError('All cycles must run in one process')
    limits = {'privateBytes': (32 * 1024**2, 512 * 1024),
              'handles': (100, 2), 'gdiObjects': (50, 1), 'userObjects': (50, 1)}
    measured = samples[5:]  # Discard five warm-up cycles.
    result = {}
    for key, (growth_limit, slope_limit) in limits.items():
        values = [s[key] for s in measured]
        if any(not isinstance(v, (int, float)) or v < 0 for v in values):
            raise ValueError('Invalid counter: ' + key)
        growth = statistics.median(values[-5:]) - statistics.median(values[:5])
        center = (len(values) - 1) / 2
        mean = statistics.mean(values)
        slope = sum((i-center)*(v-mean) for i, v in enumerate(values)) / sum((i-center)**2 for i in range(len(values)))
        result[key] = dict(growth=growth, slopePerCycle=slope,
                           growthLimit=growth_limit, slopeLimit=slope_limit,
                           suspicious=growth > growth_limit and slope > slope_limit)
    return dict(status='suspicious-growth' if any(v['suspicious'] for v in result.values()) else 'no-growth-flag',
                cycles=40, warmupCycles=5, counters=result,
                scope='Main native process only; excludes QtWebEngine children and GPU memory.',
                interpretation='Heuristic regression screen, not proof of a leak or absence of leaks.')


if __name__ == '__main__':
    try:
        report = analyze([json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()])
    except (ValueError, KeyError, OSError) as error:
        report = dict(status='invalid-measurements', error=str(error))
    Path(sys.argv[2]).write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    sys.exit(0 if report['status'] == 'no-growth-flag' else 1)
