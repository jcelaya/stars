#!/bin/sh
# Extracts a list of task deadlines out of a SWF file
# Task deadline is expressed as the ratio between actual runtime and requested runtime.

( echo '3k '
grep -v '^;' "$1" | sed -e 's/^ *//' -e 's/  */ /g' | sort -n -k 1 | while read job st wt rt p_used avg_cpu mem p_req t_req mem_req status uid gid app queue part prev_job tt; do
	[ $rt != "-1" -a $t_req != "-1" -a $rt != "0" ] && echo "$t_req $rt /pc"
done
) | dc

