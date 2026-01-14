#include "radix_tree.h"
#include "zhttp_logger.h"

#include <sstream>

namespace zhttp {

std::vector<std::string> RadixTree::split_path(const std::string &path) const {
  std::vector<std::string> segments;
  std::istringstream iss(path);
  std::string segment;

  while (std::getline(iss, segment, '/')) {
    if (!segment.empty()) {
      segments.push_back(segment);
    }
  }

  return segments;
}

std::pair<NodeType, std::string>
RadixTree::parse_segment(const std::string &seg) const {
  if (seg.empty()) {
    return {NodeType::STATIC, seg};
  }

  if (seg[0] == ':') {
    // 参数节点 :id -> param_name = "id"
    return {NodeType::PARAM, seg.substr(1)};
  }

  if (seg[0] == '*') {
    // 通配符节点 *path -> param_name = "path" (或为空)
    std::string name = seg.length() > 1 ? seg.substr(1) : "";
    return {NodeType::CATCH_ALL, name};
  }

  return {NodeType::STATIC, seg};
}

void RadixTree::insert(HttpMethod method, const std::string &path,
                       RouteHandlerWrapper handler) {
  ZHTTP_LOG_DEBUG("RadixTree::insert {} {}", method_to_string(method), path);

  std::vector<std::string> segments = split_path(path);
  RadixNodePtr current = root_;

  for (const auto &seg : segments) {
    std::pair<NodeType, std::string> parsed = parse_segment(seg);
    NodeType type = parsed.first;
    std::string param_name = parsed.second;

    RadixNodePtr child = nullptr;

    // 根据类型查找现有子节点
    if (type == NodeType::STATIC) {
      child = current->find_static_child(seg);
    } else if (type == NodeType::PARAM) {
      child = current->find_param_child();
    } else if (type == NodeType::CATCH_ALL) {
      child = current->find_catch_all_child();
    }

    if (!child) {
      // 创建新节点
      child = std::make_shared<RadixNode>();
      child->type_ = type;

      if (type == NodeType::STATIC) {
        child->path_ = seg;
      } else {
        child->path_ = seg; // 保留原始路径用于调试
        child->param_name_ = param_name;
      }

      current->add_child(child);
    }

    current = child;
  }

  // 设置处理器
  current->handlers_[method] = std::move(handler);
  ZHTTP_LOG_DEBUG("Route registered: {} {} -> node has {} handlers",
                  method_to_string(method), path, current->handlers_.size());
}

RouteMatch RadixTree::find(const std::string &path) const {
  ZHTTP_LOG_DEBUG("RadixTree::find {}", path);

  RouteMatch result;
  std::vector<std::string> segments = split_path(path);

  if (segments.empty()) {
    // 根路径
    if (root_->is_leaf()) {
      result.found = true;
      result.node = root_;
    }
    return result;
  }

  match_recursive(root_, segments, 0, result);
  return result;
}

bool RadixTree::match_recursive(const RadixNodePtr &node,
                                const std::vector<std::string> &segments,
                                size_t index, RouteMatch &result) const {
  // 所有段都匹配完成
  if (index >= segments.size()) {
    if (node->is_leaf()) {
      result.found = true;
      result.node = node;
      return true;
    }
    return false;
  }

  const std::string &seg = segments[index];

  // 优先级1: 尝试静态匹配（最高优先级，Early Return）
  RadixNodePtr static_child = node->find_static_child(seg);
  if (static_child) {
    if (match_recursive(static_child, segments, index + 1, result)) {
      return true; // Early Return: 静态匹配成功，不再尝试其他
    }
  }

  // 优先级2: 尝试参数匹配
  RadixNodePtr param_child = node->find_param_child();
  if (param_child) {
    // 保存当前参数
    std::string param_value = seg;
    if (match_recursive(param_child, segments, index + 1, result)) {
      result.params[param_child->param_name_] = param_value;
      return true;
    }
  }

  // 优先级3: 尝试通配符匹配（最低优先级）
  RadixNodePtr catch_all = node->find_catch_all_child();
  if (catch_all) {
    // 通配符匹配剩余所有路径
    std::string remaining;
    for (size_t i = index; i < segments.size(); ++i) {
      if (!remaining.empty()) {
        remaining += "/";
      }
      remaining += segments[i];
    }

    if (catch_all->is_leaf()) {
      result.found = true;
      result.node = catch_all;
      if (!catch_all->param_name_.empty()) {
        result.params[catch_all->param_name_] = remaining;
      }
      return true;
    }
  }

  return false;
}

} // namespace zhttp
