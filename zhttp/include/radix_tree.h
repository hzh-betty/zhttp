#ifndef ZHTTP_RADIX_TREE_H_
#define ZHTTP_RADIX_TREE_H_

#include "http_common.h"
#include "route_handler.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace zhttp {

/**
 * @brief 基数树节点类型
 * 优先级: STATIC > PARAM > CATCH_ALL
 */
enum class NodeType : uint8_t {
  STATIC = 0,   // 静态文本节点
  PARAM = 1,    // 参数节点 (:id)
  CATCH_ALL = 2 // 通配符节点 (*)
};

/**
 * @brief 路由处理器包装类
 */
class RouteHandlerWrapper {
public:
  RouteHandlerWrapper() = default;

  RouteHandlerWrapper(RouterCallback callback)
      : callback_(std::move(callback)) {}

  RouteHandlerWrapper(RouteHandler::ptr handler)
      : handler_(std::move(handler)) {}

  void operator()(const HttpRequest::ptr &request,
                  HttpResponse &response) const {
    if (callback_) {
      callback_(request, response);
    } else if (handler_) {
      handler_->handle(request, response);
    }
  }

  explicit operator bool() const { return callback_ || handler_; }

private:
  RouterCallback callback_;
  RouteHandler::ptr handler_;
};

// 前向声明
class RadixNode;
using RadixNodePtr = std::shared_ptr<RadixNode>;

/**
 * @brief 基数树节点
 */
class RadixNode {
public:
  using MethodHandlers = std::unordered_map<HttpMethod, RouteHandlerWrapper>;

  RadixNode() = default;
  explicit RadixNode(const std::string &path, NodeType type = NodeType::STATIC)
      : path_(path), type_(type) {}

  // 节点路径片段
  std::string path_;

  // 节点类型
  NodeType type_ = NodeType::STATIC;

  // 参数名（仅当 type_ == PARAM 时有效）
  std::string param_name_;

  // 子节点列表（按优先级排序：STATIC > PARAM > CATCH_ALL）
  std::vector<RadixNodePtr> children_;

  // 该节点的处理器（按HTTP方法）
  MethodHandlers handlers_;

  // 是否为终端节点（有处理器）
  bool is_leaf() const { return !handlers_.empty(); }

  /**
   * @brief 添加子节点（保持优先级排序）
   */
  void add_child(RadixNodePtr child) {
    // 按 NodeType 排序插入
    auto it =
        std::lower_bound(children_.begin(), children_.end(), child,
                         [](const RadixNodePtr &a, const RadixNodePtr &b) {
                           return static_cast<uint8_t>(a->type_) <
                                  static_cast<uint8_t>(b->type_);
                         });
    children_.insert(it, child);
  }

  /**
   * @brief 查找匹配的子节点
   */
  RadixNodePtr find_child(const std::string &segment) const {
    for (const auto &child : children_) {
      if (child->type_ == NodeType::STATIC && child->path_ == segment) {
        return child;
      }
    }
    return nullptr;
  }

  /**
   * @brief 查找静态子节点（精确匹配）
   */
  RadixNodePtr find_static_child(const std::string &segment) const {
    for (const auto &child : children_) {
      if (child->type_ == NodeType::STATIC && child->path_ == segment) {
        return child;
      }
    }
    return nullptr;
  }

  /**
   * @brief 查找参数子节点
   */
  RadixNodePtr find_param_child() const {
    for (const auto &child : children_) {
      if (child->type_ == NodeType::PARAM) {
        return child;
      }
    }
    return nullptr;
  }

  /**
   * @brief 查找通配符子节点
   */
  RadixNodePtr find_catch_all_child() const {
    for (const auto &child : children_) {
      if (child->type_ == NodeType::CATCH_ALL) {
        return child;
      }
    }
    return nullptr;
  }
};

/**
 * @brief 路由匹配结果
 */
struct RouteMatch {
  bool found = false;
  RadixNodePtr node;
  std::unordered_map<std::string, std::string> params;
};

/**
 * @brief 基数树路由器
 * 高效的路由匹配实现
 */
class RadixTree {
public:
  RadixTree() : root_(std::make_shared<RadixNode>()) {}

  /**
   * @brief 插入路由
   * @param method HTTP方法
   * @param path 路由路径
   * @param handler 处理器
   */
  void insert(HttpMethod method, const std::string &path,
              RouteHandlerWrapper handler);

  /**
   * @brief 查找路由
   * @param path 请求路径
   * @return 匹配结果
   */
  RouteMatch find(const std::string &path) const;

  /**
   * @brief 获取根节点
   */
  RadixNodePtr root() const { return root_; }

private:
  /**
   * @brief 分割路径为片段
   */
  std::vector<std::string> split_path(const std::string &path) const;

  /**
   * @brief 解析路径片段类型
   */
  std::pair<NodeType, std::string> parse_segment(const std::string &seg) const;

  /**
   * @brief 递归匹配
   */
  bool match_recursive(const RadixNodePtr &node,
                       const std::vector<std::string> &segments, size_t index,
                       RouteMatch &result) const;

  RadixNodePtr root_;
};

} // namespace zhttp

#endif // ZHTTP_RADIX_TREE_H_
