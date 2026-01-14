#ifndef ZHTTP_ROUTER_H_
#define ZHTTP_ROUTER_H_

#include "http_request.h"
#include "http_response.h"
#include "middleware.h"

#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace zhttp {

/**
 * @brief 路由处理函数类型
 */
using RouteHandler =
    std::function<void(const HttpRequest::ptr &, HttpResponse &)>;

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
  std::vector<std::string> param_names;                  // 参数名称列表
  std::unordered_map<HttpMethod, RouteHandler> handlers; // 方法 -> 处理器
  std::vector<Middleware::ptr> middlewares;              // 路由级中间件
};

/**
 * @brief 路由器
 * 支持静态路由、参数路由(:param)、正则表达式路由
 */
class Router {
public:
  Router();

  /**
   * @brief 注册静态/参数路由
   * @param method HTTP 方法
   * @param path 路由路径（支持 :param 格式）
   * @param handler 处理函数
   */
  void add_route(HttpMethod method, const std::string &path,
                 RouteHandler handler);

  /**
   * @brief 注册正则表达式路由
   * @param method HTTP 方法
   * @param regex_pattern 正则表达式模式
   * @param param_names 捕获组对应的参数名称
   * @param handler 处理函数
   */
  void add_regex_route(HttpMethod method, const std::string &regex_pattern,
                       const std::vector<std::string> &param_names,
                       RouteHandler handler);

  /**
   * @brief 注册 GET 路由
   */
  void get(const std::string &path, RouteHandler handler);

  /**
   * @brief 注册 POST 路由
   */
  void post(const std::string &path, RouteHandler handler);

  /**
   * @brief 注册 PUT 路由
   */
  void put(const std::string &path, RouteHandler handler);

  /**
   * @brief 注册 DELETE 路由
   */
  void del(const std::string &path, RouteHandler handler);

  /**
   * @brief 添加全局中间件
   * @param middleware 中间件对象
   */
  void use(Middleware::ptr middleware);

  /**
   * @brief 为特定路由添加中间件
   * @param path 路由路径
   * @param middleware 中间件对象
   */
  void use(const std::string &path, Middleware::ptr middleware);

  /**
   * @brief 路由请求
   * @param request HTTP 请求
   * @param response HTTP 响应
   * @return 是否找到匹配的路由
   */
  bool route(const HttpRequest::ptr &request, HttpResponse &response);

  /**
   * @brief 设置 404 处理器
   * @param handler 处理函数
   */
  void set_not_found_handler(RouteHandler handler);

private:
  /**
   * @brief 判断路由模式是否包含参数
   */
  bool has_param(const std::string &path) const;

  /**
   * @brief 从参数路由模式构建正则表达式
   * @param path 路由路径（如 /user/:id/profile）
   * @param param_names 输出参数名称列表
   * @return 正则表达式对象
   */
  std::regex build_param_regex(const std::string &path,
                               std::vector<std::string> &param_names);

  /**
   * @brief 匹配静态路由
   */
  bool match_static_route(const std::string &path, const RouteEntry &entry);

  /**
   * @brief 匹配参数/正则路由
   */
  bool match_regex_route(const std::string &path, const RouteEntry &entry,
                         std::unordered_map<std::string, std::string> &params);

  /**
   * @brief 查找匹配的路由
   */
  RouteEntry *find_route(const std::string &path, HttpMethod method,
                         std::unordered_map<std::string, std::string> &params);

private:
  std::vector<RouteEntry> routes_;           // 路由表
  std::vector<Middleware::ptr> middlewares_; // 全局中间件
  RouteHandler not_found_handler_;           // 404 处理器
};

} // namespace zhttp

#endif // ZHTTP_ROUTER_H_
