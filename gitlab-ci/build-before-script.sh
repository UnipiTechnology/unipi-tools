#!/bin/bash

set -o xtrace

apt update && apt install -y  libmodbus-dev libsystemd-dev libi2c-dev
