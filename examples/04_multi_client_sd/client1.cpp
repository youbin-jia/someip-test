// examples/04_multi_client_sd/client1.cpp
//
// "调用型" client: 只通过 SD 找到 service, 周期 echo 一次。
// 不订阅事件。展示同一个 service 可以被多个 client 同时使用。

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include <vsomeip/vsomeip.hpp>

constexpr vsomeip::service_t  SERVICE_ID  = 0x4444;
constexpr vsomeip::instance_t INSTANCE_ID = 0x0001;
constexpr vsomeip::method_t   METHOD_ECHO = 0x0421;

std::shared_ptr<vsomeip::application> g_app;
std::atomic<bool> g_up{false};
std::atomic<bool> g_running{true};

int main() {
    g_app = vsomeip::runtime::get()->create_application();
    if (!g_app->init()) return 1;

    g_app->request_service(SERVICE_ID, INSTANCE_ID);
    g_app->register_availability_handler(SERVICE_ID, INSTANCE_ID,
        [](auto, auto, bool up){
            std::cout << "[client1] service " << (up ? "UP" : "DOWN") << "\n";
            g_up = up;
        });
    g_app->register_message_handler(SERVICE_ID, INSTANCE_ID, METHOD_ECHO,
        [](auto m){
            auto p = m->get_payload();
            std::string s(reinterpret_cast<const char*>(p->get_data()), p->get_length());
            std::cout << "[client1] response: " << s << "\n";
        });

    std::signal(SIGINT, [](int){ g_running = false; if (g_app) g_app->stop(); });

    std::thread driver([&]{
        int n = 0;
        while (g_running) {
            if (g_up) {
                auto r = vsomeip::runtime::get()->create_request();
                r->set_service(SERVICE_ID); r->set_instance(INSTANCE_ID); r->set_method(METHOD_ECHO);
                std::string body = "from-client1-#" + std::to_string(++n);
                auto pl = vsomeip::runtime::get()->create_payload();
                pl->set_data(reinterpret_cast<const vsomeip::byte_t*>(body.data()), body.size());
                r->set_payload(pl);
                g_app->send(r);
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    });

    g_app->start();
    driver.join();
    return 0;
}
