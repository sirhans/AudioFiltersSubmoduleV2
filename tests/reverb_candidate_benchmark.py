"""Measure the time/quality tradeoff without changing production's candidate count.

Usage: python3 tests/reverb_candidate_benchmark.py /tmp/reverb-candidate-results.json
Requires macOS, clang and Accelerate. Runs measurements sequentially so other
benchmark jobs do not contend for CPU or memory bandwidth.
"""
import hashlib
import json
import re
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
AF = ROOT / "AudioFilters/DelayAndReverb"
COUNTS = (8, 16, 32, 64)
FILES = ("BMOptimizedReverb.c", "BMOptimizedReverb.h",
         "BMOptimizedReverbBass.c", "BMOptimizedReverbBass.h")
FLAGS = ["-std=c11", "-O3", "-DNDEBUG"]
SEEDS = [0x9E3779B9, *range(1, 16)]
# name, units, min/max seconds, RT60 seconds, upper band Hz, sample rate
CASES = [
    ("default_300", 9, .018, .18, 1.8, 300, 48000),
    ("default_50", 9, .018, .18, 1.8, 50, 48000),
    ("small_50", 9, .0018, .018, .18, 50, 48000),
    ("short_decay", 9, .018, .18, .1, 300, 48000),
    ("long_decay", 9, .018, .18, 5, 300, 48000),
    ("maximum_decay", 9, .018, .18, 10, 300, 48000),
    ("small_room_long_decay", 9, .001, .02, 1.8, 300, 48000),
    ("four_units", 4, .018, .18, 1.8, 50, 48000),
    ("sixteen_units", 16, .018, .18, 1.8, 50, 48000),
    ("default_96k", 9, .018, .18, 1.8, 300, 96000),
]


def measure(binary, case, seed, repeats):
    name, units, low, high, rt60, cutoff, fs = case
    args = [binary, units, low, high, rt60, seed, cutoff, fs, repeats]
    row = json.loads(subprocess.check_output(list(map(str, args)), text=True))
    row.update(case=name, seed=seed)
    assert abs(min(row["scores_db"]) - max(abs(row["low_db"]), abs(row["high_db"]))) < 1e-8
    return row


report = {"compiler_flags": FLAGS, "counts": COUNTS, "cases": CASES,
          "source_sha256": {name: hashlib.sha256((AF/name).read_bytes()).hexdigest()
                            for name in FILES}, "timings": [], "quality": []}
with tempfile.TemporaryDirectory(prefix="bmor-candidates-", dir="/tmp") as scratch:
    scratch = Path(scratch)
    binaries = {}
    for count in COUNTS:
        folder = scratch/str(count)
        folder.mkdir()
        for name in FILES:
            text = (AF/name).read_text()
            if name == "BMOptimizedReverb.h":
                text, replacements = re.subn(r"(?m)^#define BMOR_NUM_SIGN_PATTERNS \d+$",
                                             f"#define BMOR_NUM_SIGN_PATTERNS {count}", text)
                assert replacements == 1
            (folder/name).write_text(text)
        binary = folder/"benchmark"
        subprocess.run(["clang", *FLAGS, "-I", str(folder),
                        str(TESTS/"reverb_candidate_benchmark.c"),
                        str(folder/"BMOptimizedReverb.c"), str(folder/"BMOptimizedReverbBass.c"),
                        "-framework", "Accelerate", "-o", str(binary)], check=True)
        binaries[count] = binary

    # Nine timed complete selections for every budget and case. Rotate the
    # budget order between cases to reduce ordering bias from temperature/load.
    for index, case in enumerate(CASES):
        shift = index % len(COUNTS)
        for count in COUNTS[shift:] + COUNTS[:shift]:
            row = measure(binaries[count], case, SEEDS[0], 9)
            report["timings"].append(row)
            print(f"timing {case[0]} n={count}: median {statistics.median(row['times_ms']):.2f} ms", flush=True)
        # Each larger budget must contain exactly the smaller budget's scores.
        rows = {r["count"]: r for r in report["timings"] if r["case"] == case[0]}
        for count in COUNTS:
            assert rows[count]["scores_db"] == rows[64]["scores_db"][:count]

    # One 64-candidate evaluation supplies nested 8/16/32/64 quality results.
    # Compare sixteen fixed layouts per scenario, rather than selecting a lucky seed.
    for case in CASES:
        for seed in SEEDS:
            report["quality"].append(measure(binaries[64], case, seed, 1))
        print(f"quality {case[0]}: 16 layouts complete", flush=True)

Path(sys.argv[1]).write_text(json.dumps(report, indent=2) + "\n")
print("\nCase | candidates | median ms | median / worst deviation across 16 layouts (dB)")
for case in CASES:
    for count in COUNTS:
        timing = next(r for r in report["timings"] if r["case"] == case[0] and r["count"] == count)
        scores = [min(r["scores_db"][:count]) for r in report["quality"] if r["case"] == case[0]]
        print(f"{case[0]} | {count} | {statistics.median(timing['times_ms']):.2f} | "
              f"{statistics.median(scores):.5f} / {max(scores):.5f}")
