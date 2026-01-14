#include "zhttp.h"

using namespace zhttp;

// 自定义路由处理器示例
class HelloHandler : public RouteHandler {
public:
  void handle(const HttpRequest::ptr &req, HttpResponse &resp) override {
    resp.json("{\"message\": \"Hello from Handler class!\"}");
  }
};

class UserHandler : public RouteHandler {
public:
  void handle(const HttpRequest::ptr &req, HttpResponse &resp) override {
    std::string user_id = req->path_param("id");
    std::string json =
        "{\"id\": \"" + user_id + "\", \"name\": \"User " + user_id + "\"}";
    resp.json(json);
  }
};

// 自定义中间件示例
class LogMiddleware : public Middleware {
public:
  bool before(const HttpRequest::ptr &req, HttpResponse &) override {
    ZHTTP_LOG_INFO("Request: {} {}", method_to_string(req->method()),
                   req->path());
    return true;
  }

  void after(const HttpRequest::ptr &, HttpResponse &resp) override {
    ZHTTP_LOG_INFO("Response: {}", static_cast<int>(resp.status_code()));
  }
};

int main() {
  // 方式1: 使用建造者模式（类似Drogon）
  HttpServerBuilder()
      .listen("0.0.0.0", 8080)
      .threads(4)
      .log_level("info")
      // .daemon(true)  // 启用守护进程模式

      // 添加全局中间件
      .use(std::make_shared<LogMiddleware>())

      // 使用lambda注册路由
      .get("/",
           [](const HttpRequest::ptr &, HttpResponse &resp) {
             resp.html("<h1>Welcome to zhttp!</h1>");
           })

      .get("/api/status",
           [](const HttpRequest::ptr &, HttpResponse &resp) {
             resp.json("{\"status\": \"ok\", \"version\": \"1.0.0\"}");
           })

      // 使用处理器类注册路由
      .get("/api/hello", std::make_shared<HelloHandler>())
      .get("/api/users/:id", std::make_shared<UserHandler>())

      // POST示例
      .post("/api/data",
            [](const HttpRequest::ptr &req, HttpResponse &resp) {
              ZHTTP_LOG_INFO("Received POST data: {}", req->body());
              resp.status(HttpStatus::CREATED)
                  .json("{\"message\": \"Data created\"}");
            })

      // 自定义404
      .not_found([](const HttpRequest::ptr &, HttpResponse &resp) {
        resp.status(HttpStatus::NOT_FOUND)
            .json("{\"error\": \"Not Found\", \"code\": 404}");
      })

      .run(); // 阻塞运行

  return 0;
}
