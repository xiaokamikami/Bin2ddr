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
#include <endian.h>

#define COMPRESS_SIZE (4 * 1024 * 1024)
std::string input_file;
std::string gcpt_file;
std::string compress_file;
std::string output_file;
std::string addr_map = "bg,ba,row,col";
uint64_t base_address = 0;
uint64_t img_size = 0;
uint64_t gcpt_size = 0;
bool out_raw = false;
char *file_ram = NULL;
uint64_t *temp_ram = (uint64_t *)malloc(GB_8_SIZE);
typedef struct {
  uint8_t bg;
  uint8_t ba;
  uint8_t row;
  uint8_t col;
}addr_map_info;
addr_map_info addr_map_order;

void show_help() {
    std::cout << "Usage: mem_pred [OPTIONS]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -i, --inputfile <FILE>    Specify the input file path" << std::endl;
    std::cout << "  -o, --outputfile <FILE>   Specify the output file path" << std::endl;
    std::cout << "  -m, --addrmap <ORDER>     Specify the address map order (default \"row,ba,col,bg\")" << std::endl;
    std::cout << "  -b, --baseaddress <ADDR>  Specify the base address in hexadecimal" << std::endl;
    std::cout << "  -c, --compress            Specify the use nemu compress checkpoint" << std::endl;
    std::cout << "  -r, --restore             Specify the gcpt-restore cover checkpoint" << std::endl; 
    std::cout << "  --raw2                    Specify the use raw2 fomat out file" << std::endl;
    std::cout << "  -h, --help                Show this help message" << std::endl;
}

int main(int argc,char *argv[]) {
    int args_pars = 0;
    args_pars = args_parsingniton(argc, argv);
    if (args_pars !=0)
      return args_pars;

    std::vector<std::string> components;
    std::stringstream ss(addr_map);
    std::string component;
    printf("use addr map");
    while (std::getline(ss, component, ',')) {
      components.push_back(component);
      static int count = 0;
      count ++;
      if (component == "bg")
        addr_map_order.bg = count;
      else if(component == "ba")
        addr_map_order.ba = count;
      else if(component == "row")
        addr_map_order.row = count;
      else if(component == "col")
        addr_map_order.col = count;
      std::cout << " " << component << "=" << count;
    }
    printf("\nstart load ram\n");
    img_size = load_img(input_file.c_str()) / UINT64_SIZE;
    if (!gcpt_file.empty()) {
      gcpt_size = override_ram(gcpt_file.c_str(), 0x1100) / UINT64_SIZE;
      printf("Overwrite %d bytes from file%s\n", gcpt_size, gcpt_file.c_str());
    }

    printf("init img size %ld\n", img_size * UINT64_SIZE);
    uint64_t rd_num = mem_preload(output_file, base_address, img_size, compress_file);
    printf("transform file %ld Bytes\n", rd_num * UINT64_SIZE);
    return 0;
}

inline uint64_t construct_index_remp(unsigned int bg, unsigned int ba, unsigned int row, 
                                 unsigned int col_9_to_3, unsigned int col_2_to_0) {

    unsigned long hex_value = (bg << 28) + (ba << 26) + (row << 10) + (col_9_to_3 << 3) + col_2_to_0;
    return hex_value;
}

//get ddr addr
inline uint64_t calculate_index_hex(uint64_t index) {
    // basic col [2:0]
    unsigned int col_2_to_0 = index & 0x7;
    index = index >> 3;
    unsigned int bg = 0,ba = 0,col_9_to_3 = 0,row = 0;
    for (int i = 4; i > 0; i--) {
      if (addr_map_order.ba == i) {
        ba = (index & 0x3);
        index = index >> 2;
      } else if (addr_map_order.bg == i) {
        bg = (index & 0x3);
        index = index >> 2;
      } else if (addr_map_order.col == i) {
        col_9_to_3 = (index & 0x7F);
        index = index >> 7;
      } else if (addr_map_order.row == i) {
        row = (index & 0xFFFF);
        index = index >> 10;
      }
    }
    //printf("debug bg %x ba %x row %x col_9_to_3 %x col_2_to_0 %x\n", bg, ba, row, col_9_to_3, col_2_to_0);
    uint64_t index_hex;
    index_hex = construct_index_remp(bg, ba, row, col_9_to_3, col_2_to_0);

    return index_hex;
}

inline void mem_out_hex(std::ofstream& output, uint64_t rd_addr, uint64_t index) {
  extern uint64_t *ram;
  uint64_t data_byte = *(ram + rd_addr);
  if (data_byte != 0) {
    uint64_t addr = calculate_index_hex(index);
    output << "@" << addr 
           << " " << std::hex << std::setw(16) << std::setfill('0') << data_byte << "\n";
  }
}

uint64_t mem_out_raw2(std::ofstream& output) {
  extern uint64_t *ram;
  for (size_t i = 0; i <= img_size; i++) {
    uint64_t data_byte = *(ram + i);
    if (data_byte != 0)
      data_byte = htobe64(data_byte);
      uint64_t addr_map = calculate_index_hex(i);
      if (addr_map > GB_8_SIZE / UINT64_SIZE)
        printf("addr map over size \n");
        // input temp map RAM
        *(temp_ram + addr_map) = data_byte;
  }
  printf("addr map ok\n");
  for (size_t i = 0; i < GB_8_SIZE / UINT64_SIZE; i++) {
    uint64_t data_byte = *(temp_ram + i);
    output.write(reinterpret_cast<const char*>(&data_byte), sizeof(data_byte));
  }
  return 0;
}

//write perload
uint64_t mem_preload(const std::string& output_file, uint64_t base_address, uint64_t img_size, const std::string& compress) {
    printf("start mem preload\n");
    std::ofstream output(output_file, std::ios::out);
    if (!output) {
        std::cerr << "Error: failed to open output file" << std::endl;
        std::exit(1);
    }
    uint64_t rd_addr = 0;
    uint64_t index = base_address;

    bool use_compress = (!compress.empty());
    FILE *compress_fd = NULL;
    if (use_compress) {
      compress_fd = fopen(compress.c_str(),"r+");
      if (compress_fd == NULL)
        printf("open compress_file %s flied\n", compress.c_str());
      else
        printf("use compress_file %s load ram\n", compress.c_str());
    }

    while (true) {
      if (use_compress) {
        // out compress dat
        if (feof(compress_fd))
          break;
        if (fscanf(compress_fd, "%d", &rd_addr) != 1)
          break;
        rd_addr = rd_addr * COMPRESS_SIZE / UINT64_SIZE;
        if (rd_addr >= img_size) {
          printf("ram read addr over img size %ld\n", rd_addr);
          break;
        }

        for (size_t i = 0; i < COMPRESS_SIZE / UINT64_SIZE; i++) {
          uint64_t write_addr = rd_addr + i;
          mem_out_hex(output, rd_addr, write_addr);
          index ++;
        }
      } else if (out_raw) {
        // out raw2
        mem_out_raw2(output);
        return img_size;
      } else {
        // out dat
        if (rd_addr >= img_size) {
          printf("ram read addr over img size \n");
          break;
        }
        mem_out_hex(output, rd_addr, index);
        rd_addr += 1;
        index += 1;
      }
    }
    return index;
}

int args_error(const char *name) {
  std::cerr << "Error: no " << name << std::endl;
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
    } else if (strcmp(argv[i], "--raw2") == 0) {
      out_raw = true;
      return 0;
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