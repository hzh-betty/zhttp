#ifndef ZHTTP_HTTP_SERVER_BUILDER_H_
#define ZHTTP_HTTP_SERVER_BUILDER_H_

#include "http_server.h"
#include "middleware.h"
#include "route_handler.h"

#include <memory>
#include <string>

namespace zhttp {

/**
 * @brief HTTP服务器建造者
 * 提供链式调用API，类似Drogon的使用方式
 */
class HttpServerBuilder {
public:
  HttpServerBuilder();

  /**
   * @brief 设置监听地址
   */
  HttpServerBuilder &listen(const std::string &host, uint16_t port);

  /**
   * @brief 设置线程数
   */
  HttpServerBuilder &threads(size_t num_threads);

  /**
   * @brief 启用HTTPS
   */
  HttpServerBuilder &enable_https(const std::string &cert_file,
                                  const std::string &key_file);

  /**
   * @brief 添加全局中间件
   */
  HttpServerBuilder &use(Middleware::ptr middleware);

  /**
   * @brief 注册GET路由（回调方式）
   */
  HttpServerBuilder &get(const std::string &path, RouterCallback callback);

  /**
   * @brief 注册GET路由（处理器方式）
   */
  HttpServerBuilder &get(const std::string &path, RouteHandler::ptr handler);

  /**
   * @brief 注册POST路由（回调方式）
   */
  HttpServerBuilder &post(const std::string &path, RouterCallback callback);

  /**
   * @brief 注册POST路由（处理器方式）
   */
  HttpServerBuilder &post(const std::string &path, RouteHandler::ptr handler);

  /**
   * @brief 注册PUT路由（回调方式）
   */
  HttpServerBuilder &put(const std::string &path, RouterCallback callback);

  /**
   * @brief 注册PUT路由（处理器方式）
   */
  HttpServerBuilder &put(const std::string &path, RouteHandler::ptr handler);

  /**
   * @brief 注册DELETE路由（回调方式）
   */
  HttpServerBuilder &del(const std::string &path, RouterCallback callback);

  /**
   * @brief 注册DELETE路由（处理器方式）
   */
  HttpServerBuilder &del(const std::string &path, RouteHandler::ptr handler);

  /**
   * @brief 设置404处理器（回调方式）
   */
  HttpServerBuilder &not_found(RouterCallback callback);

  /**
   * @brief 设置404处理器（处理器方式）
   */
  HttpServerBuilder &not_found(RouteHandler::ptr handler);

  /**
   * @brief 设置日志级别
   */
  HttpServerBuilder &log_level(const std::string &level);

  /**
   * @brief 启用守护进程模式
   */
  HttpServerBuilder &daemon(bool enable = true);

  /**
   * @brief 构建并启动服务器
   * @return 服务器对象
   */
  std::shared_ptr<HttpServer> build();

  /**
   * @brief 构建并运行服务器（阻塞）
   */
  void run();

private:
  std::string host_;
  uint16_t port_;
  size_t num_threads_;
  bool use_https_;
  std::string cert_file_;
  std::string key_file_;
  bool daemon_mode_;
  std::string log_level_;

  std::vector<Middleware::ptr> middlewares_;
  std::vector<std::tuple<HttpMethod, std::string, RouteHandlerWrapper>> routes_;
  RouteHandlerWrapper not_found_handler_;

  zcoroutine::IoScheduler::ptr io_scheduler_;
};

} // namespace zhttp

#endif // ZHTTP_HTTP_SERVER_BUILDER_H_
