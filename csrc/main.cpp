#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <endian.h>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <array>
#include <fstream>
#include "../include/common.h"
#include "../include/load.h"
#include "../include/bin2ddr.h"
#ifdef PERF
#include <chrono>
#endif
#define STREAM_BUFFER_SIZE 1024 * 1024 * 256
#define COMPRESS_SIZE (4 * 1024 * 1024)

//base addr map
std::string addr_map = "bg,ba,row,col";

std::string input_file;
std::string gcpt_file;
std::string compress_file;
std::array<std::ofstream, MAX_FILE> output_files;
uint64_t base_address = 0;
uint64_t img_size = 0;
bool out_raw = false;
bool split_rank = false;
char *base_out_file = NULL;
uint32_t gcpt_over_size = 1024 * 1024 -1;
uint64_t *raw2_ram[MAX_FILE] = {};
uint8_t channel_num = 1;
uint8_t rank_num = 1;

void set_ddrmap();

typedef struct {
  uint8_t bg, ba, row, col;
  uint8_t dch, ra;
  uint8_t use_size;
}addr_map_info;
addr_map_info addr_map_order;

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

uint8_t need_files = 0;
int main(int argc, char *argv[]) {
    int args_pars = 0;
    args_pars = args_parsingniton(argc, argv);
    if (args_pars !=0)
      return args_pars;
#ifdef PERF
    auto start = std::chrono::high_resolution_clock::now();
#endif
    set_ddrmap();

    printf("\nStart load ram\n");
    img_size = load_img(input_file.c_str(), channel_num, rank_num) / UINT64_SIZE;
    if (!gcpt_file.empty()) {
      uint64_t gcpt_size = override_ram(gcpt_file.c_str(), gcpt_over_size);
      printf("Overwrite %d bytes from file%s\n", gcpt_size, gcpt_file.c_str());
    }

    printf("Init img size %ld\n", img_size * UINT64_SIZE);
    uint64_t rd_num = mem_preload(base_address, img_size, compress_file);
    printf("Transform file %ld Bytes\n", rd_num * UINT64_SIZE);
#ifdef PERF
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "run time: " << duration.count() << " ms" << std::endl;
#endif
    finish_ram();
    return 0;
}

inline uint64_t construct_index_remp(uint32_t bg, uint32_t ba, uint32_t row, 
                                 uint32_t col_9_to_3, uint32_t col_2_to_0, uint32_t rank) {
  if (split_rank == false && rank_num > 1)
    return (rank << 30) | (bg << 28) | (ba << 26) | (row << 10) | (col_9_to_3 << 3) | col_2_to_0;
  else
    return (bg << 28) | (ba << 26) | (row << 10) | (col_9_to_3 << 3) | col_2_to_0;
}

void set_ddrmap() {
    using namespace std;
    vector<string> components;
    stringstream ss(addr_map);
    string component;

    printf("Use addr map");
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
      else if(component == "ch") {
        channel_num = 2;
        addr_map_order.dch = count;
      } else if(component == "ra") {
        rank_num = 2;
        addr_map_order.ra = count;
      }
      cout << " " << component << "=" << count;
      addr_map_order.use_size = count;
    }
    need_files =  channel_num * (split_rank ? rank_num : 1);
    printf("\nOut file num %d\n", need_files);
    if (need_files > 1) {
      for (int idx = 0; idx < need_files; idx++) {
        char end_name[32];
        char first_name[32];
        char file_name[128];
        sscanf(base_out_file, "%[^.].%s", first_name, end_name);
        sprintf(file_name, "%s_%d.%s\0", first_name, idx, end_name);
        output_files[idx].open(file_name);
      }
    } else {
       output_files[0].open(base_out_file);
    }

}

//get ddr addr
inline uint64_t calculate_index_hex(uint64_t index, uint32_t *file_index) {
    // basic col [2:0]
    uint32_t col_2_to_0 = index & 0x7;
    index = index >> 3;
    static uint32_t bg = 0,ba = 0,col_9_to_3 = 0,row = 0,dch = 0,rank = 0;

    for (int i = addr_map_order.use_size; i > 0; i--) {
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
        index = index >> 16;
      } else if (channel_num != 1 && addr_map_order.dch == i) {
        dch = (index & 0x1);
        *file_index |= dch << 1;
        index = index >> 1;
      } else if (rank_num != 1 && addr_map_order.ra == i) {
        rank = (index & 0x1);
        *file_index |= rank;
        index = index >> 1;
      } else {
        printf("Check if there is a map parameter that is not parsed\n");
        assert(0);
      }
    }
    //printf("debug ra %d bg %x ba %x row %x col_9_to_3 %x col_2_to_0 %x ch %d\n", rank, bg, ba, row, col_9_to_3, col_2_to_0, *file_index);
    if (need_files == 2 && *file_index == 2) {
      *file_index = 1;
    } else if (need_files == 1) {
      *file_index = 0;
    }
    if (*file_index >= need_files) {
      printf("Debug: file_index > need_files %d \n", *file_index);
      assert(0);
    }
    uint64_t index_hex = construct_index_remp(bg, ba, row, col_9_to_3, col_2_to_0, rank);

    return index_hex;
}

std::mutex queue_mutex[MAX_FILE];
std::condition_variable cv[MAX_FILE];
struct MemoryQueues {
  uint64_t data;
  uint64_t addr;
};
std::queue<MemoryQueues> memory_queues[MAX_FILE];

bool finished = false;
void thread_write_files(const int ch) {
  MemoryQueues this_memory;
  std::string buffer;
  buffer.reserve(STREAM_BUFFER_SIZE);
  char temp[64];

  if (out_raw) {
    std::unique_lock<std::mutex> lock(queue_mutex[ch]);
    cv[ch].wait(lock, []{ return finished; });
    printf("Strat write raw2\n");
    for (size_t i = 0; i < GB_8_SIZE / UINT64_SIZE; i++) {
      uint64_t data_byte = *(raw2_ram[ch] + i);
      output_files[ch].write(reinterpret_cast<const char*>(&data_byte), sizeof(data_byte));
    }
    printf("Write raw2 ok\n");
    return;
  } else {
    while (true) {
      if(!finished) {
        {
          std::unique_lock<std::mutex> lock(queue_mutex[ch]);
          cv[ch].wait(lock, [ch]{ return (memory_queues[ch].size() > 200) || finished;});
          while (!memory_queues[ch].empty()) {
            this_memory = memory_queues[ch].front();
            memory_queues[ch].pop();
            int len = snprintf(temp, sizeof(temp), "@%lx %016lx\n", this_memory.addr, this_memory.data);
            buffer.append(temp, len);
          }
        }
      } else {
        while (!memory_queues[ch].empty()) {
          this_memory = memory_queues[ch].front();
          memory_queues[ch].pop();
          int len = snprintf(temp, sizeof(temp), "@%lx %016lx\n", this_memory.addr, this_memory.data);
          buffer.append(temp, len);
        }
      }

      if (buffer.length() > STREAM_BUFFER_SIZE) {
        output_files[ch] << buffer;
        buffer.clear();
      }

      if (finished && memory_queues[ch].empty()) {
        if (buffer.size() > 0)
          output_files[ch] << buffer;
        return;  
      }
    }
  }
}

inline void mem_out_hex(uint64_t rd_addr, uint64_t index) {
  extern uint64_t *ram;
  uint64_t data_byte = *(ram + rd_addr);
#ifdef RM_ZERO
  if (data_byte != 0) {
#endif // RM_ZERO
    uint32_t file_index = 0;
    uint64_t addr = calculate_index_hex(index, &file_index);
    {
      std::lock_guard<std::mutex> lock(queue_mutex[file_index]);
      memory_queues[file_index].push({data_byte, addr});
    }
    cv[file_index].notify_one();
#ifdef RM_ZERO
  }
#endif // RM_ZERO
}

uint64_t mem_out_raw2() {
  extern uint64_t *ram;
  for (size_t i = 0; i < img_size; i++) {
    uint64_t data_byte = *(ram + i);
    if (data_byte != 0) {
      data_byte = htobe64(data_byte);
      uint32_t file_index = 0;
      uint64_t addr_map = calculate_index_hex(i, &file_index);
      if (addr_map > GB_8_SIZE / UINT64_SIZE) {
        printf("Addr map over size %ld\n", GB_8_SIZE);
      }
      // input temp map RAM
      *(raw2_ram[file_index] + addr_map) = data_byte;
    }
  }
  return 0;
}

//write perload
uint64_t mem_preload(uint64_t base_address, uint64_t img_size, const std::string& compress) {
    printf("Start mem preload\n");

    uint64_t rd_addr = 0;
    uint64_t index = base_address >> 3;

    bool use_compress = (!compress.empty());
    FILE *compress_fd = NULL;
    if (use_compress) {
      compress_fd = fopen(compress.c_str(), "r+");
      if (compress_fd == NULL)
        printf("Open compress_file %s flied\n", compress.c_str());
      else
        printf("Use compress_file %s load ram\n", compress.c_str());
    }

    if (use_compress) {
      while (true) {
        // out compress dat
        if (feof(compress_fd))
          break;
        if (fscanf(compress_fd, "%ld", &rd_addr) != 1)
          break;
        rd_addr = rd_addr * COMPRESS_SIZE / UINT64_SIZE;
        if (rd_addr >= img_size) {
          printf("Ram read addr over img size %ld\n", rd_addr);
          break;
        }

        for (size_t i = 0; i < COMPRESS_SIZE / UINT64_SIZE; i++) {
          uint64_t write_addr = rd_addr + i;
          mem_out_hex(rd_addr, write_addr);
          index ++;
        }
      }
    } else {
      // Start a consumer thread to write to the file
      std::thread consumer_thread[need_files];
      for (size_t i = 0; i < need_files; i++){   
        consumer_thread[i] = std::thread(thread_write_files, i);
      }
      if (out_raw) {
        for (size_t i = 0; i < need_files; i++) {
          raw2_ram[i] = (uint64_t *)malloc(GB_8_SIZE);
        }
        mem_out_raw2();
        index = img_size;
      } else {
        while (rd_addr < img_size) {
          mem_out_hex(rd_addr, index);
          rd_addr += 1;
          index += 1;
        }
    }
      finished = true;
      for (size_t i = 0; i < need_files; i++) {
        cv[i].notify_one();
      }
      for (size_t i = 0; i < need_files; i++) {
        if (consumer_thread[i].joinable())
          consumer_thread[i].join();
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
    }
    else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compress") == 0) {
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