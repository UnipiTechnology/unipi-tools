#!/bin/bash


docker run -it --rm -v `pwd`/../src:/src alpine:3.15 sh -c "cd /src; apk add musl-dev libmodbus-dev make gcc; make clean; make unipi_tcp_server"
