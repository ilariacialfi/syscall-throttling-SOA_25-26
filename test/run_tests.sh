#!/usr/bin/env bash
#
# @file run_tests.sh
# @brief tests runner for the syscall throttling module: change MAX and
#        the number of concurrent processes, writing two CSV files for plotting.
#        Run from the repository root, as root, with the_usctm already loaded:
#        sudo ./test/run_tests.sh
#
# @author Ilaria Cialfi
#
# @date October 24, 2026
#
set -u
CTL=./user/throttlectl
SYS=39                       # getpid: no args, non-blocking, safe to flood
OUT=./test/output            # CSV output directory
mkdir -p "$OUT"

reset_module() {             # full reset: clears registry AND statistics
    make unmount >/dev/null 2>&1
    make mount   >/dev/null 2>&1
}

# ---- Test 1: throttling effect (completion time vs MAX) ----
N=50
echo "max,elapsed_s,peak_delay_ms" > "$OUT/throttling_effect.csv"
for MAX in 1 2 5 10 20 50 100; do
    reset_module
    $CTL on >/dev/null
    $CTL add-sys $SYS >/dev/null
    $CTL add-prog test_syscall_by_nr >/dev/null
    $CTL set-max $MAX >/dev/null

    start=$(date +%s.%N)
    ./test/test_syscall_by_nr $SYS $N
    end=$(date +%s.%N)

    elapsed=$(awk "BEGIN{printf \"%.3f\", $end-$start}")	# calculate elapsed time = end - start (3 decimals)
    
    s=$($CTL stats)
    pd=$(echo "$s" | grep -oE 'peak delay:[[:space:]]*[0-9]+' | grep -oE '[0-9]+$')	# pull the peak-delay integer from stats output
    echo "$MAX,$elapsed,${pd:-0}" >> "$OUT/throttling_effect.csv"
    echo "MAX=$MAX -> ${elapsed}s, peak ${pd:-0} ms"
done

# ---- Test 2: contention (blocked threads vs concurrency) ----
CALLS=10
echo "nproc,peak_blocked,avg_blocked" > "$OUT/concurrency.csv"
for NP in 1 2 5 10 20 30 40 50; do
    reset_module
    $CTL on >/dev/null
    $CTL add-sys $SYS >/dev/null
    $CTL add-prog test_blocked >/dev/null
    $CTL set-max 5 >/dev/null

    ./test/test_blocked $SYS $NP $CALLS

    s=$($CTL stats)
    peak=$(echo "$s" | grep -oE 'peak blocked threads:[[:space:]]*[0-9]+' | grep -oE '[0-9]+$')
    avg=$(echo  "$s" | grep -oE 'average blocked threads:[[:space:]]*[0-9.]+' | grep -oE '[0-9.]+$')
    echo "$NP,${peak:-0},${avg:-0}" >> "$OUT/concurrency.csv"
    echo "nproc=$NP -> peak ${peak:-0}, avg ${avg:-0}"
done
echo "Done: $OUT/throttling_effect.csv, $OUT/concurrency.csv"
