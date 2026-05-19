# 9.1 Kvstore：兼容resp协议的轻量级 Key-Value 存储系统

## 1. 编译与运行

### 1.1 环境要求

#### 1.1.1 基础环境

- Ubuntu 22.04，Linux 内核 6.8.0-107-generic（其它 5.8+ 版本应该也兼容）

#### 1.1.2 RDMA 环境（全量同步功能）

kvstore的主从全量同步使用 RDMA，需要通过rdma_rxe模拟一块RDMA网卡：

**软件**：

```bash
sudo apt install -y \
    rdma-core libibverbs-dev librdmacm-dev \
    ibverbs-utils ibverbs-providers
```

**硬件**：

用 Soft-RoCE 把普通网卡当 RDMA 用：
```bash
sudo modprobe rdma_rxe
sudo rdma link add rxe0 type rxe netdev "网卡名"

# 验证
ibv_devices       # 应该看到 rxe0
ibv_devinfo       # 查看设备详细信息
```

#### 1.1.3 ebpf 环境（增量同步功能）

kvstore的主从增量同步基于ebpf：

**软件**：
```bash
sudo apt install -y \
    clang llvm libbpf-dev libelf-dev zlib1g-dev \
    linux-tools-generic gcc-multilib
```


### 1.2 克隆项目

```bash
git clone --recurse-submodules http://gitlab.0voice.com/yansss/9.1-kvstore.git
```

### 1.3 编译 kvstore

```bash
cd 9.1-kvstore
make
```

### 1.4 编译 ebpf 增量同步模块

```bash
cd kvs-ebpf
make kvs_agent
cd ..
```

## 2.配置文件

kvstore 通过 `conf/kvstore.conf` 配置运行参数，启动时自动读取。

#### 2.1.1 基础配置

| 配置项 | 说明 | 默认值 |
|-------|------|-------|
| `Mode` | 运行角色：`0` 主端（master），`1` 从端（slave） | `0` |
| `Port` | kvstore 服务监听端口，客户端通过此端口连接 | `2000` |

#### 2.1.2 模块开关

| 配置项 | 说明 | 取值 |
|-------|------|------|
| `ENABLE_MODULE_SYNC` | 是否启用主从同步模块 | `0` 关 / `1` 开 |
| `ENABLE_MODULE_SAVE` | 是否启用全量持久化模块 | `0` 关 / `1` 开 |
| `ENABLE_MODULE_LOG`  | 是否启用增量持久化模块 | `0` 关 / `1` 开 |

#### 2.1.3 全量同步配置（RDMA）

从端首次接入时，通过 RDMA 从主端拉取全量数据。从端需要知道主端的 RDMA 服务地址。

| 配置项 | 说明 | 角色 |
|-------|------|-----|
| `Master_ip` | 主端 kvstore 的 IP 地址 | 从端配置 |
| `Master_port` | 主端 kvstore 的服务端口 | 从端配置 |
| `Rdma_server_ip` | 从端 RDMA 服务监听 IP | 主端配置 |
| `Rdma_port` | 从端 RDMA 服务监听端口 | 主端配置 |

#### 2.1.4 增量同步配置（eBPF）

全量同步完成后，从端切换到 eBPF 代理（`kvs_agent`）接收增量更新。

| 配置项 | 说明 | 角色 |
|-------|------|-----|
| `Agent_ip` | eBPF 代理服务的 IP | 从端配置 |
| `Agent_port` | eBPF 代理服务的端口 | 从端配置 |





## 3.协议指令（四种数据结构）

### 3.1 数组 （array）
| 指令  | 参数  | 功能说明  | 成功响应  | 失败响应  |
| :--- | :--- | :--- | :--- | :--- |
| **SET** | `<key> <value>` | 存储键值对 | +OK\r\n / :1\r\n | -ERR message\r\n |
| **GET** | `<key>` | 根据 Key 获取对应的 Value | `<value>` / $-1\r\n | -ERR message\r\n |
| **MOD** | `<key> <value>` | 修改已存在的 Key 对应的 Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **DEL** | `<key>` | 从存储引擎中删除指定的 Key-Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **EXIST** | `<key>` | 查询指定的 Key 是否存在于系统中 | :1\r\n / $-1\r\n | -ERR message\r\n |
 
### 3.2 哈希 （hash）
| 指令  | 参数  | 功能说明  | 成功响应  | 失败响应  |
| :--- | :--- | :--- | :--- | :--- |
| **HSET** | `<key> <value>` | 存储键值对 | +OK\r\n / :1\r\n | -ERR message\r\n |
| **HGET** | `<key>` | 根据 Key 获取对应的 Value | `<value>` / $-1\r\n | -ERR message\r\n |
| **HMOD** | `<key> <value>` | 修改已存在的 Key 对应的 Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **HDEL** | `<key>` | 从存储引擎中删除指定的 Key-Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **HEXIST** | `<key>` | 查询指定的 Key 是否存在于系统中 | :1\r\n / $-1\r\n | -ERR message\r\n |

### 3.3 跳表 （skiplist）
| 指令  | 参数  | 功能说明  | 成功响应  | 失败响应  |
| :--- | :--- | :--- | :--- | :--- |
| **LSET** | `<key> <value>` | 存储键值对 | +OK\r\n / :1\r\n | -ERR message\r\n |
| **LGET** | `<key>` | 根据 Key 获取对应的 Value | `<value>` / $-1\r\n | -ERR message\r\n |
| **LMOD** | `<key> <value>` | 修改已存在的 Key 对应的 Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **LDEL** | `<key>` | 从存储引擎中删除指定的 Key-Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **LEXIST** | `<key>` | 查询指定的 Key 是否存在于系统中 | :1\r\n / $-1\r\n | -ERR message\r\n |


### 3.4 红黑树 （rbtree）
| 指令  | 参数  | 功能说明  | 成功响应  | 失败响应  |
| :--- | :--- | :--- | :--- | :--- |
| **RSET** | `<key> <value>` | 存储键值对 | +OK\r\n / :1\r\n | -ERR message\r\n |
| **RGET** | `<key>` | 根据 Key 获取对应的 Value | `<value>` / $-1\r\n | -ERR message\r\n |
| **RMOD** | `<key> <value>` | 修改已存在的 Key 对应的 Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **RDEL** | `<key>` | 从存储引擎中删除指定的 Key-Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **REXIST** | `<key>` | 查询指定的 Key 是否存在于系统中 | :1\r\n / $-1\r\n | -ERR message\r\n |

### 3.5 SAVE指令 将当前内存数据持久化到磁盘文件


## 4. 核心功能模块

### 4.1 增量持久化 （AOF 机制）

为了确保内存数据在系统宕机或重启后能够恢复，实现了增量持久化功能。

#### 4.1.1 触发策略
采用“写时记录”原则，仅针对会改变内存数据状态的指令进行日志落盘，从而平衡了数据安全与磁盘 IO 性能。
* **记录指令**：`SET`、`MOD`、`DEL`。
* **忽略指令**：`GET`、`EXIST`、`SAVE`（此类指令不修改数据，无须记录）。

#### 4.1.2 实现原理

数据落盘用io_uring实现，加载持久化数据用mmap。

### 4.2 全量持久化 （SAVE）
提供了 `SAVE` 指令，用于将当前内存中的全量数据持久化到 `.rdb` 文件中。

#### 4.2.1 触发策略
解析到 `SAVE`指令时。

#### 4.2.2 实现原理

数据落盘用io_uring实现，加载持久化数据用mmap。



### 4.3 主从同步模块
实现了主从同步机制。该模块支持 **全量同步** 与 **实时同步**。

全量同步基于RDMA，增量同步基于ebpf。

```bash
# 1. 主端启动 kvstore（按主端配置）
./kvstore conf/kvstore-master.conf

# 2. 主端启动 eBPF 代理（root 权限）
sudo ./kvs-ebpf/kvs_agent

# 3. 从端启动 kvstore（按从端配置）
./kvstore conf/kvstore-slave.conf
```


## 5.测试方案

`testcase_resp/` 目录提供了一系列基于 RESP 协议的测试客户端，用于验证 kvstore 各项功能。

###编译

```bash
cd tesetcase_resp
make
```
编译后会生成多个可执行文件。    


### basic_function
```bash
./basic_function <ip> <port>
```
    测试kvstore基本功能(SET/GET/MODE/DEL/EXIST)是否正常。

### set.c
```bash
./set <ip> <port>
```
    模拟客户端发送SET/RSET/LSET/HSET命令，若kvstore成功响应，打印结果。

### get
```bash
./get <ip> <port>
```
    模拟客户端发送GET/RGET/LGET/HGET命令，若kvstore成功响应，打印结果。

### del
```bash
./del <ip> <port>
```
    模拟客户端发送DEL/RDEL/LDEL/HDEL命令，若kvstore成功响应，打印结果。

### set_save
```bash
./set_save <ip> <port>
```
    模拟客户端发送GET/RGET/LGET/HGET命令后，再发送SAVE命令 若kvstore成功响应，打印结果。

### set_blog
```bash
./set_blog <ip> <port>
```
    模拟客户端发送大key大value键值对，打印接收结果。

### get_blog
```bash
./get_blog <ip> <port>
```
    模拟客户端发送GET 大key，打印set_blog.c插入的大value。

### multicmd
```bash
./multicmd <ip> <port>
```
    模拟客户端一次性发送五十条指令，打印返回结果。


    



## 6.Kvstore 性能

虚拟机配置：
![虚拟机配置](https://img.0voice.com/6780/8ae9726839f2cc8abacb45d423f8d456.png)

Linux内核版本：ubuntu 22.04.5  6.8.0-107-generic

### 全量持久化性能

每个数据结构单独插入100万条数据：

![全量持久化性能的簇状图](https://quickchart.io/chart?c={type:'bar',data:{labels:['1k/次','1w/次','10w/次','100w/次'],datasets:[{label:'红黑树',data:[130,1207,3429,3105]},{label:'跳表',data:[142,1029,4424,3094]},{label:'哈希',data:[111,626,1072,1219]}]},options:{scales:{xAxes:[{scaleLabel:{display:true,labelString:'SAVE%20间隔'}}],yAxes:[{scaleLabel:{display:true,labelString:'qps（次/秒）'}}]}}})

### Kvstore性能对比

#### 读（GET），写（SET）性能对比

每个数据结构单独插入10万条数据：

![哈希的簇状图](https://quickchart.io/chart?c={type:'bar',data:{labels:['关闭%20AOF(SET)','关闭%20AOF(GET)','开启%20AOF(SET)','开启%20AOF(GET)'],datasets:[{label:'echo服务器',data:[3400,3400,3400,3400]},{label:'redis',data:[2839,2863,2829,2661]},{label:'kvstore',data:[2782,2878,2883,2896]}]},options:{scales:{xAxes:[{scaleLabel:{display:true,labelString:'哈希'}}],yAxes:[{scaleLabel:{display:true,labelString:'qps（次/秒）'},ticks:{beginAtZero:true}}]}}})

![跳表的簇状图](https://quickchart.io/chart?c={type:'bar',data:{labels:['关闭%20AOF(SET)','关闭%20AOF(GET)','开启%20AOF(SET)','开启%20AOF(GET)'],datasets:[{label:'echo服务器',data:[3400,3400,3400,3400]},{label:'redis',data:[3109,3148,3002,3050]},{label:'kvstore',data:[3270,3200,3205,3290]}]},options:{scales:{xAxes:[{scaleLabel:{display:true,labelString:'跳表'}}],yAxes:[{scaleLabel:{display:true,labelString:'qps（次/秒）'},ticks:{beginAtZero:true}}]}}})

![红黑树的簇状图](https://quickchart.io/chart?c={type:'bar',data:{labels:['关闭%20AOF(SET)','关闭%20AOF(GET)','开启%20AOF(SET)','开启%20AOF(GET)'],datasets:[{label:'echo服务器',data:[3400,3400,3400,3400]},{label:'kvstore',data:[3087,3173,3054,3307]}]},options:{scales:{xAxes:[{scaleLabel:{display:true,labelString:'红黑树'}}],yAxes:[{scaleLabel:{display:true,labelString:'qps（次/秒）'},ticks:{beginAtZero:true}}]}}})

#### 批量处理性能对比

每个数据结构单独插入10万条数据：

![哈希批量处理性能的簇状图](https://quickchart.io/chart?c={type:'bar',data:{labels:['10','20','40','80','160'],datasets:[{label:'redis',data:[29180,51519,97751,168350,273224]},{label:'kvstore',data:[13506,18765,21654,28793,31084]}]},options:{scales:{xAxes:[{scaleLabel:{display:true,labelString:'哈希：批量处理（条）'}}],yAxes:[{scaleLabel:{display:true,labelString:'qps（次/秒）'},ticks:{beginAtZero:true}}]}}})

![跳表批量处理性能的簇状图](https://quickchart.io/chart?c={type:'bar',data:{labels:['10','20','40','80','160'],datasets:[{label:'redis',data:[28943,49504,83472,125944,176056]},{label:'kvstore',data:[27909,59844,105042,177619,254452]}]},options:{scales:{xAxes:[{scaleLabel:{display:true,labelString:'跳表：批量处理（条）'}}],yAxes:[{scaleLabel:{display:true,labelString:'qps（次/秒）'},ticks:{beginAtZero:true}}]}}})

![红黑树批量处理性能的簇状图](https://quickchart.io/chart?c={type:'bar',data:{labels:['10','20','40','80','160'],datasets:[{label:'kvstore',data:[31176,62617,111856,182481,285714]}]},options:{scales:{xAxes:[{scaleLabel:{display:true,labelString:'红黑树：批量处理（条）'}}],yAxes:[{scaleLabel:{display:true,labelString:'qps（次/秒）'},ticks:{beginAtZero:true}}]}}})

#### 主从同步性能对比

每个数据结构单独插入10万条数据：

##### 实时数据同步的性能：

![实时数据同步性能的簇状图](https://quickchart.io/chart?c={type:'bar',data:{labels:['红黑树','跳表','哈希'],datasets:[{label:'关闭主从同步',data:[3087,3270,2782]},{label:'开启主从同步（ebpf%20转发）',data:[1814,1812,1528]},{label:'开启主从同步（send%20转发）',data:[2291,2464,2158]}]},options:{scales:{xAxes:[{scaleLabel:{display:true,labelString:'数据结构'}}],yAxes:[{scaleLabel:{display:true,labelString:'qps（次/秒）'},ticks:{beginAtZero:true}}]}}})

##### 已有数据同步的性能：

![已有数据同步性能的簇状图](https://quickchart.io/chart?c={type:'bar',data:{labels:['Rdma','Sendfile'],datasets:[{label:'文件大小（102.3MB）',data:[23.06,41.15]}]},options:{scales:{xAxes:[{scaleLabel:{display:true,labelString:''}}],yAxes:[{scaleLabel:{display:true,labelString:'传输速度（MB/s）'},ticks:{beginAtZero:true}}]}}})

#### 内存池性能对比

开始：启动 Kvstore 时的虚拟内存/物理内存

峰值：插入一百万条数据后的虚拟内存/物理内存

结束：清空一百万条数据后的虚拟内存/物理内存

![内存池（虚拟内存）的簇状图](https://quickchart.io/chart?c={type:'bar',data:{labels:['开始','峰值','结束'],datasets:[{label:'无内存池',data:[27756,152760,152760]},{label:'有内存池',data:[27760,99588,99672]},{label:'jemalloc',data:[48856,136920,136920]}]},options:{scales:{xAxes:[{scaleLabel:{display:true,labelString:'虚拟内存（VIRT）对比'}}],yAxes:[{scaleLabel:{display:true,labelString:'虚拟内存（KB）'},ticks:{beginAtZero:true}}]}}})

![内存池（物理内存）的簇状图](https://quickchart.io/chart?c={type:'bar',data:{labels:['开始','峰值','结束'],datasets:[{label:'无内存池',data:[2540,127560,127560]},{label:'有内存池',data:[2548,74452,74580]},{label:'jemalloc',data:[4904,84644,6884]}]},options:{scales:{xAxes:[{scaleLabel:{display:true,labelString:'物理内存（RES）对比'}}],yAxes:[{scaleLabel:{display:true,labelString:'物理内存（KB）'},ticks:{beginAtZero:true}}]}}})
