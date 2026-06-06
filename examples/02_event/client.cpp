// examples/02_event/client.cpp
//
// 订阅温度事件,打印每一次 notification。
//
// 学习目标:
//   1) request_event + subscribe 是两步,不能合并
//   2) 收事件复用同一个 message_handler 接口,通过 method_t 区分
//   3) 退订: unsubscribe / stop_request_service

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>

#include <vsomeip/vsomeip.hpp>

constexpr vsomeip::service_t    SERVICE_ID    = 0x2222;
constexpr vsomeip::instance_t   INSTANCE_ID   = 0x0001;
constexpr vsomeip::event_t      EVENT_ID      = 0x8001;
constexpr vsomeip::eventgroup_t EVENTGROUP_ID = 0x0001;

std::shared_ptr<vsomeip::application> g_app;

void on_event(const std::shared_ptr<vsomeip::message> &msg) {
    auto p = msg->get_payload();
    if (p->get_length() < sizeof(float)) {
        std::cout << "[client] event size too small\n";
        return;
    }
    float temperature = 0.0f;
    std::memcpy(&temperature, p->get_data(), sizeof(temperature));
    std::cout << "[client] event 0x" << std::hex << msg->get_method() << std::dec
              << " temperature=" << temperature << "°C\n";
}

void on_availability(vsomeip::service_t s, vsomeip::instance_t i, bool available) {
    std::cout << "[client] service 0x" << std::hex << s << "/0x" << i << std::dec
              << " => " << (available ? "UP" : "DOWN") << "\n";
    if (available) {
        // ★ 服务可用之后立即 subscribe (subscribe 在 service 不可用时也不会失败,
        //   但为了清晰,这里在 UP 时调用)
        g_app->subscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID);
        std::cout << "[client] subscribed eventgroup 0x"
                  << std::hex << EVENTGROUP_ID << std::dec << "\n";
    }
}

int main() {
    g_app = vsomeip::runtime::get()->create_application();
    if (!g_app->init()) return 1;

    // ★ 步骤 1: 声明感兴趣的 event。要在 subscribe 之前。
    std::set<vsomeip::eventgroup_t> groups{ EVENTGROUP_ID };
    g_app->request_event(
        SERVICE_ID, INSTANCE_ID, EVENT_ID, groups,
        vsomeip::event_type_e::ET_EVENT,
        vsomeip::reliability_type_e::RT_UNRELIABLE
    );

    // 步骤 2: 让 routing 帮我们查找 service
    g_app->request_service(SERVICE_ID, INSTANCE_ID);

    g_app->register_availability_handler(SERVICE_ID, INSTANCE_ID, on_availability);
    g_app->register_message_handler(SERVICE_ID, INSTANCE_ID, EVENT_ID, on_event);

    std::signal(SIGINT, [](int){
        if (g_app) {
            g_app->unsubscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID);
            g_app->stop();
        }
    });

    std::cout << "[client] start()\n";
    g_app->start();
    return 0;
}
