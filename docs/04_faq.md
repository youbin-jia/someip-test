# 04 — FAQ / 排错手册

按"症状 → 原因 → 修法"的格式整理。

---

## 编译期

### 症状: `fatal error: vsomeip/vsomeip.hpp: No such file or directory`
**原因**:头文件没装到 `/usr/local/include`。
**修法**:
```bash
sudo bash scripts/install_vsomeip.sh
ls /usr/local/include/vsomeip/vsomeip.hpp   # 应该存在
```

### 症状: `undefined reference to vsomeip::runtime::get()`
**原因**:链接库没拉上。
**修法**:确认 `bazel-bin/.../<binary>` 是用 `//third_party/vsomeip` 当 deps 的;`ldd` 它应该能看到 `libvsomeip3.so`。

### 症状: `bzlmod` 相关的 cmd 报错
**原因**:bazel 7 默认开了 bzlmod,但本项目走 WORKSPACE 模式。
**修法**:`.bazelrc` 里有 `common --enable_bzlmod=false`,确保你没用 `--enable_bzlmod=true` 覆盖它。

---

## 运行期 (启动)

### 症状: 进程刚启动就退出,什么都没打印
**原因**:`VSOMEIP_CONFIGURATION` 没设或路径错,vsomeip 默认 `./vsomeip.json` 找不到。
**修法**:用 `scripts/run.sh` 跑;手动跑时:
```bash
export VSOMEIP_CONFIGURATION=$PWD/config/01_request_response/service.json
export VSOMEIP_APPLICATION_NAME=service
./bazel-bin/examples/01_request_response/service
```

### 症状: 日志里 `Application(...) is initialized (... cannot read).`
**原因**:application name 在 JSON 的 `applications` 里没定义。
**修法**:确保 JSON 的 `applications[].name` 与 `VSOMEIP_APPLICATION_NAME` 完全一致(大小写敏感)。

### 症状: 日志里反复出现 `routing manager is not running, retrying...`
**原因**:routing-manager(也就是 JSON 里 `routing` 指向的那个进程)还没起。
**修法**:
1. 先启动 `routing` 字段指向的进程。
2. 或者所有进程的 `routing` 都指向同一个独立 `vsomeipd`。
3. 检查 `/tmp/vsomeip-0` 文件存不存在;不存在就是 RM 真没起来。

### 症状: 启动时 `Could not bind to ... Address already in use`
**原因**:上一次进程没干净退出,留着 listen socket;或者另一个 vsomeip 实例占着同样端口。
**修法**:
```bash
ps -ef | grep -E '(vsomeip|service|client)'  # 杀掉残余
rm -f /tmp/vsomeip-*                          # 清理 UNIX socket
ss -lntu | grep 30501                         # 检查端口
```

---

## 运行期 (通信)

### 症状: client 一直 `service ... DOWN`,从不变 UP
**原因**:99% 是 SD 多播没通。
**修法 A**(推荐): 给 lo 加多播路由
```bash
sudo ip route add 224.244.224.245/32 dev lo
sudo ip link set lo multicast on
```
**修法 B**: 把 SD 关掉,改用静态 `clients[]` 指定 `reliable_remote_port` / `unreliable_remote_port`。

### 症状: `subscribe()` 调了, 但收不到事件
排查清单:
1. service 端真的调了 `notify()` 吗?日志里有打吗?
2. service 端 `offer_event` 的 `eventgroups` 包含目标 eventgroup 吗?
3. client 端的 `request_event` 的 reliability 与 service 配置匹配吗?(`is_reliable=true` 双方必须一致)
4. 如果 event 是 UDP 的:防火墙 / 网络是否丢 UDP?用 `tcpdump -i any port <unreliable_port>` 看。

### 症状: client 调用 method, response 永远收不到
排查清单:
1. service availability UP 了吗?在 UP 之前 send() 是会丢的 (vsomeip 会 warn `application not available`).
2. service 端 `register_message_handler` 的 (service, instance, method) 三元组对吗?
3. service 端 `on_xxx` 回调里调了 `g_app->send(response)` 吗?
4. SOME/IP `Length` 字段是 `payload+8`;客户端构造 message 时 payload 没设进去(常见笔误)?

### 症状: 多 client 同时跑,其中一个收不到 event
**原因**:很多场景是因为 `offer_event` 的 `reliability` 配成了 RT_RELIABLE,但 `subscribe` 端的 `request_event` 写的是 RT_UNRELIABLE,vsomeip 不会"自动找一种对得上的"。
**修法**:把双方都对齐。

### 症状: 关掉 service 后,client 不报 DOWN
**原因**:SD 的 `cyclic_offer_delay × 大约 1.5 倍`时间内还没超时,这是预期行为。
**修法**:把 `cyclic_offer_delay` 与 `ttl` 调小(本项目默认已经是 2000ms / 3s)。

---

## 调试工具

### 1. wireshark
- 过滤器:`udp.port == 30490`(SD 报文)、`tcp.port == 30501`(业务)。
- 自动解码 SOME/IP,查看 Service-ID/Method-ID/Client-ID/Session-ID 等。

### 2. tcpdump (无 GUI)
```bash
sudo tcpdump -i any -X -nn 'port 30490 or port 30501 or port 30502'
```

### 3. vsomeip 自身的 trace
临时把 JSON 里 logging.level 改成 `debug` 或 `trace`,vsomeip 会逐条打印发出/收到的报文,带十六进制 dump,在容器里特别好用(没法装 wireshark)。

### 4. 看 UNIX socket
```bash
ls -la /tmp/vsomeip-*
ss -px src 'unix:/tmp/vsomeip*'
```
能看到哪些进程连到了 routing-manager。

---

## 性能 / 生产小贴士

- vsomeip 默认开 4 个工作线程处理 message_handler 回调,CPU 紧张时可调小;高吞吐场景调大。配置在 JSON 里:`{"applications":[{"name":"...","io_thread_count":"8"}]}`。
- 大 payload (>1400B) 走 UDP 时会自动用 SOME/IP-TP 分段,但单个 event 太大对实时性不友好,**优先拆成多个小 event**。
- 把 routing-manager 独立成 `vsomeipd` 守护进程,业务进程崩了不影响其他订阅者。这是车厂的标准做法。
- 强烈建议生产环境开 `security` 块,用 UID/GID 限制谁能 offer/request,防止误注册。

---

## 进一步阅读

- 上游文档:<https://github.com/COVESA/vsomeip/tree/master/documentation>
- 协议规范:`AUTOSAR_PRS_SOMEIPProtocol.pdf`、`AUTOSAR_PRS_SOMEIPServiceDiscoveryProtocol.pdf`(在 AUTOSAR 官网)
- 代码生成 / IDL:CommonAPI C++ + capicxx-someip-runtime,在生产里基本不会手写本项目这种"raw" vsomeip,而是从 Franca IDL 生成桩代码。
