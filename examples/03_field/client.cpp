// examples/03_field/client.cpp
//
// 调用 GET, 订阅 Notifier, 周期性 SET, 观察 Notifier 把变化推回来。

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>

#include <vsomeip/vsomeip.hpp>

constexpr vsomeip::service_t    SERVICE_ID    = 0x3333;
constexpr vsomeip::instance_t   INSTANCE_ID   = 0x0001;
constexpr vsomeip::method_t     GETTER_ID     = 0x0001;
constexpr vsomeip::method_t     SETTER_ID     = 0x0002;
constexpr vsomeip::event_t      NOTIFIER_ID   = 0x8002;
constexpr vsomeip::eventgroup_t EVENTGROUP_ID = 0x0002;

std::shared_ptr<vsomeip::application> g_app;
std::atomic<bool> g_up{false};
std::atomic<bool> g_running{true};

static uint32_t parse_u32(const std::shared_ptr<vsomeip::message>& m) {
    auto p = m->get_payload();
    uint32_t v = 0;
    if (p->get_length() >= sizeof(v))
        std::memcpy(&v, p->get_data(), sizeof(v));
    return v;
}

void on_response(const std::shared_ptr<vsomeip::message> &m) {
    std::cout << "[client] response method=0x" << std::hex << m->get_method()
              << std::dec << " value=" << parse_u32(m) << "\n";
}

void on_notify(const std::shared_ptr<vsomeip::message> &m) {
    std::cout << "[client] >>> notifier 推送 brightness=" << parse_u32(m) << "\n";
}

void on_avail(vsomeip::service_t s, vsomeip::instance_t i, bool up) {
    std::cout << "[client] service " << (up ? "UP" : "DOWN") << "\n";
    g_up = up;
    if (up) g_app->subscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID);
}

void send_get() {
    auto r = vsomeip::runtime::get()->create_request();
    r->set_service(SERVICE_ID); r->set_instance(INSTANCE_ID); r->set_method(GETTER_ID);
    g_app->send(r);
}

void send_set(uint32_t v) {
    auto r = vsomeip::runtime::get()->create_request();
    r->set_service(SERVICE_ID); r->set_instance(INSTANCE_ID); r->set_method(SETTER_ID);
    auto pl = vsomeip::runtime::get()->create_payload();
    pl->set_data(reinterpret_cast<const vsomeip::byte_t*>(&v), sizeof(v));
    r->set_payload(pl);
    g_app->send(r);
}

int main() {
    g_app = vsomeip::runtime::get()->create_application();
    if (!g_app->init()) return 1;

    g_app->request_service(SERVICE_ID, INSTANCE_ID);
    g_app->request_event(SERVICE_ID, INSTANCE_ID, NOTIFIER_ID,
                         { EVENTGROUP_ID },
                         vsomeip::event_type_e::ET_FIELD,
                         vsomeip::reliability_type_e::RT_RELIABLE);

    g_app->register_availability_handler(SERVICE_ID, INSTANCE_ID, on_avail);
    g_app->register_message_handler(SERVICE_ID, INSTANCE_ID, GETTER_ID,   on_response);
    g_app->register_message_handler(SERVICE_ID, INSTANCE_ID, SETTER_ID,   on_response);
    g_app->register_message_handler(SERVICE_ID, INSTANCE_ID, NOTIFIER_ID, on_notify);

    std::signal(SIGINT, [](int){ g_running = false; if (g_app) g_app->stop(); });

    std::thread driver([&] {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        uint32_t levels[] = { 50, 50, 80, 100, 20 };  // 第二次 50 不会触发 notify
        size_t i = 0;
        while (g_running) {
            if (g_up) {
                if (i == 0) {                       // 启动后先做一次 GET
                    std::cout << "[client] >>> GET\n";
                    send_get();
                } else {
                    uint32_t v = levels[i % (sizeof(levels)/sizeof(levels[0]))];
                    std::cout << "[client] >>> SET " << v << "\n";
                    send_set(v);
                }
                ++i;
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    });

    g_app->start();
    driver.join();
    return 0;
}
