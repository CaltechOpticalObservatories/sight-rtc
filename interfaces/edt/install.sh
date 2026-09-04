#!/bin/bash

if [ -z "$MILK_ROOT" ]; then
    echo "MILK_ROOT env variable undefined! Aborting."
    exit 1
fi
ISIO_ROOT=${MILK_ROOT}/src/ImageStreamIO

if [ -z "$MILK_INSTALLDIR" ]; then
    echo "MILK_INSTALLDIR env variable undefined! Aborting."
    exit 1
fi

## Framegrabbing executable
EDT_ROOT="/opt/EDTpdv"

TARGET="hwacq-edttake"
FILES="edttake.c ${EDT_ROOT}/libpdv.a"
IFLAGS="-I${ISIO_ROOT} -I${EDT_ROOT}"
LFLAGS="-L${MILK_INSTALLDIR}/lib/" # For ImageStreamIO. DCAM is installed straight in system libs.
lFLAGS="-lm -ldl -pthread -lImageStreamIO" # pthread and NOT lpthread
OTHFLAGS="-Wall -Wno-format-truncation -Wno-format-overflow -Wl,-rpath=${MILK_INSTALLDIR}/lib:${EDT_ROOT}"

gcc -O2 -o ${TARGET} ${FILES} ${IFLAGS} ${LFLAGS} ${lFLAGS} ${OTHFLAGS}

