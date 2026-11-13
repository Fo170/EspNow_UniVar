#!/usr/bin/env bash
# ============================================================================
#  run.sh — recette hôte de EspNow_UniVar (aucune carte, aucun flash)
#
#  Compile test_uni_var.cpp avec un Arduino minimal (arduino_shim.h) et la
#  vraie Unity_Dictionary : on teste donc le VRAI dictionnaire d'unités.
# ============================================================================
set -euo pipefail

ICI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB="$(cd "$ICI/../.." && pwd)"
UNITY="$(cd "$LIB/../Unity_Dictionary" && pwd)"

if [ ! -f "$UNITY/Unity_Dictionary.h" ]; then
  echo "ERREUR : Unity_Dictionary introuvable dans $UNITY" >&2
  exit 1
fi

SORTIE="${TMPDIR:-/tmp}/test_uni_var"
echo "== compilation =="
g++ -std=c++17 -O2 -Wall -Wextra -Werror \
    -I "$ICI" \
    -I "$LIB/src" \
    -I "$UNITY" \
    -o "$SORTIE" "$ICI/test_uni_var.cpp"

echo "== execution =="
"$SORTIE"
