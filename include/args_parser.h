#ifndef ARGS_PARSER_H
#define ARGS_PARSER_H

#include <string>

extern std::string input_file;
extern std::string gcpt_file;
extern std::string compress_file;
extern char *base_out_file;
extern uint32_t gcpt_over_size;
extern uint64_t base_address;
extern bool out_raw;
extern bool split_rank;

int args_parsingniton(int argc, char *argv[]);
void show_help();

#endif // ARGS_PARSER_H