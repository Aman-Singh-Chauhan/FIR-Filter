#ifndef DSP_FILTER_DESIGN_H
#define DSP_FILTER_DESIGN_H

#include <complex>
#include <cstddef>
#include <vector>

namespace dsp {

// Odd tap count >= 3; cutoff is strictly between DC and Nyquist.
std::vector<double> designLowPass(double samplingFrequency,
                                  double cutoffFrequency,
                                  std::size_t taps = 51);

// H(f) = sum h[k] exp(-j * 2*pi*f*k/Fs), including phase.
std::complex<double> frequencyResponse(const std::vector<double>& coefficients,
                                       double frequency,
                                       double samplingFrequency);

} // namespace dsp

#endif
