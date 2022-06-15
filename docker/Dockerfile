FROM alpine:3.15

WORKDIR /
RUN apk add libmodbus

COPY `pwd`/src/unipi_tcp_server /usr/bin/unipi_tcp_server
CMD unipi_tcp_server -l 0.0.0.0

#docker build . -t unipi-modbus:devel
#docker run -it --rm --volume `pwd`/docker.evok.iris:/etc/evok.conf  evok-test:devel
