#include "fir_filter.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace dsp {
namespace {

void validateCoefficients(const std::vector<double>& coefficients) {
    if (coefficients.empty()) {
        throw std::invalid_argument("FIR coefficients must not be empty.");
    }
    for (double coefficient : coefficients) {
        if (!std::isfinite(coefficient)) {
            throw std::invalid_argument("FIR coefficients must be finite.");
        }
    }
}

} // namespace

FIRFilter::FIRFilter(std::vector<double> coefficients)
    : coefficients_(std::move(coefficients)),
      history_(coefficients_.size(), 0.0) {
    validateCoefficients(coefficients_);
}

double FIRFilter::processSample(double input) noexcept {
    history_[nextWrite_] = input;
    double output = 0.0;
    std::size_t historyIndex = nextWrite_;

    // h[0] multiplies the newest sample; walk backwards through older samples.
    for (double coefficient : coefficients_) {
        output += coefficient * history_[historyIndex];
        historyIndex = (historyIndex == 0) ? history_.size() - 1 : historyIndex - 1;
    }

    ++nextWrite_;
    if (nextWrite_ == history_.size()) {
        nextWrite_ = 0;
    }
    return output;
}

std::vector<double> FIRFilter::processBlock(const std::vector<double>& input) {
    std::vector<double> output(input.size());
    for (std::size_t n = 0; n < input.size(); ++n) {
        output[n] = processSample(input[n]);
    }
    return output;
}

void FIRFilter::reset() noexcept {
    std::fill(history_.begin(), history_.end(), 0.0);
    nextWrite_ = 0;
}

const std::vector<double>& FIRFilter::coefficients() const noexcept {
    return coefficients_;
}

std::vector<double> referenceFIR(const std::vector<double>& input,
                               const std::vector<double>& coefficients) {
    validateCoefficients(coefficients);
    std::vector<double> output(input.size(), 0.0);
    for (std::size_t n = 0; n < input.size(); ++n) {
        const std::size_t available = std::min(coefficients.size(), n + 1);
        for (std::size_t k = 0; k < available; ++k) {
            output[n] += coefficients[k] * input[n - k];
        }
    }
    return output;
}

} // namespace dsp
