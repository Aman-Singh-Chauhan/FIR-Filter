#ifndef DSP_FIR_FILTER_H
#define DSP_FIR_FILTER_H

#include <cstddef>
#include <vector>

namespace dsp {

// Coefficients and history are allocated once. Supply finite input samples.
class FIRFilter {
public:
    explicit FIRFilter(std::vector<double> coefficients);

    double processSample(double input) noexcept;
    std::vector<double> processBlock(const std::vector<double>& input);
    void reset() noexcept;
    const std::vector<double>& coefficients() const noexcept;

private:
    std::vector<double> coefficients_;
    std::vector<double> history_;
    std::size_t nextWrite_ = 0;
};

// Offline correctness reference: each call starts with zero history.
std::vector<double> referenceFIR(const std::vector<double>& input,
                               const std::vector<double>& coefficients);

} // namespace dsp

#endif
