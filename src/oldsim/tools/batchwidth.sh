#!/bin/bash
# Extracts a list of batch widths out of a SWF file
# A batch is a set of jobs sent asynchronously, that is, a job is sent before the previous finishes

prev_uid=
declare -i batch_num=0
declare -i great_et=0
grep -v '^;' "$1" | sed -e 's/^ *//' -e 's/  */ /g' | sort -n -k 12 -k 2 | while read job st wt rt p_used avg_cpu mem p_req t_req mem_req status uid gid app queue part prev_job tt; do
	if [ "$prev_uid" != "$uid" -o $st -gt $great_et ]; then
		((batch_num++))
		great_et=0
	fi
	echo $batch_num
	if [ "$rt" = "-1" ]; then rt=0; fi
	et=$(($st + $wt + $rt))
	[ $et -gt $great_et ] && great_et=$et
	prev_uid=$uid
done | uniq -c | sed -e 's/^ *//' | cut -d ' ' -f 1

