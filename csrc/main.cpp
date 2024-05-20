#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <bitset>
#include "../include/common.h"
#include "../include/load.h"
#include "../include/bin2ddr.h"

std::string input_file;
std::string gcpt_file;
std::string compress_file;
std::string output_file;
std::string addr_map = "bg,ba,row,col";
uint64_t base_address = 0;
uint64_t img_size = 0;
uint64_t gcpt_size = 0;
char *file_ram = NULL;

void show_help() {
    std::cout << "Usage: mem_pred [OPTIONS]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -i, --inputfile <FILE>    Specify the input file path" << std::endl;
    std::cout << "  -o, --outputfile <FILE>   Specify the output file path" << std::endl;
    std::cout << "  -m, --addrmap <ORDER>     Specify the address map order (default \"row,ba,col,bg\")" << std::endl;
    std::cout << "  -b, --baseaddress <ADDR>  Specify the base address in hexadecimal" << std::endl;
    std::cout << "  -c, --compress            Specify the use nemu compress checkpoint" << std::endl;
    std::cout << "  -r, --restore             Specify the gcpt-restore cover checkpoint" << std::endl; 
    std::cout << "  -h, --help                Show this help message" << std::endl;
}

int main(int argc,char *argv[]) {
    int args_pars = 0;
    args_pars = args_parsingniton(argc, argv);
    if (args_pars !=0)
      return args_pars;

    printf("load ram\n");
    img_size = load_img(input_file.c_str()) / sizeof(uint64_t);
    if (!gcpt_file.empty()) {
      gcpt_size = override_ram(gcpt_file.c_str(), 0xf00) / sizeof(uint64_t);
      printf("Overwrite %d bytes from file%s\n", gcpt_size, gcpt_file.c_str());
    }

    printf("init img size %ld\n", img_size * sizeof(uint64_t));
    uint64_t rd_num = mem_preload(output_file, base_address, addr_map, img_size, compress_file.c_str());
    printf("transform file %ld Bytes\n", rd_num * sizeof(uint64_t));
    return 0;
}

inline std::string construct_index_remp(unsigned int bg, unsigned int ba, unsigned int row, 
                                 unsigned int col_9_to_3, unsigned int col_2_to_0) {

    unsigned long hex_value = (bg << 28) + (ba << 26) + (row << 10) + (col_9_to_3 << 3) + col_2_to_0;//bs.to_ulong();

    // use std::stringstream dec to hex
    std::stringstream ss;
    ss << std::hex << hex_value;

    return ss.str();;
}

//get ddr addr
inline std::string calculate_index_hex(uint64_t index, const std::string& addr_map) {
    std::vector<std::string> components;
    std::stringstream ss(addr_map);
    std::string component;
    while (std::getline(ss, component, ',')) {
        components.push_back(component);
    }
    //printf("input index %x \n",index);
    unsigned int col_2_to_0 = index & 0x7;
    index = index >> 3;
    unsigned int bg = (index & 0x3);
    index = index >> 2;
    unsigned int col_9_to_3 = (index & 0x7F);
    index = index >> 7;
    unsigned int ba = (index & 0x3);
    index = index >> 2;
    unsigned int row = (index & 0xFFFF);
    index = index >> 10;
    //printf("debug bg %x ba %x row %x col_9_to_3 %x col_2_to_0 %x\n", bg, ba, row, col_9_to_3, col_2_to_0);
    std::string index_hex;
    index_hex = construct_index_remp(bg, ba, row, col_9_to_3, col_2_to_0);

    return index_hex;
}

//write perload
uint64_t mem_preload(const std::string& output_file, uint64_t base_address, const std::string& addr_map, uint64_t img_size, const char *compress) {
    printf("start mem preload\n");
    std::ofstream output(output_file);
    if (!output) {
        std::cerr << "Error: failed to open output file" << std::endl;
        std::exit(1);
    }
    uint64_t rd_addr = 0;
    uint64_t index = base_address;
    extern uint64_t *ram;
    std::vector<unsigned char> buffer;
    bool use_compress = (compress != NULL);
    FILE *compress_fd = fopen(compress,"r+");
    if (compress_fd == NULL)
      printf("open compress_file %s flied\n", compress);

    while (true) {
      if (use_compress) {
        if (feof(compress_fd))
          break;
        if (fscanf(compress_fd, "%d", &rd_addr) != 1)
          break;
        rd_addr = rd_addr * 4 * 1024 * 1024 / sizeof(uint64_t);
        if (rd_addr >= img_size) {
          printf("ram read addr over img size %ld\n", rd_addr);
          break;
        }

        for (size_t i = 0; i < 4 * 1024 * 1024 / sizeof(uint64_t); i++) {
          uint64_t write_addr = rd_addr + i;
          uint64_t data_byte = *(ram + write_addr);
          if (data_byte != 0) {
            std::string addr = calculate_index_hex(write_addr, addr_map);
            output << "@" << addr 
                  << " " << std::hex << std::setw(16) << std::setfill('0') << data_byte << "\n";
          }
          index ++;
        }
      } else {
        if (rd_addr >= img_size) {
          printf("ram read addr over img size \n");
          break;
        }

        uint64_t data_byte = *(ram + rd_addr);
        if (data_byte != 0) {
          std::string addr = calculate_index_hex(index, addr_map);
          output << "@" << addr 
                << " " << std::hex << std::setw(16) << std::setfill('0') << data_byte << "\n";
        }
        rd_addr += 1;
        index += 1;
      }
    }
    return index;
}

int args_error(const char *name) {
  std::cerr << "Error: no " << name << " << std::endl";
  show_help();
  return 2;
}
int args_parsingniton(int argc,char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--inputfile") == 0) {
      if (i + 1 < argc) {
        input_file = argv[++i];
      } else {
        return args_error("input file");
      }
    } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--outputfile") == 0) {
      if (i + 1 < argc) {
        output_file = argv[++i];
      } else {
        return args_error("output file");
      }
    } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--restore") == 0) {
      if (i + 1 < argc) {
        gcpt_file = argv[++i];
      } else {
        return args_error("restore");
      }
    } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--addrmap") == 0) {
      if (i + 1 < argc) {
        addr_map = argv[++i];
      } else {
        return args_error("addrmap");
      }
    } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--baseaddress") == 0) {
      if (i + 1 < argc) {
        base_address = std::stoul(argv[++i], nullptr, 16);
      } else {
        return args_error("baseaddress");
      }
    } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compress") == 0) {
      if (i + 1 < argc) {
        compress_file = argv[++i];
      } else {
        return args_error("compress file");
      }
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      show_help();
      return 0;
    } else {
      std::cerr << "Error: unknown option " << argv[i] << std::endl;
      show_help();
      return 2;
    }
  }
  return 0;
}