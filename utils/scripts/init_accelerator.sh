#!/bin/bash

# Copyright (c) 2011-2019 Columbia University, System Level Design Group
# SPDX-License-Identifier: Apache-2.0

set -e

### Default
NAME_DEFAULT="dummy"
ESP_ROOT_DEFAULT=${PWD}
ID_DEFAULT="04A"


### Helpers
COL_RED='\033[0;31m'
COL_GREEN='\033[0;32m'
COL_CYAN='\033[0;96m'
COL_LBLUE='\033[0;44m'
COL_NORMAL='\033[0;39m'

bold=$(tput bold; echo -e $COL_CYAN)
normal=$(tput sgr0; echo -e $COL_NORMAL)
def=$(tput bold; echo -e $COL_CYAN)

die () {
    tput bold
    echo -e -n $COL_RED
    echo -n "  Error:"
    echo "${normal} ${1}"
    exit
}

warn () {
    tput bold
    echo -e -n $COL_RED
    echo -n "  Warning:"
    echo "${normal} ${1}"
}

yes_no () {
    read -p "${1} ${def}[N]${normal}: " SELECT
    SELECT=${SELECT:-N}
    case $SELECT in
	[Yy]* ) echo "y";;
	[Nn]* ) echo "n";;
	* ) echo "n";;
    esac
}

is_integer () {
    re='^[1-9][0-9]*$'
    if ! [[ $1 =~ $re ]] ; then
	echo "N"
    else
	echo "Y"
    fi
}

round_up() {
    x=$1
    y=$2
    echo $(( ((x-1) | (y-1)) + 1 ))
}

declare -A values
declare -A maxs

first_key () {
    for key in ${!values[@]}; do echo $key; break; done
}

keys_to_max () {
    for key in ${!maxs[@]}; do eval "$key"=${maxs[$key]}; done
}

keys_to_default () {
    for key in ${!maxs[@]}; do eval "$key"=${values[$key]}; done
}

echo "=== Initializing ESP accelerator template ==="
echo ""

### Acceleator name and design flow
read -p "  * Enter accelerator name ${def}[${NAME_DEFAULT}]${normal}: " NAME
NAME=${NAME:-$NAME_DEFAULT}

read -p "  * Select design flow (${bold}S${normal}tratus HLS, ${bold}V${normal}ivado HLS) ${def}[S]${normal}: " FLOW_SELECT
FLOW_SELECT=${FLOW_SELECT:-S}
case $FLOW_SELECT in
    [Ss]* ) FLOW="stratus_hls";;
    [Vv]* ) FLOW="vivado_hls";;
    * ) FLOW="stratus_hls";;
esac

read -p "  * Enter ESP path ${def}[${ESP_ROOT_DEFAULT}]${normal}: " ESP_ROOT
ESP_ROOT=${ESP_ROOT:-$ESP_ROOT_DEFAULT}

if  ! test -e ${ESP_ROOT}/accelerators/$FLOW; then
    die "${ESP_ROOT}/accelerators/$FLOW not found; ESP_ROOT not pointing to the ESP repository"
fi
if  test -e ${ESP_ROOT}/accelerators/stratus_hls/$NAME; then
    die "accelerator ${NAME} already defined"
fi
if  test -e ${ESP_ROOT}/accelerators/vivado_hls/$NAME; then
    die "accelerator ${NAME} already defined"
fi


### Basic configuration
read -p "  * Enter unique accelerator id as three hex digits ${def}[${ID_DEFAULT}]${normal}: " ID
ID=${ID:-$ID_DEFAULT}


### Configuration registers
NPARAMS=0
echo "  * Enter accelerator registers"
read -p "    - register $NPARAMS name ${def}[size]${normal}: " param
param=${param:-"size"}
while [ "${param}" != "" ]; do
    read -p "    - register $NPARAMS default value ${def}[1]${normal}: " val
    val=${val:-1}
    read -p "    - register $NPARAMS max value ${def}[${val}]${normal}: " max
    max=${max:-${val}}

    values+=( ["$param"]=$val )
    maxs+=( ["$param"]=$max )
    NPARAMS=$((NPARAMS+1))

    if [ $NPARAMS == 14 ]; then
	echo "    # Cannot specify more than 14 configuration registers"
	break;
    fi

    read -p "    - register $NPARAMS name ${def}[]${normal}: " param

done
first_param=$(first_key)
keys_to_max

### Input / Output and local memory
echo "  * Configure PLM size and create skeleton for load and store:"
read -p "    - Enter data bit-width (8, 16, 32, 64) ${def}[32]${normal}: " data_width
data_width=${data_width:-32}
while (true); do
    case $data_width in
	8 ) hsize="SIZE_BYTE"; break;;
	16 ) hsize="SIZE_HWORD"; break;;
	32 ) hsize="SIZE_WORD"; break;;
	64 ) hsize="SIZE_DWORD"; break;;
	* ) read -p "      Please enter a valid bit-width (8, 16, 32, 64) ${def}[32]${normal}: " data_width; data_width=${data_width:-32};;
    esac
done
dma_adj=$(( 8 / (data_width/8) ))

# TODO: add checks on following user input
set -f
while true; do
    read -p "    - Enter input data size in terms of configuration registers (e.g. 2 * $first_param}) ${def}[$first_param]${normal}: " data_in_size_expr
    if [ "$data_in_size_expr" == "" ]; then data_in_size_expr="$first_param"; fi
    data_in_size_max=$((data_in_size_expr))
    if [ "$(is_integer $data_in_size_max)" == "Y" ]; then
	data_in_size_max=$(round_up $data_in_size_max $dma_adj) # Make size aligned to 64-bit to prevent non-aligend offsets
	echo "      data_in_size_max = ${data_in_size_max}"; break
    else
	warn "invalid expression \"${data_in_size_expr}\""
    fi
done

while true; do
    read -p "    - Enter output data size in terms of configuration registers (e.g. 2 * $first_param) ${def}[$first_param]${normal}: " data_out_size_expr
    if [ "$data_out_size_expr" == "" ]; then data_out_size_expr="$first_param"; fi
    data_out_size_max=$((data_out_size_expr))
    if [ "$(is_integer $data_out_size_max)" == "Y" ]; then
	data_out_size_max=$(round_up $data_out_size_max $dma_adj) # Make size aligned to 64-bit to prevent non-aligend offsets
	echo "      data_out_size_max = ${data_out_size_max}"; break
    else
	warn "invalid expression \"${data_out_size_expr}\""
    fi
done

read -p "    - Enter an integer chunking factor (use 1 if you want PLM size equal to data size) ${def}[1]${normal}: " chunking_factor
chunking_factor=${chunking_factor:-1}

in_word=$(( (data_in_size_max+chunking_factor-1)/chunking_factor ))
out_word=$(( (data_out_size_max+chunking_factor-1)/chunking_factor ))
in_word=$(round_up $in_word $dma_adj)
out_word=$(round_up $out_word $dma_adj)
echo "      Input PLM has ${in_word} ${data_width}-bits words"
echo "      Output PLM has ${out_word} ${data_width}-bits words"

while true; do
    read -p "    - Enter number of input data to be processed in batch (can be function of configuration registers) ${def}[1]${normal}: " batching_factor_expr
    if [ "$batching_factor_expr" == "" ]; then batching_factor_expr="1"; fi
    batching_factor_max=$((batching_factor_expr))
    if [ "$(is_integer $batching_factor_max)" == "Y" ]; then
	echo "      batching_factor_max = ${batching_factor_max}"; break
    else
	warn "invalid expression \"${batching_factor_expr}\""
    fi
done

if [ $data_in_size_max == $data_out_size_max ]; then
    IN_PLACE=$(yes_no "    - Is output stored in place?")
else
    IN_PLACE="n"
fi
set +f

# Memomory footprint in MB
if [ "$IN_PLACE" == "y" ]; then
    memory_words=$(( batching_factor_max * data_in_size_max ))
else
    memory_words=$(( batching_factor_max * (data_in_size_max + data_out_size_max) ))
fi
memory_footprint=$(( memory_words * (data_width/8) ))
TLB_ENTRIES=$(( (memory_footprint + 1048575) / 1048576 ))
if (( $TLB_ENTRIES < 4 )); then TLB_ENTRIES=4; fi;


### Generate accelerator skeleton
CURR_DIR=${PWD}
cd $ESP_ROOT

FLOW_DIR=$FLOW
TEMPLATES_DIR=$ESP_ROOT/utils/scripts/templates/$FLOW

if [ "$FLOW" == "stratus_hls" ]; then

    dirs="src  stratus  tb"

elif [ "$FLOW" == "vivado_hls" ]; then

    dirs="src  inc  syn  tb"

fi

LOWER=$(echo $NAME | awk '{print tolower($0)}')
UPPER=$(echo $LOWER | awk '{print toupper($0)}')

ACC_DIR=$ESP_ROOT/accelerators/$FLOW_DIR/$LOWER

## initialize all design folders
for d in $dirs; do
    mkdir -p $ACC_DIR/$d
    cd $ACC_DIR/$d
    cp $TEMPLATES_DIR/$d/* .

    if cat /etc/os-release | grep -q -i ubuntu; then
        rename "s/accelerator/$LOWER/g" *
    fi

    if cat /etc/os-release | grep -q -i centos; then
        rename accelerator $LOWER *
    fi

    sed -i "s/<accelerator_name>/$LOWER/g" *
    sed -i "s/<ACCELERATOR_NAME>/$UPPER/g" *

    if [[ "$FLOW" == "stratus_hls" && "$d" == "stratus" ]]; then
	ln -s ../../common/stratus/Makefile
    fi

    if [[ "$FLOW" == "vivado_hls" && "$d" == "syn" ]]; then
	ln -s ../../common/syn/Makefile
	ln -s ../../common/syn/common.tcl
    fi
done


if [ "$FLOW" == "stratus_hls" ]; then
    ## Initialize SystemC execution folder (no HLS license required)
    mkdir -p $ACC_DIR/sim
    cd $ACC_DIR/sim
    echo "include ../../common/systemc.mk" > Makefile
    echo "$LOWER" > .gitignore
fi

if [ "$FLOW" == "vivado_hls" ]; then
    ## Initialize gitignore
    cd $ACC_DIR
    echo "$LOWER" > .gitignore
    echo "*.log" >> .gitignore
fi


## initialize xml file
cd $ACC_DIR
echo '<?xml version="1.0" encoding="UTF-8"?>' > $LOWER.xml
echo '<sld>' >> $LOWER.xml
echo -n  "  <accelerator name=\"${LOWER}\" " >> $LOWER.xml
echo -n                 "desc=\"Accelerator ${UPPER}\" " >> $LOWER.xml
echo -n                 "data_size=\"${TLB_ENTRIES}\" " >> $LOWER.xml
echo -n                 "device_id=\"${ID}\" " >> $LOWER.xml
echo                    "hls_tool=\"${FLOW}\">" >> $LOWER.xml
for key in ${!values[@]}; do
    echo -n  "    <param name=\"${key}\" " >> $LOWER.xml
    echo -n             "desc=\"${key}\" " >> $LOWER.xml
    echo "/>" >> $LOWER.xml
done
echo '  </accelerator>' >> $LOWER.xml
echo '</sld>' >> $LOWER.xml


## customize configuration registers
indent="\ \ \ \ \ \ \ \ "
if [ "$FLOW" == "stratus_hls" ]; then
    cd $ACC_DIR/src
    for key in ${!values[@]}; do
	if [ $key == $(first_key) ]; then sep=""; else sep=", "; fi
	sed -i "/\/\* <<--ctor-->> \*\//a ${indent}this->${key} = ${values[$key]};" ${LOWER}_conf_info.hpp
	sed -i "/\/\* <<--ctor-args-->> \*\//a ${indent}int32_t ${key}${sep}" ${LOWER}_conf_info.hpp
	sed -i "/\/\* <<--ctor-custom-->> \*\//a ${indent}this->${key} = ${key};" ${LOWER}_conf_info.hpp
	sed -i "/\/\* <<--eq-->> \*\//a ${indent}if (${key} != rhs.${key}) return false;" ${LOWER}_conf_info.hpp
	sed -i "/\/\* <<--assign-->> \*\//a ${indent}${key} = other.${key};" ${LOWER}_conf_info.hpp
	sed -i "/\/\* <<--print-->> \*\//a ${indent}os << \"${key}\ = \" << conf_info.${key} << \"${sep}\";" ${LOWER}_conf_info.hpp
	sed -i "/\/\* <<--params-->> \*\//a ${indent}int32_t ${key};" ${LOWER}_conf_info.hpp
    done
fi


## PLM memories for both 32-bit and 64-bit NoC
dma_width=(32 64)
if [ "$FLOW" == "stratus_hls" ]; then

    # accelerator.hpp
    cd $ACC_DIR/src
    indent="\ \ \ \ \ \ \ \ "
    sed -i "/\/\* <<--defines-->> \*\//a #define PLM_IN_WORD $in_word" ${LOWER}.hpp
    sed -i "/\/\* <<--defines-->> \*\//a #define PLM_OUT_WORD $out_word" ${LOWER}.hpp
    sed -i "/\/\* <<--defines-->> \*\//a #define DMA_SIZE $hsize" ${LOWER}.hpp
    sed -i "/\/\* <<--defines-->> \*\//a #define DATA_WIDTH $data_width" ${LOWER}.hpp

    sed -i "/\/\* <<--plm-bind-->> \*\//a ${indent}HLS_MAP_plm(plm_in_ping, PLM_IN_NAME);" ${LOWER}.hpp
    sed -i "/\/\* <<--plm-bind-->> \*\//a ${indent}HLS_MAP_plm(plm_in_pong, PLM_IN_NAME);" ${LOWER}.hpp
    sed -i "/\/\* <<--plm-bind-->> \*\//a ${indent}HLS_MAP_plm(plm_out_ping, PLM_OUT_NAME);" ${LOWER}.hpp
    sed -i "/\/\* <<--plm-bind-->> \*\//a ${indent}HLS_MAP_plm(plm_out_pong, PLM_OUT_NAME);" ${LOWER}.hpp

    for d in ${dma_width[@]}; do
	p=$(( (d+data_width-1)/data_width ))

	# memlist.txt
	cd $ACC_DIR
	plm_in_name=${LOWER}_plm_block_in_dma${d}
	plm_out_name=${LOWER}_plm_block_out_dma${d}
	echo "${plm_in_name} $in_word $data_width ${p}w:0r 0w:1r" >> memlist.txt
	echo "${plm_out_name} $out_word $data_width 1w:0r 0w:${p}r" >> memlist.txt

	# accelerator_directives.hpp
	cd $ACC_DIR/src
	dbpw=$(( (data_width+d-1)/d ))
	dwpb=$(( d/data_width )) # Zero on 32-bit DMA and 64-bit word
	sed -i "s/\/\* <<--dbpw${d}-->> \*\//${dbpw}/g" ${LOWER}_directives.hpp
	sed -i "s/\/\* <<--dwpb${d}-->> \*\//${dwpb}/g" ${LOWER}_directives.hpp
	sed -i "s/\/\* <<--plm_in_name${d}-->> \*\//\"${plm_in_name}\"/g" ${LOWER}_directives.hpp
	sed -i "s/\/\* <<--plm_out_name${d}-->> \*\//\"${plm_out_name}\"/g" ${LOWER}_directives.hpp
    done

fi

if [ "$FLOW" == "vivado_hls" ]; then

    # espacc_config.h
    cd $ACC_DIR/inc
    sed -i "s/\/\* <<--plm-in-word-->> \*\//$in_word/g" espacc_config.h
    sed -i "s/\/\* <<--plm-out-word-->> \*\//$out_word/g" espacc_config.h

    # espacc.h espacc.cc tb.cc
    indent="\	\ "
    cd $ACC_DIR
    for key in ${!values[@]}; do
	for f in "inc/espacc.h src/espacc.cc"; do
	    sed -i "/\/\* <<--params-->> \*\//a ${indent}const unsigned conf_info_${key}," ${f}
	done
	sed -i "/\/\* <<--params-->> \*\//a ${indent}const unsigned ${key} = ${values[$key]};" tb/tb.cc
	sed -i "/\/\* <<--local-params-->> \*\//a ${indent}const unsigned ${key} = conf_info_${key};" src/espacc.cc
	sed -i "/\/\* <<--compute-params-->> \*\//a ${indent}const unsigned ${key}," src/espacc.cc
	for f in "tb/tb.cc src/espacc.cc"; do
	    sed -i "/\/\* <<--args-->> \*\//a ${indent}${indent}${key}," ${f}
	done
    done

    if [ "$IN_PLACE" == "y" ]; then
	sed -i "s/\/\* <<--store-offset-->> \*\//0/g" src/espacc.cc
    else
	sed -i "s/\/\* <<--store-offset-->> \*\//store_offset/g" src/espacc.cc
    fi
    if [ "$IN_PLACE" == "y" ]; then
	sed -i "s/\/\* <<--store-offset-->> \*\//0/g" tb/tb.cc
    else
	sed -i "s/\/\* <<--store-offset-->> \*\//dma_in_size/g" tb/tb.cc
    fi
    for f in "tb/tb.cc src/espacc.cc"; do
	sed -i "s/\/\* <<--number of transfers-->> \*\//${batching_factor_expr}/g" ${f}
	sed -i "s/\/\* <<--data_in_size-->> \*\//${data_in_size_expr}/g" ${f}
	sed -i "s/\/\* <<--data_out_size-->> \*\//${data_out_size_expr}/g" ${f}
	sed -i "s/\/\* <<--chunking-factor-->> \*\//${chunking_factor}/g" ${f}
    done

    # syn/custom.tcl
    cd $ACC_DIR
    if [ $data_width == 64 ]; then
	dma_allowed="64"
    else
	dma_allowed="32 64"
    fi
    sed -i "s/<<--dma-width-->>/${dma_allowed}/g" syn/custom.tcl
    sed -i "s/<<--data-widths-->>/${data_width}/g" syn/custom.tcl
fi


## Load/Store
if [ "$FLOW" == "stratus_hls" ]; then

    # accelerator.cpp
    cd $ACC_DIR/src
    for key in ${!values[@]}; do
	indent="\ \ \ \ "
	sed -i "/\/\* <<--params-->> \*\//a ${indent}int32_t ${key};" ${LOWER}.cpp
	indent="\ \ \ \ \ \ \ \ "
	sed -i "/\/\* <<--local-params-->> \*\//a ${indent}${key} = config.${key};" ${LOWER}.cpp
    done
    sed -i "s/\/\* <<--number of transfers-->> \*\//${batching_factor_expr}/g" ${LOWER}.cpp
    sed -i "s/\/\* <<--data_in_size-->> \*\//${data_in_size_expr}/g" ${LOWER}.cpp
    sed -i "s/\/\* <<--data_out_size-->> \*\//${data_out_size_expr}/g" ${LOWER}.cpp
    if [ "$IN_PLACE" == "y" ]; then
	sed -i "s/\/\* <<--store-offset-->> \*\//0/g" ${LOWER}.cpp
    else
	sed -i "s/\/\* <<--store-offset-->> \*\//store_offset/g" ${LOWER}.cpp
    fi
fi


### Generate skeleton for unit testbench
if [ "$FLOW" == "stratus_hls" ]; then

    # system.hpp
    cd $ACC_DIR/tb
    sed -i "s/\/\* <<--mem-footprint-->> \*\//${memory_footprint}/g" system.hpp
    sed -i "s/\/\* <<--data-width-->> \*\//${data_width}/g" system.hpp
    for key in ${!values[@]}; do
	indent="\ \ \ \ "
	sed -i "/\/\* <<--params-->> \*\//a ${indent}int32_t ${key};" system.hpp
	indent="\ \ \ \ \ \ \ \ "
	sed -i "/\/\* <<--params-default-->> \*\//a ${indent}${key} = ${values[$key]};" system.hpp
    done

    # system.cpp
    sed -i "s/\/\* <<--data-width-->> \*\//${data_width}/g" system.cpp
    sed -i "s/\/\* <<--number of transfers-->> \*\//${batching_factor_expr}/g" system.cpp
    sed -i "s/\/\* <<--data_in_size-->> \*\//${data_in_size_expr}/g" system.cpp
    sed -i "s/\/\* <<--data_out_size-->> \*\//${data_out_size_expr}/g" system.cpp
    for key in ${!values[@]}; do
	indent="\ \ \ \ \ \ \ \ "
	sed -i "/\/\* <<--params-->> \*\//a ${indent}config.${key} = ${key};" system.cpp
    done
    if [ "$IN_PLACE" == "y" ]; then
	sed -i "s/\/\* <<--store-offset-->> \*\//0/g" system.cpp
    else
	sed -i "s/\/\* <<--store-offset-->> \*\//in_size/g" system.cpp
    fi
fi


### Device driver folders initialization
TEMPLATES_DIR=$ESP_ROOT/utils/scripts/templates/drivers
SOFT_DIR=$ESP_ROOT/soft
CORE_MAIN="leon3"
dirs="barec app linux"

## initialize all driver folders
cd $SOFT_DIR/leon3/drivers/include
cp $TEMPLATES_DIR/include/accelerator.h .

if cat /etc/os-release | grep -q -i ubuntu; then
    rename "s/accelerator/$LOWER/g" accelerator.h
fi

if cat /etc/os-release | grep -q -i centos; then
    rename accelerator $LOWER accelerator.h
fi

sed -i "s/<accelerator_name>/$LOWER/g" ${LOWER}.h
sed -i "s/<ACCELERATOR_NAME>/$UPPER/g" ${LOWER}.h
for d in $dirs; do
    new_dir=$SOFT_DIR/leon3/drivers/$LOWER/$d
    mkdir -p $new_dir
    cd $new_dir
    cp $TEMPLATES_DIR/$d/* .

    if cat /etc/os-release | grep -q -i ubuntu; then
        rename "s/accelerator/$LOWER/g" *
    fi

    if cat /etc/os-release | grep -q -i centos; then
	rename accelerator $LOWER *
    fi

    sed -i "s/<accelerator_name>/$LOWER/g" *
    sed -i "s/<ACCELERATOR_NAME>/$UPPER/g" *
done

for core in "ariane"; do
    cd $SOFT_DIR/$core/drivers/include
    ln -s ../../../leon3/drivers/include/${LOWER}.h
    for d in $dirs; do
	new_dir=$SOFT_DIR/$core/drivers/$LOWER/$d
	mkdir -p $new_dir
	cd $new_dir
	for f in $(ls ../../../../leon3/drivers/$LOWER/$d); do
	    ln -s ../../../../leon3/drivers/$LOWER/$d/$f
	done
    done
done

## Linux driver include file
cd $SOFT_DIR/leon3/drivers/include
indent="\	"
for key in ${!values[@]}; do
    sed -i "/\/\* <<--regs-->> \*\//a ${indent}unsigned ${key};" ${LOWER}.h
done

## Linux and baremetal drivers
cd $SOFT_DIR/leon3/drivers/$LOWER
indent="\	"
sed -i "s/\/\* <<--id-->> \*\//${ID}/g" linux/${LOWER}.c

user_reg_offset=64
for key in ${!values[@]}; do
    key_upper=$(echo $key | awk '{print toupper($0)}')
    reg_offset_hex=$(printf '%x\n' ${user_reg_offset})
    register_name="${UPPER}_${key_upper}_REG"

    for f in "linux/${LOWER}.c barec/${LOWER}.c"; do
	sed -i "/\/\* <<--regs-->> \*\//a #define ${register_name} 0x${reg_offset_hex}" ${f}
    done
    sed -i "/\/\* <<--regs-config-->> \*\//a ${indent}iowrite32be(a->${key}, esp->iomem + ${register_name});" linux/${LOWER}.c
    sed -i "/\/\* <<--regs-config-->> \*\//a ${indent}${indent}iowrite32(dev, ${register_name}, ${key});" barec/${LOWER}.c
    user_reg_offset=$((user_reg_offset + 4))
done

for f in "barec/${LOWER}.c app/cfg.h"; do
    sed -i "s/\/\* <<--token-type-->> \*\//int${data_width}_t/g" ${f}
done

sed -i "s/\/\* <<--id-->> \*\//0x${ID}/g" barec/${LOWER}.c

for f in "barec/${LOWER}.c app/${LOWER}.c app/cfg.h"; do
    sed -i "s/\/\* <<--number of transfers-->> \*\//${batching_factor_expr}/g" ${f}
    sed -i "s/\/\* <<--data_in_size-->> \*\//${data_in_size_expr}/g" ${f}
    sed -i "s/\/\* <<--data_out_size-->> \*\//${data_out_size_expr}/g" ${f}
    if [ "$IN_PLACE" == "y" ]; then
	sed -i "s/\/\* <<--store-offset-->> \*\//0/g" ${f}
    else
	sed -i "s/\/\* <<--store-offset-->> \*\//in_len/g" ${f}
    fi
done

for key in ${!values[@]}; do
    key_upper=$(echo $key | awk '{print toupper($0)}')
    sed -i "/\/\* <<--params-def-->> \*\//a #define ${key_upper} ${values[$key]}" app/cfg.h
    sed -i "/\/\* <<--params-->> \*\//a const int32_t ${key} = ${key_upper};" app/cfg.h
    sed -i "/\/\* <<--params-->> \*\//a const int32_t ${key} = ${values[$key]};" barec/${LOWER}.c
    sed -i "/\/\* <<--print-params-->> \*\//a ${indent}printf(\"  .${key} = %d\\\n\", ${key});" app/${LOWER}.c
    sed -i "/\/\* <<--descriptor-->> \*\//a ${indent}${indent}.desc.${LOWER}_desc.${key} = ${key_upper}," app/cfg.h
done

## ESP library update
cd $SOFT_DIR/leon3/drivers/libesp
sed -i "/\/\/ <<--esp-ioctl-->>/a ${indent}${indent}break;" libesp.c
sed -i "/\/\/ <<--esp-ioctl-->>/a ${indent}${indent}rc = ioctl(info->fd, ${UPPER}_IOC_ACCESS, info->desc.${LOWER}_desc);" libesp.c
sed -i "/\/\/ <<--esp-ioctl-->>/a ${indent}case ${LOWER} :" libesp.c
sed -i "/\/\/ <<--esp-prepare-->>/a ${indent}${indent}${indent}break;" libesp.c
sed -i "/\/\/ <<--esp-prepare-->>/a ${indent}${indent}${indent}esp_prepare(&info->desc.${LOWER}_desc.esp);" libesp.c
sed -i "/\/\/ <<--esp-prepare-->>/a ${indent}${indent}case ${LOWER} :" libesp.c
cd $SOFT_DIR/leon3/drivers/include
sed -i "/\/\/ <<--esp-include-->>/a #include \"${LOWER}.h\"" libesp.h
sed -i "/\/\/ <<--esp-enum-->>/a ${indent}${LOWER}," libesp.h
sed -i "/\/\/ <<--esp-descriptor-->>/a ${indent}struct ${LOWER}_access ${LOWER}_desc;" libesp.h



echo ""
echo "=== Generated accelerator skeleton for $NAME ==="



cd $CURR_DIR

