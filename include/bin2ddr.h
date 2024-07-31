#ifndef BINDDR_H
#define BINDDR_H

extern char *out_img_file;
extern char *memory_config_get;
uint64_t mem_preload(uint64_t base_address, uint64_t img_size,const std::string& compress);
int args_parsingniton(int argc, char *argv[]);
#endif
