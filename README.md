#如何编译
    make && make clean
#设计方案

#测试方案和可行性
    1.测试全量持久化
        编译后打开config.c 将rdb_enable设置为1 aof_strategy设置为no 然后一个终端启动./kvstore 0 另一个启动./testcase 1
        等待testcase数据输送完成后 关闭kvstore终端和testcase 启动./kvstore 0 等待全量持久化后启动./testcase 2
        完成验证
    2.测试增量持久化
        编译后打开config.c 将aof_strategy 设置为always 然后一个终端启动./kvstore 0 另一个启动./testcase 3
        等待数据发送后 重启./kvstore 0 等待完成持久化后 启动./testcase 4
        完成验证
    3.测试主从同步
        将testcase中的COUNT改为50000 编译后打开config.c rdb_enable设置为1 aof_strategy设置为always 打开终端运行./kvstore 0
        然后另一个终端打开./testcase 5 根据步骤完成即可 注:AOF RDB文件需清空
    4.使用redis_cli进行交互
        清空RDB AOF文件后打开终端 运行./kvstore 0 等待一会后用另一个终端执行redis-cli -p 2000(本机连接)
        发送PING SET 和 GET命令进行验证

    
#无内存池
    开始虚拟内存: 174.30 MB
    开始物理内存: 41.53 MB
    最大虚拟内存: 186.42 MB
    最大物理内存: 53.70 MB
    VmPeak: 186.42 MB
    VmHWM: 53.70 MB
    结束虚拟内存: 186.42 MB
    结束物理内存: 53.70 MB

#有内存池
    开始虚拟内存: 174.30 MB
    开始物理内存: 41.68 MB
    最大虚拟内存: 186.42 MB
    最大物理内存: 54.16 MB
    VmPeak: 186.42 MB
    VmHWM: 54.16 MB
    结束虚拟内存: 186.42 MB
    结束物理内存: 54.16 MB
#jemalloc
    开始虚拟内存: 198.24 MB
    开始物理内存: 43.71 MB
    最大虚拟内存: 203.74 MB
    最大物理内存: 50.13 MB
    VmPeak: 203.74 MB
    VmHWM: 50.14 MB
    结束虚拟内存: 203.74 MB
    结束物理内存: 49.99 MB






# kvstore
