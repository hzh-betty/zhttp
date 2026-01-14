#include "http_server_builder.h"
#include "daemon.h"
#include "zhttp_logger.h"

#include "address.h"
#include "https_server.h"
#include "io/io_scheduler.h"

#include <csignal>

namespace zhttp {

HttpServerBuilder::HttpServerBuilder()
    : host_("0.0.0.0"), port_(8080), num_threads_(4), use_https_(false),
      daemon_mode_(false), log_level_("info") {}

HttpServerBuilder &HttpServerBuilder::listen(const std::string &host,
                                             uint16_t port) {
  host_ = host;
  port_ = port;
  return *this;
}

HttpServerBuilder &HttpServerBuilder::threads(size_t num_threads) {
  num_threads_ = num_threads;
  return *this;
}

HttpServerBuilder &
HttpServerBuilder::enable_https(const std::string &cert_file,
                                const std::string &key_file) {
  use_https_ = true;
  cert_file_ = cert_file;
  key_file_ = key_file;
  return *this;
}

HttpServerBuilder &HttpServerBuilder::use(Middleware::ptr middleware) {
  middlewares_.push_back(std::move(middleware));
  return *this;
}

HttpServerBuilder &HttpServerBuilder::get(const std::string &path,
                                          RouterCallback callback) {
  routes_.emplace_back(HttpMethod::GET, path,
                       RouteHandlerWrapper(std::move(callback)));
  return *this;
}

HttpServerBuilder &HttpServerBuilder::get(const std::string &path,
                                          RouteHandler::ptr handler) {
  routes_.emplace_back(HttpMethod::GET, path,
                       RouteHandlerWrapper(std::move(handler)));
  return *this;
}

HttpServerBuilder &HttpServerBuilder::post(const std::string &path,
                                           RouterCallback callback) {
  routes_.emplace_back(HttpMethod::POST, path,
                       RouteHandlerWrapper(std::move(callback)));
  return *this;
}

HttpServerBuilder &HttpServerBuilder::post(const std::string &path,
                                           RouteHandler::ptr handler) {
  routes_.emplace_back(HttpMethod::POST, path,
                       RouteHandlerWrapper(std::move(handler)));
  return *this;
}

HttpServerBuilder &HttpServerBuilder::put(const std::string &path,
                                          RouterCallback callback) {
  routes_.emplace_back(HttpMethod::PUT, path,
                       RouteHandlerWrapper(std::move(callback)));
  return *this;
}

HttpServerBuilder &HttpServerBuilder::put(const std::string &path,
                                          RouteHandler::ptr handler) {
  routes_.emplace_back(HttpMethod::PUT, path,
                       RouteHandlerWrapper(std::move(handler)));
  return *this;
}

HttpServerBuilder &HttpServerBuilder::del(const std::string &path,
                                          RouterCallback callback) {
  routes_.emplace_back(HttpMethod::DELETE, path,
                       RouteHandlerWrapper(std::move(callback)));
  return *this;
}

HttpServerBuilder &HttpServerBuilder::del(const std::string &path,
                                          RouteHandler::ptr handler) {
  routes_.emplace_back(HttpMethod::DELETE, path,
                       RouteHandlerWrapper(std::move(handler)));
  return *this;
}

HttpServerBuilder &HttpServerBuilder::not_found(RouterCallback callback) {
  not_found_handler_ = RouteHandlerWrapper(std::move(callback));
  return *this;
}

HttpServerBuilder &HttpServerBuilder::not_found(RouteHandler::ptr handler) {
  not_found_handler_ = RouteHandlerWrapper(std::move(handler));
  return *this;
}

HttpServerBuilder &HttpServerBuilder::log_level(const std::string &level) {
  log_level_ = level;
  return *this;
}

HttpServerBuilder &HttpServerBuilder::daemon(bool enable) {
  daemon_mode_ = enable;
  return *this;
}

std::shared_ptr<HttpServer> HttpServerBuilder::build() {
  // 初始化日志
  init_logger();

  // 守护进程化
  if (daemon_mode_) {
    ZHTTP_LOG_INFO("Starting daemon mode...");
    if (Daemon::daemonize() < 0) {
      ZHTTP_LOG_ERROR("Failed to daemonize");
      return nullptr;
    }
  }

  // 创建IO调度器
  io_scheduler_ =
      std::make_shared<zcoroutine::IoScheduler>(num_threads_, "zhttp_io");
  io_scheduler_->start();

  // 创建服务器
  std::shared_ptr<HttpServer> server;
  if (use_https_) {
    auto https_server = std::make_shared<HttpsServer>(io_scheduler_);
    https_server->set_ssl_certificate(cert_file_, key_file_);
    server = https_server;
    ZHTTP_LOG_INFO("HTTPS server created");
  } else {
    server = std::make_shared<HttpServer>(io_scheduler_);
    ZHTTP_LOG_INFO("HTTP server created");
  }

  // 注册中间件
  for (auto &mw : middlewares_) {
    server->router().use(mw);
  }

  // 注册路由
  for (auto &route : routes_) {
    HttpMethod method = std::get<0>(route);
    const std::string &path = std::get<1>(route);
    RouteHandlerWrapper &wrapper = std::get<2>(route);

    // 使用内部方法注册
    if (method == HttpMethod::GET) {
      server->router().add_route(
          method, path,
          [wrapper](const HttpRequest::ptr &req, HttpResponse &resp) {
            wrapper(req, resp);
          });
    } else if (method == HttpMethod::POST) {
      server->router().add_route(
          method, path,
          [wrapper](const HttpRequest::ptr &req, HttpResponse &resp) {
            wrapper(req, resp);
          });
    } else if (method == HttpMethod::PUT) {
      server->router().add_route(
          method, path,
          [wrapper](const HttpRequest::ptr &req, HttpResponse &resp) {
            wrapper(req, resp);
          });
    } else if (method == HttpMethod::DELETE) {
      server->router().add_route(
          method, path,
          [wrapper](const HttpRequest::ptr &req, HttpResponse &resp) {
            wrapper(req, resp);
          });
    }
  }

  // 设置404处理器
  if (not_found_handler_) {
    server->router().set_not_found_handler(
        [this](const HttpRequest::ptr &req, HttpResponse &resp) {
          not_found_handler_(req, resp);
        });
  }

  // 绑定地址
  auto addr = std::make_shared<znet::IPv4Address>(host_, port_);
  if (!server->bind(addr)) {
    ZHTTP_LOG_ERROR("Failed to bind to {}:{}", host_, port_);
    return nullptr;
  }

  ZHTTP_LOG_INFO("Server bound to {}:{}", host_, port_);
  return server;
}

void HttpServerBuilder::run() {
  auto server = build();
  if (!server) {
    ZHTTP_LOG_ERROR("Failed to build server");
    return;
  }

  // 启动服务器
  if (!server->start()) {
    ZHTTP_LOG_ERROR("Failed to start server");
    return;
  }

  ZHTTP_LOG_INFO("Server started successfully");

  // 信号处理
  signal(SIGINT, [](int) {
    ZHTTP_LOG_INFO("Received SIGINT, shutting down...");
    exit(0);
  });

  signal(SIGTERM, [](int) {
    ZHTTP_LOG_INFO("Received SIGTERM, shutting down...");
    exit(0);
  });

  // 阻塞等待
  while (true) {
    sleep(1);
  }
}

} // namespace zhttp
