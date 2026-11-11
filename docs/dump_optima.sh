#!/bin/sh
# Dump every optimal configuration of the N x N x N rooks problem, twice:
# once with R_OPTIM3 alone (ground truth: one representative per orbit under
# permutation of the heights, no x/y symmetry breaking at all) and once with
# R_OPTIM1 + R_OPTIM2 + R_OPTIM3 + R_OPTIM6 (every symmetry breaker under
# test).  docs/orbits.py then checks that the second run still reaches every
# orbit of the first.
#
#   sh docs/dump_optima.sh <N> <optimum> <outdir>
#
# Works by building a copy of main.cpp in which update_best() is replaced by
# print_config(), so best_card never moves off R_MIN - 1 and *every*
# configuration with at least <optimum> rooks is printed instead of only the
# improving ones.  main.cpp itself is not modified.  R_OPTIM4 is left off: its
# bound needs R_OPTIM1, which the ground-truth run does not have.
set -e
N="$1"
OPT="$2"
OUT="${3:-.}"
test -n "$N" -a -n "$OPT" || { echo "usage: $0 <N> <optimum> [outdir]" >&2; exit 2; }
mkdir -p "$OUT"
SRC="$OUT/main_dumpall.cpp"
sed 's/c->update_best();/c->print_config();/' main.cpp > "$SRC"
CXXFLAGS="-std=c++11 -O3 -march=native -DNDEBUG -DN=$N -DR_MIN=$OPT"
g++ $CXXFLAGS -DR_OPTIM3=1 -o "$OUT/dump_truth" "$SRC" -lpthread
g++ $CXXFLAGS -DR_OPTIM1=1 -DR_OPTIM2=1 -DR_OPTIM3=1 -DR_OPTIM6=1 \
	-o "$OUT/dump_broken" "$SRC" -lpthread
echo "dumping ground truth (R_OPTIM3 alone) for N=$N, >= $OPT rooks"
"$OUT/dump_truth" > "$OUT/optima_truth_$N.txt"
echo "dumping R_OPTIM1+2+3+6 for N=$N, >= $OPT rooks"
"$OUT/dump_broken" > "$OUT/optima_sym_$N.txt"
wc -l "$OUT/optima_truth_$N.txt" "$OUT/optima_sym_$N.txt"
