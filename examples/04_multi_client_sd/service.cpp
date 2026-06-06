// examples/04_multi_client_sd/service.cpp
//
// 一个稍微完整的"提供者":同时暴露
//   - method 0x0421 (echo)
//   - event  0x8004 (周期性时间戳广播)
// 让多个 client 自己通过 SD 找到它。

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>

#include <vsomeip/vsomeip.hpp>

constexpr vsomeip::service_t    SERVICE_ID    = 0x4444;
constexpr vsomeip::instance_t   INSTANCE_ID   = 0x0001;
constexpr vsomeip::method_t     METHOD_ECHO   = 0x0421;
constexpr vsomeip::event_t      EVENT_TICK    = 0x8004;
constexpr vsomeip::eventgroup_t EVENTGROUP_ID = 0x0004;

std::shared_ptr<vsomeip::application> g_app;
std::atomic<bool> g_running{true};

void on_echo(const std::shared_ptr<vsomeip::message> &req) {
    auto p = req->get_payload();
    std::string text(reinterpret_cast<const char*>(p->get_data()), p->get_length());
    std::cout << "[service] echo from client 0x" << std::hex << req->get_client()
              << std::dec << " text=\"" << text << "\"\n";

    auto resp = vsomeip::runtime::get()->create_response(req);
    auto out  = vsomeip::runtime::get()->create_payload();
    std::string reply = "echo[" + std::to_string(req->get_client()) + "]: " + text;
    out->set_data(reinterpret_cast<const vsomeip::byte_t*>(reply.data()), reply.size());
    resp->set_payload(out);
    g_app->send(resp);
}

void on_state(vsomeip::state_type_e s) {
    if (s != vsomeip::state_type_e::ST_REGISTERED) return;
    g_app->offer_service(SERVICE_ID, INSTANCE_ID);
    g_app->offer_event(
        SERVICE_ID, INSTANCE_ID, EVENT_TICK, { EVENTGROUP_ID },
        vsomeip::event_type_e::ET_EVENT,
        std::chrono::milliseconds::zero(),
        false, true, nullptr,
        vsomeip::reliability_type_e::RT_UNRELIABLE
    );
    std::cout << "[service] offered service & event\n";
}

int main() {
    g_app = vsomeip::runtime::get()->create_application();
    if (!g_app->init()) return 1;

    g_app->register_state_handler(on_state);
    g_app->register_message_handler(SERVICE_ID, INSTANCE_ID, METHOD_ECHO, on_echo);

    std::signal(SIGINT, [](int){ g_running = false; if (g_app) g_app->stop(); });

    std::thread ticker([&]{
        std::this_thread::sleep_for(std::chrono::seconds(1));
        uint32_t tick = 0;
        while (g_running) {
            auto pl = vsomeip::runtime::get()->create_payload();
            pl->set_data(reinterpret_cast<const vsomeip::byte_t*>(&tick), sizeof(tick));
            g_app->notify(SERVICE_ID, INSTANCE_ID, EVENT_TICK, pl, true);
            std::cout << "[service] tick=" << tick << "\n";
            ++tick;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    std::cout << "[service] start()\n";
    g_app->start();
    ticker.join();
    return 0;
}
