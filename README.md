# Bin2ddr
A memory file used to convert an executable file to a DDR file

Convert generic binary files:
`./bin2ddr -i xxx.bin -o ./out.dat -m "row,ba,col,bg" ` 

`./bin2ddr -i ckpt.zstd -o ./out.dat -m "row,ba,col,bg" -r gcpt.bin` 

memh compression functions:
This function removes unused checkpoint memory by analyzing the program in the checkpoint 

(mayby need to manually update the version of nemu).

`./ready-to-run/riscv64-nemu-interpreter -b --restore ckpt.zstd -r gcpt.bin -I 40000000 --mem_use_record_file compress-out.txt`

`./bin2ddr -i ckpt.zstd -o ./out.dat -m "row,ba,col,bg" -r gcpt.bin -c compress-out.txt` 