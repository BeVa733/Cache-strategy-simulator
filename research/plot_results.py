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


def average_normalized(rows, levels=None):
    grouped = {}
    for row in rows:
        config = row["config"] if levels is None else "/".join(
            row[f"level{level}"] for level in range(1, levels + 1))
        if config.startswith("OPT"):
            continue
        key = (row["scenario"], config)
        grouped.setdefault(key, []).append(float(row["normalized"]))

    return [(scenario, config, sum(values) / len(values))
            for (scenario, config), values in grouped.items()]


def best_by_scenario(rows, three_level=False):
    best = {}
    levels = 3 if three_level else None
    for scenario, config, value in average_normalized(rows, levels):
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
    axis.set_ylabel("Average normalized hits")
    axis.set_ylim(0, 1.05)
    axis.set_title("Best average result by cache depth")
    axis.legend()
    figure.tight_layout()
    figure.savefig(output_dir / "level_comparison.png", dpi=150)
    plt.close(figure)


def plot_all_systems(rows, levels, output_path):
    values = average_normalized(rows, levels)
    values.sort(key=lambda value: value[2], reverse=True)
    labels = [value[1] for value in values]
    normalized = [value[2] for value in values]
    colors = ["tab:green" if index < 3 else "tab:blue"
              for index in range(len(values))]

    figure, axis = plt.subplots(figsize=(10, max(6, len(values) * 0.32)))
    bars = axis.barh(labels, normalized, color=colors)
    axis.invert_yaxis()
    axis.axvline(1.0, color="black", linewidth=1, linestyle="--")
    axis.set_xlim(0, 1.08)
    axis.set_xlabel("Average normalized hits")
    axis.set_title(f"All {levels}-level systems, averaged by trace length")
    axis.bar_label(bars, fmt="%.3f", padding=3, fontsize=8)
    figure.tight_layout()
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def plot_single_level_policies(results_dir, output_dir):
    rows = [row for row in read_rows(results_dir / "normalized_results.csv")
            if row["levels"] == "1" and row["config"] != "OPT"]
    scenarios = sorted({row["scenario"] for row in rows})
    policies = ["LRU", "LFU", "2Q", "ARC", "LIRS"]
    values = {(scenario, config): value
              for scenario, config, value in average_normalized(rows)}
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
    axis.set_ylabel("Average normalized hits")
    axis.set_ylim(0, 1.05)
    axis.set_title("Single-level policies, averaged by trace length")
    axis.legend(ncols=6, loc="upper center")
    figure.tight_layout()
    figure.savefig(output_dir / "single_level_policies.png", dpi=150)
    plt.close(figure)


def plot_trace_lengths(results_dir, output_dir):
    rows = [row for row in read_rows(results_dir / "normalized_results.csv")
            if row["levels"] == "1" and row["config"] != "OPT"]
    scenarios = ["cycle", "hot", "scan", "phase", "random", "mixed"]
    policies = ["LRU", "LFU", "2Q", "ARC", "LIRS"]
    figure, axes = plt.subplots(2, 3, figsize=(13, 7), sharex=True,
                               sharey=True)
    request_labels = {1920: "1.9k", 6400: "6.4k", 19200: "19.2k",
                      64000: "64k"}

    for axis, scenario in zip(axes.flat, scenarios):
        scenario_rows = [row for row in rows
                         if row["scenario"] == scenario]
        for policy in policies:
            values = sorted(
                (int(row["requests"]), float(row["normalized"]))
                for row in scenario_rows if row["config"] == policy
            )
            axis.plot([value[0] for value in values],
                      [value[1] for value in values], marker="o",
                      label=policy)

        axis.set_title(scenario)
        axis.set_xscale("log")
        axis.set_xticks(list(request_labels))
        axis.set_xticklabels(list(request_labels.values()))
        axis.minorticks_off()
        axis.set_ylim(0, 1.05)
        axis.grid(alpha=0.3)

    figure.supxlabel("Trace length, requests")
    figure.supylabel("Hits / OPT hits")
    handles, labels = axes[0, 0].get_legend_handles_labels()
    figure.legend(handles, labels, loc="upper center", ncols=len(policies),
                  bbox_to_anchor=(0.5, 0.99))
    figure.tight_layout(rect=(0, 0, 1, 0.92))
    figure.savefig(output_dir / "trace_length_scaling.png", dpi=150)
    plt.close(figure)


def cost_ratios(rows, levels):
    references = {}
    for row in rows:
        if row["level1" if levels > 1 else "policy"].startswith("OPT"):
            key = (row["scenario"], row["requests"])
            references[key] = float(row["total_cost"])

    grouped = {}
    for row in rows:
        first = row["level1"] if levels > 1 else row["policy"]
        if first.startswith("OPT"):
            continue

        if levels == 1:
            config = row["policy"]
        else:
            config = "/".join(row[f"level{level}"]
                              for level in range(1, levels + 1))
        reference_key = (row["scenario"], row["requests"])
        ratio = float(row["total_cost"]) / references[reference_key]
        key = (row["scenario"], config)
        grouped.setdefault(key, []).append(ratio)

    return [(scenario, config, sum(values) / len(values))
            for (scenario, config), values in grouped.items()]


def plot_cost_systems(rows, levels, output_path):
    values = cost_ratios(rows, levels)
    values.sort(key=lambda value: value[2])
    labels = [value[1] for value in values]
    ratios = [value[2] for value in values]
    colors = ["tab:green" if index < 3 else "tab:blue"
              for index in range(len(values))]

    figure, axis = plt.subplots(figsize=(10, max(6, len(values) * 0.32)))
    bars = axis.barh(labels, ratios, color=colors)
    axis.invert_yaxis()
    axis.axvline(1.0, color="black", linewidth=1, linestyle="--")
    axis.set_xlabel("Average cost / OPT cost (lower is better)")
    axis.set_title(f"Average cost of all {levels}-level systems")
    axis.bar_label(bars, fmt="%.3f", padding=3, fontsize=8)
    figure.tight_layout()
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def plot_single_level_costs(results_dir, output_dir):
    rows = read_rows(results_dir / "research_cost_results.csv")
    values = {(scenario, config): ratio
              for scenario, config, ratio in cost_ratios(rows, 1)}
    scenarios = sorted({row["scenario"] for row in rows})
    policies = ["LRU", "LFU", "2Q", "ARC", "LIRS"]
    positions = range(len(scenarios))
    width = 0.16

    figure, axis = plt.subplots(figsize=(11, 5))
    for index, policy in enumerate(policies):
        ratios = [values[(scenario, policy)] for scenario in scenarios]
        axis.bar([position + index * width for position in positions], ratios,
                 width, label=policy)

    axis.axhline(1.0, color="black", linewidth=1, linestyle="--",
                 label="OPT")
    axis.set_xticks([position + 2 * width for position in positions],
                    scenarios)
    axis.set_ylabel("Average cost / OPT cost (lower is better)")
    axis.set_title("Single-level cost, averaged by trace length")
    axis.legend(ncols=6, loc="upper center")
    figure.tight_layout()
    figure.savefig(output_dir / "cost_single_level_policies.png", dpi=150)
    plt.close(figure)


def plot_cost_level_comparison(results_dir, output_dir):
    all_values = {
        1: cost_ratios(
            read_rows(results_dir / "research_cost_results.csv"), 1),
        2: cost_ratios(
            read_rows(results_dir / "two_level_cost_results.csv"), 2),
        3: cost_ratios(
            read_rows(results_dir / "three_level_cost_results.csv"), 3),
    }
    best = {}
    for levels, values in all_values.items():
        mixed = [value for value in values if value[0] == "mixed"]
        best[levels] = min(mixed, key=lambda value: value[2])

    labels = [f"{levels} level\n{best[levels][1]}" for levels in (1, 2, 3)]
    ratios = [best[levels][2] for levels in (1, 2, 3)]
    figure, axis = plt.subplots(figsize=(8, 5))
    bars = axis.bar(labels, ratios)
    axis.axhline(1.0, color="black", linewidth=1, linestyle="--")
    axis.set_ylabel("Average cost / OPT cost (lower is better)")
    axis.set_title("Best average cost by cache depth")
    axis.bar_label(bars, fmt="%.3f", padding=3)
    figure.tight_layout()
    figure.savefig(output_dir / "cost_level_comparison.png", dpi=150)
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
    plot_trace_lengths(results_dir, output_dir)
    plot_cost_systems(
        read_rows(results_dir / "two_level_cost_results.csv"), 2,
        output_dir / "cost_two_level_systems.png")
    plot_cost_systems(
        read_rows(results_dir / "three_level_cost_results.csv"), 3,
        output_dir / "cost_three_level_systems.png")
    plot_single_level_costs(results_dir, output_dir)
    plot_cost_level_comparison(results_dir, output_dir)
    print(f"Plots saved to {output_dir}")


if __name__ == "__main__":
    main()
