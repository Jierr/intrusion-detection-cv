#! /bin/sh
### BEGIN INIT INFO
# Provides: intrusion-detection
# Required-Start: $syslog
# Required-Stop: $syslog
# Default-Start: 2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: starts /usr/local/bin/tapocam for EMail alerts during an home intrusion
# Description:
### END INIT INFO

NAME=intrusion-detection
DAEMON=/usr/local/bin/tapocam
OPTIONS="--foreground false --run-dir /tmp/${NAME}/ --url rtsp://tapocam:Tapver1fy@192.168.2.204:554/stream2 --email pierre.bockner@posteo.de --storage /media/extern-usb/Home/surveillance --scripts /usr/local/bin"

test -x $DAEMON || exit 0

case "$1" in
    start)
    echo -n "Starting intrusion-detection as service tapocam: "
    mkdir -p /tmp/${NAME} 
    start-stop-daemon --start --exec $DAEMON -- ${OPTIONS}
    echo "tapocam / intrusion-detection."
    ;;
    stop)
    echo -n "Shutting down intrusion-detection as service: "
    start-stop-daemon --stop --oknodo --retry 30 --exec $DAEMON
    echo "tapocam / intrusion-detection."
    ;;
    restart)
    echo -n "Restarting intrusion-detection as service: " 
    start-stop-daemon --stop --oknodo --retry 30 --exec $DAEMON    
    mkdir -p /tmp/${NAME} 
    start-stop-daemon --start --exec $DAEMON -- ${OPTIONS}
    echo "tapocam / intrusion-detection."
    ;;

    *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
esac
exit 0

