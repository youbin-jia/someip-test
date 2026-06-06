# 03 — Service Discovery (SOME/IP-SD)

> SD 是 SOME/IP 的"零配置自动发现"层。理解它,你才能解释:
> 为什么 client 启动时还没有目标 IP 也能找到 service?为什么 service 重启后 client 不需要做任何事就能恢复?

## 1. 它解决什么问题

SOME/IP 协议本身只规定"发什么字节",**不告诉你要把字节发到哪个 IP/端口**。
SD 子协议补上这块:
- service 通过多播广播 "我是谁、住在哪、还活多久"
- client 通过多播询问 "谁有服务 0xXXXX?"
- 双方还可以单播交换 "我要订阅 / 订阅成功" 这种点对点消息

## 2. SD 报文长什么样

SD 报文也是 SOME/IP 报文,只是用了一个特殊的 (Service, Method) 组合:

```
Service-ID = 0xFFFF
Method-ID  = 0x8100
Msg Type   = 0x02 (Notification)
```

它的 payload 由若干 **Entry** 和 **Option** 组成:

```
Flags + Reserved
N Entries
M Options
Entry 1 ...
Entry N
Option 1 ...
Option M
```

Entry 表示动作 (`OfferService` / `FindService` / `SubscribeEventgroup` / `SubscribeEventgroupAck` / `StopOfferService` / ...),
Option 携带具体信息 (IP/端口/协议/...)。

## 3. 完整握手时序

下面是示例 4 启动时 client2(订阅 tick 事件)与 service 的完整 SD 流程:

```
client2                              service                                multicast 224.244.224.245:30490
   │                                    │                                            │
   │                                    │── OfferService(0x4444/0x1, ttl=3) ─────────▶│  (周期 2s)
   │ ◀──── 来自多播的 OfferService ────────┼────────────────────────────────────────────│
   │                                    │                                            │
   │── SubscribeEventgroup(0x0004) ─────▶│  (单播, 不再走多播 - 已知目标)                  │
   │ ◀── SubscribeEventgroupAck ────────│                                            │
   │                                    │                                            │
   │ ◀── 真实的 Event 0x8004 (UDP) ──────│  (业务流量, 走另一个端口 30505)                 │
   │ ◀── 真实的 Event 0x8004 (UDP) ──────│                                            │
   │                                    │                                            │
   │                                  (service 退出)                                  │
   │                                    │── StopOfferService ───────────────────────▶│
   │ ◀── StopOfferService 收到 ──────────┼────────────────────────────────────────────│
   │  → 触发 availability(false) 回调      │                                            │
```

如果 service 没来得及发 StopOfferService 就 crash,client 也会基于 OfferService 的 **TTL 超时**判定服务下线。

## 4. 重要时序参数

JSON 里的 `service-discovery` 块:

| 字段 | 含义 | 调小后果 | 调大后果 |
|------|------|---------|---------|
| `initial_delay_min/max`         | 启动后第一次发 SD 的延迟(随机区间,ms)   | 启动更快 | 防风暴更稳 |
| `repetitions_base_delay`        | 启动期快速重发的初始间隔 (ms)           | 重连快   | 网络压力低 |
| `repetitions_max`               | 启动期重发次数                       | 同上     | 同上 |
| `cyclic_offer_delay`            | 稳态阶段 OfferService 周期 (ms)       | 故障发现快 | CPU/带宽低 |
| `ttl`                          | 一次 Offer/Sub 的存活时间 (秒)         | 故障检测快 | 抗抖动强 |
| `request_response_delay.min/max` | 收到 FindService 后回复的随机延迟 (ms) | 应答快   | 多服务方时减少冲突 |

**经验值**(本项目 default):
- 学习/本机调试:`cyclic_offer_delay=2000`, `ttl=3` ⇒ 关掉 service 后 ~5 秒内 client 感知到 DOWN。
- 生产车上:`cyclic_offer_delay=1000`, `ttl=3` 是常见组合;对实时性要求更高的子系统会调到 `ttl=1`。

## 5. 多播配置 (本机 lo)

vsomeip 的 SD 默认走 `224.244.224.245:30490`。
在本机上跑示例时,**lo 接口默认不参与多播路由**,会让你看到 `client 永远等不到 OfferService`。修法:

```bash
sudo ip route add 224.244.224.245/32 dev lo
sudo ifconfig lo multicast        # 老系统
sudo ip link set lo multicast on  # 新系统
```

执行一次后机器重启会丢,不在意的话写到 `/etc/network/if-up.d/` 或 systemd unit 里。

如果你不想配多播,有两种"绕开 SD"的办法:
1. 同主机:直接用同一个 routing-manager,offer/request 通过 UNIX socket 互发,**理论上不需要 SD**。
   但 vsomeip 在跨主机和同主机之间没有特别区分,关掉 SD 的话 client 还是要靠 `clients[]` 静态指端口。
2. 把 `service-discovery.enable=false`,然后:
   - service JSON 写明 `services[]`
   - client JSON 写 `clients[]` 静态指向远端 `reliable_remote_port` / `unreliable_remote_port`。

## 6. 跨主机部署

```
┌────── ECU A (192.168.1.10) ──────┐     ┌────── ECU B (192.168.1.20) ──────┐
│  service.json:                    │     │  client.json:                     │
│    unicast: 192.168.1.10          │     │    unicast: 192.168.1.20          │
│    services[]: 0x4444/0x1        │     │    routing: <local-RM>            │
│    routing: service               │     │    SD enable: true                │
│    SD enable: true                │     │                                   │
└──────────────────┬───────────────┘     └──────────────────┬────────────────┘
                   │                                          │
                   └─────── 224.244.224.245:30490 ────────────┘
                              (多播, 必须可达)
                            +
                   30504 TCP / 30505 UDP 单播 (业务流量)
```

清单:
- [ ] 两端 `unicast` 是各自网卡 IP,不是 `127.0.0.1`
- [ ] 多播在两端可达 (`ping -I eth0 224.244.224.245` 或者 `iperf` 验证)
- [ ] 防火墙放行 `30490/udp` 以及业务端口
- [ ] 时钟相差不大(SD 不依赖绝对时间,但日志对齐方便)
- [ ] 两端 `routing` 各自指向**自己**那台机器上的 RM,**不要**让两台机器共用一个 RM

## 7. 用 wireshark 看 SD

过滤器:`udp.port == 30490`。Wireshark 自带 SOME/IP 解码器,能直接展开 Entry/Option 树。
注意:某些发行版自带的 wireshark 版本太老,不识别 `OfferService` 0x01 这类 entry-type;升级到 ≥ 3.4 即可。
