#! /bin/bash

usage() { echo "Usage: sudo -E $0 <malleable p4 prog name> <p4r.c path>" 1>&2; exit 1; }

if [ -z $SDE ]; then
    echo "ERROR: SDE Environment variable is not set"
    exit 1
fi

if [ $# -lt 2 ]; then
  usage
fi

name=$(basename $1 .p4)

export CURR=$(pwd)
export CONTEXT_PATH=$CURR"/context/"
export PATH=$PATH:$CONTEXT_PATH/bin

set -e

mkdir -p ${CONTEXT_PATH}

{ echo "======Install stub apis to mantis folder======"; } 2> /dev/null
python ${SDE}/pkgsrc/bf-drivers/pd_api_gen/generate_tofino_pd.py --context_json $SDE/install/share/tofinopd/${name}/context.json -o tmp
sed -i "1s/.*/#include \"pd.h\"/" tmp/src/pd.c
sed -i 's/p4_pd_dev_target_t/dev_target_t/g' tmp/src/pd.c
sed -i 's/p4_pd_dev_target_t/dev_target_t/g' tmp/pd/pd.h
cp tmp/src/pd.c ${CONTEXT_PATH}
cp tmp/pd/pd.h ${CONTEXT_PATH}
rm -rf tmp

{ echo "======Example program independent set up: set up ports for connectivities======"; } 2> /dev/null
python pi_setup.py

{ echo "======Make sure the shared object is update-to-date======"; } 2> /dev/null
cp $2 ${CONTEXT_PATH}"p4r.c"
cd ${CONTEXT_PATH}
gcc -w -c -fPIC p4r.c -std=c99 -o p4r.o -I$SDE/pkgsrc/bf-drivers/include/
gcc -shared -std=c99 -o p4r.so p4r.o

{ echo "======Trigger prologue======"; } 2> /dev/null
mantis_pid=$(ps -a | grep "mantis_ctl" | tr -s " " | cut -d " " -f 1)
if [ -z ${mantis_pid} ]
then
    mantis_pid=$(ps -a | grep "mantis_ctl" | tr -s " " | cut -d " " -f 2)
fi
sudo kill -USR1 ${mantis_pid}
