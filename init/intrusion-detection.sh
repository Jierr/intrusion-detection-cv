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

DAEMON=/usr/local/bin/tapocam
OPTIONS="--url rtsp://user:password@localhost:554/stream --email <your-email> --storage <image-storage-path> --scripts /usr/local/bin"
NAME=intrusion-detection

test -x $DAEMON || exit 0

case "$1" in
    start)
    echo -n "Starting intrusion-detection as service tapocam: "
    start-stop-daemon --start --background --exec $DAEMON -- ${OPTIONS}
    echo "tapocam."
    ;;
    stop)
    echo -n "Shutting down intrusion-detection as service: "
    start-stop-daemon --stop --oknodo --retry 30 --exec $DAEMON
    echo "tapocam."
    ;;
    restart)
    echo -n "Restarting intrusion-detection as service: " 
    start-stop-daemon --stop --oknodo --retry 30 --exec $DAEMON
    start-stop-daemon --start --background --exec $DAEMON -- ${OPTIONS}
    echo "tapocam."
    ;;

    *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
esac
exit 0

