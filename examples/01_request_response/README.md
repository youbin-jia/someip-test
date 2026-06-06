# 示例 1 — Request/Response (远程方法调用)

## 这个例子做什么

最简单的 SOME/IP 用法:client 调用 service 的 `echo` 方法,把字符串原样取回来。

```
 ┌──────────┐    Request (Method 0x0421, payload="hello-1")    ┌──────────┐
 │  client  │ ──────────────────────────────────────────────▶  │ service  │
 │  0x1002  │                                                   │  0x1001  │
 │          │ ◀──────────────────────────────────────────────   │          │
 └──────────┘    Response (payload="echo: hello-1")             └──────────┘
```

## ID 分配

| 角色 | 值 | 说明 |
|------|----|------|
| Service-ID  | `0x1111` | 服务的全局 ID,client/server/JSON 必须一致 |
| Instance-ID | `0x0001` | 服务实例 ID,允许同一 Service 有多个实例 |
| Method-ID   | `0x0421` | 方法 ID。SOME/IP 规定 bit15=0 表示 Method,=1 表示 Event |
| client(应用)ID | `0x1001` / `0x1002` | 在 JSON 里给每个进程预先分配,方便日志追踪 |

## 运行

```bash
# 终端 A
bash scripts/run.sh 01 service

# 终端 B
bash scripts/run.sh 01 client
```

预期输出 (service 端):
```
[service] start(),按 Ctrl+C 退出
[service] 已注册到 routing-manager,offer_service
[service] 收到 echo 请求, payload="hello-1" (来自 client 0x1002)
...
```

预期输出 (client 端):
```
[client] availability: service=0x1111 instance=0x1 => UP
[client] 发送 request #1 payload="hello-1"
[client] 收到响应: "echo: hello-1"
```

## 关键代码导读

### service 端的三步走

1. **`create_application` + `init`** — 读取 `VSOMEIP_CONFIGURATION` 指向的 JSON,从 `VSOMEIP_APPLICATION_NAME` 拿到自己的名字。
2. **`offer_service(SERVICE, INSTANCE)`** — 这一行是关键。它告诉 vsomeip 的 routing-manager:"我提供 0x1111/0x0001 这个服务"。
   随后 SD 会周期性发送 `OfferService` 报文(默认 2 秒一次,见 `cyclic_offer_delay`)。
3. **`register_message_handler`** — 把 (service, instance, method) 三元组绑到一个 C++ 回调上。请求到达时 vsomeip 会在内部线程池里调用它。

### client 端的三步走

1. **`request_service`** — 表达"我对这个服务感兴趣"。一旦 routing-manager 收到对应的 `OfferService`,就会触发 availability 回调。
2. **`register_availability_handler`** — 当服务上/下线时被调。**永远不要**在服务还没 UP 时就 `send()`,vsomeip 会把消息丢掉(并打 warn)。
3. **`create_request` + `send`** — 异步发送。响应通过 `register_message_handler` 中注册的回调返回。每个 request 都会被自动分配一个 session-id,vsomeip 用它来配对响应。

## SOME/IP 报文头(抓包验证)

如果你用 wireshark 抓 `lo` 接口 (`tcp.port == 30501`),会看到这样的报文:
```
Service-ID  : 0x1111
Method-ID   : 0x0421
Length      : N+8           (=payload+8 bytes header)
Client-ID   : 0x1002        (在 JSON 中分配)
Session-ID  : 0x0001        (递增,vsomeip 自动管)
Protocol    : 0x01
Interface   : 0x01
Message-Type: 0x00 (REQUEST) / 0x80 (RESPONSE)
Return-Code : 0x00 (E_OK)
Payload     : "hello-1"
```

## 常见问题

- **client 一直显示 "等待 service ..." 永远不 UP**
  → 99% 是 SD 多播没通。最常见的原因是 `lo` 接口没启用多播路由:
    ```bash
    sudo ip route add 224.244.224.245/32 dev lo
    sudo ifconfig lo multicast
    ```
- **进程立即报 `routing manager not connected`**
  → 通常是上一次跑挂了的 service 没清干净 `/tmp/vsomeip-*` socket 文件。`rm /tmp/vsomeip-*` 即可。
