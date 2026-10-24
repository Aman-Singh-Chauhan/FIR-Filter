"""Plot C++ output CSV files; all signal processing stays in the C++ engine.

Run from the project directory:
    python scripts/plot.py
    python scripts/plot.py --results results --show
"""

import argparse
import csv
from pathlib import Path
import sys


def read_columns(path, headings):
    """Read named numeric columns and give an actionable CSV error."""
    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames is None or not set(headings).issubset(reader.fieldnames):
            raise ValueError(f"{path}: expected columns {', '.join(headings)}")
        columns = [[] for _ in headings]
        for line, row in enumerate(reader, start=2):
            try:
                values = [float(row[heading]) for heading in headings]
            except (TypeError, ValueError) as error:
                raise ValueError(f"{path}, line {line}: invalid numeric value") from error
            for column, value in zip(columns, values):
                column.append(value)
    if not columns[0]:
        raise ValueError(f"{path}: CSV contains no data")
    return columns


def plot_signal(plt, times, amplitudes, title, color, destination, filtered=False):
    figure, axis = plt.subplots(figsize=(10, 4), constrained_layout=True)
    axis.plot(times, amplitudes, color=color, linewidth=1.4)
    axis.set(title=title, xlabel="Time (seconds)", ylabel="Amplitude")
    # A short view makes both the wanted sine and unwanted ripple visible.
    axis.set_xlim(times[0], min(times[-1], times[0] + 0.12))
    axis.grid(True, alpha=0.3)
    if filtered:
        # Configuration labels refer to the default demonstration, not inferred
        # signal measurements. The CSV stores samples without time alignment.
        axis.text(
            0.99, 0.96,
            "Default 51-tap design: 12.5 ms group delay\n"
            "Startup occupies the first 50 samples (25 ms)",
            transform=axis.transAxes, ha="right", va="top", fontsize=9,
            bbox={"facecolor": "white", "edgecolor": "0.8", "alpha": 0.9},
        )
    figure.savefig(destination, dpi=160)
    return figure


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--results", type=Path,
        default=Path(__file__).resolve().parents[1] / "results",
        help="directory containing the C++ CSV outputs (default: project/results)",
    )
    parser.add_argument("--show", action="store_true", help="also open plot windows")
    args = parser.parse_args()

    try:
        import matplotlib
        if not args.show:
            matplotlib.use("Agg")  # Save figures without a desktop/display.
        import matplotlib.pyplot as plt
    except ImportError:
        parser.exit(1, "matplotlib is required for plotting. Install with: "
                    "python -m pip install matplotlib\n")

    results = args.results.resolve()
    try:
        input_time, input_amplitude = read_columns(
            results / "input_signal.csv", ("time_seconds", "amplitude"))
        output_time, output_amplitude = read_columns(
            results / "filtered_signal.csv", ("time_seconds", "amplitude"))
        frequencies, gains_db = read_columns(
            results / "filter_response.csv", ("frequency_hz", "gain_db"))
    except (OSError, ValueError) as error:
        parser.exit(1, f"Cannot load C++ results: {error}\nRun the C++ application first.\n")

    figures = [
        plot_signal(plt, input_time, input_amplitude,
                    "Original signal: 50 Hz + 0.5 x 400 Hz", "#2769aa",
                    results / "original_signal.png"),
        plot_signal(plt, output_time, output_amplitude,
                    "Filtered signal: low-pass FIR output", "#087f5b",
                    results / "filtered_signal.png", filtered=True),
    ]
    figure, axis = plt.subplots(figsize=(10, 4), constrained_layout=True)
    axis.plot(frequencies, gains_db, color="#7734a6", linewidth=1.5)
    axis.set(title="FIR frequency response (calculated in C++)",
             xlabel="Frequency (Hz)", ylabel="Gain (dB)")
    axis.set_xlim(0.0, frequencies[-1])
    axis.grid(True, alpha=0.3)
    for frequency, label, color in (
        (50, "50 Hz desired", "#087f5b"),
        (150, "150 Hz design cutoff", "#be6a00"),
        (400, "400 Hz unwanted", "#bf3b3b"),
    ):
        axis.axvline(frequency, linestyle="--", linewidth=1.0, color=color, label=label)
    axis.legend(loc="upper right", fontsize=9)
    figure.savefig(results / "filter_response.png", dpi=160)
    figures.append(figure)

    for name in ("original_signal.png", "filtered_signal.png", "filter_response.png"):
        print(f"Saved {results / name}")
    if args.show:
        plt.show()
    for figure in figures:
        plt.close(figure)
    return 0


if __name__ == "__main__":
    sys.exit(main())
