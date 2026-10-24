#include "filter_design.h"
#include "fir_filter.h"
#include "signal_generator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// These checks remain active in Release builds, unlike the assert macro.
void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void checkNear(double actual, double expected, double tolerance,
               const std::string& message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        std::ostringstream details;
        details.precision(17);
        details << message << ": expected " << expected << ", got " << actual
                << " (absolute tolerance " << tolerance << ')';
        throw std::runtime_error(details.str());
    }
}

void checkVectors(const std::vector<double>& actual,
                  const std::vector<double>& expected, double tolerance,
                  const std::string& message) {
    check(actual.size() == expected.size(), message + ": size mismatch");
    for (std::size_t index = 0; index < actual.size(); ++index) {
        checkNear(actual[index], expected[index], tolerance,
                  message + " at sample " + std::to_string(index));
    }
}

template <typename Function>
void expectInvalid(Function operation, const std::string& message) {
    try {
        operation();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(message + ": expected std::invalid_argument");
}

std::vector<double> randomSignal(std::size_t sampleCount) {
    std::mt19937 generator(2026);
    std::uniform_real_distribution<double> distribution(-2.0, 2.0);
    std::vector<double> samples(sampleCount);
    for (double& sample : samples) {
        sample = distribution(generator);
    }
    return samples;
}

void testIdentity() {
    const auto input = randomSignal(1000);
    dsp::FIRFilter filter({1.0});
    checkVectors(filter.processBlock(input), input, 0.0, "identity filter");
    check(filter.coefficients() == std::vector<double>{1.0},
          "coefficient inspection must preserve coefficients");
}

void testZerosAndImpulse() {
    const std::vector<double> coefficients{0.1, -0.2, 0.4, 0.7};
    dsp::FIRFilter filter(coefficients);
    const std::vector<double> zeros(100, 0.0);
    checkVectors(filter.processBlock(zeros), zeros, 0.0, "zero input");

    std::vector<double> impulse(12, 0.0);
    impulse[0] = 1.0;
    std::vector<double> expected(12, 0.0);
    std::copy(coefficients.begin(), coefficients.end(), expected.begin());
    checkVectors(filter.processBlock(impulse), expected, 0.0,
                 "impulse response with asymmetric coefficients");

    dsp::FIRFilter zeroFilter({0.0, 0.0, 0.0});
    checkVectors(zeroFilter.processBlock(randomSignal(100)), zeros, 0.0,
                 "zero coefficients");
}

void testCircularBufferDryRun() {
    dsp::FIRFilter filter({0.25, 0.5, 0.25});
    const std::vector<double> input{1.0, 2.0, 3.0, 4.0, 0.0, 0.0};
    const std::vector<double> expected{0.25, 1.0, 2.0, 3.0, 2.75, 1.0};
    for (std::size_t index = 0; index < input.size(); ++index) {
        checkNear(filter.processSample(input[index]), expected[index], 0.0,
                  "three-tap circular-buffer dry run");
    }
}

void testReferenceAgreement() {
    // Symmetric low-pass coefficients can hide a reversed-history bug.
    const std::vector<double> coefficients{0.21, -0.09, 0.43, 0.02,
                                           0.10, -0.08, 0.03};
    for (const std::size_t count : {std::size_t{0}, std::size_t{1},
                                    std::size_t{3}, std::size_t{4099}}) {
        const auto input = randomSignal(count);
        dsp::FIRFilter filter(coefficients);
        checkVectors(filter.processBlock(input),
                     dsp::referenceFIR(input, coefficients), 1e-12,
                     "reference/circular agreement");
    }
}

void testBlockContinuity() {
    const auto input = randomSignal(4099);
    const std::vector<double> coefficients{0.17, -0.31, 0.05, 0.61, -0.02};
    dsp::FIRFilter wholeFilter(coefficients);
    const auto expected = wholeFilter.processBlock(input);

    dsp::FIRFilter blockFilter(coefficients);
    std::vector<double> actual;
    actual.reserve(input.size());
    std::size_t offset = 0;
    // Boundaries intentionally do not align to the tap count or ring wrap.
    for (const std::size_t requested : {std::size_t{0}, std::size_t{1},
                                        std::size_t{7}, std::size_t{0},
                                        std::size_t{3}, std::size_t{63},
                                        std::size_t{2}, std::size_t{512},
                                        std::size_t{5}, input.size()}) {
        const auto count = std::min(requested, input.size() - offset);
        const std::vector<double> block(input.begin() + offset,
                                        input.begin() + offset + count);
        const auto output = blockFilter.processBlock(block);
        actual.insert(actual.end(), output.begin(), output.end());
        offset += count;
    }
    check(offset == input.size(), "block test must consume the complete input");
    checkVectors(actual, expected, 0.0, "state continuity across blocks");

    dsp::FIRFilter sampleFilter(coefficients);
    std::vector<double> sampleOutput;
    sampleOutput.reserve(input.size());
    for (double sample : input) {
        sampleOutput.push_back(sampleFilter.processSample(sample));
    }
    checkVectors(sampleOutput, expected, 0.0, "sample/block agreement");
}

void testReset() {
    const std::vector<double> coefficients{0.2, -0.1, 0.4, 0.5};
    const auto input = randomSignal(93);
    dsp::FIRFilter filter(coefficients);
    const auto expected = filter.processBlock(input);
    filter.processBlock({4.0, -3.0});
    filter.reset();
    checkVectors(filter.processBlock(input), expected, 0.0,
                 "reset must clear history and restore write position");
    filter.reset();
    filter.reset();
    checkNear(filter.processSample(1.0), coefficients.front(), 0.0,
              "repeated reset");
}

void testInvalidFilterParameters() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    for (double invalidFs : {0.0, -2000.0, nan, infinity}) {
        expectInvalid([&] { dsp::designLowPass(invalidFs, 150.0, 51); },
                      "invalid filter sampling frequency");
    }
    for (double invalidCutoff : {0.0, -1.0, 1000.0, 1001.0, nan, infinity}) {
        expectInvalid([&] { dsp::designLowPass(2000.0, invalidCutoff, 51); },
                      "cutoff must be finite and strictly inside Nyquist");
    }
    for (const std::size_t taps : {std::size_t{0}, std::size_t{1},
                                    std::size_t{2}, std::size_t{50}}) {
        expectInvalid([&] { dsp::designLowPass(2000.0, 150.0, taps); },
                      "tap count must be odd and at least three");
    }
    for (const auto& coefficients :
         std::vector<std::vector<double>>{{}, {nan}, {1.0, infinity}, {-infinity}}) {
        expectInvalid([&] { dsp::FIRFilter filter(coefficients); },
                      "invalid streaming filter coefficients");
        expectInvalid([&] { dsp::referenceFIR({1.0}, coefficients); },
                      "invalid reference filter coefficients");
    }
}

void testSignalValidation() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const std::vector<dsp::Tone> validTones{{50.0, 1.0}};
    for (double invalidFs : {0.0, -2000.0, nan, infinity}) {
        expectInvalid([&] { dsp::generateSignal(invalidFs, 80, validTones); },
                      "invalid signal sampling frequency");
    }
    expectInvalid([&] { dsp::generateSignal(2000.0, 0, validTones); },
                  "zero sample count");
    expectInvalid([] { dsp::generateSignal(2000.0, 80, {}); },
                  "empty tone list");
    for (double frequency : {0.0, -50.0, 1000.0, 1001.0, nan, infinity}) {
        expectInvalid([&] { dsp::generateSignal(2000.0, 80, {{frequency, 1.0}}); },
                      "invalid tone frequency");
    }
    for (double amplitude : {-1.0, nan, infinity}) {
        expectInvalid([&] { dsp::generateSignal(2000.0, 80, {{50.0, amplitude}}); },
                      "invalid tone amplitude");
    }
    checkVectors(dsp::generateSignal(2000.0, 80, {{50.0, 0.0}}),
                 std::vector<double>(80, 0.0), 0.0, "zero tone amplitude");

    const std::vector<double> samples(80, 0.0);
    for (double invalidFs : {0.0, -2000.0, nan, infinity}) {
        expectInvalid([&] { dsp::estimateAmplitude(samples, 50.0, invalidFs, 0, 80); },
                      "invalid correlation sampling frequency");
    }
    for (double frequency : {0.0, -50.0, 1000.0, nan, infinity}) {
        expectInvalid([&] { dsp::estimateAmplitude(samples, frequency, 2000.0, 0, 80); },
                      "invalid correlation frequency");
    }
    expectInvalid([&] { dsp::estimateAmplitude(samples, 50.0, 2000.0, 0, 0); },
                  "zero correlation count");
    expectInvalid([&] { dsp::estimateAmplitude(samples, 50.0, 2000.0, 0, 79); },
                  "correlation window must contain whole cycles");
    expectInvalid([] { dsp::estimateAmplitude({1.0}, 1e-9, 2000.0, 0, 1); },
                  "near-zero cycles must not count as a coherent window");
    expectInvalid([&] { dsp::estimateAmplitude(samples, 50.0, 2000.0, 81, 40); },
                  "correlation start past the signal");
    expectInvalid([&] { dsp::estimateAmplitude(samples, 50.0, 2000.0, 40, 80); },
                  "correlation window extends past the signal");
    expectInvalid([&] {
        dsp::estimateAmplitude(samples, 50.0, 2000.0,
                               std::numeric_limits<std::size_t>::max(), 40);
    }, "correlation bounds check must not overflow");
}

void testCoefficientDesign() {
    const auto coefficients = dsp::designLowPass(2000.0, 150.0);
    check(coefficients.size() == 51, "default low-pass design must have 51 taps");
    for (std::size_t index = 0; index < coefficients.size(); ++index) {
        checkNear(coefficients[index], coefficients[coefficients.size() - 1 - index],
                  1e-14, "linear-phase coefficient symmetry");
    }
    checkNear(std::accumulate(coefficients.begin(), coefficients.end(), 0.0),
              1.0, 1e-13, "unity DC coefficient sum");
    checkNear(std::abs(dsp::frequencyResponse(coefficients, 0.0, 2000.0)),
              1.0, 1e-13, "unity DC frequency response");
}

void testMeasuredAttenuation() {
    constexpr double sampleRate = 2000.0;
    // 1920 samples contain exactly 48 cycles at 50 Hz and 384 at 400 Hz.
    // Starting at sample 80 excludes all 50 samples of FIR startup history.
    constexpr std::size_t start = 80;
    constexpr std::size_t count = 1920;
    const auto input = dsp::generateSignal(sampleRate, 2000,
                                           {{50.0, 1.0}, {400.0, 0.5}});
    const auto coefficients = dsp::designLowPass(sampleRate, 150.0, 51);
    dsp::FIRFilter filter(coefficients);
    const auto output = filter.processBlock(input);
    checkVectors(output, dsp::referenceFIR(input, coefficients), 1e-12,
                 "default signal reference/circular agreement");

    const double input50 = dsp::estimateAmplitude(input, 50.0, sampleRate, start, count);
    const double input400 = dsp::estimateAmplitude(input, 400.0, sampleRate, start, count);
    const double output50 = dsp::estimateAmplitude(output, 50.0, sampleRate, start, count);
    const double output400 = dsp::estimateAmplitude(output, 400.0, sampleRate, start, count);
    checkNear(input50, 1.0, 1e-12, "generated 50 Hz amplitude");
    checkNear(input400, 0.5, 1e-12, "generated 400 Hz amplitude");
    const double gain50 = std::abs(dsp::frequencyResponse(coefficients, 50.0, sampleRate));
    const double gain400 = std::abs(dsp::frequencyResponse(coefficients, 400.0, sampleRate));
    checkNear(output50, input50 * gain50, 1e-11,
              "measured 50 Hz amplitude agrees with expected response");
    checkNear(output400, input400 * gain400, 1e-11,
              "measured 400 Hz amplitude agrees with expected response");
    check(gain50 > 0.9 && gain50 < 1.1, "50 Hz component must be preserved");
    check(output400 / input400 < 0.01, "400 Hz must be suppressed by at least 40 dB");
    check(output400 / output50 < 0.01,
          "filtered 400 Hz amplitude must be much smaller than filtered 50 Hz");
    const double measuredDb = 20.0 * std::log10(output400 / input400);
    const double expectedDb = 20.0 * std::log10(gain400);
    checkNear(measuredDb, expectedDb, 1e-6,
              "measured attenuation agrees with expected frequency response");
}

}  // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"identity filter", testIdentity},
        {"zero input and impulse response", testZerosAndImpulse},
        {"circular-buffer dry run", testCircularBufferDryRun},
        {"reference/circular agreement", testReferenceAgreement},
        {"state continuity between blocks", testBlockContinuity},
        {"reset", testReset},
        {"invalid filter parameters", testInvalidFilterParameters},
        {"signal and measurement validation", testSignalValidation},
        {"symmetric normalized coefficient design", testCoefficientDesign},
        {"50 Hz preservation and 400 Hz attenuation", testMeasuredAttenuation},
    };
    std::size_t failures = 0;
    for (const auto& test : tests) {
        try {
            test.second();
            std::cout << "[PASS] " << test.first << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.first << ": " << error.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << test.first << ": unknown exception\n";
        }
    }
    std::cout << tests.size() - failures << '/' << tests.size()
              << " tests passed.\n";
    return failures == 0 ? 0 : 1;
}
