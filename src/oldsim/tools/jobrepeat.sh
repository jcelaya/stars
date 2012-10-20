#!/bin/bash
# Extracts a list of job repetitions out of a SWF file
# jobs are repeated when consecutive jobs have the same parameters
# a value of one means no repetition

grep -v '^;' "$1" | sed -e 's/^ *//' -e 's/  */ /g' | sort -n -k 12 -k 2 | cut -d ' ' -f 8-10,12,14 | uniq -c | sed 's/^ *//' | cut -d ' ' -f 1

