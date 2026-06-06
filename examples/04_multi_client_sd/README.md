# 示例 4 — 多客户端 + Service Discovery

## 这个例子做什么

一个 service,两个 client,把前面学到的全用上,并把 SD 的工作流程暴露给你看。

```
                ┌──────────┐
   ┌──── echo ──│ client1  │
   │            └──────────┘
   ▼
┌──────────┐
│ service  │   ── tick (UDP, 1Hz) ────┐
│  0x4001  │                          ▼
└──────────┘                    ┌──────────┐
                                │ client2  │
                                └──────────┘
```

- **client1**:每 2 秒调一次 `echo` 方法 (TCP/30504)
- **client2**:订阅 `tick` 事件 (UDP/30505) 并打印
- **service**:同时处理 echo 调用 + 周期 notify

## 运行(三个终端)

```bash
# 终端 A
bash scripts/run.sh 04 service

# 终端 B
bash scripts/run.sh 04 client1

# 终端 C
bash scripts/run.sh 04 client2
```

启动顺序无所谓 —— 这正是 SD 的意义。任意先启动 client,服务一上线 client 都会立刻收到 `availability UP`。

## 关键观察:routing-manager

打开三个进程的日志,你会发现**每个 client 启动时都会试图当 routing-manager**,但只有一个会成功。
这是 vsomeip 的"宿主进程"机制:

- `routing` 字段在 JSON 里指定哪个应用名字担任 routing-manager(本例都指向 `service`)
- 第一个启动的 routing-manager 会监听 `/tmp/vsomeip-0` UNIX socket
- 其他进程通过这个 socket 把 SOME/IP 流量上交给它统一发出去 —— 多进程共享一组真实的网络连接

这意味着:
- **service 这个进程必须先起**(或者你可以另起一个独立的 `vsomeipd` 充当 routing-manager,这是生产环境推荐做法)
- 三个进程的 application name 必须各不相同(`service`/`client1`/`client2`,在各自的 JSON 中分别声明)

## SD 报文流(抓包视角)

如果你用 `sudo tcpdump -i any port 30490 -X` 抓 SD 报文,会看到:

```
service → 224.244.224.245:30490    OfferService(0x4444/0x0001, ...)   每 2 秒一次
client1 → 224.244.224.245:30490    FindService(0x4444/0x0001)         启动时 + 几次重试
client2 → service-IP:30490         SubscribeEventgroup(0x0004)        unicast 直接发给提供者
service → client2-IP:30490         SubscribeEventgroupAck             同上
```

注意 `Find` 和 `Offer` 走多播,`Subscribe` 走单播 —— 因为前者要广播搜索,后者已经知道目标了。

## 关闭 service 后再开

把 service 进程 Ctrl+C 杀掉,client1/client2 几秒内会打印 `service DOWN`(基于 SD 的 TTL 超时),
然后再重启 service,它们会自动重新连上,无需任何操作 —— 这就是 SD 的容错价值。
