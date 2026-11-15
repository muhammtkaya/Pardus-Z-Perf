#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Birim testlerini derleyip calistirir. Depo kokunden calistirin:
#   sudo tests/run-unit-tests.sh
# cgroup/policy testleri root ve cgroup v2 gerektirir; root degilse atlanir (77).
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
S="$ROOT/src/zperfd"
OUT="${TMPDIR:-/tmp}"
CC="${CC:-gcc}"

pass=0; fail=0; skip=0

run() {  # ad  kaynak...
  local name="$1"; shift
  local bin="$OUT/$name"
  echo "=== $name ==="
  # -I"$S" tek arguman: yol bosluk icerse bile bolunmez.
  if ! "$CC" -std=c11 -Wall -Wextra -D_GNU_SOURCE -I"$S" "$@" -o "$bin"; then
    echo "  DERLEME HATASI"; fail=$((fail+1)); return
  fi
  "$bin"; local rc=$?
  if   [ $rc -eq 0 ];  then pass=$((pass+1))
  elif [ $rc -eq 77 ]; then echo "  (atlandi: root gerekli)"; skip=$((skip+1))
  else fail=$((fail+1)); fi
  echo ""
}

run test_psi_parse "$ROOT/tests/unit/test_psi_parse.c" "$S/psi.c" "$S/util.c" "$S/log.c"
run test_applist   "$ROOT/tests/unit/test_applist.c"   "$S/applist.c" "$S/util.c" "$S/log.c"
run test_ipc       "$ROOT/tests/unit/test_ipc.c"       "$S/ipc.c" "$S/util.c" "$S/log.c"
run test_cgroup    "$ROOT/tests/unit/test_cgroup.c"    "$S/cgroup.c" "$S/util.c" "$S/log.c"
run test_policy    "$ROOT/tests/unit/test_policy.c"    "$S/policy.c" "$S/applist.c" "$S/cgroup.c" "$S/util.c" "$S/log.c"

echo "SONUC: $pass gecti, $fail kaldi, $skip atlandi"
[ $fail -eq 0 ]
