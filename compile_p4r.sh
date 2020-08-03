#! /bin/bash

# The script takes p4r code and generates .p4 malleable data plane and .c agent code

set -e
set -euo pipefail


output_path=out/
verbose=0

usage() { echo "Usage: $0 [-vv] [-o output_dir] input_file" 1>&2; exit 1; }
while getopts ":o:v" opt
do
  case "${opt}" in
    o ) output_path=${OPTARG};;
    v ) verbose=$(($verbose + 1));;
    \? ) usage;;
  esac
done
shift $((OPTIND-1))


if [ $# -lt 1 ]; then
  usage
fi
	
# Parse input file
input_file=$1
infile_prefix=$(basename -- "$input_file")
infile_prefix="${infile_prefix%.*}"

# Parse output director.  Ensures that output_path ends with a slash
[[ "${output_path}" != */ ]] && output_path="${output_path}/"
mkdir -p ${output_path}

output_base="${output_path}${infile_prefix}"
output_c_fn="${output_base}_mantis.c"
output_p4_fn="${output_base}_mantis.p4"
output_include_fn="${output_base}_mantis.include"

# Parse verbosity
if [ $verbose -eq 1 ]; then
  export PRINT_FLAGS=-DINFO
elif [ $verbose -ge 2 ]; then
  export PRINT_FLAGS=-DVERBOSE
fi


set -x

{ echo "==============Synthesize .p4 and .c files=============="; } 2> /dev/null
{ echo "Output base: ${output_base}"; } 2> /dev/null
make clean
make -j4
./frontend -i ${input_file} -o ${output_base}

{ echo "==============Run preprocesser and install the agent implementation=============="; } 2> /dev/null
echo "#include \"pd.h\"" >> ${output_include_fn} && cat ${output_include_fn} > ${output_path}"/p4r.c" && g++ -E ${output_c_fn} | sed 's/_MANTIS_NL_\s*;/_MANTIS_NL_/g' | sed 's/_MANTIS_NL_/\n /g' | grep "^[^#]" >> ${output_path}"/p4r.c"
rm ${output_include_fn} ${output_c_fn}

{ echo "==============Compile output p4 to tofino target with p4c=============="; } 2> /dev/null
output_namebase=$(basename ${output_base})
abs_output_p4_path=$(cd ${output_path}; pwd)"/"${output_namebase}"_mantis.p4"
./p4_14_compile.sh ${abs_output_p4_path}
