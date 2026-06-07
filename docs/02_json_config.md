# 02 — vsomeip JSON 配置详解

vsomeip 的运行时几乎完全由 JSON 驱动 —— 即使是同一份代码,换一个 JSON,就可以从 server 变 client、从本机变跨网段、从 TCP 变 UDP。

## 找到配置的优先级

进程启动时,vsomeip 按以下顺序查找配置:

1. 环境变量 `VSOMEIP_CONFIGURATION` 指定的文件
2. 当前目录下 `./vsomeip.json`
3. `/etc/vsomeip.json`

应用名字按这个顺序找:
1. `VSOMEIP_APPLICATION_NAME`
2. `create_application("xxx")` 调用时传入的字符串
3. JSON `applications` 里只有一个时,直接用它

**本项目用 `scripts/run.sh` 设置上面两个环境变量。**

## 一份"完整"配置长什么样

下面把每个常见字段都写上注释,你可以照着改:

```json
{
    // ─── 网络 ───────────────────────────────────────────────────
    "unicast": "127.0.0.1",          // 本机的 IP (绑哪个网卡)
    "netmask": "255.255.255.0",      // 网段, 跨主机时必填

    // ─── 日志 ───────────────────────────────────────────────────
    "logging": {
        "level": "info",             // trace|debug|info|warning|error|fatal
        "console": "true",
        "file":  { "enable": "false", "path": "/var/log/vsomeip.log" },
        "dlt":   "false"             // DLT (Diagnostic Log and Trace), AUTOSAR 标准
    },

    // ─── 应用列表 ───────────────────────────────────────────────
    // 每个 application name 对应一个进程 (或一个 create_application)
    // id 是它的 16-bit Client-ID, 在 SOME/IP 报文头里出现
    "applications": [
        { "name": "service", "id": "0x1001" }
    ],

    // ─── 服务声明 (offer 一方写) ─────────────────────────────────
    "services": [
        {
            "service":  "0x1111",                       // Service-ID
            "instance": "0x0001",                       // Instance-ID
            "reliable":   { "port": "30501" },          // TCP 端口 (可选)
            "unreliable":  "30502",                     // UDP 端口 (可选)
            // 至少要写 reliable 或 unreliable 之一

            "events": [
                {
                    "event":       "0x8001",            // Event-ID
                    "is_field":    "false",             // true=Field 的 Notifier
                    "is_reliable": "false",             // false=UDP, true=TCP
                    "update-cycle": "1000"              // ms; 内部最小推送间隔
                }
            ],
            "eventgroups": [
                {
                    "eventgroup": "0x0001",
                    "events":     [ "0x8001" ]          // 这一组里包含哪些 event
                }
            ]
        }
    ],

    // ─── client 的静态远端声明 (可选) ─────────────────────────────
    // 开启 SD 时通常不写,client 依赖 FindService/OfferService 发现远端服务
    // 关闭 SD 时才需要写,用来静态指定远端服务端口
    "clients": [
        {
            "service":  "0x2222",
            "instance": "0x0001",
            "reliable_remote_port": "30501",
            "unreliable_remote_port": "30502"
        }
    ],

    // ─── routing-manager ────────────────────────────────────────
    "routing": "service",   // 指向某个 application name; 它担任 routing-manager
                            // 同主机所有 vsomeip 进程必须写一致
    // 也可以写一个对象指定独立守护进程, 见下文 "独立 vsomeipd"

    // ─── Service Discovery ─────────────────────────────────────
    "service-discovery": {
        "enable":    "true",
        "multicast": "224.244.224.245",   // SOME/IP-SD 多播地址 (AUTOSAR 默认)
        "port":      "30490",             // SD 端口 (AUTOSAR 默认)
        "protocol":  "udp",               // 必须 udp

        // 启动后多久第一次广播 OfferService (ms)
        "initial_delay_min":      "10",
        "initial_delay_max":      "100",

        // 启动期 OfferService 的"快速重发", 第 N 次延迟 = base * 2^(N-1)
        "repetitions_base_delay": "200",
        "repetitions_max":        "3",

        // 每次 OfferService/SubscribeAck 的 TTL, 单位秒。0xFFFFFF 表示永久
        "ttl":                    "3",

        // 稳态阶段每隔多久重发一次 OfferService (ms)
        "cyclic_offer_delay":     "2000",

        // 收到 FindService 后,服务方延迟多久回 (ms),用来抖开
        "request_response_delay": { "minimum": "10", "maximum": "100" }
    }
}
```

## 关键点详解

### `routing` 字段的三种写法

```json
"routing": "service"                         // 1) 简写: 名字叫 service 的应用是 RM
"routing": { "name": "vsomeipd",             // 2) 显式独立守护进程模式
              "host": "vsomeipd",
              "uid": "0", "gid": "0" }
"routing": { "host": { "name": "service",    // 3) 带访问控制 (vsomeip 3.4+)
                       "uid": "1000",
                       "gid": "1000" } }
```

### 为什么 client 的 JSON 也要写 `routing` 和 SD?

- `routing` 必须和 service 端**字面一致**,否则 client 进程会以为自己应该当 RM,起不来。
- `service-discovery.enable=true` 才会生效 SD;关了的话 vsomeip 只能用静态的 `clients[]`/`services[]` 配置。

### `update-cycle` 与 `update_on_change_only`

这两个常被搞混:
- **`update-cycle` (JSON 里, 单位 ms)**:vsomeip 内部对该 event 的最小重发周期。设了它后,即便 `notify()` 被密集调用也只会按这个周期发出去。设 0 关掉。
- **`update_on_change_only` (offer_event() 的代码参数)**:`true` 时,只有 payload 真的变了才发。

两者可叠加:`update-cycle=100ms` + `update_on_change_only=true` ⇒ 每 100ms 检查一次,变了才发。

### `clients[]` 必要性

`clients[]` 在 SD 关闭、需要静态指向远端 IP/端口时使用;在我们这些走 SD 的例子里**完全可以省略**。
省略时,client 依赖 SOME/IP-SD 的 `FindService` / `OfferService` 找到 service 的地址和端口。

### 多 service / 多 client 场景

配置规则:

1. 每个进程都要在自己的 JSON 里声明自己的 `applications[].name`。
2. 同一台机器上的所有 vsomeip 进程,`routing` 必须指向同一个 application name。
3. offer 服务的进程,在 `services[]` 里声明自己提供的服务。
4. client 进程如果走 SD,通常只需要写 `applications`、`routing`、`service-discovery`,不需要写 `services[]` 或 `clients[]`。

例如 1 个 service + 2 个 client:

```json
// service.json
{
    "applications": [
        { "name": "service", "id": "0x4001" }
    ],
    "services": [
        {
            "service": "0x4444",
            "instance": "0x0001",
            "reliable": { "port": "30504" },
            "unreliable": "30505"
        }
    ],
    "routing": "service",
    "service-discovery": { "enable": "true" }
}
```

```json
// client1.json
{
    "applications": [
        { "name": "client1", "id": "0x4002" }
    ],
    "routing": "service",
    "service-discovery": { "enable": "true" }
}
```

```json
// client2.json
{
    "applications": [
        { "name": "client2", "id": "0x4003" }
    ],
    "routing": "service",
    "service-discovery": { "enable": "true" }
}
```

如果是多个 service 进程,比如 `service_a` 和 `service_b`,也仍然只能有一个 routing-manager:

```json
// service_a.json
{
    "applications": [
        { "name": "service_a", "id": "0x5001" }
    ],
    "services": [
        {
            "service": "0x1111",
            "instance": "0x0001",
            "reliable": { "port": "30511" }
        }
    ],
    "routing": "service_a",
    "service-discovery": { "enable": "true" }
}
```

```json
// service_b.json
{
    "applications": [
        { "name": "service_b", "id": "0x5002" }
    ],
    "services": [
        {
            "service": "0x2222",
            "instance": "0x0001",
            "reliable": { "port": "30521" }
        }
    ],
    "routing": "service_a",
    "service-discovery": { "enable": "true" }
}
```

这里 `service_a` 是 routing-manager,所以 `service_b` 和所有 client 的 `"routing"` 都要写 `"service_a"`。

注意:

- `applications[].id` 是 Client-ID,每个 application 必须唯一。
- `services[].service` 是 Service-ID,按业务服务唯一规划。
- 多个 service 不能监听同一个 TCP/UDP 端口。
- `VSOMEIP_APPLICATION_NAME` 必须和当前进程 JSON 里的 `applications[].name` 完全一致。

### 安全 (`security`) 块

vsomeip 支持基于 UID/GID 的访问控制(谁能 offer 哪些服务、谁能 request),配置块叫 `security`,
本项目 4 个例子都没启用 —— 学习阶段先不引入。生产环境一般会强制开启。

## 跨主机配置要点

把 `unicast` 改成本机网卡 IP(不再是 127.0.0.1),并确保:
1. 多播地址 `224.244.224.245` 在两台机器之间能通(交换机要支持 IGMP,或者用 `iptables` 验证)。
2. 路由表里有正确的多播路由:
   ```bash
   sudo ip route add 224.244.224.245/32 dev eth0
   ```
3. 防火墙开放 `udp/30490` 以及你声明的所有 `reliable/unreliable` 端口。

## 调试小技巧

- 把 `logging.level` 调到 `debug` 或 `trace`,vsomeip 会打印每条 SOME/IP 报文的关键字段,**比 wireshark 还方便**(尤其当走的是 UNIX socket、抓不到包时)。
- `VSOMEIP_CONFIGURATION_MODULE` 环境变量可加载自定义配置插件,99% 的人用不到。
- 启动报错 `cannot read configuration from .../vsomeip.json` 说明 JSON 路径不对,先 `echo $VSOMEIP_CONFIGURATION` 检查。
