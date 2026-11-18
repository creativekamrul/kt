#!/usr/bin/env python3
"""Turn the simulation CSVs into the report's evidence figures.

Reads the CSVs written by sdp_sim (see apps/simharness) and writes PNGs into
docs/figures/. Development-time dependency only: the game itself never needs
Python or matplotlib. Run:

    python tools/make_charts.py --main out/main --steep out/steep --out docs/figures
"""
from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

TARGET = 0.75
BAND = (0.70, 0.80)
GROUPS = ["weak", "mid", "strong"]
ARM_LABELS = {
    "latent": "latent Thompson (shipped)",
    "thompson_arms": "independent Beta arms",
    "ucb1": "UCB1 (shaped)",
    "greedy": "greedy (no exploration)",
    "static": "static difficulty",
}
ARM_ORDER = ["latent", "thompson_arms", "ucb1", "greedy", "static"]


def read_children(directory: Path, arm: str) -> list[dict]:
    path = directory / f"children_{arm}.csv"
    if not path.exists():
        return []
    with path.open() as handle:
        return list(csv.DictReader(handle))


def read_attempts(directory: Path, arm: str) -> list[dict]:
    path = directory / f"attempts_{arm}.csv"
    if not path.exists():
        return []
    with path.open() as handle:
        return list(csv.DictReader(handle))


def read_summary(directory: Path, arm: str) -> dict:
    path = directory / f"summary_{arm}.csv"
    json_path = directory / f"summary_{arm}.json"
    if json_path.exists():
        return json.loads(json_path.read_text())
    return {}


def available_arms(directory: Path) -> list[str]:
    return [a for a in ARM_ORDER if (directory / f"children_{a}.csv").exists()]


def present_arms(directory: Path) -> list[str]:
    return [a for a in available_arms(directory) if a]


def figure_flow_zone(directory: Path, out: Path) -> None:
    """The money chart: success rate per ability group, adaptive vs static."""
    arms = present_arms(directory)
    fig, axes = plt.subplots(1, 2, figsize=(13, 5))
    width = 0.8 / max(1, len(arms))

    for ax, metric, title in (
        (axes[0], "success", "Whole run"),
        (axes[1], "success_last_third", "Steady state (last third)"),
    ):
        ax.axhspan(BAND[0], BAND[1], color="#2e7d32", alpha=0.12,
                   label="flow zone (70-80%)")
        ax.axhline(TARGET, color="#2e7d32", lw=1.2, ls="--")
        for index, arm in enumerate(arms):
            rows = read_children(directory, arm)
            means, errors = [], []
            for group in GROUPS:
                values = [float(r[metric]) for r in rows if r["group"] == group]
                means.append(float(np.mean(values)) if values else 0.0)
                errors.append(float(np.std(values)) if values else 0.0)
            positions = np.arange(len(GROUPS)) + index * width - 0.4 + width / 2
            ax.bar(positions, means, width, yerr=errors, capsize=3,
                   label=ARM_LABELS.get(arm, arm))
        ax.set_xticks(np.arange(len(GROUPS)))
        ax.set_xticklabels([g.capitalize() for g in GROUPS])
        ax.set_ylim(0, 1.05)
        ax.set_ylabel("success rate")
        ax.set_title(f"Success rate by ability group - {title}")
        ax.grid(axis="y", alpha=0.25)
    axes[0].legend(fontsize=8, loc="upper left")
    fig.suptitle("Simulated learners: latent Thompson sampling brings group success rates closer to the 75% target")
    fig.tight_layout()
    fig.savefig(out / "flow_zone_by_group.png", dpi=150)
    plt.close(fig)


def figure_pt_true(directory: Path, out: Path) -> None:
    """Where the served items actually landed relative to the learner's ability."""
    arms = present_arms(directory)
    columns = 3
    rows = (len(arms) + columns - 1) // columns
    fig, axes = plt.subplots(rows, columns, figsize=(4.2 * columns, 3.2 * rows), squeeze=False)

    for index, arm in enumerate(arms):
        ax = axes[index // columns][index % columns]
        data = [float(r["p_true"]) for r in read_attempts(directory, arm)]
        ax.hist(data, bins=24, range=(0, 1), color="#1565c0", alpha=0.85)
        ax.axvspan(BAND[0], BAND[1], color="#2e7d32", alpha=0.15)
        ax.set_title(ARM_LABELS.get(arm, arm), fontsize=10)
        ax.set_xlim(0, 1)
        ax.set_xlabel("P(correct) predicted for that item")
        if index % columns == 0:
            ax.set_ylabel("attempts")
    for index in range(len(arms), rows * columns):
        axes[index // columns][index % columns].axis("off")
    fig.suptitle("Distribution of item difficulty relative to the child "
                 "(green = flow zone)")
    fig.tight_layout()
    fig.savefig(out / "pt_true_distribution.png", dpi=150)
    plt.close(fig)


def figure_difficulty_climb(directory: Path, out: Path, arm: str = "latent",
                            block: int = 10) -> None:
    """Difficulty climbs as ability grows - and success stays pinned."""
    attempts = read_attempts(directory, arm)
    if not attempts:
        return

    by_group = defaultdict(list)
    for row in attempts:
        by_group[row["group"]].append(row)

    fig, axes = plt.subplots(1, 2, figsize=(13, 4.6))
    for group in GROUPS:
        rows = sorted(by_group.get(group, []), key=lambda r: float(r["index"]))
        if not rows:
            continue
        blocks = defaultdict(list)
        for row in rows:
            blocks[int(float(row["index"])) // block].append(row)
        xs = sorted(blocks)
        tiers = [np.mean([float(r["tier"]) for r in blocks[b]]) for b in xs]
        success = [np.mean([float(r["correct"]) for r in blocks[b]]) for b in xs]
        axes[0].plot([b * block for b in xs], tiers, marker="o", ms=3, label=group)
        axes[1].plot([b * block for b in xs], success, marker="o", ms=3, label=group)

    axes[0].set_title(f"Difficulty served over time ({ARM_LABELS.get(arm, arm)})")
    axes[0].set_ylabel("mean difficulty tier")
    axes[1].axhspan(BAND[0], BAND[1], color="#2e7d32", alpha=0.12)
    axes[1].axhline(TARGET, color="#2e7d32", ls="--")
    axes[1].set_ylim(0, 1.05)
    axes[1].set_title("Success rate over time")
    axes[1].set_ylabel("success rate")
    for ax in axes:
        ax.set_xlabel(f"attempt number (averaged over {block})")
        ax.grid(alpha=0.25)
        ax.legend()
    fig.tight_layout()
    fig.savefig(out / "difficulty_climb.png", dpi=150)
    plt.close(fig)


def figure_spread(directory: Path, out: Path) -> None:
    """Per-child steady-state success: the spread is what matters."""
    arms = present_arms(directory)
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.axhspan(BAND[0], BAND[1], color="#2e7d32", alpha=0.12, label="flow zone (70-80%)")
    ax.axhline(TARGET, color="#2e7d32", ls="--", lw=1.2)
    rng = np.random.default_rng(7)

    for index, arm in enumerate(arms):
        rows = read_children(directory, arm)
        values = [float(r["success_last_third"]) for r in rows]
        if not values:
            continue
        jitter = rng.uniform(-0.16, 0.16, size=len(values))
        ax.scatter(np.full(len(values), index) + jitter, values, s=12, alpha=0.55,
                   label=ARM_LABELS.get(arm, arm) if index == 0 else None)
        ax.hlines(np.mean(values), index - 0.28, index + 0.28, color="black", lw=2)

    ax.set_xticks(range(len(arms)))
    ax.set_xticklabels([ARM_LABELS.get(a, a) for a in arms], rotation=15, ha="right")
    ax.set_ylabel("per-child success rate (last third)")
    ax.set_ylim(0, 1.05)
    ax.set_title("One dot per simulated child (black bar = mean)")
    ax.grid(axis="y", alpha=0.25)
    fig.tight_layout()
    fig.savefig(out / "per_child_spread.png", dpi=150)
    plt.close(fig)


def figure_robustness(main: Path, steep: Path, out: Path) -> None:
    """Mean |deviation| from the target under two different learner models."""
    arms = [a for a in ARM_ORDER if (main / f"summary_{a}.json").exists()]
    labels, main_values, steep_values = [], [], []
    for arm in arms:
        main_summary = read_summary(main, arm)
        steep_summary = read_summary(steep, arm)
        if not main_summary or not steep_summary:
            continue
        labels.append(ARM_LABELS.get(arm, arm))
        main_values.append(float(main_summary["mean_abs_deviation_from_target"]))
        steep_values.append(float(steep_summary["mean_abs_deviation_from_target"]))

    if not labels:
        return

    positions = np.arange(len(labels))
    width = 0.38
    fig, ax = plt.subplots(figsize=(10, 4.6))
    ax.bar(positions - width / 2, main_values, width, label="learner matches engine assumptions (slope 0.9)",
           color="#1565c0")
    ax.bar(positions + width / 2, steep_values, width,
           label="learner deliberately different (slope 1.4, guess 0.15)", color="#ef6c00")
    ax.set_xticks(positions)
    ax.set_xticklabels(labels, rotation=15, ha="right")
    ax.set_ylabel("mean |success - 0.75|")
    ax.set_title("Distance from the flow-zone target (lower is better) - robustness check")
    ax.legend(fontsize=8)
    ax.grid(axis="y", alpha=0.25)
    fig.tight_layout()
    fig.savefig(out / "robustness.png", dpi=150)
    plt.close(fig)


def write_summary_table(directory: Path, out: Path) -> None:
    """Machine-readable table so the report numbers are never retyped by hand."""
    arms = present_arms(directory)
    lines = ["arm,group,children,success,success_last_third,in_flow_zone_fraction,"
             "mean_tier_first_third,mean_tier_last_third"]
    for arm in arms:
        summary = read_summary(directory, arm)
        by_group = summary.get("by_group", {}) if summary else {}
        for group in GROUPS:
            entry = by_group.get(group)
            if not entry:
                continue
            lines.append(
                f"{arm},{group},{entry['children']},{entry['success_rate']:.4f},"
                f"{entry['success_rate_last_third']:.4f},{entry['in_flow_zone_fraction']:.4f},"
                f"{entry['mean_tier_first_third']:.3f},{entry['mean_tier_last_third']:.3f}")
    (out / "summary_table.csv").write_text("\n".join(lines) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--main", default="out/main")
    parser.add_argument("--steep", default="out/steep")
    parser.add_argument("--out", default="docs/figures")
    args = parser.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    main_dir = Path(args.main)

    figure_flow_zone(main_dir, out)
    figure_pt_true(main_dir, out)
    figure_difficulty_climb(main_dir, out)
    figure_spread(main_dir, out)
    figure_robustness(main_dir, Path(args.steep), out)
    write_summary_table(main_dir, out)

    print("wrote figures to", out)
    for path in sorted(out.glob("*.png")):
        print(" ", path.name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
