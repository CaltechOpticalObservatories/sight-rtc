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

TARGET="hwint-bmc1k"
FILES="run_1k.c"

#BMC_DIR="${SCEXAO_HW}/drivers/bmc2k"
BMC_DIR="/opt/Boston_Micromachines/sys"

IFLAGS="-I${ISIO_ROOT} -I${BMC_DIR}/src/inc"
LFLAGS="-L${BMC_DIR}/lib/ -L${MILK_INSTALLDIR}/lib/"
lFLAGS="-lrt -lbmcmd -lm -lpthread -lImageStreamIO"
OTHFLAGS="-Wall -Wl,-rpath=${MILK_INSTALLDIR}/lib:${BMC_DIR}/lib/"

## Executable
gcc -O2 -o ${TARGET} ${FILES} ${IFLAGS} ${LFLAGS} ${lFLAGS} ${OTHFLAGS}

# Formerly, we had a chown root:root and a chmod u+s. Unclear that we should keep that.
# Meaning there's no way to set RTprio and cpuset internally. Let milk have it.

#mkdir -p ${SCEXAO_HW}/bin/
#mv ${TARGET} ${SCEXAO_HW}/bin/
