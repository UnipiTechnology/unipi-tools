# Unipi Tools for Unipi PLC

Basic tools for communicating with I/O on Unipi PLC (Neuron, Axon, Patron, Iris)

- unipi_modbus_tcp - Modbus TCP server allowing access to I/O
- fwspi - firmware update utility to flash internal modules via SPI
- fwserial - firmware update utility to flash Unipi Extensions connected via RS-485
- fwi2c - firmware update utility to flash internal modules through I2C
- libunipichannel.so - library to directly talk to I/O via kernel modules

## Building

Required tools and libraries for successful build:
  - libmodbus libsystemd libmhash
  - pkg-config autotools autoconf automake libtool

On Debian install this packages.
```
apt-get install libmodbus-dev libsystemd-dev libmhash-dev libi2c-dev
apt-get install pkg-config autotools-dev autoconf automake libtool
```
Configure build and install tools:
```
./autogen.sh
./configure --prefix=/usr --sysconfdir=/etc 
make
make install
```

## License
License GPL-2.0

License LGPL-2.1 for library 
