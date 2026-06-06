# someip-test 项目说明

vsomeip3 学习项目，通过 4 个递进示例演示 SOME/IP 的核心机制。

## 项目结构

- `examples/` — 4 个示例源码（01~04，复杂度递增）
- `config/` — 每个示例对应的 vsomeip JSON 配置文件
- `docs/` — 中文学习文档
- `scripts/` — 安装脚本与运行脚本
- `third_party/vsomeip/` — 将系统安装的 vsomeip3 包装为 Bazel cc_library

## 构建系统

使用 **Bazel**（MODULE.bazel / WORKSPACE.bazel）。vsomeip3 本身由 CMake 构建并安装到系统，业务代码通过 Bazel 编译。

```bash
# 安装 vsomeip3（仅首次）
sudo bash scripts/install_vsomeip.sh

# 构建某个示例
bazel build //examples/01_request_response/...

# 运行示例
bash scripts/run.sh 01_request_response
```

## 示例说明

| 目录 | 机制 |
|------|------|
| `01_request_response` | 最小 hello-world：方法调用 + 响应 |
| `02_event` | 周期推送传感器值（publish/subscribe） |
| `03_field` | Getter / Setter / Notifier 三件套 |
| `04_multi_client_sd` | 1 service + 2 client，走真实 SD 自动发现 |

## 构建与测试环境

- **主机**：`192.168.31.55`
- **用户**：`jyb`
- **密码**：`0`
- 登录方式：`ssh jyb@192.168.31.55`

vsomeip3 的编译、安装及示例运行均在此远程主机上执行。

## 开发约定

- C++ 标准：C++14（与 vsomeip3 保持一致）
- 配置文件：每个示例独立的 JSON 配置，位于 `config/<示例名>/`
- 不要直接修改 `third_party/vsomeip/BUILD.bazel` 中的系统路径，保持通用性
