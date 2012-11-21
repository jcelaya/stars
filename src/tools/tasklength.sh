#!/bin/sh
# Extracts a list of task lengths out of a SWF file
# Task length is measured in FLOPs, but it is not found directly in the SWF format. Average CPU speed is needed.

avgspeed=$2
grep -v '^;' "$1" | sed -e 's/^ *//' -e 's/  */ /g' | while read job st wt rt p_used avg_cpu mem p_req t_req mem_req status uid gid app queue part prev_job tt; do
	[ $rt != "-1" ] && echo $(($rt * $avgspeed))
done

