#include "http_server.h"
#include "zhttp_logger.h"

#include "address.h"
#include "io/io_scheduler.h"

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace zhttp;

// 注意：集成测试需要网络环境，可能需要特殊配置
// 这里提供基本的服务器创建和路由测试

class HttpServerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // 创建 IO 调度器
    io_scheduler_ = std::make_shared<zcoroutine::IoScheduler>(2, "test_io");
    io_scheduler_->start();

    // 创建 HTTP 服务器
    server_ = std::make_shared<HttpServer>(io_scheduler_);
  }

  void TearDown() override {
    if (server_) {
      server_->stop();
    }
    if (io_scheduler_) {
      io_scheduler_->stop();
    }
  }

  zcoroutine::IoScheduler::ptr io_scheduler_;
  HttpServer::ptr server_;
};

TEST_F(HttpServerTest, ServerCreation) { EXPECT_NE(server_, nullptr); }

TEST_F(HttpServerTest, RouteRegistration) {
  bool handler_called = false;

  server_->router().get(
      "/test", [&handler_called](const HttpRequest::ptr &, HttpResponse &resp) {
        handler_called = true;
        resp.status(HttpStatus::OK).text("OK");
      });

  // 模拟路由匹配
  auto request = std::make_shared<HttpRequest>();
  request->set_method(HttpMethod::GET);
  request->set_path("/test");
  HttpResponse response;

  bool found = server_->router().route(request, response);

  EXPECT_TRUE(found);
  EXPECT_TRUE(handler_called);
  EXPECT_EQ(response.status_code(), HttpStatus::OK);
}

TEST_F(HttpServerTest, MiddlewareIntegration) {
  // 创建一个简单的中间件
  class TestMiddleware : public Middleware {
  public:
    TestMiddleware(bool &before_called, bool &after_called)
        : before_called_(before_called), after_called_(after_called) {}

    bool before(const HttpRequest::ptr &, HttpResponse &resp) override {
      before_called_ = true;
      resp.header("X-Test-Middleware", "before");
      return true;
    }

    void after(const HttpRequest::ptr &, HttpResponse &resp) override {
      after_called_ = true;
      resp.header("X-Test-After", "after");
    }

  private:
    bool &before_called_;
    bool &after_called_;
  };

  bool before_called = false;
  bool after_called = false;

  server_->router().use(
      std::make_shared<TestMiddleware>(before_called, after_called));
  server_->router().get("/middleware-test",
                        [](const HttpRequest::ptr &, HttpResponse &resp) {
                          resp.status(HttpStatus::OK).text("OK");
                        });

  auto request = std::make_shared<HttpRequest>();
  request->set_method(HttpMethod::GET);
  request->set_path("/middleware-test");
  HttpResponse response;

  server_->router().route(request, response);

  EXPECT_TRUE(before_called);
  EXPECT_TRUE(after_called);
  EXPECT_EQ(response.headers().at("X-Test-Middleware"), "before");
  EXPECT_EQ(response.headers().at("X-Test-After"), "after");
}

TEST_F(HttpServerTest, NotFoundRoute) {
  auto request = std::make_shared<HttpRequest>();
  request->set_method(HttpMethod::GET);
  request->set_path("/nonexistent");
  HttpResponse response;

  bool found = server_->router().route(request, response);

  EXPECT_FALSE(found);
  EXPECT_EQ(response.status_code(), HttpStatus::NOT_FOUND);
}

// 完整的网络集成测试（需要真实网络）
// 可以通过环境变量控制是否运行
TEST_F(HttpServerTest, DISABLED_FullNetworkTest) {
  // 注册路由
  server_->router().get("/", [](const HttpRequest::ptr &, HttpResponse &resp) {
    resp.status(HttpStatus::OK).json("{\"status\":\"ok\"}");
  });

  // 绑定地址
  auto addr = znet::IPAddress::create("127.0.0.1", 18080);
  ASSERT_TRUE(server_->bind(addr));

  // 启动服务器（非阻塞）
  ASSERT_TRUE(server_->start());

  // 等待服务器启动
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // 这里可以添加 curl 或其他 HTTP 客户端测试
  // 由于是集成测试，通常在 CI 环境中运行

  server_->stop();
}

int main(int argc, char **argv) {
  zhttp::init_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
