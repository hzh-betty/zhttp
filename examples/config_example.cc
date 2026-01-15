/**
 * @file config_example.cc
 * @brief 演示使用 TOML 配置文件和共享栈模式构建 HttpServer
 */

#include "zhttp.h"
#include <iostream>

using namespace zhttp;

/**
 * 方式一：从 TOML 文件加载配置
 */
void example_from_toml_file() {
  std::cout << "=== Example: Load from TOML file ===" << std::endl;

  HttpServerBuilder builder;

  // 从 TOML 配置文件加载
  builder.from_config("server_config.toml")
      .get("/",
           [](const HttpRequest::ptr &, HttpResponse &resp) {
             resp.json(R"({"message": "Hello from TOML config!"})");
           })
      .get("/status", [](const HttpRequest::ptr &, HttpResponse &resp) {
        resp.json(R"({"status": "ok"})");
      });

  // 获取并打印配置
  const auto &config = builder.config();
  std::cout << "Host: " << config.host << std::endl;
  std::cout << "Port: " << config.port << std::endl;
  std::cout << "Threads: " << config.num_threads << std::endl;
  std::cout << "Stack Mode: " << stack_mode_to_string(config.stack_mode)
            << std::endl;

  // 如果需要实际运行，取消下面的注释
  // builder.run();
}

/**
 * 方式二：使用链式 API 配置共享栈模式
 */
void example_shared_stack() {
  std::cout << "\n=== Example: Shared Stack Mode ===" << std::endl;

  HttpServerBuilder builder;

  builder.listen("0.0.0.0", 8080)
      .threads(8)
      .use_shared_stack() // 使用共享栈模式
      .log_level("debug")
      .get("/", [](const HttpRequest::ptr &, HttpResponse &resp) {
        resp.text("Running with shared stack!");
      });

  const auto &config = builder.config();
  std::cout << "Stack Mode: " << stack_mode_to_string(config.stack_mode)
            << std::endl;
  std::cout << "Threads: " << config.num_threads << std::endl;

  // builder.run();
}

/**
 * 方式三：使用链式 API 配置独立栈模式
 */
void example_independent_stack() {
  std::cout << "\n=== Example: Independent Stack Mode ===" << std::endl;

  HttpServerBuilder builder;

  builder.listen("0.0.0.0", 8080)
      .threads(4)
      .use_independent_stack() // 使用独立栈模式（默认）
      .get("/", [](const HttpRequest::ptr &, HttpResponse &resp) {
        resp.text("Running with independent stack!");
      });

  const auto &config = builder.config();
  std::cout << "Stack Mode: " << stack_mode_to_string(config.stack_mode)
            << std::endl;

  // builder.run();
}

/**
 * 方式四：使用 ServerConfig 对象
 */
void example_with_config_object() {
  std::cout << "\n=== Example: ServerConfig Object ===" << std::endl;

  // 手动创建配置
  ServerConfig config;
  config.host = "127.0.0.1";
  config.port = 9090;
  config.num_threads = 2;
  config.stack_mode = StackMode::SHARED;
  config.log_level = "info";
  config.server_name = "MyCustomServer/1.0";

  // 验证配置
  if (!config.validate()) {
    std::cerr << "Invalid configuration!" << std::endl;
    return;
  }

  // 导出为 TOML 字符串
  std::cout << "Generated TOML:\n" << config.to_toml_string() << std::endl;

  // 使用配置构建服务器
  HttpServerBuilder builder;
  builder.from_config(config).get(
      "/", [](const HttpRequest::ptr &, HttpResponse &resp) {
        resp.text("Hello!");
      });

  // builder.run();
}

/**
 * 方式五：从 TOML 字符串解析
 */
void example_from_toml_string() {
  std::cout << "\n=== Example: Parse TOML String ===" << std::endl;

  const char *toml_str = R"(
[server]
host = "localhost"
port = 3000

[threads]
count = 2
stack_mode = "shared"

[logging]
level = "debug"
)";

  ServerConfig config = ServerConfig::from_toml_string(toml_str);

  std::cout << "Parsed config:" << std::endl;
  std::cout << "  Host: " << config.host << std::endl;
  std::cout << "  Port: " << config.port << std::endl;
  std::cout << "  Threads: " << config.num_threads << std::endl;
  std::cout << "  Stack Mode: " << stack_mode_to_string(config.stack_mode)
            << std::endl;
}

int main() {
  init_logger(zlog::LogLevel::value::INFO);

  try {
    // 运行各种示例
    // example_from_toml_file();  // 需要配置文件存在
    example_shared_stack();
    example_independent_stack();
    example_with_config_object();
    example_from_toml_string();

    std::cout << "\n=== All examples completed! ===" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
