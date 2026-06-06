// examples/01_request_response/client.cpp
//
// 最小 SOME/IP 客户端 —— 周期性调用一次 echo, 打印响应。
//
// 学习目标:
//   1) request_service / is_available 的语义
//   2) 怎么构造一个 Request 并 send()
//   3) 用 message_handler 接收 Response (异步回调,不要在回调里阻塞)

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <mutex>
#include <thread>

#include <vsomeip/vsomeip.hpp>

constexpr vsomeip::service_t  SERVICE_ID  = 0x1111;
constexpr vsomeip::instance_t INSTANCE_ID = 0x0001;
constexpr vsomeip::method_t   METHOD_ECHO = 0x0421;

std::shared_ptr<vsomeip::application> g_app;
std::atomic<bool> g_service_available{false};
std::atomic<bool> g_running{true};

// 当 SD 通知"服务可用 / 不可用"时被调
void on_availability(vsomeip::service_t s, vsomeip::instance_t i, bool available) {
    std::cout << "[client] availability: service=0x" << std::hex << s
              << " instance=0x" << i << " => "
              << (available ? "UP" : "DOWN") << std::dec << "\n";
    g_service_available = available;
}

void on_response(const std::shared_ptr<vsomeip::message> &resp) {
    auto p = resp->get_payload();
    std::string text(reinterpret_cast<const char*>(p->get_data()), p->get_length());
    std::cout << "[client] 收到响应: \"" << text << "\""
              << " (session=0x" << std::hex << resp->get_session() << std::dec << ")\n";
}

void send_one_request(int counter) {
    auto req = vsomeip::runtime::get()->create_request();
    req->set_service(SERVICE_ID);
    req->set_instance(INSTANCE_ID);
    req->set_method(METHOD_ECHO);

    auto payload = vsomeip::runtime::get()->create_payload();
    std::string body = "hello-" + std::to_string(counter);
    payload->set_data(reinterpret_cast<const vsomeip::byte_t*>(body.data()), body.size());
    req->set_payload(payload);

    g_app->send(req);   // 异步发送,响应通过 message_handler 回来
    std::cout << "[client] 发送 request #" << counter << " payload=\"" << body << "\"\n";
}

int main() {
    g_app = vsomeip::runtime::get()->create_application();
    if (!g_app->init()) { std::cerr << "init 失败\n"; return 1; }

    // ★ 告诉 routing-manager "我对 0x1111/0x0001 感兴趣"
    g_app->request_service(SERVICE_ID, INSTANCE_ID);

    // 服务可用性变化回调
    g_app->register_availability_handler(SERVICE_ID, INSTANCE_ID, on_availability);

    // 接收 echo 的 response: client 端 message_handler 用 ANY_METHOD 也行,
    // 但显式写出方法 ID 可读性更好
    g_app->register_message_handler(SERVICE_ID, INSTANCE_ID, METHOD_ECHO, on_response);

    std::signal(SIGINT, [](int){ g_running = false; if (g_app) g_app->stop(); });

    // start() 阻塞, 所以我们另开一个线程跑业务循环
    std::thread sender([&] {
        int n = 0;
        while (g_running) {
            if (g_service_available) {
                send_one_request(++n);
            } else {
                std::cout << "[client] 等待 service ...\n";
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    g_app->start();
    sender.join();
    return 0;
}
