#ifndef BINDDR_H
#define BINDDR_H

extern char *out_img_file;
extern char *memory_config_get;
uint64_t mem_preload(const std::string& output_file, uint64_t base_address, const std::string& addr_map, uint64_t img_size,const char *compress);
int args_parsingniton(int argc, char *argv[]);
#endif
