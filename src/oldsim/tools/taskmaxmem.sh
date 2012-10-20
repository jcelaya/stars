#!/bin/sh
# Extracts a list of used memory values out of a SWF file

grep -v '^;' "$1" | sed -e 's/^ *//' -e 's/  */ /g' | cut -d ' ' -f 7 | egrep -v -e '^-1$|^0$'

