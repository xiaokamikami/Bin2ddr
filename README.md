# Bin2ddr
A memory file used to convert an executable file to a DDR file

## Project Dependencies 
This project requires the following third-party libraries
### Boost library
#### Ubuntu/Debian
```bash
sudo apt install libboost-all-dev
```

## use to normal sim-ddr :
Convert generic binary files:

`./bin2ddr -i xxx.bin -o ./out.dat -m "row,ba,col,bg" `

`./bin2ddr -i ckpt.zstd -o ./out.dat -m "row,ba,col,bg" -r gcpt.bin`

## use to fpga :

`make FPGA=1`

` ./bin2ddr -i ready-to-run/linux-xs.bin -o liunx.bin -m "row,ba,col,bg"`

## use to memh compression :
This function removes unused checkpoint memory by analyzing the program in the checkpoint

Use `make nemu-update` before use to update the nemu file used

### example
`./ready-to-run/riscv64-nemu-interpreter -b --restore ckpt.zstd -r gcpt.bin -I 40000000 --mem_use_record_file compress-out.txt`

`./bin2ddr -i ckpt.zstd -o ./out.dat -m "row,ba,col,bg" -r gcpt.bin -c compress-out.txt`
