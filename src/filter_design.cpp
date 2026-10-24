#include "filter_design.h"

#include <cmath>
#include <numeric>
#include <stdexcept>

namespace dsp {
namespace {
constexpr double pi = 3.14159265358979323846;

void validateSamplingFrequency(double samplingFrequency) {
    if (!std::isfinite(samplingFrequency) || samplingFrequency <= 0.0) {
        throw std::invalid_argument("Sampling frequency must be finite and positive.");
    }
}
} // namespace

std::vector<double> designLowPass(double samplingFrequency,
                                  double cutoffFrequency,
                                  std::size_t taps) {
    validateSamplingFrequency(samplingFrequency);
    if (!std::isfinite(cutoffFrequency) || cutoffFrequency <= 0.0 ||
        cutoffFrequency >= samplingFrequency / 2.0) {
        throw std::invalid_argument("Cutoff must satisfy 0 < cutoff < Fs/2.");
    }
    if (taps < 3 || taps % 2 == 0) {
        throw std::invalid_argument("Filter length must be odd and at least 3.");
    }

    const double normalizedCutoff = cutoffFrequency / samplingFrequency;
    const std::size_t center = (taps - 1) / 2;
    std::vector<double> coefficients(taps);
    for (std::size_t n = 0; n < taps; ++n) {
        const double offset = static_cast<double>(n) - static_cast<double>(center);
        const double ideal = (n == center)
            ? 2.0 * normalizedCutoff
            : std::sin(2.0 * pi * normalizedCutoff * offset) / (pi * offset);
        const double window = 0.54 - 0.46 * std::cos(
            2.0 * pi * static_cast<double>(n) / static_cast<double>(taps - 1));
        coefficients[n] = ideal * window;
    }

    const double sum = std::accumulate(coefficients.begin(), coefficients.end(), 0.0);
    if (!std::isfinite(sum) || sum <= 0.0) {
        throw std::invalid_argument("Cannot normalize this filter design.");
    }
    for (double& coefficient : coefficients) {
        coefficient /= sum;
    }
    return coefficients;
}

std::complex<double> frequencyResponse(const std::vector<double>& coefficients,
                                       double frequency,
                                       double samplingFrequency) {
    validateSamplingFrequency(samplingFrequency);
    if (!std::isfinite(frequency) || frequency < 0.0 ||
        frequency > samplingFrequency / 2.0) {
        throw std::invalid_argument("Response frequency must be between 0 and Fs/2.");
    }
    if (coefficients.empty()) {
        throw std::invalid_argument("FIR coefficients must not be empty.");
    }

    std::complex<double> response{0.0, 0.0};
    for (std::size_t k = 0; k < coefficients.size(); ++k) {
        if (!std::isfinite(coefficients[k])) {
            throw std::invalid_argument("FIR coefficients must be finite.");
        }
        const double angle = -2.0 * pi * (frequency / samplingFrequency)
                             * static_cast<double>(k);
        response += coefficients[k] * std::complex<double>{std::cos(angle), std::sin(angle)};
    }
    return response;
}

} // namespace dsp
