// examples/02_event/service.cpp
//
// 周期性向所有订阅者推送一个 "温度" 事件 (event 0x8001)。
//
// 学习目标:
//   1) offer_event() 怎么把事件 + eventgroup 关系告诉 vsomeip
//   2) notify() 是发布动作,内部会自动发给所有已订阅的 client
//   3) Event 通常走 UDP, 大于 1400 字节的事件 vsomeip 会自动分段

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include <vsomeip/vsomeip.hpp>

constexpr vsomeip::service_t    SERVICE_ID    = 0x2222;
constexpr vsomeip::instance_t   INSTANCE_ID   = 0x0001;
constexpr vsomeip::event_t      EVENT_ID      = 0x8001;   // bit15=1 => Event
constexpr vsomeip::eventgroup_t EVENTGROUP_ID = 0x0001;

std::shared_ptr<vsomeip::application> g_app;
std::atomic<bool> g_running{true};

void on_state(vsomeip::state_type_e state) {
    if (state != vsomeip::state_type_e::ST_REGISTERED) return;
    std::cout << "[service] registered, offer service & event\n";

    g_app->offer_service(SERVICE_ID, INSTANCE_ID);

    // ★ 显式声明: 这个事件属于哪个 eventgroup
    //   client 是按 eventgroup 订阅的,不是按单个 event。
    std::set<vsomeip::eventgroup_t> groups{ EVENTGROUP_ID };
    g_app->offer_event(
        SERVICE_ID, INSTANCE_ID, EVENT_ID, groups,
        vsomeip::event_type_e::ET_EVENT,           // 普通事件 (非 field)
        std::chrono::milliseconds::zero(),         // 不在内部做节流
        false,                                     // update_on_change_only=false
        true,                                      // notification on subscription
        nullptr,                                   // epsilon_change_func
        vsomeip::reliability_type_e::RT_UNRELIABLE // UDP 推送
    );
}

int main() {
    g_app = vsomeip::runtime::get()->create_application();
    if (!g_app->init()) return 1;
    g_app->register_state_handler(on_state);

    std::signal(SIGINT, [](int){ g_running = false; if (g_app) g_app->stop(); });

    std::thread publisher([&] {
        // 等 routing 准备好,简单 sleep 即可,生产代码可用 condition_variable
        std::this_thread::sleep_for(std::chrono::seconds(1));

        int tick = 0;
        while (g_running) {
            // 模拟温度: 20.0°C 起步, 缓慢漂移
            float temperature = 20.0f + (tick++ % 10) * 0.3f;

            auto payload = vsomeip::runtime::get()->create_payload();
            // 4 字节 little-endian float
            const auto *bytes = reinterpret_cast<const vsomeip::byte_t*>(&temperature);
            payload->set_data(bytes, sizeof(temperature));

            // ★ notify(): 把数据缓存进 event,然后推给所有订阅者
            //   force=true 表示即便值没变也推 (默认 false 时,update_on_change_only 会过滤)
            g_app->notify(SERVICE_ID, INSTANCE_ID, EVENT_ID, payload, true);
            std::cout << "[service] notify temperature=" << temperature << "°C\n";

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    std::cout << "[service] start()\n";
    g_app->start();
    publisher.join();
    return 0;
}
