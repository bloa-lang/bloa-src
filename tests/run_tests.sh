#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 path/to/bloa"
  exit 2
fi

BLOA="$1"
ROOT="$(cd "$(dirname "$0")" && pwd)"
TMP="$ROOT/tmp"
rm -rf "$TMP"
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

run() {
  local script="${1}"
  local expected="${2}"
  local name
  name="$(basename "$script")"
  echo "Running $name"
  local output
  output="$("$BLOA" "$script")"
  if [[ "$output" != "$expected" ]]; then
    echo "FAILED $name"
    echo "Expected:"$'\n'"$expected"
    echo "Got:"$'\n'"$output"
    exit 1
  fi
}

run "$ROOT/test_json.bloa" '[["a","b","c"],["1","2","3"]]'
run "$ROOT/test_csv.bloa" '[["a","b","c"],["1","2","3"]]'
run "$ROOT/test_misc.bloa" $'true\nYWJj\nabc\ntrue\nfoo_bar\ntrue\nbar\n.txt\n/tmp\nfoo.txt\n3\ntrue'

echo "All tests passed."
