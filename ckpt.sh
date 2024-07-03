#!/bin/bash
#example: sh ckpt_comp.sh -g ../test/test.gz -r ./test/gcpt.bin  
max_instrs=40000000
while getopts r:g: opt
do 
	case "${opt}" in
		r) gcpt=${OPTARG};;
		g) ckpt=${OPTARG};;
	esac
done
echo "gcpt: $gcpt";

./bin2ddr -i $ckpt -o ./out.dat -m "row,ba,col,bg" -r $gcpt