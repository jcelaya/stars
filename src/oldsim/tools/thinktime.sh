#!/bin/sh
# Extracts a list of think times out of a SWF file
# think times are the time lapse between two consecutive jobs, when this job is under 20 minutes

prev_uid=
prev_et=
grep -v '^;' "$1" | sed -e 's/^ *//' -e 's/  */ /g' | sort -n -k 12 -k 2 | while read job st wt rt p_used avg_cpu mem p_req t_req mem_req status uid gid app queue part prev_job tt; do
	if [ "$prev_uid" = "$uid" ]; then
		tt=$(($st - $prev_et))
		[ $tt -gt 0 -a $tt -le 1200 ] && echo $tt
	fi
	if [ "$rt" = "-1" ]; then rt=0; fi
	prev_et=$(($st + $wt + $rt))
	prev_uid=$uid
done

