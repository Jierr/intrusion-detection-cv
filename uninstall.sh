#!/bin/sh

prefix=/usr/local/bin
init_dir=/etc/init.d
service_dir=/etc/systemd/system
binary="tapocam send.sh"
init="intrusion-detection.sh"
service="intrusion-detection.service"

if [ -n "$1" ]
then
	prefix=$1
fi

systemctl stop intrusion-detection.service
systemctl disable intrusion-detection.service

for file in $binary
do
	dst=$prefix/$file
	echo Uninstalling $dst
	rm $dst
done

echo Uninstalling $init_dir/$init
rm $init_dir/$init
echo Uninstalling $service_dir/$service
rm $service_dir/$service



