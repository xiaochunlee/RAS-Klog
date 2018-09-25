#!/bin/sh

execbin="/usr/sbin/getlog"
binconf="/etc/klog/getlog_config"

if [ ! -x "$execbin" ];then
	echo "$execbin" not exists or is not executable!
	exit
else 
	echo "Starting exec $execbin ..."
fi

if [ ! -f "$binconf" ];then
	echo We should specify the store log path or use the default path.
	$execbin
else
	echo Read config form $binconf
	$execbin
fi
