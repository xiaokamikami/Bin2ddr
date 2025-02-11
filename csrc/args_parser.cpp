#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include "../include/args_parser.h"

void show_help() {
    using namespace std;
    cout << "Usage: mem_pred [OPTIONS]" << endl;
    cout << "Options:" << endl;
    cout << "  -i, --inputfile <FILE>    Specify the input file path" << endl;
    cout << "  -o, --outputfile <FILE>   Specify the output file path" << endl;
    cout << "  -m, --addrmap <ORDER>     Specify the address map order (default \"row,ba,col,bg\")" << endl;
    cout << "  -b, --baseaddress <ADDR>  Specify the base address in hexadecimal" << endl;
    cout << "  -c, --compress            Specify the use nemu compress checkpoint" << endl;
    cout << "  -r, --restore             Specify the gcpt-restore cover checkpoint" << endl; 
    cout << "  --raw2                    Specify the use raw2 fomat out file" << endl;
    cout << "  -s, --split_rank          Split the rank in a ch into different files" << endl;
    cout << "  --overrid                 Reset the size of using the length flag in gcpt_restore" << endl;
    cout << "  -h, --help                Show this help message" << endl;
    exit(0);
}

int args_error(const char *name) {
  std::cerr << "Error: no " << name << std::endl;
  show_help();
  return 2;
}

int args_parsingniton(int argc, char *argv[]) {
  extern std::string addr_map, input_file, gcpt_file, compress_file;
  extern uint64_t base_address;
  extern uint32_t gcpt_over_size;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--inputfile") == 0) {
      if (i + 1 < argc) {
        input_file = argv[++i];
      } else {
        return args_error("input file");
      }
    } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--outputfile") == 0) {
      if (i + 1 < argc) {
        base_out_file = argv[++i];
      } else {
        return args_error("output file");
      }
    } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--restore") == 0) {
      if (i + 1 < argc) {
        gcpt_file = argv[++i];
      } else {
        return args_error("restore");
      }
      // Get the lower four bytes
      uint32_t data;
      FILE *fp = fopen(gcpt_file.c_str(), "rb");
      fseek(fp, 4, SEEK_SET);  
      if (fread(&data, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        return args_error("ailed to read 8 bytes from file");
      }
      fclose(fp);
      gcpt_over_size = data;
      printf("reset gcpt over size %d  hex:%x\n", gcpt_over_size, gcpt_over_size);
    } else if (strcmp(argv[i], "--overrid") == 0) {
      gcpt_over_size = std::stoul(argv[++i], nullptr, 16);
      printf("set gcpt_over_size %lx\n", gcpt_over_size);
    } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--addrmap") == 0) {
      if (i + 1 < argc) {
        addr_map = argv[++i];
      } else {
        return args_error("addrmap");
      }
    } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--baseaddress") == 0) {
      if (i + 1 < argc) {
        base_address = std::stoul(argv[++i], nullptr, 16);
        printf("set baseaddress %lx\n", base_address);
      } else {
        return args_error("baseaddress");
      }
    } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compress") == 0) {
      if (i + 1 < argc) {
        compress_file = argv[++i];
      } else {
        return args_error("compress file");
      }
    } else if (strcmp(argv[i], "--raw2") == 0) {
      out_raw = true;
    } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--split-rank") == 0) {
      split_rank = true;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      show_help();
    } else {
      std::cerr << "Error: unknown option " << argv[i] << std::endl;
      show_help();
    }
  }

  return 0;
}