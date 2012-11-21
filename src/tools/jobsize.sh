#!/bin/sh
# Extracts a list of job sizes out of a SWF file
# job size is extracted from the requested number of processors

grep -v '^;' "$1" | sed -e 's/^ *//' -e 's/  */ /g' | cut -d ' ' -f 8 | grep -v -e -1

