#include "router.h"
#include "zhttp_logger.h"

namespace zhttp {

Router::Router() {
  // 默认 404 处理器
  RouterCallback default_404 = [](const HttpRequest::ptr & /*request*/,
                                  HttpResponse &response) {
    response.status(HttpStatus::NOT_FOUND)
        .content_type("text/html; charset=utf-8")
        .body("<html><body><h1>404 Not Found</h1></body></html>");
  };
  not_found_handler_ = RouteHandlerWrapper(std::move(default_404));
}

bool Router::is_dynamic_path(const std::string &path) const {
  return path.find(':') != std::string::npos ||
         path.find('*') != std::string::npos;
}

// ========== 路由注册 ==========

void Router::add_route_internal(HttpMethod method, const std::string &path,
                                RouteHandlerWrapper wrapper) {
  ZHTTP_LOG_DEBUG("Router::add_route {} {}", method_to_string(method), path);

  if (is_dynamic_path(path)) {
    // 动态路由 -> 基数树
    radix_tree_.insert(method, path, std::move(wrapper));
    ZHTTP_LOG_DEBUG("Added to radix tree: {}", path);
  } else {
    // 静态路由 -> 哈希表
    static_routes_[path].handlers[method] = std::move(wrapper);
    ZHTTP_LOG_DEBUG("Added to hash map: {}", path);
  }
}

void Router::add_route(HttpMethod method, const std::string &path,
                       RouterCallback callback) {
  add_route_internal(method, path, RouteHandlerWrapper(std::move(callback)));
}

void Router::add_route(HttpMethod method, const std::string &path,
                       RouteHandler::ptr handler) {
  add_route_internal(method, path, RouteHandlerWrapper(std::move(handler)));
}

void Router::add_regex_route_internal(
    HttpMethod method, const std::string &regex_pattern,
    const std::vector<std::string> &param_names, RouteHandlerWrapper wrapper) {
  ZHTTP_LOG_DEBUG("Router::add_regex_route {} {}", method_to_string(method),
                  regex_pattern);

  // 查找是否已存在相同 pattern 的路由
  for (auto &entry : regex_routes_) {
    if (entry.pattern == regex_pattern) {
      entry.handlers[method] = std::move(wrapper);
      return;
    }
  }

  // 创建新的正则路由条目
  RegexRouteEntry entry;
  entry.pattern = regex_pattern;
  entry.regex = std::regex(regex_pattern);
  entry.param_names = param_names;
  entry.handlers[method] = std::move(wrapper);
  regex_routes_.push_back(std::move(entry));
}

void Router::add_regex_route(HttpMethod method,
                             const std::string &regex_pattern,
                             const std::vector<std::string> &param_names,
                             RouterCallback callback) {
  add_regex_route_internal(method, regex_pattern, param_names,
                           RouteHandlerWrapper(std::move(callback)));
}

void Router::add_regex_route(HttpMethod method,
                             const std::string &regex_pattern,
                             const std::vector<std::string> &param_names,
                             RouteHandler::ptr handler) {
  add_regex_route_internal(method, regex_pattern, param_names,
                           RouteHandlerWrapper(std::move(handler)));
}

// ========== 便捷方法 ==========

void Router::get(const std::string &path, RouterCallback callback) {
  add_route(HttpMethod::GET, path, std::move(callback));
}

void Router::get(const std::string &path, RouteHandler::ptr handler) {
  add_route(HttpMethod::GET, path, std::move(handler));
}

void Router::post(const std::string &path, RouterCallback callback) {
  add_route(HttpMethod::POST, path, std::move(callback));
}

void Router::post(const std::string &path, RouteHandler::ptr handler) {
  add_route(HttpMethod::POST, path, std::move(handler));
}

void Router::put(const std::string &path, RouterCallback callback) {
  add_route(HttpMethod::PUT, path, std::move(callback));
}

void Router::put(const std::string &path, RouteHandler::ptr handler) {
  add_route(HttpMethod::PUT, path, std::move(handler));
}

void Router::del(const std::string &path, RouterCallback callback) {
  add_route(HttpMethod::DELETE, path, std::move(callback));
}

void Router::del(const std::string &path, RouteHandler::ptr handler) {
  add_route(HttpMethod::DELETE, path, std::move(handler));
}

// ========== 中间件 ==========

void Router::use(Middleware::ptr middleware) {
  if (middleware) {
    global_middlewares_.push_back(std::move(middleware));
  }
}

void Router::use(const std::string &path, Middleware::ptr middleware) {
  if (middleware) {
    route_middlewares_[path].push_back(std::move(middleware));
  }
}

// ========== 路由匹配 ==========

RouteContext Router::find_route(const std::string &path, HttpMethod method) {
  RouteContext ctx;

  ZHTTP_LOG_DEBUG("Router::find_route {} {}", method_to_string(method), path);

  // 第一层: 静态路由哈希表查找 O(1)
  auto static_it = static_routes_.find(path);
  if (static_it != static_routes_.end()) {
    auto handler_it = static_it->second.handlers.find(method);
    if (handler_it != static_it->second.handlers.end()) {
      ctx.found = true;
      ctx.handler = handler_it->second;
      ctx.middlewares = static_it->second.middlewares;
      ZHTTP_LOG_DEBUG("Found in static routes (hash map): {}", path);
      return ctx;
    }
  }

  // 第二层: 基数树查找（动态路由，按优先级）
  RouteMatch match = radix_tree_.find(path);
  if (match.found && match.node) {
    auto handler_it = match.node->handlers_.find(method);
    if (handler_it != match.node->handlers_.end()) {
      ctx.found = true;
      ctx.handler = handler_it->second;
      ctx.params = std::move(match.params);
      ZHTTP_LOG_DEBUG("Found in radix tree: {}, params count: {}", path,
                      ctx.params.size());
      return ctx;
    }
  }

  // 第三层: 正则路由匹配（回退）
  for (const auto &entry : regex_routes_) {
    std::smatch match_result;
    if (std::regex_match(path, match_result, entry.regex)) {
      auto handler_it = entry.handlers.find(method);
      if (handler_it != entry.handlers.end()) {
        ctx.found = true;
        ctx.handler = handler_it->second;
        ctx.middlewares = entry.middlewares;

        // 提取参数
        for (size_t i = 0;
             i < entry.param_names.size() && i + 1 < match_result.size(); ++i) {
          ctx.params[entry.param_names[i]] = match_result[i + 1].str();
        }
        ZHTTP_LOG_DEBUG("Found in regex routes: {}", path);
        return ctx;
      }
    }
  }

  ZHTTP_LOG_DEBUG("Route not found: {}", path);
  return ctx;
}

bool Router::route(const HttpRequest::ptr &request, HttpResponse &response) {
  // 查找路由
  RouteContext ctx = find_route(request->path(), request->method());

  // 设置路径参数
  for (const auto &pair : ctx.params) {
    const_cast<HttpRequest *>(request.get())
        ->set_path_param(pair.first, pair.second);
  }

  // 构建中间件链
  MiddlewareChain chain;

  // 添加全局中间件
  for (const auto &mw : global_middlewares_) {
    chain.add(mw);
  }

  // 添加路由级中间件
  auto mw_it = route_middlewares_.find(request->path());
  if (mw_it != route_middlewares_.end()) {
    for (const auto &mw : mw_it->second) {
      chain.add(mw);
    }
  }

  // 添加匹配到的路由中间件
  for (const auto &mw : ctx.middlewares) {
    chain.add(mw);
  }

  // 执行 before 中间件
  bool should_continue = chain.execute_before(request, response);

  if (should_continue) {
    if (ctx.found) {
      // 执行处理器
      ctx.handler(request, response);
    } else {
      // 404 处理
      not_found_handler_(request, response);
    }
  }

  // 执行 after 中间件（逆序）
  chain.execute_after(request, response);

  return ctx.found;
}

void Router::set_not_found_handler(RouterCallback callback) {
  not_found_handler_ = RouteHandlerWrapper(std::move(callback));
}

void Router::set_not_found_handler(RouteHandler::ptr handler) {
  not_found_handler_ = RouteHandlerWrapper(std::move(handler));
}

} // namespace zhttp
