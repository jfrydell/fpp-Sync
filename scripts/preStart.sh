#!/bin/sh

echo "Running fpp-Sync PreStart Script"

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make "SRCDIR=${SRCDIR}"
