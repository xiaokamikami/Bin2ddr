CXX = g++  
CXXFLAGS = -std=c++11 -Wextra -pedantic -pthread -Wall -O3

PROM = bin2ddr  
  
# 指定源文件和头文件搜索路径  
SRCDIR = ./csrc/  
INCDIR = ./include/  
OBJDIR = ./obj/  

CHANNEL ?= 1
RANK ?= 1

ifeq ($(PERF), 1)
CXXFLAGS += -DPERF
endif
ifneq ($(MAX_FILE),)
CXXFLAGS += -DMAX_FILE=$(MAX_FILE)
else
CXXFLAGS += -DMAX_FILE=4
endif

CXXFLAGS += -DCONFIG_CHANNEL=$(CHANNEL)
CXXFLAGS += -DCONFIG_RANK=$(RANK)

LDFLAGS =-L. -lz -lzstd
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
  
# 添加一个 phony 目标来确保 obj 目录总是存在的  
.PHONY: dirs  
dirs:  
	$(shell mkdir -p $(OBJDIR))  
  
# 让 all 成为默认目标，并确保 obj 目录存在  
all: dirs $(PROM)