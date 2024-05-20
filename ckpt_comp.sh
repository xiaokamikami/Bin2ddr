#!/bin/bash
#example: sh ckpt_comp.sh -g ../test/test.gz -r ./test/gcpt.bin  
nemu="./ready-to-run/riscv64-nemu-interpreter"
compress="./out/compress-out.txt"
max_instrs=40000000
while getopts n:c:r:g: opt
do 
	case "${opt}" in
		n) $nemu=${OPTARG};;
        c) $compress=${OPTARG};; 
		r) gcpt=${OPTARG};;
		g) ckpt=${OPTARG};;
	esac
done
echo "gcpt: $gcpt";

$nemu -b --restore $ckpt -r $gcpt -I $max_instrs --mem_use_record_file $compress
./bin2ddr -i $ckpt -o ./out.dat -m "row,ba,col,bg" -r $gcpt -c $compress