"""Reproducible sign-only sweep. Build /tmp/reverb_sign_test first.

Each row holds the delay layout fixed across all sign candidates.
The 1.75 dB count is a diagnostic, never an acceptance/rejection criterion.
Usage: python3 reverb_sign_study.py /tmp/reverb-sign-results.json
"""
import concurrent.futures
import json
import subprocess
import sys

SCENARIOS = [
    ("default bass", 9, .018, .18, 1.8),
    ("short decay", 9, .018, .18, .1),
    ("medium decay", 9, .018, .18, .7),
    ("long decay", 9, .018, .18, 5),
    ("small room long decay", 9, .001, .02, 1.8),
    ("scaled small room", 9, .0018, .018, .18),
    ("large room", 9, .05, .5, 5),
    ("four units", 4, .018, .18, 1.8),
    ("sixteen units", 16, .018, .18, 1.8),
    ("one unit", 1, .018, .18, 1.8),
    ("maximum decay", 9, .018, .18, 10),
]


def measure(job):
    scenario, seed, high = job
    name, units, low_delay, high_delay, rt60 = scenario
    command = ["/tmp/reverb_sign_test", units, low_delay, high_delay, rt60, seed, high]
    row = json.loads(subprocess.check_output(list(map(str, command)), text=True))
    assert row["after"] <= row["before"] + 1e-7
    assert abs(row["after"] - min(row["candidates"])) < 1e-7
    row.update(name=name, units=units, min_delay=low_delay, max_delay=high_delay,
               rt60=rt60, seed=seed, high_hz=high)
    return row


jobs = [(case, seed, high) for case in SCENARIOS
        for seed in [0x9E3779B9, *range(1, 16)] for high in [50, 300, 2000]]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    rows = list(pool.map(measure, jobs))
with open(sys.argv[1], "w") as output:
    json.dump(rows, output, indent=2)
print("scenario | band | before range | after range | within 1.75 before/after")
for name, *_ in SCENARIOS:
    for high in [50, 300, 2000]:
        group = [r for r in rows if r["name"] == name and r["high_hz"] == high]
        before = [r["before"] for r in group]
        after = [r["after"] for r in group]
        print(f"{name} | 20–{high} | {min(before):.3f}–{max(before):.3f} | "
              f"{min(after):.3f}–{max(after):.3f} | "
              f"{sum(x <= 1.75 for x in before)}/16 → {sum(x <= 1.75 for x in after)}/16")
print(f"{len(rows)} configurations; every selected score is the minimum of all candidates.")
