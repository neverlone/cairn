#!/bin/sh
# Smoke test: build cairn and check every example against its expected output.
set -e
cc -O2 -Wall -Wextra -o cairn cairn.c

fail=0
check() {
    name=$1; expected=$2
    got=$(./cairn "examples/$name.cairn")
    if [ "$got" = "$expected" ]; then
        echo "ok   - $name"
    else
        echo "FAIL - $name"
        echo "  expected: $(printf '%s' "$expected" | tr '\n' '|')"
        echo "  got:      $(printf '%s' "$got" | tr '\n' '|')"
        fail=1
    fi
}

check hello     "Hello, World!"
check countdown "10
9
8
7
6
5
4
3
2
1"
check fib "0
1
1
2
3
5
8
13
21
34"
check triangle "*
**
***
****
*****"

# error cases should exit non-zero
for prog in '+' '5 0 /' '['; do
    if printf '%s' "$prog" | ./cairn >/dev/null 2>&1; then
        echo "FAIL - error case '$prog' should have failed"; fail=1
    else
        echo "ok   - error case '$prog' rejected"
    fi
done

[ "$fail" = 0 ] && echo "all tests passed" || { echo "some tests failed"; exit 1; }
