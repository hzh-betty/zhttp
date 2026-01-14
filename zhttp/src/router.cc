#include "router.h"

#include "zhttp_logger.h"

#include <sstream>

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

bool Router::has_param(const std::string &path) const {
  return path.find(':') != std::string::npos;
}

std::regex Router::build_param_regex(const std::string &path,
                                     std::vector<std::string> &param_names) {
  param_names.clear();
  std::ostringstream regex_pattern;
  regex_pattern << "^";

  size_t pos = 0;
  while (pos < path.size()) {
    if (path[pos] == ':') {
      // 参数开始
      size_t end = pos + 1;
      while (end < path.size() && path[end] != '/') {
        ++end;
      }
      std::string param_name = path.substr(pos + 1, end - pos - 1);
      param_names.push_back(param_name);
      regex_pattern << "([^/]+)";
      pos = end;
    } else {
      // 普通字符，需要转义正则特殊字符
      char c = path[pos];
      if (c == '.' || c == '+' || c == '*' || c == '?' || c == '^' ||
          c == '$' || c == '(' || c == ')' || c == '[' || c == ']' ||
          c == '{' || c == '}' || c == '|' || c == '\\') {
        regex_pattern << '\\';
      }
      regex_pattern << c;
      ++pos;
    }
  }

  regex_pattern << "$";
  return std::regex(regex_pattern.str());
}

// 内部实现
void Router::add_route_internal(HttpMethod method, const std::string &path,
                                RouteHandlerWrapper wrapper) {
  // 查找是否已存在相同 pattern 的路由
  for (auto &entry : routes_) {
    if (entry.pattern == path) {
      entry.handlers[method] = std::move(wrapper);
      return;
    }
  }

  // 创建新的路由条目
  RouteEntry entry;
  entry.pattern = path;

  if (has_param(path)) {
    entry.type = RouteEntry::Type::PARAM;
    entry.regex = build_param_regex(path, entry.param_names);
  } else {
    entry.type = RouteEntry::Type::STATIC;
  }

  entry.handlers[method] = std::move(wrapper);
  routes_.push_back(std::move(entry));
}

// 回调函数方式
void Router::add_route(HttpMethod method, const std::string &path,
                       RouterCallback callback) {
  add_route_internal(method, path, RouteHandlerWrapper(std::move(callback)));
}

// 处理器对象方式
void Router::add_route(HttpMethod method, const std::string &path,
                       RouteHandler::ptr handler) {
  add_route_internal(method, path, RouteHandlerWrapper(std::move(handler)));
}

// 内部实现
void Router::add_regex_route_internal(
    HttpMethod method, const std::string &regex_pattern,
    const std::vector<std::string> &param_names, RouteHandlerWrapper wrapper) {
  // 查找是否已存在相同 pattern 的路由
  for (auto &entry : routes_) {
    if (entry.type == RouteEntry::Type::REGEX &&
        entry.pattern == regex_pattern) {
      entry.handlers[method] = std::move(wrapper);
      return;
    }
  }

  RouteEntry entry;
  entry.type = RouteEntry::Type::REGEX;
  entry.pattern = regex_pattern;
  entry.regex = std::regex(regex_pattern);
  entry.param_names = param_names;
  entry.handlers[method] = std::move(wrapper);
  routes_.push_back(std::move(entry));
}

// 回调函数方式
void Router::add_regex_route(HttpMethod method,
                             const std::string &regex_pattern,
                             const std::vector<std::string> &param_names,
                             RouterCallback callback) {
  add_regex_route_internal(method, regex_pattern, param_names,
                           RouteHandlerWrapper(std::move(callback)));
}

// 处理器对象方式
void Router::add_regex_route(HttpMethod method,
                             const std::string &regex_pattern,
                             const std::vector<std::string> &param_names,
                             RouteHandler::ptr handler) {
  add_regex_route_internal(method, regex_pattern, param_names,
                           RouteHandlerWrapper(std::move(handler)));
}

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

void Router::use(Middleware::ptr middleware) {
  if (middleware) {
    middlewares_.push_back(std::move(middleware));
  }
}

void Router::use(const std::string &path, Middleware::ptr middleware) {
  if (!middleware) {
    return;
  }

  // 查找已有路由并添加中间件
  for (auto &entry : routes_) {
    if (entry.pattern == path) {
      entry.middlewares.push_back(std::move(middleware));
      return;
    }
  }

  // 如果路由不存在，创建一个空路由条目仅用于中间件
  RouteEntry entry;
  entry.pattern = path;
  if (has_param(path)) {
    entry.type = RouteEntry::Type::PARAM;
    entry.regex = build_param_regex(path, entry.param_names);
  } else {
    entry.type = RouteEntry::Type::STATIC;
  }
  entry.middlewares.push_back(std::move(middleware));
  routes_.push_back(std::move(entry));
}

bool Router::match_static_route(const std::string &path,
                                const RouteEntry &entry) {
  return path == entry.pattern;
}

bool Router::match_regex_route(
    const std::string &path, const RouteEntry &entry,
    std::unordered_map<std::string, std::string> &params) {
  std::smatch match;
  if (std::regex_match(path, match, entry.regex)) {
    // 提取参数
    for (size_t i = 0; i < entry.param_names.size() && i + 1 < match.size();
         ++i) {
      params[entry.param_names[i]] = match[i + 1].str();
    }
    return true;
  }
  return false;
}

RouteEntry *
Router::find_route(const std::string &path, HttpMethod method,
                   std::unordered_map<std::string, std::string> &params) {
  // 优先匹配静态路由
  for (auto &entry : routes_) {
    if (entry.type == RouteEntry::Type::STATIC) {
      if (match_static_route(path, entry)) {
        if (entry.handlers.find(method) != entry.handlers.end()) {
          return &entry;
        }
      }
    }
  }

  // 然后匹配参数路由
  for (auto &entry : routes_) {
    if (entry.type == RouteEntry::Type::PARAM) {
      if (match_regex_route(path, entry, params)) {
        if (entry.handlers.find(method) != entry.handlers.end()) {
          return &entry;
        }
      }
    }
  }

  // 最后匹配正则路由
  for (auto &entry : routes_) {
    if (entry.type == RouteEntry::Type::REGEX) {
      if (match_regex_route(path, entry, params)) {
        if (entry.handlers.find(method) != entry.handlers.end()) {
          return &entry;
        }
      }
    }
  }

  return nullptr;
}

bool Router::route(const HttpRequest::ptr &request, HttpResponse &response) {
  std::unordered_map<std::string, std::string> params;
  RouteEntry *entry = find_route(request->path(), request->method(), params);

  // 设置路径参数
  for (const auto &pair : params) {
    // const_cast 用于设置参数，因为 request 是共享指针
    const_cast<HttpRequest *>(request.get())
        ->set_path_param(pair.first, pair.second);
  }

  // 构建中间件链
  MiddlewareChain chain;

  // 添加全局中间件
  for (const auto &mw : middlewares_) {
    chain.add(mw);
  }

  // 添加路由级中间件
  if (entry) {
    for (const auto &mw : entry->middlewares) {
      chain.add(mw);
    }
  }

  // 执行 before 中间件
  bool should_continue = chain.execute_before(request, response);

  if (should_continue) {
    if (entry) {
      // 执行处理器
      auto handler_it = entry->handlers.find(request->method());
      if (handler_it != entry->handlers.end()) {
        handler_it->second(request, response);
      }
    } else {
      // 404 处理
      not_found_handler_(request, response);
    }
  }

  // 执行 after 中间件（逆序）
  chain.execute_after(request, response);

  return entry != nullptr;
}

void Router::set_not_found_handler(RouterCallback callback) {
  not_found_handler_ = RouteHandlerWrapper(std::move(callback));
}

void Router::set_not_found_handler(RouteHandler::ptr handler) {
  not_found_handler_ = RouteHandlerWrapper(std::move(handler));
}

} // namespace zhttp
