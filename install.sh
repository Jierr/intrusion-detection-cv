#!/bin/sh -e

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

for file in $binary
do
	dst=$prefix/$file
	echo Installing $file to $dst
	cp bin/$file $dst
	chmod 755 $dst
done

for file in $init
do
	dst=$init_dir/$file
	echo Installing $file to $dst
	cp init/$file $dst
	chmod 755 $dst
done

for file in $service
do
	dst=$service_dir/$file
	echo Installing $file to $dst
	cp init/$file $dst
	systemctl enable intrusion-detection.service
	systemctl start intrusion-detection.service
done

