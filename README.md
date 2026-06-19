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


## 3. 通信协议

kvstore 当前支持两种客户端通信协议：自定义的 **KVSP 协议** 和兼容 Redis 的 **RESP 协议**。服务端会在客户端连接建立后，根据首个请求报文的起始内容自动识别协议类型，并在该连接后续通信中使用对应的解析方式。

### 3.1 协议兼容性

kvstore 支持以下两种请求格式：

| 协议   | 说明                                                                      |
| ---- | ----------------------------------------------------------------------- |
| KVSP | kvstore 自定义协议，使用 `#<body_length>\r\n^<tok_len>&<tok>...\r\n` 格式描述一条完整命令 |
| RESP | 兼容 Redis RESP 数组与批量字符串格式，可通过 hiredis 等 Redis 客户端发送请求                    |

协议识别规则如下：

| 请求起始内容 | 协议类型 |
| ------ | ---- |
| `#`    | KVSP |
| `*`    | RESP |

其中，KVSP 协议请求必须以 `#` 开头，RESP 协议请求必须以 `*` 开头。服务端在识别协议后，会为当前客户端连接绑定对应的协议解析函数。

### 3.2 KVSP 请求格式

KVSP 协议采用文本化长度前缀格式。一条完整请求由协议头和命令 body 组成：

```text
#<body_length>\r\n<body>
```

其中：

| 字段              | 说明                               |
| --------------- | -------------------------------- |
| `#`             | KVSP 请求起始标志                      |
| `<body_length>` | body 部分的总字节长度                    |
| `\r\n`          | 协议头结束标志                          |
| `<body>`        | 命令参数区，由若干个 token 组成，并以 `\r\n` 结束 |

body 内部的参数编码方式如下：

```text
^<tok_len>&<tok>^<tok_len>&<tok>...\r\n
```

其中：

| 字段          | 说明             |
| ----------- | -------------- |
| `^`         | token 长度字段起始标志 |
| `<tok_len>` | 当前 token 的字节长度 |
| `&`         | token 内容起始标志   |
| `<tok>`     | token 内容       |
| `\r\n`      | body 结束标志      |

因此，一条 KVSP 请求的完整格式为：

```text
#<body_length>\r\n^<len1>&<arg1>^<len2>&<arg2>...\r\n
```

需要注意的是，`body_length` 表示 body 部分的总字节数，包含 body 末尾的 `\r\n`。

### 3.3 KVSP 请求示例

#### SET 请求

命令：

```text
SET key value
```

对应 KVSP 编码为：

```text
#22\r\n^3&SET^3&key^5&value\r\n
```

其中 body 部分为：

```text
^3&SET^3&key^5&value\r\n
```

body 长度计算如下：

| 参数      | 编码形式       | 字节数 |
| ------- | ---------- | --- |
| `SET`   | `^3&SET`   | 6   |
| `key`   | `^3&key`   | 6   |
| `value` | `^5&value` | 8   |
| 结束字符    | `\r\n`     | 2   |
| 合计      | -          | 22  |

#### GET 请求

命令：

```text
GET key
```

对应 KVSP 编码为：

```text
#14\r\n^3&GET^3&key\r\n
```

其中 body 部分为：

```text
^3&GET^3&key\r\n
```

body 长度计算如下：

| 参数    | 编码形式     | 字节数 |
| ----- | -------- | --- |
| `GET` | `^3&GET` | 6   |
| `key` | `^3&key` | 6   |
| 结束字符  | `\r\n`   | 2   |
| 合计    | -        | 14  |

#### SAVE 请求

命令：

```text
SAVE
```

对应 KVSP 编码为：

```text
#9\r\n^4&SAVE\r\n
```

其中 body 部分为：

```text
^4&SAVE\r\n
```

body 长度计算如下：

| 参数     | 编码形式      | 字节数 |
| ------ | --------- | --- |
| `SAVE` | `^4&SAVE` | 7   |
| 结束字符   | `\r\n`    | 2   |
| 合计     | -         | 9   |

### 3.4 RESP 兼容格式

kvstore 同时兼容 Redis RESP 协议中的数组和批量字符串格式，因此可以使用 hiredis 等 Redis 客户端发送请求。

例如，命令：

```text
SET key value
```

对应 RESP 编码为：

```text
*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n
```

命令：

```text
GET key
```

对应 RESP 编码为：

```text
*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n
```

需要注意的是，kvstore 当前主要兼容 Redis RESP 的命令编码格式，并不表示完整实现 Redis 的所有命令语义。

### 3.5 响应格式

当前 kvstore 的响应格式保持 RESP-like 风格，主要包括：

| 响应格式                    | 含义                   |
| ----------------------- | -------------------- |
| `+OK\r\n`               | 操作成功                 |
| `-ERR message\r\n`      | 操作失败                 |
| `:1\r\n`                | 查询结果为存在，或写入时 key 已存在 |
| `$-1\r\n`               | 查询结果为空，或 key 不存在     |
| `$<len>\r\n<value>\r\n` | 返回字符串 value          |

例如，GET 请求成功返回 value 时：

```text
$5\r\nvalue\r\n
```

当 key 不存在时：

```text
$-1\r\n
```




## 4.协议指令（四种数据结构）

### 4.1 数组 （array）
| 指令  | 参数  | 功能说明  | 成功响应  | 失败响应  |
| :--- | :--- | :--- | :--- | :--- |
| **SET** | `<key> <value>` | 存储键值对 | +OK\r\n / :1\r\n | -ERR message\r\n |
| **GET** | `<key>` | 根据 Key 获取对应的 Value | `<value>` / $-1\r\n | -ERR message\r\n |
| **MOD** | `<key> <value>` | 修改已存在的 Key 对应的 Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **DEL** | `<key>` | 从存储引擎中删除指定的 Key-Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **EXIST** | `<key>` | 查询指定的 Key 是否存在于系统中 | :1\r\n / $-1\r\n | -ERR message\r\n |
 
### 4.2 哈希 （hash）
| 指令  | 参数  | 功能说明  | 成功响应  | 失败响应  |
| :--- | :--- | :--- | :--- | :--- |
| **HSET** | `<key> <value>` | 存储键值对 | +OK\r\n / :1\r\n | -ERR message\r\n |
| **HGET** | `<key>` | 根据 Key 获取对应的 Value | `<value>` / $-1\r\n | -ERR message\r\n |
| **HMOD** | `<key> <value>` | 修改已存在的 Key 对应的 Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **HDEL** | `<key>` | 从存储引擎中删除指定的 Key-Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **HEXIST** | `<key>` | 查询指定的 Key 是否存在于系统中 | :1\r\n / $-1\r\n | -ERR message\r\n |

### 4.3 跳表 （skiplist）
| 指令  | 参数  | 功能说明  | 成功响应  | 失败响应  |
| :--- | :--- | :--- | :--- | :--- |
| **LSET** | `<key> <value>` | 存储键值对 | +OK\r\n / :1\r\n | -ERR message\r\n |
| **LGET** | `<key>` | 根据 Key 获取对应的 Value | `<value>` / $-1\r\n | -ERR message\r\n |
| **LMOD** | `<key> <value>` | 修改已存在的 Key 对应的 Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **LDEL** | `<key>` | 从存储引擎中删除指定的 Key-Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **LEXIST** | `<key>` | 查询指定的 Key 是否存在于系统中 | :1\r\n / $-1\r\n | -ERR message\r\n |


### 4.4 红黑树 （rbtree）
| 指令  | 参数  | 功能说明  | 成功响应  | 失败响应  |
| :--- | :--- | :--- | :--- | :--- |
| **RSET** | `<key> <value>` | 存储键值对 | +OK\r\n / :1\r\n | -ERR message\r\n |
| **RGET** | `<key>` | 根据 Key 获取对应的 Value | `<value>` / $-1\r\n | -ERR message\r\n |
| **RMOD** | `<key> <value>` | 修改已存在的 Key 对应的 Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **RDEL** | `<key>` | 从存储引擎中删除指定的 Key-Value | +OK\r\n / $-1\r\n | -ERR message\r\n |
| **REXIST** | `<key>` | 查询指定的 Key 是否存在于系统中 | :1\r\n / $-1\r\n | -ERR message\r\n |

### 4.5 SAVE指令 将当前内存数据持久化到磁盘文件


## 5. 核心功能模块

### 5.1 增量持久化 （AOF 机制）

为了确保内存数据在系统宕机或重启后能够恢复，实现了增量持久化功能。

#### 5.1.1 触发策略
采用“写时记录”原则，仅针对会改变内存数据状态的指令进行日志落盘，从而平衡了数据安全与磁盘 IO 性能。
* **记录指令**：`SET`、`MOD`、`DEL`。
* **忽略指令**：`GET`、`EXIST`、`SAVE`（此类指令不修改数据，无须记录）。

#### 5.1.2 实现原理

数据落盘用io_uring实现，加载持久化数据用mmap。

### 5.2 全量持久化 （SAVE）
提供了 `SAVE` 指令，用于将当前内存中的全量数据持久化到 `.rdb` 文件中。

#### 5.2.1 触发策略
解析到 `SAVE`指令时。

#### 5.2.2 实现原理

数据落盘用io_uring实现，加载持久化数据用mmap。



### 5.3 主从同步模块
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


## 6.测试方案

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


    



## 7.Kvstore 性能

虚拟机配置：
![虚拟机配置](https://img.0voice.com/6780/8ae9726839f2cc8abacb45d423f8d456.png)

Linux内核版本：ubuntu 22.04.5  6.8.0-107-generic

### 全量持久化性能

测试数据基于哈希：一百万条数据


| SAVE间隔 \ 性能指标 | qps |
| :--- | :---: |
| **1k\次** | 16013 |
| **1w\次** | 17866 |
| **10w\次** | 15507 |
| **100w\次** | 3300 |


### Kvstore性能对比

#### 读（GET），写（SET）性能对比

测试数据基于哈希：十万条数据

**性能指标：qps**  

**echo服务器：3324**

| 测试条件 \ 测试对象 | redis | kvstore |
| :--- | :---: |  :---: |
| **关闭AOF（写性能）** | 3097 | 3272 |
| **关闭AOF（读性能）** | 3188 | 3244 |
| **开启AOF（写性能）** | 2953 | 3208 |
| **开启AOF（读性能）** | 3004 | 3260 |

#### 批量处理性能对比

测试数据基于哈希：十万条数据

**性能指标：qps**  

| 批量处理（条） \ 测试对象 | redis | kvstore |
| :--- | :---: |  :---: |
| **10** | 29180 | 32733 |
| **20** | 51519 | 70921 |
| **40** | 97751 | 128534 |
| **80** | 168350 | 229357 |
| **120** | 273224 | 300540 |

#### 主从同步性能对比

##### 实时数据同步的性能：

测试数据基于哈希：十万条数据

| 测试条件 \ 性能指标 | qps |
| :--- | :---: |
| **关闭主从同步** | 3334 |
| **开启主从同步：uprobe** | 4684 |
| **开启主从同步：探测TCP的recv** | 5722 |
| **开启主从同步：网络send转发** | 2485 |


##### 已有数据同步的性能：

文件大小：1.03GB

| 测试方法 \ 性能指标 | 传输速度 (MB/s) | 网络吞吐量 (Mbps) |
| :--- | :---: | :---: |
| **Iperf3** | 36.975  | 295.8 |
| **Sendfile** | 35.488 | 283.9 |
| **ib_write_bw** | 22.5 | 180 |
| **RDMA** | 22.22 | 177.76 |

#### 内存分配方案性能对比

开始：启动 Kvstore 时的虚拟内存/物理内存

峰值：插入一百万条数据后的虚拟内存/物理内存

结束：清空一百万条数据后的虚拟内存/物理内存


| 分配方案 \ 虚拟内存 (KB) | 开始  | 峰值  | 结束  |
| :--- | :---: | :---: | :---: |
| **系统默认 malloc** | 27756 | 152760 | 27788 |
| **jemalloc** | 48856 | 136920 | 136920 |
| **自定义内存池** | 28084 | 106548 | 28600 |


| 分配方案 \ 物理内存 (KB) | 开始  | 峰值  | 结束  |
| :--- | :---: | :---: | :---: |
| **系统默认 malloc** | 2540 | 127560 | 2716 |
| **jemalloc** | 4904 | 84644 | 6884 |
| **自定义内存池** | 2856 | 81320 | 3496 |
