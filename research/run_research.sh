#!/usr/bin/env bash

set -e

./build/research/research_runner
./build/research/two_level_runner

./build/research/results_analyzer
./build/research/results_analyzer \
  research/results/two_level_cost_results.csv \
  research/results/two_level_cost_leaders.csv

./build/research/three_level_runner
./build/research/results_normalizer
python3 research/plot_results.py
