#ifndef DSP_SIGNAL_GENERATOR_H
#define DSP_SIGNAL_GENERATOR_H

#include <cstddef>
#include <vector>

namespace dsp {

struct Tone {
    double frequency;
    double amplitude;
};

// Positive sample count, nonempty tones; frequencies in (0, Fs/2).
std::vector<double> generateSignal(double samplingFrequency,
                                   std::size_t sampleCount,
                                   const std::vector<Tone>& tones);

// Quadrature correlation, magnitude independent of phase. The selected segment
// must contain whole cycles of the queried tone; other tones must be orthogonal
// over the same window to avoid leakage into this estimate.
double estimateAmplitude(const std::vector<double>& samples,
                         double frequency,
                         double samplingFrequency,
                         std::size_t start,
                         std::size_t count);

} // namespace dsp

#endif
