#
# Regular cron jobs for the picar package
#
0 4	* * *	root	[ -x /usr/bin/picar_maintenance ] && /usr/bin/picar_maintenance
