#!/usr/bin/env python3
"""Build optimized-reverb checks on macOS without the synth application.

Default: ASan/UBSan control and bass-selection tests, including release checks.
--thread-sanitizer: test concurrent parameter/configuration publication.
--benchmark: compare the block processor with scalar/BMReverb references and time it.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
mode = parser.add_mutually_exclusive_group()
mode.add_argument('--thread-sanitizer', action='store_true')
mode.add_argument('--benchmark', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
library = root/'AudioFilters'
includes = [f'-I{p}' for p in sorted({library, *(p.parent for p in library.rglob('*.h'))})]


def source(name):
    matches = list(library.rglob(name))
    if len(matches) != 1:
        raise RuntimeError(f'Expected one {name}, found {len(matches)}')
    return str(matches[0])


with tempfile.TemporaryDirectory(prefix='optimized-reverb-') as folder:
    binary = str(Path(folder)/'check')

    def run(test, sources, flags):
        command = ['clang', '-std=c11', *flags, *includes, str(root/'tests'/test),
                   *map(source, sources), '-framework', 'Accelerate',
                   '-framework', 'CoreFoundation', '-o', binary]
        subprocess.run(command, check=True)
        subprocess.run([binary], check=True)

    core = ['BMOptimizedReverb.c']
    if args.thread_sanitizer:
        run('reverb_control_test.c', core, ['-O1', '-g', '-fsanitize=thread'])
    elif args.benchmark:
        sources = core + '''BMReverb.c BMFirstOrderArray.c BMMultiLevelSVF.c
        BMWetDryMixer.c BMStereoWidener.c BMSorting.c BMMultiLevelBiquad.c
        BMSmoothGain.c BMGetOSVersion.c'''.split()
        run('reverb_bench.c', sources, ['-O3', '-DNDEBUG'])
    else:
        flags = ['-O1', '-g', '-fsanitize=address,undefined']
        run('reverb_control_test.c', core, flags)
        run('reverb_control_test.c', core, [*flags, '-DNDEBUG'])
        run('reverb_sign_test.c', core+['BMOptimizedReverbBass.c'], flags)
        subprocess.run(['clang++', '-std=c++17', '-x', 'c++', '-fsyntax-only',
                        *includes, '-'], input='#include "BMOptimizedReverbBass.h"\n',
                       text=True, check=True)
