# BMOptimizedReverb

A stereo, wet-only feedback delay network with four delays per unit. The default
is nine units; `BMOptimizedReverb_initWithNumDelayUnits` accepts any feasible
positive count, including odd and even counts. Unit count, sample rate and
allocation capacity are fixed at initialization. The caller supplies wet/dry
mixing and tone shaping.

## Lifecycle and parameter updates

Check the Boolean result of initialization. Allocation failure returns false;
invalid arguments assert in debug builds and return false in release. After a
failed initialization, only free or another initialization is valid. The default
delay range must fit the supplied capacity and sample rate.

The RT60 and delay-time setters publish atomic pending values. Only the audio
thread changes the active network, at the beginning of `process` (including
zero-length calls). RT60 changes retain the tail. Delay changes redraw the lines
and clear both their storage and pending feedback. Separate setters are not a
transaction; the minimum/maximum delay pair itself is always coherent.

`setConfiguration` publishes delay limits, RT60, seed and output-sign pattern as
one request. Its three-slot mailbox has one control producer and one audio
consumer. Each owns a slot and exchanges the third using acquire/release atomics;
neither waits or allocates. Latest request wins. Complete requests take precedence
over legacy requests pending at the same process boundary, and clear the tail.
Use complete requests when applying a measured bass configuration.

Invalid complete configurations return false in every build without replacing a
pending valid request. `init`, `free`, `clear` and the test helper require exclusive
access. Do not copy an initialized struct. Changes may click; applications needing
smooth transitions can crossfade between two instances.

`BMMultibandReverb_init` also returns a Boolean, and its wrapper forwards complete
configurations through `BMMultibandReverb_setConfiguration`. The wrapper tracks
requested delay bounds so consecutive individual-bound setters compose correctly
before the audio thread applies them. Follow the wrapper's serialization contract.

## Unique delay lengths without retries

Truncate the delay limits to integer sample positions. Require a minimum of two
samples and at least `4 * numDelayUnits` positions in the inclusive range. Invalid
ranges are rejected, never widened.

Keep the endpoints. Divide the remaining lower half into disjoint integer
intervals, draw once in each, then reflect those positions into the upper half.
Disjoint intervals guarantee uniqueness and bounds; reflection preserves the
exact midpoint mean. No retries, sorting or repair pass are needed. Alternate
sorted lengths between channels and shuffle within each channel. A local RNG
makes layouts deterministic without affecting process-wide randomness.

## Select the best of sixteen output-sign patterns

The optional `BMOptimizedReverbBass.c/.h` analyzer requires Accelerate. It evaluates
all **16** balanced patterns at fixed delay lengths, routing and decay gains.
Eight arrangements are paired with their sign inverses. Inverting signs negates
the impulse response without changing wet power, so eight impulse renders and
FFT pairs provide all sixteen scores.

The score is the worst absolute deviation of average passband energy from 0 dB
over every steady wet gain `w` from zero to 0.5, assuming dry gain `sqrt(1-w*w)`.
Measure a mono impulse into both input channels. Average linear power across
both outputs and uniformly spaced FFT bins between the supplied frequency bounds,
then convert to dB. Include the coherent dry contribution: if `A = mean(Re(H))`
and `B = mean(|H|²)`, mixed power is

```
1 + w*w*(B - 1) + 2*w*sqrt(1 - w*w)*A
```

Substituting `w = sin(theta)` gives a sinusoid whose endpoints and stationary
points give the extrema without a wet-value grid. DC is a diagnostic, not the
selection criterion. The closest candidate always wins; exact ties select the
first. There is no gain correction, threshold-driven retry, rejection for missing
±1.75 dB, or minimum-decay restriction. Small unit counts can yield repeated sign
patterns; work is still bounded at sixteen scores.

Run analysis on a control/worker thread. It reads only the live instance's
immutable metadata and renders into a private reverb. Renders last at least two
seconds and two RT60 periods plus maximum delay, rounded up to a power of two.
The cap is 2^23 samples (96 MiB of FFT buffers plus the private network).
Invalid inputs, resource failure or an unsupported analysis size return false
without changing the live instance or result. The measured response is a sampled
band-average estimate, not a bound on individual frequencies, arbitrary stereo
material, parameter-change transients, or an application's complete crossover/EQ
chain. Remeasure after delay, RT60 or passband changes.

```c
BMOptimizedReverbConfiguration request = {
    .minDelay_seconds = .018f, .maxDelay_seconds = .18f, .rt60 = 1.8f,
    .delaySeed = BMOR_DEFAULT_DELAY_SEED, .signPattern = 0
};
BMOptimizedReverbBassResult result;
if (BMOptimizedReverb_selectBassConfiguration(&reverb, &request, 20, 300, &result))
    request = result.configuration;
// On analysis failure, this example retains the requested settings with default signs.
bool published = BMOptimizedReverb_setConfiguration(&reverb, &request);
```

## Candidate-count measurements

Apple M1 Pro, 16 GB, native arm64, clang `-O3 -DNDEBUG`, 48 kHz. Timings are
medians of nine complete selections after a warm-up, including temporary
allocations, FFT setup, renders and cleanup. Each larger budget contains the
same initial candidates. Deviation is the worst band-average score across sixteen
layouts, including dry and every wet gain from zero through 0.5.

For nine units, 18–180 ms delays and RT60 1.8 seconds:

| Candidates | Time, 20–300 Hz | Deviation, 20–300 Hz | Deviation, 20–50 Hz |
|---:|---:|---:|---:|
| 8 | 27.55 ms | 0.02070 dB | 0.12285 dB |
| 16 | 41.03 ms | 0.01742 dB | 0.06940 dB |
| 32 | 68.42 ms | 0.00983 dB | 0.06940 dB |
| 64 | 125.10 ms | 0.00563 dB | 0.03744 dB |

For 1.8–18 ms delays, RT60 0.18 seconds and 20–50 Hz, the worst scores were
0.437, 0.269, 0.216 and 0.165 dB respectively. At RT60 10 seconds with default
delays, costs rose to 116, 190, 343 and 632 ms, while worst 20–300 Hz deviation
only improved from 3.938 to 3.804 dB. More signs cannot reliably remove excess
energy from long decays in short delays. Sixteen was selected as the default
compromise between narrow-band improvement and control-update latency.

[Raw results](tests/reverb_candidate_results.json) include ten scenarios, 160
layout/band configurations, timings, all candidate scores, original source hashes
and double-length FFT checks. In the checked default/worst layouts, doubling FFT
length changed selected scores by at most 0.00085 dB in the default 20–300 Hz case,
0.00378 dB in default 20–50 Hz, and 0.01098 dB in the short-delay 20–50 Hz case.
Timings are local measurements, not performance guarantees.

## Verification

From the library root, on macOS:

```sh
python3 tests/check-optimized-reverb.py
python3 tests/check-optimized-reverb.py --thread-sanitizer
python3 tests/check-optimized-reverb.py --benchmark
python3 tests/check-multiband-reverb.py
python3 tests/reverb_candidate_benchmark.py /tmp/reverb-candidate-results.json
```

The checks cover 14,000 delay distributions, exact means and distinct lengths,
parameter changes, concurrent complete requests, unchanged layouts across signs,
balanced/inverse signs, explicit dry-impulse mixing, double-length FFTs, all
sixteen scores, and successful selection outside the diagnostic tolerance.
The benchmark compares block processing against scalar and BMReverb references,
including configurable unit counts, arbitrary buffer sizes and in-place use.

For additional response measurements or the broad scenario sweep:

```sh
AF=AudioFilters/DelayAndReverb
clang -std=c11 -O2 -I"$AF" tests/reverb_sign_test.c \
  "$AF"/BMOptimizedReverb.c "$AF"/BMOptimizedReverbBass.c \
  -framework Accelerate -o /tmp/reverb_sign_test
python3 tests/reverb_sign_study.py /tmp/reverb-sign-results.json
clang -O2 -I"$AF" tests/reverb_bass_test.c "$AF"/BMOptimizedReverb.c \
  -framework Accelerate -o /tmp/reverb_bass_test
```
