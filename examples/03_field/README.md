# 示例 3 — Field (Getter / Setter / Notifier 三件套)

## 这个例子做什么

把"亮度 brightness"建模成一个 SOME/IP **Field**。client 可以:
- `GET` — 取当前值
- `SET` — 写新值,service 会回响应,并自动 `notify` 给所有订阅者
- 订阅 Notifier — 任何变化(包括别人 SET 的)都会被推送过来

```
client          service
  │ GET (0x0001) ─▶ │
  │  ◀─ Response  │  brightness=50
  │ SET 80 (0x0002)─▶│
  │  ◀─ Response 80 │
  │   ◀─ Notify 80 │ (eventgroup 0x0002, event 0x8002)
```

## 为什么 SOME/IP 要专门搞 "Field"

实际系统里"远程属性"很常见(车速、空调档位、车窗状态…),它们既要能查、能改、还要变化时通知。
直接用 method+event 拼也行,但 AUTOSAR/SOME/IP 把这套**约定**做成了一等公民:
- 在 IDL/工具链里 (FrancaIDL、CommonAPI),Field 是声明式的;
- 在 vsomeip 里,把事件 `event_type` 设为 `ET_FIELD` 就开启了几个特别行为:
    1. 新订阅者会立即收到一次"当前值"(initial-field 通知);
    2. vsomeip 内部会缓存最近一次 notify 的 payload,在 `is_field=true` 配置下转给新订阅者。

## ID 约定

| 用途 | ID | 备注 |
|------|----|----|
| Getter (Method)   | `0x0001` | 习惯用 0x0001..0x7FFF 给 Method 留区间 |
| Setter (Method)   | `0x0002` | |
| Notifier (Event)  | `0x8002` | bit15=1 必须 |
| Eventgroup        | `0x0002` | |

> 注:GETTER/SETTER/NOTIFIER 之间**没有协议层关系**,它们只是同一个 Field 的三种入口。
> 在 vsomeip 里你必须自己保证它们的语义一致(SET 后要 `notify`、GET 返回最新值)。CommonAPI / capicxx 等代码生成工具会自动生成这些胶水。

## 运行

```bash
bash scripts/run.sh 03 service
bash scripts/run.sh 03 client
```

client 输出大致是:
```
[client] service UP
[client] >>> notifier 推送 brightness=50    <- initial-field, 来自订阅
[client] >>> GET
[client] response method=0x1 value=50
[client] >>> SET 50
[client] response method=0x2 value=50       <- 没变化, 不应有新 notify
[client] >>> SET 80
[client] response method=0x2 value=80
[client] >>> notifier 推送 brightness=80    <- 变化了, 收到推送
...
```

观察点:**SET 同一个值**时,客户端不会收到新的 notifier 推送(因为 service 端做了 `changed` 判断)。
这就是 vsomeip 文档里说的 "epsilon change" 概念的最朴素版本。

## 多 client 共用 Field

你可以再开一个 client(复制 client.cpp 改个 application 名)同时订阅。一个 client 的 SET 会让所有 client 都收到 Notifier —— 这正是 Field 在车载场景里的核心价值:**状态全局一致 + 变更广播**。
