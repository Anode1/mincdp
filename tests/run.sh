#!/bin/sh
# run.sh -- the mincdp regression suite (make ut). Two kinds of tests:
#   codeut  browser-free unit tests of the client's pure helpers (always runs)
#   uiut    the client driving real headless Chrome -- interaction (hands) and a
#           screenshot (eyes). SKIPs when cc / javac / Chrome are absent.
# Exit non-zero iff a NON-skipped layer FAILS. POSIX sh.

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 1
fail=0; skip=0

layer() {  # layer NAME CMD...
    name=$1; shift
    out=$("$@" 2>&1); rc=$?
    case $rc in
        0)  printf '  %-20s PASS\n' "$name" ;;
        77) printf '  %-20s SKIP\n' "$name"; skip=$((skip + 1)) ;;
        *)  printf '  %-20s FAIL\n' "$name"; fail=$((fail + 1)); echo "$out" | sed 's/^/      /' ;;
    esac
}

codeut()   { cc -std=c99 -Wall -Wextra -o tests/codeut tests/codeut.c || return 1; ./tests/codeut; }
democ()    { cc -std=c99 -Wall -Wextra -o c/demo c/demo.c || return 1; sh demo.sh c/demo; }
demojava() { command -v javac >/dev/null 2>&1 || return 77
             javac -d java/out java/Cdp.java java/Demo.java || return 1
             sh demo.sh java -cp java/out Demo; }

echo "mincdp regression:"
layer "codeut (units)"  codeut
layer "uiut hands: C"   democ
layer "uiut hands: Java" demojava
layer "uiut eyes: shot" sh tests/shot.sh

echo
if [ $fail -eq 0 ]; then echo "OK    (skipped: $skip)"; exit 0
else echo "FAILED: $fail    (skipped: $skip)"; exit 1; fi
