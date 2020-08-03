#! /bin/bash

usage() { echo "Usage: sudo -E $0 <p4 prog name>" 1>&2; exit 1; }

if [ -z $SDE ]; then
    echo "ERROR: SDE Environment variable is not set"
    exit 1
fi

if [ $# -lt 1 ]; then
  usage
fi

PROG=mantis_ctl
gcc -I${SDE}/pkgsrc/bf-drivers/include -I${SDE}/install/include -Wall -Wno-missing-field-initializers -Werror -Wshadow -g -O2 -std=c99 -o ${PROG} ${PROG}.c -ldriver -lbfsys -lbf_switchd_lib -lm -ldl -lpthread -L${SDE}/install/lib -Wl,-rpath -Wl,${SDE}/install/lib

export SDE_INSTALL=$SDE"/install"
sudo ./${PROG} $SDE_INSTALL $SDE_INSTALL/share/p4/targets/tofino/$1.conf