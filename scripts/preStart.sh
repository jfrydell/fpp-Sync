#!/bin/sh

echo "Running fpp-LoRa PreStart Script"

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make "SRCDIR=${SRCDIR}"
