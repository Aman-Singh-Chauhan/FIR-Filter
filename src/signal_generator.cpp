#include "signal_generator.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dsp {
namespace {
constexpr double pi = 3.14159265358979323846;

void validateFrequency(double frequency, double samplingFrequency) {
    if (!std::isfinite(samplingFrequency) || samplingFrequency <= 0.0) {
        throw std::invalid_argument("Sampling frequency must be finite and positive.");
    }
    if (!std::isfinite(frequency) || frequency <= 0.0 ||
        frequency >= samplingFrequency / 2.0) {
        throw std::invalid_argument("Tone frequency must satisfy 0 < frequency < Fs/2.");
    }
}
} // namespace

std::vector<double> generateSignal(double samplingFrequency,
                                   std::size_t sampleCount,
                                   const std::vector<Tone>& tones) {
    if (!std::isfinite(samplingFrequency) || samplingFrequency <= 0.0) {
        throw std::invalid_argument("Sampling frequency must be finite and positive.");
    }
    if (sampleCount == 0 || tones.empty()) {
        throw std::invalid_argument("Provide a positive sample count and at least one tone.");
    }
    double amplitudeSum = 0.0;
    for (const Tone& tone : tones) {
        validateFrequency(tone.frequency, samplingFrequency);
        if (!std::isfinite(tone.amplitude) || tone.amplitude < 0.0) {
            throw std::invalid_argument("Tone amplitude must be finite and nonnegative.");
        }
        amplitudeSum += tone.amplitude;
    }
    if (!std::isfinite(amplitudeSum)) {
        throw std::invalid_argument("The sum of tone amplitudes must be finite.");
    }

    std::vector<double> samples(sampleCount, 0.0);
    for (std::size_t n = 0; n < sampleCount; ++n) {
        for (const Tone& tone : tones) {
            const double angle = 2.0 * pi * (tone.frequency / samplingFrequency)
                                 * static_cast<double>(n);
            samples[n] += tone.amplitude * std::sin(angle);
        }
    }
    return samples;
}

double estimateAmplitude(const std::vector<double>& samples,
                         double frequency,
                         double samplingFrequency,
                         std::size_t start,
                         std::size_t count) {
    validateFrequency(frequency, samplingFrequency);
    if (count == 0 || start > samples.size() || count > samples.size() - start) {
        throw std::invalid_argument("Amplitude measurement segment is empty or out of range.");
    }
    const double cycles = static_cast<double>(count) * (frequency / samplingFrequency);
    if (std::round(cycles) < 1.0 ||
        std::abs(cycles - std::round(cycles)) > 1e-10 * std::max(1.0, cycles)) {
        throw std::invalid_argument("Amplitude measurement requires a whole number of cycles.");
    }

    double sineSum = 0.0;
    double cosineSum = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t n = start + i;
        if (!std::isfinite(samples[n])) {
            throw std::invalid_argument("Measured samples must be finite.");
        }
        const double angle = 2.0 * pi * (frequency / samplingFrequency)
                             * static_cast<double>(n);
        sineSum += samples[n] * std::sin(angle);
        cosineSum += samples[n] * std::cos(angle);
    }
    return 2.0 * std::hypot(sineSum, cosineSum) / static_cast<double>(count);
}

} // namespace dsp
