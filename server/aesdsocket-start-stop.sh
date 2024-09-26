#!/bin/sh
# daemon start and stop script
# Author: Cedric Camacho

case "$1" in
  start)
    echo "Starting aesdsocket daemon"
    start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
    ;;
  stop)
    echo "Stopping aesdsocket daemon"
    start-stop-daemon -K -n aesdsocket
    ;;
  *)
    echo "Usage: $0 {start|stop}"
  exit 1
esac

exit 0