// examples/03_field/service.cpp
//
// 演示 SOME/IP 中的 "Field" —— 是一个 GET / SET / Notifier 的语法糖组合,
// 表示一个"远程属性 / 状态"。本例的 field 是: brightness (亮度, uint32)。
//
// 内部其实由三件套实现:
//   - Method 0x0001 = Getter   (Request -> Response 当前值)
//   - Method 0x0002 = Setter   (Request 携带新值 -> Response 含确认)
//   - Event  0x8002 = Notifier (值变化时自动推送)
//
// 这里我们手动注册三个 handler, 让你看清 vsomeip 提供的"自动 field"背后的运作。

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>

#include <vsomeip/vsomeip.hpp>

constexpr vsomeip::service_t    SERVICE_ID    = 0x3333;
constexpr vsomeip::instance_t   INSTANCE_ID   = 0x0001;
constexpr vsomeip::method_t     GETTER_ID     = 0x0001;
constexpr vsomeip::method_t     SETTER_ID     = 0x0002;
constexpr vsomeip::event_t      NOTIFIER_ID   = 0x8002;
constexpr vsomeip::eventgroup_t EVENTGROUP_ID = 0x0002;

std::shared_ptr<vsomeip::application> g_app;

// 受保护的 field 状态
std::mutex g_mu;
uint32_t   g_brightness = 50;     // 0..100

static std::shared_ptr<vsomeip::payload> make_payload_u32(uint32_t v) {
    auto pl = vsomeip::runtime::get()->create_payload();
    pl->set_data(reinterpret_cast<const vsomeip::byte_t*>(&v), sizeof(v));
    return pl;
}

void notify_brightness(uint32_t v) {
    g_app->notify(SERVICE_ID, INSTANCE_ID, NOTIFIER_ID, make_payload_u32(v), true);
}

void on_get(const std::shared_ptr<vsomeip::message> &req) {
    uint32_t v;
    { std::lock_guard<std::mutex> lk(g_mu); v = g_brightness; }
    auto resp = vsomeip::runtime::get()->create_response(req);
    resp->set_payload(make_payload_u32(v));
    g_app->send(resp);
    std::cout << "[service] GET brightness -> " << v << "\n";
}

void on_set(const std::shared_ptr<vsomeip::message> &req) {
    auto p = req->get_payload();
    if (p->get_length() < sizeof(uint32_t)) return;
    uint32_t new_v = 0;
    std::memcpy(&new_v, p->get_data(), sizeof(new_v));

    bool changed = false;
    { std::lock_guard<std::mutex> lk(g_mu);
      changed = (new_v != g_brightness);
      g_brightness = new_v; }

    // SET 的响应通常带回新值, 这是 SOME/IP 约定
    auto resp = vsomeip::runtime::get()->create_response(req);
    resp->set_payload(make_payload_u32(new_v));
    g_app->send(resp);
    std::cout << "[service] SET brightness=" << new_v
              << (changed ? " (changed -> notify)" : " (unchanged)") << "\n";

    if (changed) notify_brightness(new_v);
}

void on_state(vsomeip::state_type_e s) {
    if (s != vsomeip::state_type_e::ST_REGISTERED) return;
    g_app->offer_service(SERVICE_ID, INSTANCE_ID);

    // 把 NOTIFIER_ID 注册成 ET_FIELD —— 这样订阅时会自动发一次"当前值"
    g_app->offer_event(
        SERVICE_ID, INSTANCE_ID, NOTIFIER_ID, { EVENTGROUP_ID },
        vsomeip::event_type_e::ET_FIELD,
        std::chrono::milliseconds::zero(),
        false, true, nullptr,
        vsomeip::reliability_type_e::RT_RELIABLE
    );

    // 初始化 field 缓存值, 否则首个订阅者拿到的"initial event"是空 payload
    notify_brightness(g_brightness);
    std::cout << "[service] field offered, initial=" << g_brightness << "\n";
}

int main() {
    g_app = vsomeip::runtime::get()->create_application();
    if (!g_app->init()) return 1;

    g_app->register_state_handler(on_state);
    g_app->register_message_handler(SERVICE_ID, INSTANCE_ID, GETTER_ID, on_get);
    g_app->register_message_handler(SERVICE_ID, INSTANCE_ID, SETTER_ID, on_set);

    std::signal(SIGINT, [](int){ if (g_app) g_app->stop(); });
    std::cout << "[service] start()\n";
    g_app->start();
    return 0;
}
