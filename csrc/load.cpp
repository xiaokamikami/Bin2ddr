#include <iostream>
#include <sys/mman.h>
#include <zlib.h>
#include <fcntl.h>
#include <zstd.h>

#include "../include/load.h"

uint64_t load_bin(const char* img_name);
bool isGzFile(const char *filename);
bool isZstdFile(const char *filename);
long readFromGz(void *ptr, const char *file_name, long buf_size, uint8_t load_type);
long readFromZstd(void *ptr, const char *file_name, long buf_size, uint8_t load_type);

uint64_t *ram = NULL;
uint64_t ram_size = 0;
void finish_ram() {
    munmap(ram, ram_size);
}
uint64_t load_img(const char * image, const uint8_t ch_num, const uint8_t rank_num) {
    ram_size = GB_8_SIZE * ch_num * rank_num;
    ram = (uint64_t *)mmap(NULL, ram_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_NORESERVE, -1, 0);
    if (ram == (uint64_t *)MAP_FAILED) {
      printf("Warning: Insufficient phisical memory\n");
      ram_size = GB_8_SIZE;
      ram = (uint64_t *)mmap(NULL, ram_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
      if (ram == (uint64_t *)MAP_FAILED) {
        printf("Error: Cound not mmap 0x%lx bytes\n", ram_size);
        assert(0);
      }
    }
    printf("Total DDR Capacity %lu GB RAM\n", ram_size / (1024 * 1024 * 1024));

    if (ram == NULL) {
      assert(0);
    }
    if (isGzFile(image)) {
      printf("Gzip file detected and loading image from extracted gz file\n");
      return readFromGz(ram, image, ram_size, LOAD_RAM);
    } else if (isZstdFile(image)) {
      printf("Zstd file detected and loading image from extracted zstd file\n");
      return readFromZstd(ram, image, ram_size, LOAD_RAM);
    } else {
      return load_bin(image);
    }
}

uint64_t override_ram(const char* img_name, uint64_t over_size) {
  FILE *fp = fopen(img_name, "rb");
  assert(fp != NULL);

  size_t size;
  fseek(fp, 0, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  if (over_size < size)
    size = over_size;

  int ret = fread(ram, size, 1, fp);
  assert(ret == 1);

  fclose(fp);
  return size;
}

// RAW image
uint64_t load_bin(const char* img_name) {
  FILE *fp = fopen(img_name, "rb");
  assert(fp != NULL);

  size_t size;
  fseek(fp, 0, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  int ret = fread(ram, size, 1, fp);
  assert(ret == 1);

  printf("Read %lu bytes from file %s\n", size, img_name);

  fclose(fp);
  return size;
}

// Return whether the file is a gz file
bool isGzFile(const char *filename) {
#ifdef NO_GZ_COMPRESSION
  return false;
#endif
  int fd = -1;

  fd = open(filename, O_RDONLY);
  assert(fd);

  uint8_t buf[2];

  size_t sz = read(fd, buf, 2);
  if (sz != 2) {
    close(fd);
    return false;
  }

  close(fd);

  const uint8_t gz_magic[2] = {0x1f, 0x8B};
  return memcmp(buf, gz_magic, 2) == 0;
}

// Return whether the file is a zstd file
bool isZstdFile(const char *filename) {
#ifdef NO_ZSTD_COMPRESSION
  return false;
#endif
  int fd = -1;

  fd = open(filename, O_RDONLY);
  assert(fd);

  uint8_t buf[4];

  size_t sz = read(fd, buf, 4);
  if (sz != 4) {
    close(fd);
    return false;
  }

  close(fd);

  const uint8_t zstd_magic[4] = {0x28, 0xB5, 0x2F, 0xFD};
  return memcmp(buf, zstd_magic, 4) == 0;
}

long readFromGz(void *ptr, const char *file_name, long buf_size, uint8_t load_type) {
#ifndef NO_GZ_COMPRESSION
  assert(buf_size > 0);
  gzFile compressed_mem = gzopen(file_name, "rb");

  if (compressed_mem == NULL) {
    printf("Can't open compressed binary file '%s'", file_name);
    return -1;
  }

  uint64_t curr_size = 0;
  const uint32_t chunk_size = 16384;

  // Only load from RAM need check
  if (load_type == LOAD_RAM && (buf_size % chunk_size) != 0) {
    printf("buf_size must be divisible by chunk_size\n");
    assert(0);
  }

  long *temp_page = new long[chunk_size];

  while (curr_size < buf_size) {
    uint32_t bytes_read = gzread(compressed_mem, temp_page, chunk_size * sizeof(long));
    if (bytes_read == 0) {
      break;
    }
    for (uint32_t x = 0; x < bytes_read / sizeof(long) + 1; x++) {
      if (*(temp_page + x) != 0) {
        long *pmem_current = (long *)((uint8_t *)ptr + curr_size + x * sizeof(long));
        *pmem_current = *(temp_page + x);
      }
    }
    curr_size += bytes_read;
  }

  if (gzread(compressed_mem, temp_page, chunk_size) > 0) {
    printf("File size is larger than buf_size!\n");
    assert(0);
  }
  printf("Read %lu bytes from gz stream in total\n", curr_size);

  delete[] temp_page;

  if (gzclose(compressed_mem)) {
    printf("Error closing '%s'\n", file_name);
    return -1;
  }
  return curr_size;
#else
  return 0;
#endif
}

long readFromZstd(void *ptr, const char *file_name, long buf_size, uint8_t load_type) {
#ifndef NO_ZSTD_COMPRESSION
  assert(buf_size > 0);

  int fd = -1;
  int file_size = 0;
  uint8_t *compress_file_buffer = NULL;
  size_t compress_file_buffer_size = 0;

  uint64_t curr_size = 0;
  const uint32_t chunk_size = 16384;

  // Only load from RAM need check
  if (load_type == LOAD_RAM && (buf_size % chunk_size) != 0) {
    printf("buf_size must be divisible by chunk_size\n");
    return -1;
  }

  fd = open(file_name, O_RDONLY);
  if (fd < 0) {
    printf("Can't open compress binary file '%s'", file_name);
    return -1;
  }

  file_size = lseek(fd, 0, SEEK_END);
  if (file_size == 0) {
    printf("File size must not be zero");
    return -1;
  }

  lseek(fd, 0, SEEK_SET);

  compress_file_buffer = new uint8_t[file_size];
  assert(compress_file_buffer);

  compress_file_buffer_size = read(fd, compress_file_buffer, file_size);
  if (compress_file_buffer_size != file_size) {
    close(fd);
    free(compress_file_buffer);
    printf("Zstd compressed file read failed, file size: %d, read size: %ld\n", file_size, compress_file_buffer_size);
    return -1;
  }

  close(fd);

  ZSTD_inBuffer input_buffer = {compress_file_buffer, compress_file_buffer_size, 0};

  long *temp_page = new long[chunk_size];

  ZSTD_DStream *dstream = ZSTD_createDStream();
  if (!dstream) {
    printf("Can't create zstd dstream object\n");
    delete[] compress_file_buffer;
    delete[] temp_page;
    return -1;
  }

  size_t init_result = ZSTD_initDStream(dstream);
  if (ZSTD_isError(init_result)) {
    printf("Can't init zstd dstream object: %s\n", ZSTD_getErrorName(init_result));
    ZSTD_freeDStream(dstream);
    delete[] compress_file_buffer;
    delete[] temp_page;
    return -1;
  }

  while (curr_size < buf_size) {

    ZSTD_outBuffer output_buffer = {temp_page, chunk_size * sizeof(long), 0};

    size_t decompress_result = ZSTD_decompressStream(dstream, &output_buffer, &input_buffer);

    if (ZSTD_isError(decompress_result)) {
      printf("Decompress failed: %s\n", ZSTD_getErrorName(decompress_result));
      ZSTD_freeDStream(dstream);
      delete[] compress_file_buffer;
      delete[] temp_page;
      return -1;
    }

    if (output_buffer.pos == 0) {
      break;
    }

    for (uint32_t x = 0; x < output_buffer.pos / sizeof(long) + 1; x++) {
      if (*(temp_page + x) != 0) {
        long *pmem_current = (long *)((uint8_t *)ptr + curr_size + x * sizeof(long));
        *pmem_current = *(temp_page + x);
      }
    }
    curr_size += output_buffer.pos;
  }

  ZSTD_outBuffer output_buffer = {temp_page, chunk_size * sizeof(long), 0};
  size_t decompress_result = ZSTD_decompressStream(dstream, &output_buffer, &input_buffer);
  if (ZSTD_isError(decompress_result) || output_buffer.pos != 0) {
    printf("Decompress failed: %s\n", ZSTD_getErrorName(decompress_result));
    printf("Binary size larger than memory\n");
    ZSTD_freeDStream(dstream);
    delete[] compress_file_buffer;
    delete[] temp_page;
    return -1;
  }

  ZSTD_freeDStream(dstream);
  delete[] compress_file_buffer;
  delete[] temp_page;

  return curr_size;
#else
  return 0;
#endif
}