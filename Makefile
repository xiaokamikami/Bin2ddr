CXX = g++  
CXXFLAGS = -std=c++17 -Wextra -pedantic -pthread -Wall -O3

PROM = bin2ddr  
  
# 指定源文件和头文件搜索路径  
SRCDIR = ./csrc/  
INCDIR = ./include/  
OBJDIR = ./obj/  

CHANNEL ?= 1
RANK ?= 1

ifneq ($(MAKECMDGOALS),)
ifeq ($(filter-out nemu-update nemu-clean,$(MAKECMDGOALS)),)
include nemu.mk
endif
endif

ifeq ($(PERF), 1)
CXXFLAGS += -DPERF
endif
ifeq ($(RM_ZERO), 1)
CXXFLAGS += -DRM_ZERO
endif
ifeq ($(FPGA), 1)
CXXFLAGS += -DUSE_FPGA
endif
ifneq ($(MAX_FILE),)
CXXFLAGS += -DMAX_FILE=$(MAX_FILE)
else
CXXFLAGS += -DMAX_FILE=4
endif

LDFLAGS =-L. -lz -lzstd -lfmt
# 使用 -I 选项指定头文件搜索路径  
CPPFLAGS = -I$(INCDIR)  
# 指定对象文件的目录  
VPATH = $(SRCDIR):$(OBJDIR)  
  
# 列出所有的源文件  
SRCS = $(shell find $(SRCDIR) -name "*.cpp")  
  
# 生成对应的对象文件列表，并指定它们应该在 obj 目录下  
OBJS = $(patsubst $(SRCDIR)%.cpp,$(OBJDIR)%.o,$(SRCS))  

# 创建 obj 目录（如果它不存在）  
$(shell mkdir -p $(OBJDIR))  
  
# 链接目标程序  
$(PROM): $(OBJS)  
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)  
  
# 编译规则  
$(OBJDIR)%.o: $(SRCDIR)%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# 清理规则  
clean:
	rm bin2ddr
	rm -rf $(OBJDIR) $(PROM)
.PHONY: all dirs nemu-update nemu-clean
dirs:
	@mkdir -p $(OBJDIR)
all: dirs $(PROM)