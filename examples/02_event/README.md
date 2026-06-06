# 示例 2 — Event / Notification (发布订阅)

## 这个例子做什么

service 周期(1 秒)推送"温度"事件,client 订阅后实时收到值。

```
                    Event 0x8001 (eventgroup 0x0001)
 ┌──────────┐  ─────────────────────────────────▶  ┌──────────┐
 │ service  │  notify(temperature=20.3°C)           │ client   │
 │  0x2001  │                                       │  0x2002  │
 └──────────┘                                       └──────────┘
```

## 核心概念

| 概念 | 解释 |
|------|------|
| **Event** | 单条事件流。Event-ID 的最高位 (bit15) 必须为 `1` —— 0x8001 满足这个规则 |
| **Eventgroup** | 一组 Event 的逻辑容器。**订阅是按 eventgroup,不按单个 event**。这个设计让 service 可以一次发布多个相关事件,client 一次订阅全收到 |
| `is_field`  | JSON 里此项为 `false` 表示纯 Event;`true` 表示 Field 的 Notifier(下一个例子用到) |
| `is_reliable` | `false` => UDP, 适合周期性高频小报文;`true` => TCP |

## offer_event 的参数

```cpp
g_app->offer_event(
    SERVICE_ID, INSTANCE_ID, EVENT_ID,
    {EVENTGROUP_ID},                              // 它属于哪些组
    event_type_e::ET_EVENT,                       // 普通 Event(其他: ET_FIELD, ET_SELECTIVE_EVENT)
    cycle = 0ms,                                  // 内部节流周期, 0 表示不节流
    update_on_change_only = false,                // true 时只在值变化才推
    notification_on_sub   = true,                 // 新订阅者立即收到一次最近值 (Initial Event)
    epsilon_change_func   = nullptr,              // 自定义"变化判定"
    reliability           = RT_UNRELIABLE         // UDP
);
```

## client 订阅的两步

```cpp
g_app->request_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, {EVENTGROUP_ID}, ...);
g_app->subscribe   (SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID);
```

注意:
- `request_event` 是**本地**的声明:让 routing-manager 知道我准备订阅。
- `subscribe` 才会真正在 SD 层发出 `SubscribeEventgroup` 报文。
- 顺序不能反:先 request,再 subscribe。

## 运行

```bash
# 终端 A
bash scripts/run.sh 02 service

# 终端 B
bash scripts/run.sh 02 client
```

预期 client 输出:
```
[client] start()
[client] service 0x2222/0x1 => UP
[client] subscribed eventgroup 0x1
[client] event 0x8001 temperature=20°C
[client] event 0x8001 temperature=20.3°C
...
```

## 进阶玩法

- 启动 client → 等几秒再启动 service → 你会看到 `UP` 出现后立刻收到第一个事件,这是 `notification_on_sub=true` 的功劳。
- 把 `update_on_change_only` 改成 `true`,然后让 service 用同一个值反复 `notify`,client 收不到重复事件。
- 多开几个 client 实例(改 client.json 里的 application id),同时订阅,会看到 service 每次 notify 都广播给所有订阅者 —— 实际上 vsomeip 在 UDP 多播未配置时会逐个 unicast 发送。
