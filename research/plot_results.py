#!/usr/bin/env python3
"""Create small plots from the research CSV files."""

import csv
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def read_rows(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def best_by_scenario(rows, three_level=False):
    best = {}
    for row in rows:
        config = row["config"] if not three_level else "/".join(
            (row["level1"], row["level2"], row["level3"]))
        if config.startswith("OPT"):
            continue
        scenario = row["scenario"]
        value = float(row["normalized"])
        if scenario not in best or value > best[scenario]["value"]:
            best[scenario] = {"config": config, "value": value}
    return best


def plot_level_comparison(results_dir, output_dir):
    normalized = read_rows(results_dir / "normalized_results.csv")
    three_level = read_rows(results_dir / "three_level_results.csv")
    best = {
        1: best_by_scenario([r for r in normalized if r["levels"] == "1"]),
        2: best_by_scenario([r for r in normalized if r["levels"] == "2"]),
        3: best_by_scenario(three_level, three_level=True),
    }
    scenarios = sorted(
        set.intersection(*(set(values) for values in best.values()))
    )
    positions = range(len(scenarios))
    width = 0.25

    figure, axis = plt.subplots(figsize=(10, 5))
    for index, level in enumerate((1, 2, 3)):
        values = [best[level].get(scenario, {}).get("value", 0.0)
                  for scenario in scenarios]
        labels = [best[level].get(scenario, {}).get("config", "")
                  for scenario in scenarios]
        bars = axis.bar([p + index * width for p in positions], values, width,
                        label=f"{level} level")
        for bar, label in zip(bars, labels):
            axis.text(bar.get_x() + bar.get_width() / 2,
                      bar.get_height() - 0.02, label, ha="center", va="top",
                      rotation=90, color="white", fontsize=8)

    axis.set_xticks([p + width for p in positions], scenarios)
    axis.set_ylabel("Normalized hits")
    axis.set_ylim(0, 1.05)
    axis.set_title("Best normalized result by cache depth")
    axis.legend()
    figure.tight_layout()
    figure.savefig(output_dir / "level_comparison.png", dpi=150)
    plt.close(figure)


def plot_all_systems(rows, levels, output_path):
    rows = [row for row in rows if not row["level1"].startswith("OPT")]
    rows.sort(key=lambda row: float(row["normalized"]), reverse=True)
    labels = ["/".join(row[f"level{level}"]
                       for level in range(1, levels + 1)) for row in rows]
    values = [float(row["normalized"]) for row in rows]
    colors = ["tab:green" if index < 3 else "tab:blue"
              for index in range(len(rows))]

    figure, axis = plt.subplots(figsize=(10, max(6, len(rows) * 0.32)))
    bars = axis.barh(labels, values, color=colors)
    axis.invert_yaxis()
    axis.axvline(1.0, color="black", linewidth=1, linestyle="--")
    axis.set_xlim(0, 1.08)
    axis.set_xlabel("Normalized hits")
    axis.set_title(f"All {levels}-level systems on mixed workload")
    axis.bar_label(bars, fmt="%.3f", padding=3, fontsize=8)
    figure.tight_layout()
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def plot_single_level_policies(results_dir, output_dir):
    rows = [row for row in read_rows(results_dir / "normalized_results.csv")
            if row["levels"] == "1" and row["config"] != "OPT"]
    scenarios = sorted({row["scenario"] for row in rows})
    policies = ["LRU", "LFU", "2Q", "ARC", "LIRS"]
    values = {(row["scenario"], row["config"]):
              float(row["normalized"]) for row in rows}
    positions = range(len(scenarios))
    width = 0.16

    figure, axis = plt.subplots(figsize=(11, 5))
    for index, policy in enumerate(policies):
        heights = [values.get((scenario, policy), 0.0)
                   for scenario in scenarios]
        bars = axis.bar([p + index * width for p in positions], heights,
                        width, label=policy)
        for bar, height in zip(bars, heights):
            if height == 0.0:
                axis.text(bar.get_x() + bar.get_width() / 2, 0.01, "0",
                          ha="center", va="bottom", fontsize=8)

    axis.axhline(1.0, color="black", linewidth=1, linestyle="--",
                 label="OPT")
    axis.set_xticks([p + 2 * width for p in positions], scenarios)
    axis.set_ylabel("Normalized hits")
    axis.set_ylim(0, 1.05)
    axis.set_title("Single-level policies")
    axis.legend(ncols=6, loc="upper center")
    figure.tight_layout()
    figure.savefig(output_dir / "single_level_policies.png", dpi=150)
    plt.close(figure)


def main():
    results_dir = (
        Path(sys.argv[1]) if len(sys.argv) > 1 else Path("research/results")
    )
    output_dir = results_dir / "plots"
    output_dir.mkdir(parents=True, exist_ok=True)
    plot_level_comparison(results_dir, output_dir)
    plot_all_systems(read_rows(results_dir / "two_level_results.csv"), 2,
                     output_dir / "two_level_systems.png")
    plot_all_systems(read_rows(results_dir / "three_level_results.csv"), 3,
                     output_dir / "three_level_systems.png")
    plot_single_level_policies(results_dir, output_dir)
    print(f"Plots saved to {output_dir}")


if __name__ == "__main__":
    main()
