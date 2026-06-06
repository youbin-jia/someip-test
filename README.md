# vsomeip 学习项目 (someip-test)

> 一个**自带文档、自带构建、自带一键运行**的 vsomeip3 学习仓库。
> 通过 4 个递进示例,系统讲清楚 SOME/IP 的 **Request/Response、Event、Field、Service Discovery** 四大核心机制。

vsomeip 是 GENIVI / COVESA 维护的 SOME/IP 协议栈 C++ 实现 (项目地址: <https://github.com/COVESA/vsomeip>)。
SOME/IP (Scalable service-Oriented MiddlewarE over IP) 是 AUTOSAR 标准中面向以太网的车载服务通信中间件协议。

## 目录结构

```
someip-test/
├── README.md                      # ← 你现在看的这份
├── WORKSPACE.bazel                # Bazel workspace
├── .bazelrc                       # Bazel 编译选项
├── BUILD.bazel                    # 顶层 BUILD(空,占位)
├── scripts/
│   ├── install_vsomeip.sh         # 一键编译安装 vsomeip3 到 /usr/local
│   └── run.sh                     # 一键启动某个示例 (service + client)
├── third_party/
│   └── vsomeip/BUILD.bazel        # 把系统装的 vsomeip3 包成 cc_library
├── config/                        # 每个示例的 vsomeip JSON 配置
│   ├── 01_request_response/
│   ├── 02_event/
│   ├── 03_field/
│   └── 04_multi_client_sd/
├── examples/                      # 4 个示例的源码
│   ├── 01_request_response/       # 最小 hello-world: 调一个方法,拿一个响应
│   ├── 02_event/                  # 周期推送传感器值 (publish/subscribe)
│   ├── 03_field/                  # Getter / Setter / Notifier 三件套
│   └── 04_multi_client_sd/        # 1 service + 2 client, 走真实 SD 自动发现
└── docs/                          # 中文学习文档
    ├── 01_concepts.md             # 原理与核心概念
    ├── 02_json_config.md          # vsomeip JSON 配置完整解析
    ├── 03_service_discovery.md    # SD 工作机制 & 抓包视角
    └── 04_faq.md                  # 常见坑 & 排错
```

## 快速开始

### 1) 安装 vsomeip3 (只需一次)

```bash
sudo bash scripts/install_vsomeip.sh
```

这会克隆 vsomeip 源码到 `/tmp/vsomeip-build`,用 CMake 编译并 `make install` 到 `/usr/local`。
> ⚠️ 为什么不用纯 bazel 构建 vsomeip 自身?vsomeip 上游只提供 CMake,把它再翻译成 bazel 既容易过期又会偏离学习重点。
> 工程上更常见的做法是:**第三方依赖用其原生构建系统装到系统里,业务代码用 bazel** —— 我们走这条路。

### 2) Bazel 构建所有示例

```bash
bazel build //examples/...
```

### 3) 运行示例

每个示例都需要分别启动 **service** 和 **client** 两个进程。打开两个终端:

```bash
# 终端 A — service
bash scripts/run.sh 01 service

# 终端 B — client
bash scripts/run.sh 01 client
```

`scripts/run.sh <编号> <角色>` 会自动:
1. 设置 `VSOMEIP_CONFIGURATION=config/<编号>_*/<角色>.json`
2. 设置 `VSOMEIP_APPLICATION_NAME=<角色>`
3. 启动对应的可执行文件

把 `01` 换成 `02 / 03 / 04` 即可运行其它示例。

## 推荐学习路径

按顺序阅读 + 实跑:

1. `docs/01_concepts.md` — **先看这个**,理解 Service-ID / Instance-ID / Method-ID / Event-ID / Eventgroup 的关系。
2. `examples/01_request_response/` — 最小例子,看清楚一次远程方法调用都发生了什么。
3. `examples/02_event/` — 学 publish / subscribe 模式。
4. `examples/03_field/` — 看 SOME/IP 怎么把"属性"封装成 Get/Set/Notify。
5. `docs/02_json_config.md` + `docs/03_service_discovery.md` — 配置和 SD 协议。
6. `examples/04_multi_client_sd/` — 真实场景:多 client 通过 SD 自动找到 service。
7. `docs/04_faq.md` — 调试 / 抓包 / 常见错误。

## 适用环境

- Linux (Debian / Ubuntu 推荐)
- gcc ≥ 9, CMake ≥ 3.13, Boost ≥ 1.71
- bazel 6.x / 7.x

本项目的所有进程默认都跑在 **同一台机器** 上,通过 `127.0.0.1` 通信,这是 vsomeip 学习最方便的形态。
跨主机配置请看 `docs/03_service_discovery.md` 末尾的「跨主机部署」一节。
