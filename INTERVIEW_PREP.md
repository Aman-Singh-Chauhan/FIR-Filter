# Interview Preparation

This guide supports a short, defensible explanation of the project. First run the program and understand the three-tap dry run. Then explain the 51-tap demo using the numbers from your own run.

## Part A: Explain the project

### A 30-second introduction

> I built a C++17 signal-processing engine that removes a 400 Hz interference tone while preserving a 50 Hz signal. I designed a 51-tap low-pass FIR filter using a windowed sinc and a Hamming window. I implemented both direct convolution and a circular-buffer version, checked that their outputs match, measured tone attenuation, and benchmarked processing time. The sample-processing path uses fixed history storage and performs no dynamic allocation. It is a desktop simulation that demonstrates techniques useful in embedded DSP.

### A two-minute explanation

> My test signal combines a 50 Hz sine wave of amplitude 1 with a 400 Hz sine wave of amplitude 0.5. I generate 2000 samples at 2000 Hz, which represents one second. The goal is to retain the low-frequency signal and suppress the higher-frequency interference.
>
> I design a 51-tap low-pass filter with a 150 Hz cutoff. I sample the ideal sinc impulse response, apply a Hamming window, and divide the coefficients by their sum to obtain unity DC gain. The finite filter has a transition band, so I calculate its actual frequency response rather than assume an exact attenuation at the cutoff.
>
> For each new input, the filter stores it in a circular buffer and multiplies the newest sample and its history by the coefficients. It adds those products to produce one output. Wrapping indices lets the filter reuse the same memory without shifting the full history. Each sample requires work proportional to the number of taps and does not allocate memory.
>
> I compare its output with a direct-convolution reference. I also estimate the amplitudes of both tones using sine and cosine correlation after startup. Combining both correlations handles phase delay. I compare those measured gains with the response calculated directly from the coefficients.
>
> Finally, I run automated tests, export CSV files, draw plots using optional Python, and measure repeated filtering with `std::chrono`. The symmetric coefficients introduce 25 samples, or 12.5 milliseconds, of group delay. Desktop benchmark results show throughput on this machine; they do not prove a microcontroller deadline. An embedded port would need target-specific acquisition and timing validation.

Insert your actual measured gains into this explanation if asked for numerical results. Do not memorize benchmark numbers as universal performance claims.

### End-to-end workflow

1. Choose sampling rate, sample count, frequencies, amplitudes, cutoff, and tap count.
2. Validate parameters and generate the input signal in C++.
3. Calculate and normalize the FIR coefficients.
4. Filter the same input using the direct reference and circular buffer.
5. Compare every output sample within a small numerical tolerance.
6. Exclude startup samples and estimate the 50 Hz and 400 Hz amplitudes.
7. Compare measured amplitude ratios with the calculated frequency response.
8. Benchmark repeated filtering on a larger workload and report memory estimates.
9. Export signals, coefficients, and response to CSV; optionally plot them.

### File and function walkthrough

| File | Important code and its purpose |
| --- | --- |
| `include/fir_filter.h` | Declares `dsp::FIRFilter` and `referenceFIR()`. Shows the public interface and private state. |
| `src/fir_filter.cpp` | Constructor owns coefficients and allocates zero history; `processSample()` writes one sample and accumulates convolution; `processBlock()` calls the sample path for each input; `reset()` clears history and restores the write index; `coefficients()` exposes read-only inspection; `referenceFIR()` uses direct indexing. |
| `include/filter_design.h` | Declares `designLowPass()` and `frequencyResponse()`. |
| `src/filter_design.cpp` | Validates the design, handles the sinc center separately, applies Hamming weights, normalizes, and sums complex exponentials for response. |
| `include/signal_generator.h` | Declares `Tone`, `generateSignal()`, and `estimateAmplitude()`. |
| `src/signal_generator.cpp` | Validates and sums configured sine tones; measures a known tone with sine and cosine correlations over a whole-cycle segment. |
| `src/main.cpp` | Coordinates the demo, reference comparison, numerical analysis, benchmark, and CSV writing. |
| `tests/test_fir.cpp` | Checks mathematical behavior and state management through automated tests. |
| `scripts/plot.py` | Reads exported CSVs and draws three graphs with matplotlib. It does not implement DSP. |
| `CMakeLists.txt` | Selects C++17, builds the application/tests, and registers CTest. |

### Major decisions you should defend

| Decision | Reason and tradeoff |
| --- | --- |
| FIR rather than IIR | Direct convolution is easy to verify, and symmetric taps provide linear phase. FIR may require more operations than an IIR meeting similar magnitude requirements. |
| 51 odd taps | An integer center simplifies sinc design and gives 25 samples of delay. This is a manageable teaching example, not an optimized specification for a real modem. |
| Hamming window | Simple formula and lower sidelobes than rectangular truncation, at the cost of a wider transition. |
| `double` | Makes numerical analysis straightforward on a desktop. A target may favor `float` or fixed point. |
| `std::vector` history | RAII ownership and runtime tap count, with one allocation at construction and a fixed size during processing. |
| Circular buffer | Retains exactly the needed history across blocks without shifting it. Index wraparound needs careful testing. |
| Separate reference | Provides an easy-to-read mathematical baseline for catching indexing mistakes. |
| Known-tone correlation | Simple, phase-aware measurement suited to the two coherent test tones. It is not a general spectrum analyzer. |
| CSV plus optional Python | Keeps all engine calculations in C++, while making results easy to inspect and plot. |
| No hardware framework yet | Keeps the core small enough to understand. Hardware acquisition and timing are separate future work. |

## Part B: DSP fundamentals

### 1. What is a digital filter?

A digital filter transforms sampled numerical data to change its frequency content or other characteristics. Here, the filter reduces a high-frequency sinusoid while keeping a low-frequency sinusoid.

### 2. What is FIR?

FIR means finite impulse response. For a filter initially at rest, a single impulse produces a response that ends after finitely many samples. An `M`-tap FIR uses the present input and `M-1` past inputs. It has no feedback from past outputs.

### 3. What is IIR?

IIR means infinite impulse response. It usually uses past outputs as well as inputs. Feedback can make the impulse response theoretically continue forever, even though a stable filter's response decays.

### 4. FIR versus IIR

| Property | FIR | IIR |
| --- | --- | --- |
| Typical equation | Weighted input samples. | Weighted input samples and previous outputs. |
| Feedback | None in direct FIR. | Present. |
| Stability | Finite coefficients give BIBO stability mathematically. | Depends on pole locations and implementation. |
| Linear phase | Available with suitable coefficient symmetry. | Generally not exact over a useful band. |
| Work for a sharp response | Often needs more coefficients. | Often meets magnitude goals with lower order. |
| Finite-precision concern | Rounding and overflow still matter. | Rounding, overflow, stability, and limit cycles can matter. |

BIBO means bounded-input, bounded-output. An FIR can still overflow a numerical type; mathematical stability does not prevent arithmetic errors.

### 5. What is convolution?

Convolution combines an input signal with an impulse response. For this causal FIR:

```text
y[n] = h[0]*x[n] + h[1]*x[n-1] + ... + h[M-1]*x[n-M+1]
```

Each output is a dot product between the taps and input history ordered from newest to oldest.

### 6. What are filter coefficients?

Coefficients are the weights `h[k]`. Their values and arrangement determine the frequency response. They can be positive or negative; they need not all add constructively at every frequency. In this project their sum is normalized to one, giving unity DC gain.

### 7. What is a low-pass filter?

A low-pass filter passes a lower-frequency region and attenuates a higher-frequency region. A practical finite filter has passband ripple, a transition band, and finite stopband rejection.

### 8. What is cutoff frequency?

It is a frequency used to describe or design the boundary between passing and rejecting frequencies. Its exact definition depends on the filter and specification. In this windowed-sinc design, 150 Hz sets the ideal low-pass edge; the realized response has a transition and must be evaluated. Do not assume it is exactly the -3 dB point.

### 9. What is sampling frequency?

Sampling frequency is the number of samples acquired each second. At 2000 Hz, samples are spaced `1/2000 = 0.0005` seconds, or 500 microseconds, apart. The digital sequence does not need to be processed at wall-clock speed in this offline demo.

### 10. What is the Nyquist theorem?

A signal bandlimited below half the sampling rate can be reconstructed ideally from its samples under the theorem's assumptions. To avoid aliasing, the sampling rate must exceed twice the highest signal frequency of interest. At 2000 Hz, Nyquist frequency is 1000 Hz. A physical acquisition system also needs analog filtering before sampling to suppress content above Nyquist.

### 11. What is the Hamming window?

It is a symmetric weighting sequence:

```text
w[n] = 0.54 - 0.46*cos(2*pi*n/(M-1))
```

It reduces the ends of the selected impulse-response segment. Its endpoint values are 0.08, not zero.

### 12. Why use windowing?

The ideal sinc extends infinitely. Simply cutting it off creates a rectangular window and relatively large frequency sidelobes. Hamming weighting reduces those sidelobes, with a wider transition as a tradeoff. No finite tap count gives a perfectly abrupt brick-wall response.

### 13. What is linear phase?

Linear phase means phase changes linearly with frequency in the relevant band. It corresponds to a constant time delay, helping preserve the relative timing of passed frequency components. Symmetric real FIR coefficients give this structure. That does not mean every frequency has the same gain, or that rejected components keep their shape.

### 14. What is group delay?

Group delay describes the delay associated with the slope of phase versus angular frequency. This odd-length symmetric FIR has delay `(M-1)/2 = 25` samples, which is 12.5 ms at 2000 Hz. It is a property of the filter response, separate from CPU processing time and acquisition buffering.

### 15. What happens when taps increase?

For a comparable design with the same window, the transition generally becomes narrower. Per-sample arithmetic, history memory, coefficient memory, and linear-phase delay all increase. Window sidelobe characteristics still constrain rejection behavior. Choose taps using measured requirements rather than assuming more is always useful.

## Part C: Embedded programming concepts

### 1. Why is C++ suitable for DSP?

C++ supports compiled numerical loops, explicit data types, predictable data layout, and direct hardware integration when needed. RAII containers help manage ownership. Performance depends on the implementation, compiler, and target; the language alone does not guarantee it.

### 2. What is a circular buffer?

It is fixed storage reused through wrapping indices. After the last position, the write index returns to the first. Here the next input replaces the oldest retained sample. The logical order of samples changes even though the physical vector is not shifted.

### 3. Why avoid dynamic allocation during sample processing?

Allocation can add variable latency, fail, and contribute to heap fragmentation. Allocating the history once keeps sample processing simpler and more predictable. `processSample()` does not resize its vectors or allocate output. `processBlock()` returns a vector and therefore has a different allocation contract. The FIR constructor rejects empty or nonfinite coefficients. Samples supplied to the processing path must be finite; the sample method does not validate each input.

### 4. How could this run on an STM32?

First select the MCU, sample source, numeric type, and timing budget. Construct or statically prepare the filter before acquisition. An ADC can fill DMA buffers; the program can process each completed block with the same filtering logic and send results to a consumer. Measure execution time on the target and check overruns. Adapt allocation, exception handling, and output handling to the platform. This repository contains none of those hardware drivers or a deployable STM32 firmware image.

### 5. Desktop processing versus real-time firmware

Desktop code can process a saved block as quickly as possible. Real-time firmware must finish the required work by deadlines while handling interrupts, data arrival, buffering, and other tasks. Average desktop throughput is helpful evidence, but worst-case target latency and scheduling determine deadline safety.

### 6. How can FIR be optimized for limited resources?

Start by measuring the target. Possible steps include fewer taps if the specification permits, `float` instead of `double`, fixed point with explicit scaling and saturation, pairing samples for symmetric coefficients, and target SIMD or DSP instructions. Coefficients can sometimes reside in read-only flash. Each choice has accuracy, portability, or code-complexity costs. For this initial project, the clear direct algorithm is deliberate.

### 7. How does DMA help continuous acquisition?

DMA moves samples between a peripheral and RAM with less CPU involvement per sample. In a double-buffer arrangement, one region can fill while software processes the other. The CPU must finish before the region is reused. DMA does not perform this FIR calculation, remove timing requirements, or automatically prevent overruns. Buffer ownership and, on some MCUs, cache coherency need attention.

## Part D: Dry run the code

Use `h = [0.25, 0.5, 0.25]` and inputs `[1, 2, 3, 4]`. Set all earlier input values to zero. The write index starts at zero and points to the slot for the **next** input.

| Step | Input | Write slot | Physical buffer after write | Convolution using newest first | Output | Next write slot |
| --- | --- | --- | --- | --- | --- | --- |
| Initial | -- | -- | `[0, 0, 0]` | -- | -- | 0 |
| 0 | 1 | 0 | `[1, 0, 0]` | `0.25*1 + 0.5*0 + 0.25*0` | 0.25 | 1 |
| 1 | 2 | 1 | `[1, 2, 0]` | `0.25*2 + 0.5*1 + 0.25*0` | 1.00 | 2 |
| 2 | 3 | 2 | `[1, 2, 3]` | `0.25*3 + 0.5*2 + 0.25*1` | 2.00 | 0 |
| 3 | 4 | 0 | `[4, 2, 3]` | `0.25*4 + 0.5*3 + 0.25*2` | 3.00 | 1 |

At step 3, read indices are `0, 2, 1`. The read wraps from zero to two, giving values `4, 3, 2`. The coefficients remain in the same order. The oldest sample, 1, is no longer needed and has been overwritten.

You should be able to write this loop from memory in words:

1. Store the incoming sample at the write index.
2. Start a read index at that same slot and set the accumulator to zero.
3. Multiply each coefficient by the current history value, accumulate, and move the read index backward with wraparound.
4. Advance the write index with wraparound.
5. Return the accumulated output.

Reset means filling the history with zero and restoring write index zero. It does not change the taps. Continuing another block without reset preserves this exact history.

## Part E: 20 interview questions and answers

### 1. What problem does your project solve?

It removes an unwanted 400 Hz sinusoid from a sampled signal while preserving a desired 50 Hz sinusoid. It also demonstrates coefficient design, stateful filtering, validation, and measured performance.

### 2. Why did you choose an FIR filter?

Its direct equation is easy to verify, it has no feedback, and symmetric coefficients provide linear phase. The tradeoff is that a sharp response can require many taps and more computation than an IIR alternative.

### 3. Why are there 51 taps?

It is an odd count with a simple integer center, manageable arithmetic, and useful separation between the selected tones. It gives a 25-sample delay. I would choose a production count from transition, rejection, CPU, and latency requirements.

### 4. What does one call to `processSample()` do?

It writes one input into the circular history, computes a dot product of taps and newest-to-oldest samples, advances the write position, and returns one output. It retains the history for the next call.

### 5. What is the time and memory complexity?

There are `M` multiply-accumulate steps per sample, giving `O(M)` time. Coefficients and history require `O(M)` owned storage. Filtering a block into a returned vector adds `O(N)` output storage for `N` samples.

### 6. Does a circular buffer make convolution asymptotically faster?

No. It avoids shifting history and supports streaming with fixed history storage, but direct convolution still costs `O(M)` per sample. Its measured desktop speed is not guaranteed to beat a contiguous indexed reference.

### 7. Why use a reference implementation?

It directly expresses the equation with simple indexing. Comparing the same input and coefficients helps detect reversed coefficient order, incorrect wraparound, and initial-state mistakes in the circular version.

### 8. How do you handle samples before time zero?

They are zero. The reference skips unavailable past indices; the circular filter starts with zero-filled history. Both therefore use the same initial conditions.

### 9. How do you know consecutive blocks are handled correctly?

The FIR object keeps history and write index between calls. An automated test compares one-block output with concatenated outputs from multiple consecutive blocks. Resetting between those blocks would change the result.

### 10. Why normalize the coefficient sum?

The response at DC equals the sum of coefficients. Dividing every coefficient by that sum makes DC gain one within floating-point precision. It does not guarantee exactly unit gain at every passband frequency.

### 11. Why not use only sine correlation to measure amplitude?

A delay changes the tone's phase and can move energy into the cosine correlation. Taking `sqrt(S*S + C*C)` measures the amplitude independently of that phase for the coherent measurement segment. The function requires at least one whole query-frequency cycle; the caller also needs other signal components to be orthogonal over that segment for an uncontaminated estimate.

### 12. Why exclude startup samples, and why start at sample 80?

A zero-state FIR initially lacks real past samples, so its output contains a startup transient. Starting at 80 is beyond `M-1 = 50` samples and leaves 1920 samples: exactly 48 cycles of 50 Hz and 384 of 400 Hz. This prevents cross-tone leakage in the chosen correlation measurement.

### 13. How do you calculate attenuation in dB?

Use `20*log10(output_amplitude/input_amplitude)` for amplitude ratios. A suppressed component gives a negative value. The factor would be 10 for a power ratio. A theoretical zero output corresponds to negative infinity dB; numerical displays may use a floor for plotting.

### 14. How do you independently predict the measured gain?

Evaluate `H(f) = sum(h[k]*exp(-j*2*pi*f*k/Fs))`. Its magnitude predicts the steady-state amplitude ratio. I compare that prediction against the known-tone measurement rather than judging the plot alone.

### 15. Is 150 Hz necessarily the -3 dB frequency?

No. It is the cutoff parameter of the ideal low-pass used in this windowed-sinc design. The realized finite response must be evaluated to determine the gain at 150 Hz and the location of any desired threshold.

### 16. What causes the output delay?

The symmetric 51-tap impulse response is centered at tap 25, giving constant group delay of 25 samples, or 12.5 ms. This is inherent to this filter design and remains even if the CPU computes each result extremely quickly.

### 17. Can you claim this is hard real-time?

No. The project measures average desktop filtering performance. Hard real-time claims require worst-case timing and deadline validation on the target, including interrupts, acquisition, buffering, and other tasks.

### 18. How do you keep benchmarks meaningful?

Use a Release build, a sufficiently large input, repeated runs, and `std::chrono`; keep signal generation and file output outside filtering timing. Observe output checksums so the results are used. Disclose that the reference allocates its return vector while the circular benchmark writes preallocated output. Background activity and hardware still affect results.

### 19. Why does the sample-processing path avoid owning raw pointers?

Vectors own their allocations through RAII and release memory automatically. The constructor prepares fixed history storage, and `processSample()` only updates existing elements. This reduces ownership mistakes and avoids per-sample allocation without requiring manual memory management.

### 20. What would you change for a modem PHY or STM32 target?

Start from actual sample rates, rejection requirements, latency, CPU, and RAM budgets. Select a suitable numeric format, integrate a real acquisition/output path, and verify timing on hardware. DMA blocks, symmetric-tap arithmetic, or target SIMD may help. A cellular PHY would also require substantially more processing and verification than this real-valued two-tone demonstration.

## Before the interview

- Run the build, tests, application, and plots yourself.
- Explain the three-tap circular-buffer table without looking at the code.
- Know the difference between filter group delay, block buffering delay, and CPU time.
- Report actual measured values and the conditions under which they were measured.
- Be clear about what the project implements and what a future hardware port would require.
