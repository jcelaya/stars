#!/bin/bash
# Extracts a list of interbatch times out of a SWF file
# The interbatch time is the time between two consecutive jobs in the same batch

prev_uid=
declare -i prev_st=0
declare -i great_et=0
grep -v '^;' "$1" | sed -e 's/^ *//' -e 's/  */ /g' | sort -n -k 12 -k 2 | while read job st wt rt p_used avg_cpu mem p_req t_req mem_req status uid gid app queue part prev_job tt; do
	if [ "$rt" = "-1" ]; then rt=0; fi
	et=$(($st + $wt + $rt))
	if [ "$prev_uid" != "$uid" -o $st -gt $great_et ]; then
		great_et=0
	else
		ibt=$(($st - $prev_st))
		# filter interbatch times greater than one hour
		[ $ibt -le 3600 ] && echo $ibt
	fi
	if [ $et -gt $great_et ]; then
		great_et=$et
	fi
	prev_st=$st
	prev_uid=$uid
done

