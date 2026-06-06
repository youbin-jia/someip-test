# someip-test 项目说明

vsomeip3 学习项目，通过 4 个递进示例演示 SOME/IP 的核心机制。

## 项目结构

- `examples/` — 4 个示例源码（01~04，复杂度递增）
- `config/` — 每个示例对应的 vsomeip JSON 配置文件
- `docs/` — 中文学习文档
- `scripts/` — 安装脚本与运行脚本
- `third_party/vsomeip/` — 将系统安装的 vsomeip3 包装为 Bazel cc_library

## 示例说明

| 目录 | 机制 |
|------|------|
| `01_request_response` | 最小 hello-world：方法调用 + 响应 |
| `02_event` | 周期推送传感器值（publish/subscribe） |
| `03_field` | Getter / Setter / Notifier 三件套 |
| `04_multi_client_sd` | 1 service + 2 client，走真实 SD 自动发现 |

---

## 构建与测试环境

- **主机**：`192.168.31.55`（Ubuntu 24.04，Linux 6.17，x86_64）
- **用户**：`jyb` / **密码**：`0`
- **登录**：`sshpass -p '0' ssh jyb@192.168.31.55`
- **代码路径**：`~/someip-test`
- **注意**：远程机器无法访问 GitHub / 外网，代码通过 rsync 同步

### 已安装组件

| 组件 | 版本 | 位置 |
|------|------|------|
| g++ | 13.3.0 | 系统 |
| cmake | 3.28.3 | 系统 |
| Boost | apt 安装 | 系统 |
| vsomeip3 | 3.5.5 | `/usr/local/lib/libvsomeip3*.so` |
| Bazel | 7.4.1 | `~/bin/bazel`（installer 安装，非系统路径）|

---

## 构建方法

### 使用 Bazel（推荐）

```bash
# SSH 到远程机器后
export PATH=$HOME/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
cd ~/someip-test

# 构建全部示例
bazel build //examples/...

# 构建单个示例
bazel build //examples/01_request_response/...
```

> Bazel 二进制在 `~/bin/bazel`，不在系统 PATH，每次 SSH 登录需要 `export PATH=$HOME/bin:$PATH`，
> 或将其加入 `~/.bashrc`。

### 使用 g++ 直接编译（备用）

```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
g++ -std=c++14 -I/usr/local/include -o service \
    examples/01_request_response/service.cpp \
    -lvsomeip3 -lboost_system -lboost_thread -lpthread
```

### 同步代码到远程（本机执行）

```bash
# 远程机器无法访问 GitHub，从本机 rsync 同步
sshpass -p '0' rsync -az --exclude='.git' --exclude='._*' \
    /path/to/someip-test/ jyb@192.168.31.55:~/someip-test/
```

---

## 运行示例

每个示例需要 **两个终端**，分别运行 service 和 client。

```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
cd ~/someip-test

# 终端 1 — service（同时充当 routing manager）
VSOMEIP_APPLICATION_NAME=service \
VSOMEIP_CONFIGURATION=config/01_request_response/service.json \
  bazel-bin/examples/01_request_response/service

# 终端 2 — client（等 service 启动后再运行）
VSOMEIP_APPLICATION_NAME=client \
VSOMEIP_CONFIGURATION=config/01_request_response/client.json \
  bazel-bin/examples/01_request_response/client
```

**关键**：必须设置 `VSOMEIP_APPLICATION_NAME`，否则应用使用默认 ID `0xffff` 导致与配置文件不匹配。

---

## vsomeip3 重新安装（如系统重装）

远程机器无法访问外网，需要在**本机**下载后传输：

```bash
# 1. 本机下载 vsomeip 源码
git clone --depth 1 --branch 3.5.5 https://github.com/COVESA/vsomeip.git /tmp/vsomeip-src

# 2. 用 rsync 同步到远程（避免 macOS tar 的 ._ 文件污染）
sshpass -p '0' rsync -az --exclude='._*' --exclude='.DS_Store' \
    /tmp/vsomeip-src/ jyb@192.168.31.55:~/vsomeip-src/

# 3. 远程机器编译安装
ssh jyb@192.168.31.55
cd ~/vsomeip-src && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DENABLE_SIGNAL_HANDLING=1 ..
make -j$(nproc)
echo "0" | sudo -S make install && echo "0" | sudo -S ldconfig
```

## Bazel 重新安装（如需要）

```bash
# 1. 本机下载 installer（53MB）
curl -fL https://releases.bazel.build/7.4.1/release/bazel-7.4.1-installer-linux-x86_64.sh \
    -o /tmp/bazel-installer-linux.sh

# 2. 传到远程并安装
sshpass -p '0' rsync -az /tmp/bazel-installer-linux.sh jyb@192.168.31.55:~/
sshpass -p '0' ssh jyb@192.168.31.55 'chmod +x ~/bazel-installer-linux.sh && ~/bazel-installer-linux.sh --user'
# 安装后二进制在 ~/bin/bazel
```

---

## 常见问题

### `Couldn't connect to /tmp/vsomeip-0`
routing manager 未启动。确认 service 进程的配置文件中有 `"routing": "service"`，
且 service 先于 client 启动。

### 应用 ID 显示 `ffff`（默认值）
未设置 `VSOMEIP_APPLICATION_NAME` 环境变量，导致与 JSON 配置中的 `name` 不匹配。

### `sudo: a terminal is required`
SSH 非交互式执行 sudo 时，用 `echo "0" | sudo -S <cmd>` 代替直接 `sudo`。

### macOS → Linux 传输源码报 `._*` 编译错误
macOS `tar` 会附带 `com.apple.provenance` 扩展属性，在 Linux 上产生 `._xxx.cpp` 空文件被误编译。
改用 `rsync --exclude='._*'` 同步，不要用 `tar` 打包。

---

## 开发约定

- C++ 标准：C++14（与 vsomeip3 保持一致）
- 配置文件：每个示例独立的 JSON，位于 `config/<示例名>/`
- 不要修改 `third_party/vsomeip/BUILD.bazel` 中的系统路径
- `create_application()` 不传名字时从 `VSOMEIP_APPLICATION_NAME` 读取，运行时必须设置
