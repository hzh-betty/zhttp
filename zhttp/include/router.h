#ifndef ZHTTP_ROUTER_H_
#define ZHTTP_ROUTER_H_

#include "http_request.h"
#include "http_response.h"
#include "middleware.h"
#include "route_handler.h"

#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace zhttp {

/**
 * @brief 路由处理器包装类
 * 统一处理 RouterCallback 和 RouteHandler::ptr
 */
class RouteHandlerWrapper {
public:
  RouteHandlerWrapper() = default;

  // 从回调函数构造
  RouteHandlerWrapper(RouterCallback callback)
      : callback_(std::move(callback)) {}

  // 从处理器对象构造
  RouteHandlerWrapper(RouteHandler::ptr handler)
      : handler_(std::move(handler)) {}

  // 执行处理
  void operator()(const HttpRequest::ptr &request,
                  HttpResponse &response) const {
    if (callback_) {
      callback_(request, response);
    } else if (handler_) {
      handler_->handle(request, response);
    }
  }

  // 检查是否有效
  explicit operator bool() const { return callback_ || handler_; }

private:
  RouterCallback callback_;
  RouteHandler::ptr handler_;
};

/**
 * @brief 路由条目
 */
struct RouteEntry {
  /**
   * @brief 路由类型
   */
  enum class Type {
    STATIC, // 静态路由 (如 /api/users)
    PARAM,  // 参数路由 (如 /api/users/:id)
    REGEX   // 正则表达式路由
  };

  Type type = Type::STATIC; // 路由类型
  std::string pattern;      // 路由模式
  std::regex regex;         // 正则表达式（当 type == REGEX）
  std::vector<std::string> param_names; // 参数名称列表
  std::unordered_map<HttpMethod, RouteHandlerWrapper>
      handlers;                             // 方法 -> 处理器
  std::vector<Middleware::ptr> middlewares; // 路由级中间件
};

/**
 * @brief 路由器
 * 支持静态路由、参数路由(:param)、正则表达式路由
 */
class Router {
public:
  Router();

  /**
   * @brief 注册静态/参数路由（回调函数方式）
   */
  void add_route(HttpMethod method, const std::string &path,
                 RouterCallback callback);

  /**
   * @brief 注册静态/参数路由（处理器对象方式）
   */
  void add_route(HttpMethod method, const std::string &path,
                 RouteHandler::ptr handler);

  /**
   * @brief 注册正则表达式路由（回调函数方式）
   */
  void add_regex_route(HttpMethod method, const std::string &regex_pattern,
                       const std::vector<std::string> &param_names,
                       RouterCallback callback);

  /**
   * @brief 注册正则表达式路由（处理器对象方式）
   */
  void add_regex_route(HttpMethod method, const std::string &regex_pattern,
                       const std::vector<std::string> &param_names,
                       RouteHandler::ptr handler);

  /**
   * @brief 注册 GET 路由（回调函数）
   */
  void get(const std::string &path, RouterCallback callback);

  /**
   * @brief 注册 GET 路由（处理器对象）
   */
  void get(const std::string &path, RouteHandler::ptr handler);

  /**
   * @brief 注册 POST 路由（回调函数）
   */
  void post(const std::string &path, RouterCallback callback);

  /**
   * @brief 注册 POST 路由（处理器对象）
   */
  void post(const std::string &path, RouteHandler::ptr handler);

  /**
   * @brief 注册 PUT 路由（回调函数）
   */
  void put(const std::string &path, RouterCallback callback);

  /**
   * @brief 注册 PUT 路由（处理器对象）
   */
  void put(const std::string &path, RouteHandler::ptr handler);

  /**
   * @brief 注册 DELETE 路由（回调函数）
   */
  void del(const std::string &path, RouterCallback callback);

  /**
   * @brief 注册 DELETE 路由（处理器对象）
   */
  void del(const std::string &path, RouteHandler::ptr handler);

  /**
   * @brief 添加全局中间件
   */
  void use(Middleware::ptr middleware);

  /**
   * @brief 为特定路由添加中间件
   */
  void use(const std::string &path, Middleware::ptr middleware);

  /**
   * @brief 路由请求
   */
  bool route(const HttpRequest::ptr &request, HttpResponse &response);

  /**
   * @brief 设置 404 处理器（回调函数）
   */
  void set_not_found_handler(RouterCallback callback);

  /**
   * @brief 设置 404 处理器（处理器对象）
   */
  void set_not_found_handler(RouteHandler::ptr handler);

private:
  bool has_param(const std::string &path) const;
  std::regex build_param_regex(const std::string &path,
                               std::vector<std::string> &param_names);
  bool match_static_route(const std::string &path, const RouteEntry &entry);
  bool match_regex_route(const std::string &path, const RouteEntry &entry,
                         std::unordered_map<std::string, std::string> &params);
  RouteEntry *find_route(const std::string &path, HttpMethod method,
                         std::unordered_map<std::string, std::string> &params);

  void add_route_internal(HttpMethod method, const std::string &path,
                          RouteHandlerWrapper wrapper);
  void add_regex_route_internal(HttpMethod method,
                                const std::string &regex_pattern,
                                const std::vector<std::string> &param_names,
                                RouteHandlerWrapper wrapper);

private:
  std::vector<RouteEntry> routes_;           // 路由表
  std::vector<Middleware::ptr> middlewares_; // 全局中间件
  RouteHandlerWrapper not_found_handler_;    // 404 处理器
};

} // namespace zhttp

#endif // ZHTTP_ROUTER_H_
