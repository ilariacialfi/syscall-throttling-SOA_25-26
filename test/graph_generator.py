#!/usr/bin/env python3
#
# @file graph_generator.py
# @brief reads the two CSV files produced by run_tests.sh (in test/output/)
#        and renders two PNG charts for the report:
#          throttling_effect.png : completion time vs MAX
#          concurrency.png       : peak/avg blocked threads vs concurrency
#        Usage (from root dir): sudo python3 ./test/graph_generator.py
#        Requires: matplotlib  (sudo apt install -y python3-matplotlib)
#
# @author Ilaria Cialfi
#
# @date October 24, 2026
#
import csv
import os
import sys
import matplotlib
matplotlib.use("Agg")                      # headless: no display needed
import matplotlib.pyplot as plt

OUT = "./test/output"

def read(name):
    path = os.path.join(OUT, name)
    if not os.path.exists(path):
        sys.exit(f"missing {path} - run run_tests.sh first")
    with open(path) as f:
        return list(csv.DictReader(f))

# ---- Chart 1: completion time vs MAX ----
d = read("throttling_effect.csv")
max_vals = [int(r["max"]) for r in d]
plt.figure(figsize=(9, 6))
plt.plot(max_vals, [float(r["elapsed_s"]) for r in d], marker="o")
plt.xticks(max_vals)
plt.xlabel("MAX (invocazioni al secondo)")
plt.ylabel("Tempo di completamento (s)")
plt.title("Effetto del throttling su 50 invocazioni di getpid")
plt.grid(True, which="both", linestyle=":")
plt.tight_layout()
plt.savefig(os.path.join(OUT, "throttling_effect.png"), dpi=150)

# ---- Chart 2: blocked threads vs concurrency ----
d = read("concurrency.csv")
nproc = [int(r["nproc"]) for r in d]
plt.figure(figsize=(9, 6))
plt.plot(nproc, [float(r["peak_blocked"]) for r in d], marker="o", label="picco")
plt.plot(nproc, [float(r["avg_blocked"])  for r in d], marker="s", label="media")
plt.xticks(nproc)
plt.xlabel("Processi concorrenti")
plt.ylabel("Thread bloccati")
plt.title("Thread bloccati in funzione della concorrenza (MAX=5)")
plt.legend()
plt.grid(True, linestyle=":")
plt.tight_layout()
plt.savefig(os.path.join(OUT, "concurrency.png"), dpi=150)

print(f"Saved: {OUT}/throttling_effect.png, {OUT}/concurrency.png")
