#include "filter_design.h"
#include "fir_filter.h"
#include "signal_generator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Consume every output after timing so each benchmark performs observable work.
volatile double benchmarkSink = 0.0;

double checksum(const std::vector<double>& samples) {
    double result = 0.0;
    for (std::size_t n = 0; n < samples.size(); ++n) {
        result += samples[n] * static_cast<double>(1 + n % 17);
    }
    return result;
}

double gainDb(double amplitudeRatio) {
    return amplitudeRatio > 0.0
               ? 20.0 * std::log10(amplitudeRatio)
               : -std::numeric_limits<double>::infinity();
}

std::ofstream openCsv(const std::filesystem::path& path) {
    std::ofstream stream;
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream.open(path);
    stream << std::setprecision(17);
    return stream;
}

void exportSignal(const std::filesystem::path& path,
                  const std::vector<double>& signal,
                  double samplingFrequency) {
    auto stream = openCsv(path);
    stream << "sample,time_seconds,amplitude\n";
    for (std::size_t n = 0; n < signal.size(); ++n) {
        stream << n << ',' << static_cast<double>(n) / samplingFrequency
               << ',' << signal[n] << '\n';
    }
    // Explicit close makes delayed disk-write failures visible to main().
    stream.close();
}

void exportResults(const std::filesystem::path& directory,
                   const std::vector<double>& input,
                   const std::vector<double>& output,
                   const std::vector<double>& coefficients,
                   double samplingFrequency) {
    std::filesystem::create_directories(directory);
    exportSignal(directory / "input_signal.csv", input, samplingFrequency);
    exportSignal(directory / "filtered_signal.csv", output, samplingFrequency);

    auto taps = openCsv(directory / "filter_coefficients.csv");
    taps << "tap,coefficient\n";
    for (std::size_t k = 0; k < coefficients.size(); ++k) {
        taps << k << ',' << coefficients[k] << '\n';
    }
    taps.close();

    auto response = openCsv(directory / "filter_response.csv");
    response << "frequency_hz,magnitude,gain_db\n";
    // The demonstration uses Fs = 2000 Hz, so this samples 0..1000 Hz
    // inclusively at 1 Hz, including both tones and the 150 Hz cutoff.
    const auto nyquistHz = static_cast<unsigned int>(samplingFrequency / 2.0);
    for (unsigned int frequency = 0; frequency <= nyquistHz; ++frequency) {
        const double magnitude = std::abs(dsp::frequencyResponse(
            coefficients, static_cast<double>(frequency), samplingFrequency));
        response << frequency << ',' << magnitude << ',' << gainDb(magnitude)
                 << '\n';
    }
    response.close();
}

struct BenchmarkResult {
    double meanSeconds;
    double minSeconds;
    double maxSeconds;
};

// Keep the two timing loops explicit: their allocation scopes are different.
BenchmarkResult benchmarkCircular(const std::vector<double>& input,
                                  const std::vector<double>& coefficients,
                                  std::size_t repetitions) {
    dsp::FIRFilter filter(coefficients);
    std::vector<double> output(input.size());
    for (std::size_t n = 0; n < input.size(); ++n) {
        output[n] = filter.processSample(input[n]);
    }
    benchmarkSink = benchmarkSink + checksum(output); // Warm up before timing.

    double total = 0.0;
    double shortest = std::numeric_limits<double>::infinity();
    double longest = 0.0;
    for (std::size_t run = 0; run < repetitions; ++run) {
        filter.reset(); // Reset and allocation are outside the measured loop.
        const auto begin = std::chrono::steady_clock::now();
        for (std::size_t n = 0; n < input.size(); ++n) {
            output[n] = filter.processSample(input[n]);
        }
        const auto end = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(end - begin).count();
        total += seconds;
        shortest = std::min(shortest, seconds);
        longest = std::max(longest, seconds);
        benchmarkSink = benchmarkSink + checksum(output);
    }
    return {total / static_cast<double>(repetitions), shortest, longest};
}

BenchmarkResult benchmarkReference(const std::vector<double>& input,
                                   const std::vector<double>& coefficients,
                                   std::size_t repetitions) {
    benchmarkSink = benchmarkSink + checksum(dsp::referenceFIR(input, coefficients));
    double total = 0.0;
    double shortest = std::numeric_limits<double>::infinity();
    double longest = 0.0;
    for (std::size_t run = 0; run < repetitions; ++run) {
        const auto begin = std::chrono::steady_clock::now();
        const auto output = dsp::referenceFIR(input, coefficients);
        const auto end = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(end - begin).count();
        total += seconds;
        shortest = std::min(shortest, seconds);
        longest = std::max(longest, seconds);
        benchmarkSink = benchmarkSink + checksum(output);
    }
    return {total / static_cast<double>(repetitions), shortest, longest};
}

void printBenchmark(const std::string& name, const BenchmarkResult& result,
                    std::size_t samples, double samplingFrequency) {
    const double samplesPerSecond = static_cast<double>(samples) / result.meanSeconds;
    std::cout << "\n  " << name << '\n'
              << "    Mean total time/run : " << result.meanSeconds * 1e3 << " ms\n"
              << "    Min / max time/run  : " << result.minSeconds * 1e3 << " / "
              << result.maxSeconds * 1e3 << " ms\n"
              << "    Average time/sample : "
              << result.meanSeconds * 1e9 / static_cast<double>(samples) << " ns\n"
              << "    Throughput          : " << samplesPerSecond << " samples/s\n"
              << "    Throughput / Fs     : " << samplesPerSecond / samplingFrequency
              << " x\n";
}

int run(const std::filesystem::path& outputDirectory) {
    constexpr double samplingFrequency = 2000.0;
    constexpr std::size_t sampleCount = 2000;
    constexpr double cutoffFrequency = 150.0;
    constexpr std::size_t tapCount = 51;
    const std::vector<dsp::Tone> tones{{50.0, 1.0}, {400.0, 0.5}};

    std::cout << "=========================================\n"
              << " FIR DIGITAL SIGNAL PROCESSING ENGINE\n"
              << "=========================================\n\n"
              << "Sampling Frequency : " << samplingFrequency << " Hz\n"
              << "Input Samples      : " << sampleCount << '\n'
              << "Filter Type        : Low-Pass FIR\n"
              << "Cutoff Frequency   : " << cutoffFrequency << " Hz\n"
              << "Filter Taps        : " << tapCount << '\n'
              << "Window             : Hamming\n"
              << "Group Delay        : " << (tapCount - 1) / 2 << " samples ("
              << 1000.0 * static_cast<double>(tapCount - 1) / (2.0 * samplingFrequency)
              << " ms)\n\n"
              << "Generating signal...\n";
    const auto input = dsp::generateSignal(samplingFrequency, sampleCount, tones);
    std::cout << "Designing FIR coefficients...\n";
    const auto coefficients = dsp::designLowPass(samplingFrequency, cutoffFrequency, tapCount);
    std::cout << "Applying FIR filter...\n";
    dsp::FIRFilter filter(coefficients);
    const auto output = filter.processBlock(input);
    const auto reference = dsp::referenceFIR(input, coefficients);
    double maximumError = 0.0;
    for (std::size_t n = 0; n < output.size(); ++n) {
        maximumError = std::max(maximumError, std::abs(output[n] - reference[n]));
    }
    if (maximumError > 1e-12) {
        throw std::runtime_error("Circular-buffer and reference results differ.");
    }
    std::cout << std::scientific << std::setprecision(3)
              << "Maximum reference error: " << maximumError << '\n';

    // Discard more than M-1 startup samples. Both tone periods divide 40,
    // so 1920 samples contain whole cycles and avoid correlation leakage.
    constexpr std::size_t analysisStart = 80;
    constexpr std::size_t analysisCount = sampleCount - analysisStart;
    std::cout << std::fixed << std::setprecision(9)
              << "\nSignal Analysis (steady-state samples 80..1999):\n"
              << "Frequency   Input amplitude   Output amplitude   Measured dB   Expected dB\n";
    for (const auto& tone : tones) {
        const double before = dsp::estimateAmplitude(
            input, tone.frequency, samplingFrequency, analysisStart, analysisCount);
        const double after = dsp::estimateAmplitude(
            output, tone.frequency, samplingFrequency, analysisStart, analysisCount);
        const double expected = std::abs(dsp::frequencyResponse(
            coefficients, tone.frequency, samplingFrequency));
        std::cout << std::setw(6) << std::setprecision(0) << tone.frequency << " Hz  "
                  << std::setprecision(9) << std::setw(16) << before << "  "
                  << std::setw(16) << after << "  "
                  << std::setprecision(6) << std::setw(12) << gainDb(after / before)
                  << "  " << std::setw(12) << gainDb(expected) << '\n';
    }
    const double cutoffMagnitude = std::abs(dsp::frequencyResponse(
        coefficients, cutoffFrequency, samplingFrequency));
    std::cout << "Computed response at design cutoff: " << gainDb(cutoffMagnitude)
              << " dB\n"
              << "Negative dB indicates attenuation; magnitude correlation accounts for phase delay.\n";

    constexpr std::size_t benchmarkSamples = 200000;
    constexpr std::size_t repetitions = 12;
    std::cout << "\nPerformance (" << benchmarkSamples << " samples/run, "
              << repetitions << " measured runs each, one warm-up each):\n";
    const auto benchmarkInput = dsp::generateSignal(
        samplingFrequency, benchmarkSamples, tones);
    const auto circularTiming = benchmarkCircular(benchmarkInput, coefficients, repetitions);
    const auto referenceTiming = benchmarkReference(benchmarkInput, coefficients, repetitions);
    std::cout << std::fixed << std::setprecision(3);
    printBenchmark("Circular-buffer FIR", circularTiming, benchmarkSamples, samplingFrequency);
    printBenchmark("Reference FIR", referenceTiming, benchmarkSamples, samplingFrequency);
    std::cout << "\n  Circular timing uses preallocated output; filter construction/reset excluded.\n"
              << "  Reference timing includes output allocation/initialization, excludes destruction.\n"
              << "  Signal generation and checksums are outside both timing intervals.\n"
              << "  Observable benchmark checksum: " << benchmarkSink << '\n'
              << "  Desktop throughput is an estimate; it does not guarantee real-time deadlines.\n"
              << "\nMemory (logical double payload; object/allocator overhead excluded):\n"
              << "  Circular history : " << tapCount * sizeof(double) << " bytes\n"
              << "  Coefficients     : " << tapCount * sizeof(double) << " bytes\n"
              << "  Filter total     : " << 2 * tapCount * sizeof(double) << " bytes, O(M)\n"
              << "  Reference takes a complete input block and produces a same-sized output block.\n"
              << "  Benchmark input/output blocks each hold " << benchmarkSamples * sizeof(double)
              << " bytes; these are outside the circular filter state.\n";

    exportResults(outputDirectory, input, output, coefficients, samplingFrequency);
    std::cout << "\nCSV output saved to " << outputDirectory.string() << "\n"
              << "Processing completed successfully.\n";
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            std::cout << "Usage: " << argv[0] << " [output-directory]\n"
                      << "Default output directory: results\n";
            return 0;
        }
        if (argc > 2) {
            throw std::invalid_argument("Usage: fir_engine [output-directory]");
        }
        return run(argc == 2 ? std::filesystem::path(argv[1]) : std::filesystem::path("results"));
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
