#! /bin/bash

# The script takes p4r code and generates .p4 malleable data plane and .c agent code

if [ $# -ne 2 ]
  then
    echo "Usage: ./compile_p4r.sh <p4r input file path> <output file path>"
    exit
fi

output_path=$(dirname $2)

output_c_fn=$2"_mantis.c"
output_p4_fn=$2"_mantis.p4"
output_include_fn=$2"_mantis.include"

echo "==============Synthesize .p4 and .c files=============="
make clean && make -j4 &&
./frontend -i $1 -o $2 || exit

echo "==============Run preprocesser and install the agent implementation=============="
echo "#include \"pd.h\"" >> ${output_include_fn} && cat ${output_include_fn} > ${output_path}"/p4r.c" && gcc -E ${output_c_fn} | sed 's/_MANTIS_NL_\s*;/_MANTIS_NL_/g' | sed 's/_MANTIS_NL_/\n /g' | grep "^[^#]" >> ${output_path}"/p4r.c"
rm ${output_include_fn}

echo "==============compile output p4 to tofino target with p4c=============="
# Use custom p4_14 compilation script, omitted for public repo
output_namebase=$(basename $2)
abs_output_p4_path=$(cd ${output_path}; pwd)"/"${output_namebase}"_mantis.p4"
./p4_14_compile.sh ${abs_output_p4_path}