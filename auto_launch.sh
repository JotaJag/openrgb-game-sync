#!/bin/sh
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
/usr/bin/timeout --kill-after=1s 6s /usr/bin/python3 "$root/auto_prepare.py" --prelaunch
exit 0
