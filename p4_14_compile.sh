#! /bin/bash

# set -x

# Replace the following 2 pointers to local paths to bf-sde and bf-p4c
export SDE="/home/leoyu/bf_sde_bsp_collections/bf-sde-9.0.0"
export P4C="/home/leoyu/bf_sde_bsp_collections/bf-sde-9.0.0/install/bin/bf-p4c"

export CURR=$(pwd)
export SDE_BUILD=$SDE"/build"
export SDE_INSTALL=$SDE"/install"
# Not applied to sudo
export PATH=$SDE_INSTALL/bin:$PATH

# Requires full path of file
echo "Compile "$1
prog_name=$(basename $1 .p4)

DEFAULT_ARGS="--std p4_14 -g --verbose 3 --create-graphs --display-power-budget --arch tna --target tofino"
WITH_BFRT="--bf-rt-schema custom_build/$name.tofino/bfrt.json"
WITH_P4RT="--p4runtime-file custom_build/$name.tofino/p4info.proto.txt --p4runtime-format text"
P4_BUILD=$SDE_BUILD"/myprog"

check_stats() {
    cat<<EOF
===============     
Number of CPUs: $(nproc)
Distribution: $(uname -a)
Linux Arch: $(uname -m)
=============== 
EOF
}

check_env() {
    if [ -z $SDE ]; then
        echo "ERROR: SDE Environment variable is not set"
        exit 1
    else 
        echo "Using SDE ${SDE}"
    fi

    if [ ! -d $SDE ]; then
        echo "  ERROR: \$SDE ($SDE) is not a directory"
        exit 1
    fi

    return 0
}

build_p4_14() {

    echo "Building $1 in build_dir $P4_BUILD ... "
    
    # Better to use the p4-build infrastructure, p4c only deals with DP
    # bf-p4c $DEFAULT_ARGS -o $P4_BUILD"/"$prog_name.tofino ${CURR}/$1 

    # bf-p4c by default, no need for --with-p4c
    cd $SDE/pkgsrc/p4-build
    ./configure \
    --prefix=$SDE_INSTALL --enable-thrift --with-tofino \
    P4_NAME=$prog_name P4_PATH=$1 P4_ARCHITECTURE=tna P4_VERSION=p4-14 P4FLAGS="-g --create-graphs" &&

    make clean && make && make install &&

    echo "Install conf needed by the driver ... "
    CONF_IN=${SDE}"/pkgsrc/p4-examples/tofino/tofino_single_device.conf.in"

    if [ ! -f $CONF_IN ]; then
        cat <<EOF
ERROR: Template config.in file `$CONF_IN` missing.
EOF
        return 1
    fi

    CONF_OUT_DIR=${SDE_INSTALL}/share/p4/targets/tofino
    sed -e "s/TOFINO_SINGLE_DEVICE/${prog_name}/"  \
        $CONF_IN                                 \
        > ${CONF_OUT_DIR}/${prog_name}.conf

    return 0
}

check_stats
check_env
build_p4_14 "$@"

cd ${CURR}

exit 0
