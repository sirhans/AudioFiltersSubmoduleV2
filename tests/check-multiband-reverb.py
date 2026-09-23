#!/usr/bin/env python3
"""Build and run the multiband reverb regression tests on macOS (Clang/Accelerate)."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
library = root / 'AudioFilters'
sources = '''BMMultibandReverb.c BMOptimizedReverb.c BMCrossover.c
BMMultiLevelSVF.c BMMultiLevelBiquad.c BMWetDryMixer.c BMSmoothGain.c
BMVectorOps.c BMIntegerMath.c BMGetOSVersion.c'''.split()


def find(name):
    matches = list(library.rglob(name))
    if len(matches) != 1:
        raise RuntimeError(f'Expected one {name}, found {len(matches)}')
    return str(matches[0])


includes = [f'-I{p}' for p in sorted({library, *(p.parent for p in library.rglob('*.h'))})]
with tempfile.TemporaryDirectory(prefix='multiband-reverb-') as temp:
    binary = str(Path(temp) / 'check')
    command = ['clang', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined',
               '-Wno-pass-failed', *includes, str(root / 'tests/multiband_reverb_test.c'),
               *map(find, sources), '-framework', 'Accelerate',
               '-framework', 'CoreFoundation', '-o', binary]
    subprocess.run(command, check=True)
    subprocess.run([binary], check=True)
    # In release builds malformed control values must leave settings unchanged.
    subprocess.run([*command, '-DNDEBUG'], check=True)
    subprocess.run([binary], check=True)
    # Verify that the public header is usable from C++ / Objective-C++ clients.
    subprocess.run(['clang++', '-std=c++17', '-x', 'c++', '-fsyntax-only',
                    *includes, '-'], input='#include "BMMultibandReverb.h"\n',
                   text=True, check=True)
