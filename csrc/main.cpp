#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <endian.h>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <array>
#include <stack> 
#include <fmt/core.h>
#include <boost/lockfree/spsc_queue.hpp>
#include "../include/common.h"
#include "../include/load.h"
#include "../include/bin2ddr.h"
#include "../include/args_parser.h"
#ifdef PERF
#include <chrono>
#endif
#define STREAM_BUFFER_SIZE 1024 * 1024 * 32
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
        output_files[idx].open(file_name, std::ios::out | std::ios::trunc);
      }
    } else {
       output_files[0].open(base_out_file, std::ios::out | std::ios::trunc);
    }

}

//get ddr addr
inline uint64_t calculate_index_hex(uint64_t index, uint32_t *file_index) {
    // basic col [2:0]
    uint32_t bg = 0,ba = 0,col_9_to_3 = 0,row = 0,dch = 0,rank = 0;
    uint32_t col_2_to_0 = index & 0x7;
    index = index >> 3;
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
boost::lockfree::spsc_queue<MemoryQueues, boost::lockfree::capacity<1024 * 1024>> memory_queues[MAX_FILE];

bool finished = false;
void thread_write_files(const int ch) {
  MemoryQueues this_memory;
  std::string buffer;
  buffer.reserve(STREAM_BUFFER_SIZE);
  if (out_raw) {
    std::unique_lock<std::mutex> lock(queue_mutex[ch]);
    cv[ch].wait(lock, []{ return finished; });
    printf("Strat write raw2\n");
    for (size_t i = 0; i < GB_8_SIZE / UINT64_SIZE; i++) {
      uint64_t data_byte = *(raw2_ram[ch] + i);
      output_files[ch].write(reinterpret_cast<const char*>(&data_byte), sizeof(data_byte));
    }
    return;
  } else {
    while (true) {
      if(!finished) {
        {
          {
            std::unique_lock<std::mutex> lock(queue_mutex[ch]);
            cv[ch].wait(lock, [ch]{ return (memory_queues[ch].read_available() > 1024) || finished;});
          }
          if (finished) continue;
          #ifdef USE_FPGA
            uint64_t start_addr = 0;
            static std::stack<uint64_t> memory_stacks;
            while (memory_queues[ch].read_available() < 128) {
              for (size_t i = 0; i < 128; i++) {
                this_memory = memory_queues[ch].front();
                memory_queues[ch].pop();
                if (i == 0) {
                  start_addr = this_memory.addr;
                  buffer += fmt::format("{:016x}\n", this_memory.addr * sizeof(uint64_t));
                } else if (start_addr + i != this_memory.addr) {
                  break;
                }
                memory_stacks.push(this_memory.data);
              }
              for (size_t i = memory_stacks.size(); i > 0; i--) {
                uint64_t data = memory_stacks.top();
                memory_stacks.pop();
                buffer += fmt::format("{:016x}", this_memory.data);
              }
              buffer += fmt::format("\n");
            }
          #else
            for (size_t i = 0; i < 1024; ++i) {
              memory_queues[ch].pop(this_memory);
              buffer += fmt::format("@{:x} {:016x}\n", this_memory.addr, this_memory.data);
            }
          #endif
        }
      } else {
        size_t queue_size = memory_queues[ch].read_available();
        for (size_t i = 0; i < queue_size; ++i) {
          memory_queues[ch].pop(this_memory);
          #ifdef USE_FPGA
            buffer += fmt::format("{:x}\n{:016x}\n", this_memory.addr * sizeof(uint64_t), this_memory.data);
          #else
            buffer += fmt::format("@{:x} {:016x}\n", this_memory.addr, this_memory.data);
          #endif
        }
      }

      if (buffer.size() > STREAM_BUFFER_SIZE) {
        output_files[ch].write(buffer.data(), buffer.length());
        buffer.clear();
        if(finished)
          return;
      } else if (finished && memory_queues[ch].empty()) {
        if (buffer.size() > 0)
          output_files[ch].write(buffer.data(), buffer.length());
        return;  
      }
    } // END WHILE
  }
}

inline void mem_out_hex(uint64_t rd_addr, uint64_t index) {
  extern uint64_t *ram;
  uint64_t data_byte = *(ram + rd_addr);
  uint32_t file_index = 0;
  static uint32_t data_count[MAX_FILE] = {};
#ifdef RM_ZERO
  if (data_byte == 0)
    return;
#endif // RM_ZERO
#ifndef USE_FPGA
    uint64_t addr = calculate_index_hex(index, &file_index);
#else
    calculate_index_hex(rd_addr, &file_index);
    uint64_t addr = rd_addr;
#endif // USE_FPGA
      while (true) {
        if (memory_queues[file_index].write_available() >= 10) {
          memory_queues[file_index].push({data_byte, addr});
          break;
        } else {
          std::this_thread::sleep_for(std::chrono::nanoseconds(10));
        }
      }
    data_count[file_index] ++;
    if (data_count[file_index] == 1024) {
      cv[file_index].notify_one();
      data_count[file_index] = 0;
    }
}

uint64_t mem_out_raw2(bool use_compress, uint64_t offset = 0) {
  extern uint64_t *ram;
  uint64_t ram_size = use_compress ? COMPRESS_SIZE / UINT64_SIZE: img_size;
  for (size_t i = offset; i < ram_size; i++) {
    uint64_t data_byte = *(ram + i);
    if (data_byte != 0) {
      uint32_t file_index = 0;
      data_byte = htobe64(data_byte);
#ifndef USE_FPGA
      uint64_t addr_map = calculate_index_hex(i, &file_index);
      // input temp map RAM
      *(raw2_ram[file_index] + addr_map) = data_byte;
      if (addr_map > GB_8_SIZE / UINT64_SIZE) {
        printf("Addr map over size %ld\n", GB_8_SIZE);
      }
#else
      if (need_files > 1) {
        calculate_index_hex(i, &file_index);
      }
      *(raw2_ram[file_index] + i) = data_byte;
#endif // USE_FPGA
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

    std::thread consumer_thread[need_files];
    for (size_t i = 0; i < need_files; i++){   
      consumer_thread[i] = std::thread(thread_write_files, i);
    }

    if (use_compress) {
      while (true) {
        // out compress dat
        if (feof(compress_fd) || fscanf(compress_fd, "%ld", &rd_addr) != 1)
          break;
        rd_addr = rd_addr * COMPRESS_SIZE / UINT64_SIZE;
        if (rd_addr >= img_size) {
          printf("Ram read addr over img size %ld\n", rd_addr);
          break;
        }
        if (out_raw) {
          for (size_t i = 0; i < need_files; i++) {
            raw2_ram[i] = (uint64_t *)malloc(GB_8_SIZE);
          }
          mem_out_raw2(use_compress);
        } else {
          for (size_t i = 0; i < COMPRESS_SIZE / UINT64_SIZE; i++) {
            uint64_t write_addr = rd_addr + i;
            mem_out_hex(write_addr, 0);
          }
        }
      }
    } else {
      // Start a consumer thread to write to the file
      if (out_raw) {
        for (size_t i = 0; i < need_files; i++) {
          raw2_ram[i] = (uint64_t *)malloc(GB_8_SIZE);
        }
        mem_out_raw2(false);
        index = img_size;
      } else {
        while (rd_addr < img_size) {
          mem_out_hex(rd_addr, index);
          rd_addr += 1;
          index += 1;
        }
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
    return index;
}
