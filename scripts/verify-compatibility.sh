#!/usr/bin/env bash
set -Eeuo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
mkdir -p build
run_checks() {
  date -Is
  git rev-parse HEAD
  for test in installer pipe_service hardening automation chroma_compat rest_configuration rest; do
    python3 "tests/${test}_test.py"
  done
  bash tests/host-build.sh
  QT_QPA_PLATFORM=offscreen build/integration-test
  if [[ "${1:-}" == --wine ]]; then
    python3 tests/wine_smoke.py
    python3 tests/sdk_wine_smoke.py
    python3 tests/extended_wine_smoke.py
  else
    echo 'Wine/Proton no ejecutado: usa --wine para probar los adaptadores en prefijos temporales.'
  fi
  echo 'PASS: comprobaciones solicitadas completadas. No se ha instalado ningún plugin.'
}
run_checks "$@" 2>&1 | tee build/compatibility-verification.txt
