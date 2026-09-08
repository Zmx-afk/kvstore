CC = gcc
FLAGS = -I ./NtyCo/core/ -I ./src/include/ -L ./NtyCo/ -lntyco -lpthread -luring -ldl #-ljemalloc

# 分模块定义源文件
SRCS_ENGINE = src/engine/kvs_array.c src/engine/kvs_rbtree.c src/engine/kvs_hash.c src/engine/kvs_skiptable.c
SRCS_NETWORK = src/network/reactor.c src/network/ntyco.c src/network/proactor.c src/network/protocol_resp.c
SRCS_PERSIST = src/persist/persistence.c
SRCS_UTILS = src/utils/memory.c src/utils/Mm_pool.c src/utils/timer.c src/utils/log.c
SRCS_MAIN = src/main.c src/kvstore.c MS_replication.c src/config.c

# 合并所有源文件
SRCS = $(SRCS_ENGINE) $(SRCS_NETWORK) $(SRCS_PERSIST) $(SRCS_UTILS) $(SRCS_MAIN)
TESTCASE_SRCS = testcase.c src/network/protocol_resp.c src/utils/log.c src/persist/persistence.c $(SRCS_ENGINE) $(SRCS_UTILS) src/kvstore.c MS_replication.c src/config.c $(SRCS_NETWORK)

TARGET = kvstore
SUBDIR = ./NtyCo/
TESTCASE = testcase

OBJS = $(SRCS:.c=.o)
TESTCASE_OBJS = $(TESTCASE_SRCS:.c=.o)

all: $(SUBDIR) $(TARGET) $(TESTCASE)

$(SUBDIR):
	@echo "Building NtyCo..."
	make -C $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(FLAGS)

$(TESTCASE): $(TESTCASE_OBJS)
	$(CC) -o $@ $^ $(FLAGS)

%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(TESTCASE_OBJS) $(TARGET) $(TESTCASE)
