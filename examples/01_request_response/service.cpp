// examples/01_request_response/service.cpp
//
// 最小 SOME/IP 服务端 —— 只做一件事:
//   收到 Method 0x0421 的 Request, 把 payload 原样回个 Response。
//
// 你需要重点看的几行代码,我都用 ★ 标了。
//
// 学习目标:
//   1) 一个 vsomeip 应用的完整生命周期: init -> register handlers -> start -> stop
//   2) offer_service 是怎么把"我有这个服务"广播出去的
//   3) on_message_t 的回调签名,怎么从 message 里拿 payload, 怎么发回响应

#include <csignal>
#include <iomanip>
#include <iostream>
#include <thread>

#include <vsomeip/vsomeip.hpp>

// ★ 全局唯一的 ID 三元组 —— 必须和 client / JSON 一致
constexpr vsomeip::service_t  SERVICE_ID  = 0x1111;
constexpr vsomeip::instance_t INSTANCE_ID = 0x0001;
constexpr vsomeip::method_t   METHOD_ECHO = 0x0421;

// vsomeip 把所有控制权放在 application 对象上。它是线程安全的,但其 start()
// 是阻塞的(内部跑一个 io_context),通常我们用单独线程启动它。
std::shared_ptr<vsomeip::application> g_app;

void on_echo_request(const std::shared_ptr<vsomeip::message> &request) {
    auto payload_in = request->get_payload();
    std::string text(reinterpret_cast<const char*>(payload_in->get_data()),
                     payload_in->get_length());
    std::cout << "[service] 收到 echo 请求, payload=\"" << text << "\""
              << " (来自 client 0x" << std::hex << request->get_client() << ")\n";

    // ★ 用 runtime::create_response 构造一条与请求"配对"的响应:
    //   它会自动复用 request 的 client/session/method 等字段,我们只需填 payload。
    auto response = vsomeip::runtime::get()->create_response(request);
    auto payload_out = vsomeip::runtime::get()->create_payload();
    std::string reply = "echo: " + text;
    payload_out->set_data(reinterpret_cast<const vsomeip::byte_t*>(reply.data()),
                          reply.size());
    response->set_payload(payload_out);

    g_app->send(response);  // ★ send() 在底层根据 client_id 路由回到对应的 socket
}

void on_state(vsomeip::state_type_e state) {
    if (state == vsomeip::state_type_e::ST_REGISTERED) {
        std::cout << "[service] 已注册到 routing-manager,offer_service\n";
        // ★ 真正"我提供这个服务"的动作。
        //   一旦 offer 出去,SD 就会开始周期性 OfferService 报文。
        g_app->offer_service(SERVICE_ID, INSTANCE_ID);
    }
}

int main() {
    g_app = vsomeip::runtime::get()->create_application();   // 名字从 VSOMEIP_APPLICATION_NAME 拿

    if (!g_app->init()) {
        std::cerr << "init 失败\n"; return 1;
    }

    // 状态回调:用来知道 routing-manager 准备好了,可以 offer 了
    g_app->register_state_handler(on_state);

    // 注册 message 回调: 当方法 0x0421 的请求到达时进入 on_echo_request
    g_app->register_message_handler(SERVICE_ID, INSTANCE_ID, METHOD_ECHO,
                                    on_echo_request);

    // 优雅退出
    std::signal(SIGINT, [](int){
        std::cout << "\n[service] 退出中...\n";
        if (g_app) {
            g_app->stop_offer_service(SERVICE_ID, INSTANCE_ID);
            g_app->stop();
        }
    });

    std::cout << "[service] start(),按 Ctrl+C 退出\n";
    g_app->start();   // 阻塞,直到 stop() 被调用
    return 0;
}
