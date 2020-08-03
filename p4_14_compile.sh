#! /bin/bash

# Usage:
# sudo ./p4_14_compile.sh <p4_prog_absolute_path>

export P4C=$SDE"/install/bin/bf-p4c"
export CURR=$(pwd)
export SDE_BUILD=$SDE"/build"
export SDE_INSTALL=$SDE"/install"
export PATH=$SDE_INSTALL/bin:$PATH

prog_name=$(basename $1 .p4)
P4_BUILD=$SDE_BUILD"/mantis"

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

    cd $SDE/pkgsrc/p4-build
    ./configure \
    --prefix=$SDE_INSTALL --enable-thrift --with-tofino \
    P4_NAME=$prog_name P4_PATH=$1 P4_ARCHITECTURE=tna P4_VERSION=p4-14 P4FLAGS="-g --create-graphs" &&

    make clean && make && make install &&

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

check_env
build_p4_14 "$@"

cd ${CURR}

exit 0
