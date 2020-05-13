#!/bin/sh

fgrep '"MPN"' board.kicad_sch | awk '{ print $3; }' | sort | uniq -c | sed -e 's/\([0-9][0-9]*\) /\1,/' | sed -e 's/^ *//'

