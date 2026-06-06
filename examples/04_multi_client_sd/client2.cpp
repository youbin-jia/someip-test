// examples/04_multi_client_sd/client2.cpp
//
// "订阅型" client: 同样通过 SD 找到 service, 但只订阅 tick 事件。
// 不调方法。和 client1 一起跑,展示同一服务的两种使用面。

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>

#include <vsomeip/vsomeip.hpp>

constexpr vsomeip::service_t    SERVICE_ID    = 0x4444;
constexpr vsomeip::instance_t   INSTANCE_ID   = 0x0001;
constexpr vsomeip::event_t      EVENT_TICK    = 0x8004;
constexpr vsomeip::eventgroup_t EVENTGROUP_ID = 0x0004;

std::shared_ptr<vsomeip::application> g_app;

int main() {
    g_app = vsomeip::runtime::get()->create_application();
    if (!g_app->init()) return 1;

    g_app->request_service(SERVICE_ID, INSTANCE_ID);
    g_app->request_event(SERVICE_ID, INSTANCE_ID, EVENT_TICK, { EVENTGROUP_ID },
                         vsomeip::event_type_e::ET_EVENT,
                         vsomeip::reliability_type_e::RT_UNRELIABLE);

    g_app->register_availability_handler(SERVICE_ID, INSTANCE_ID,
        [](auto, auto, bool up){
            std::cout << "[client2] service " << (up ? "UP" : "DOWN") << "\n";
            if (up) g_app->subscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID);
        });

    g_app->register_message_handler(SERVICE_ID, INSTANCE_ID, EVENT_TICK,
        [](auto m){
            auto p = m->get_payload();
            uint32_t v = 0;
            if (p->get_length() >= sizeof(v)) std::memcpy(&v, p->get_data(), sizeof(v));
            std::cout << "[client2] tick=" << v << "\n";
        });

    std::signal(SIGINT, [](int){ if (g_app) g_app->stop(); });
    g_app->start();
    return 0;
}
