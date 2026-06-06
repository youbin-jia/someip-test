# 01 — SOME/IP & vsomeip 核心概念

> 看完这篇,你应该能用一段话给同事讲清楚:**什么是 SOME/IP,vsomeip 在里头扮演什么角色,Service-ID/Instance-ID/Method-ID/Event-ID/Eventgroup 各自管什么**。

## 1. SOME/IP 是什么

SOME/IP = **S**calable service-**O**riented **M**iddlewar**E** over **IP**。

它是 AUTOSAR 标准化的、跑在 IP 之上的**面向服务的车载通信中间件**协议。
解决的是这个问题:车里几十个 ECU 通过以太网互通时,怎么用统一、可发现、可扩展的方式做 RPC 和事件订阅。

它的层次大致是:
```
┌────────────────────────────┐
│   应用层(用户代码)            │  ← 你写的 service / client
├────────────────────────────┤
│   SOME/IP 协议头 + payload   │  ← 协议本身规定的字节布局
├────────────────────────────┤
│   SOME/IP-SD(服务发现)      │  ← 单独的子协议, 跑在 UDP 多播
├────────────────────────────┤
│   TCP / UDP                │
├────────────────────────────┤
│   IP / Ethernet            │
└────────────────────────────┘
```

**vsomeip** 是 COVESA 维护的开源 C++ 实现,把这一整套封装成易用的 API。

## 2. SOME/IP 报文格式 (16 字节固定头 + payload)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Service-ID            |          Method-ID            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            Length                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Client-ID             |          Session-ID           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Protocol Vsn  | Iface Vsn      |   Msg Type    |   Return Code |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          Payload (N)                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| 字段 | 长度 | 含义 |
|------|------|------|
| Service-ID  | 16 bit | 哪个服务,例如 0x1111 |
| Method-ID   | 16 bit | 服务里哪个 endpoint。bit15=0 是方法,bit15=1 是事件 |
| Length      | 32 bit | 从 Client-ID 到 payload 末尾的字节数 |
| Client-ID   | 16 bit | 调用方应用的 ID(同主机内唯一) |
| Session-ID  | 16 bit | 自增,用来配对 Request/Response |
| Msg Type    | 8 bit  | 0x00=Request, 0x80=Response, 0x40=RequestNoReturn, 0x02=Notification, 0x81=Error... |
| Return Code | 8 bit  | 0x00=E_OK,其余值表示各种错误 |

vsomeip 帮你处理所有这些字段,你只需关心 (Service, Instance, Method/Event) 和 payload。

## 3. ID 体系

### Service-ID (16-bit)
**全网唯一**地标识"什么样的服务"。0x1111、0x2222 这种是测试值;实车里通常按 OEM 文档分配。

### Instance-ID (16-bit)
**同一种服务的不同副本**。例如 4 个轮速传感器服务,Service-ID 都是 `0xABCD`,Instance-ID 分别是 `0x0001..0x0004`。
**协议头里没有 Instance-ID 字段** —— 它是通过"Service-ID + 目标 IP/端口"组合在 SD 层映射出来的。

### Method-ID 与 Event-ID (16-bit, 共享空间)
属于同一个 Service。
- **bit15 = 0**:Method (RPC 调用)。习惯把 Getter/Setter 等放在 0x0001..0x7FFF。
- **bit15 = 1**:Event (订阅推送)。0x8000..0xFFFF。

### Eventgroup-ID (16-bit)
若干 Event 的逻辑组,**订阅是按组进行的**。
一个 Event 可以同时属于多个 Eventgroup;一个 Eventgroup 可以包含多个 Event。

### Client-ID (16-bit) 与 Session-ID (16-bit)
- Client-ID 标识"谁在调用",通常在 JSON `applications.id` 中静态分配。
- Session-ID 由 vsomeip 在每次发送 Request 时自增,响应里原样带回来,用于 Request/Response 配对。

## 4. 通信模式三件套

| 模式 | 谁发起 | 协议层动作 | 典型用法 |
|------|--------|-----------|----------|
| **Request/Response**         | client | client 发 Request → server 发 Response | 调一个方法,例如 lockDoors() |
| **Fire-and-Forget**          | client | 只发 Request,不要响应 (Msg Type = 0x40) | 通知类调用,例如 logEvent() |
| **Notification (Event)**     | server | 主动推送给所有订阅者 (Msg Type = 0x02) | 广播状态/数据,例如车速 |

Field 不是新模式,而是上述前两种的**约定组合**:
- Getter / Setter 用 Request/Response
- Notifier 用 Notification

## 5. 传输:TCP vs UDP

| 选 TCP (`reliable`)        | 选 UDP (`unreliable`)       |
|---------------------------|----------------------------|
| 关键控制指令 / 不能丢包       | 高频小数据 / 周期推送 / 大尺寸事件 |
| 大数据 / 流式               | 多播订阅                    |
| 默认 method 用 TCP,大多更稳    | 默认 event 用 UDP            |

vsomeip 允许你在 JSON 里**同时**给一个 service 配 reliable + unreliable 端口,然后按 method/event 各自标 `is_reliable`。

> 当 UDP payload 超过 1400 字节,vsomeip 会用 SOME/IP-TP 协议自动切包重组,这一点对应用透明。

## 6. vsomeip 的进程模型

```
       ┌─────────────────────────────────────────────────┐
       │         routing-manager 进程                      │
       │  (offer 服务的进程之一, 或独立的 vsomeipd)          │
       │                                                  │
       │   监听 /tmp/vsomeip-0       监听网卡 (TCP/UDP)     │
       └─────┬────────────────────┬───────────────────────┘
       UNIX  │                    │   ↑                    
       sock  │                    │   │ SD/SOME/IP         
             ▼                    │                        
   ┌─────────────────┐    ┌───────────────┐                
   │ client/service  │    │ client/service│                
   │   进程 A         │    │   进程 B       │                
   │  (routing=...)  │    │  (routing=...)│                
   └─────────────────┘    └───────────────┘                
```

要点:
- 同一台机器上的多个 vsomeip 应用**共享一个 routing-manager**,通过 `/tmp/vsomeip-*` UNIX socket 互通。
- 哪个应用当 routing-manager 由 **JSON 里的 `routing` 字段**决定 —— 它指向某个 application name。
- 启动顺序:routing-manager 必须先起,否则其他进程会一直 retry。
- 生产环境通常另起一个 `vsomeipd` 守护进程独立担任 routing-manager,业务进程都不当。

## 7. 学习路径建议

1. 跑通 `examples/01_request_response/` —— 心里有了"我发起一次 RPC,这是怎么发生的"的图像。
2. 用 wireshark 抓 `lo` 的 TCP/30501 端口,亲眼看 SOME/IP 报文头 16 字节。
3. 跑 `examples/02_event/`,理解订阅链:`request_event` → `subscribe` → on_message_handler。
4. 读 `docs/02_json_config.md`,把"为什么这些字段长这样"搞清楚。
5. 跑 `examples/04_multi_client_sd/`,抓 UDP/30490 看 SD 报文流。
6. 读 `docs/03_service_discovery.md` 把 SD 的细节、跨主机部署、TTL 调优弄通。
