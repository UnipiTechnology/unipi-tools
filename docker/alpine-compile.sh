#!/bin/bash

[ -z "$1" ] && { echo "Requires image name as parameter!" >&2; exit 1; }

docker run -it --rm -v `pwd`/src:/src alpine:3.15 sh -c "cd /src; apk add musl-dev libmodbus-dev make gcc; make clean; make unipi_tcp_server"

docker build . -t "$1"
