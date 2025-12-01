#include <atomic>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <queue>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/byte_multi_array.hpp>
#include <thread>
#include <vector>

using namespace std::chrono;

static size_t PACK_SIZE = 0; // 动态设置
static std::atomic<int> g_rate = 10;
static std::string g_node_name = "";
static std::string g_topic_name = "";
static std::string g_mode = "";

class BigPublisher : public rclcpp::Node {
public:
  BigPublisher(const std::string &node_name, const std::string &topic_name,
               int buffer_size, int rate)
      : Node(node_name), PACK_SIZE_BYTES(buffer_size), current_id_(0) {

    RCLCPP_INFO(this->get_logger(),
                "Publisher启动 - 节点: %s, 主题: %s, 消息大小: %d KB, 速率: %d "
                "消息/批次",
                node_name.c_str(), topic_name.c_str(), buffer_size / 1024,
                rate);

    // 创建发布者，QoS设置为10
    pub_ =
        this->create_publisher<std_msgs::msg::ByteMultiArray>(topic_name, 10);

    // 创建发布线程
    th_ = std::thread([this, rate]() {
      std_msgs::msg::ByteMultiArray msg;
      msg.data.resize(PACK_SIZE_BYTES, 0x11); // 填充数据

      auto t_prev = steady_clock::now();
      uint64_t sent_this_second = 0;
      uint64_t total_sent = 0;
      uint64_t bytes_sent_this_second = 0;

      while (rclcpp::ok()) {
        // 批量发布
        for (size_t i = 0; i < rate; i++) {
          // 在消息头部添加ID和时间戳
          uint64_t msg_id = current_id_++;
          auto send_time = steady_clock::now();

          // 将ID和时间戳编码到消息前16字节
          if (PACK_SIZE_BYTES >= 16) {
            memcpy(&msg.data[0], &msg_id, sizeof(msg_id));
            auto time_ns = send_time.time_since_epoch().count();
            memcpy(&msg.data[8], &time_ns, sizeof(time_ns));
          }

          pub_->publish(msg);

          sent_this_second++;
          total_sent++;
          bytes_sent_this_second += PACK_SIZE_BYTES;
        }

        // 每秒统计一次
        auto now = steady_clock::now();
        auto elapsed_ms = duration_cast<milliseconds>(now - t_prev).count();

        if (elapsed_ms >= 1000) {
          // 精确计算实际每秒的带宽
          double actual_elapsed_seconds = elapsed_ms / 1000.0;
          double actual_msg_per_sec = sent_this_second / actual_elapsed_seconds;
          double actual_mbps = bytes_sent_this_second /
                               (1024.0 * 1024.0 * actual_elapsed_seconds);

          RCLCPP_INFO(
              this->get_logger(),
              "[PUB STAT] 发送速率: %.0f msg/s, 带宽: %.2f MB/s, 总计: %lu",
              actual_msg_per_sec, actual_mbps, total_sent);

          sent_this_second = 0;
          bytes_sent_this_second = 0;
          t_prev = now;
        }

        // 短延迟以避免占用过多CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
    th_.detach();
  }

  ~BigPublisher() {
    if (th_.joinable()) {
      th_.join();
    }
  }

private:
  size_t PACK_SIZE_BYTES;
  std::atomic<uint64_t> current_id_;
  rclcpp::Publisher<std_msgs::msg::ByteMultiArray>::SharedPtr pub_;
  std::thread th_;
};

class BigSubscriber : public rclcpp::Node {
public:
  BigSubscriber(const std::string &node_name, const std::string &topic_name,
                int buffer_size)
      : Node(node_name), PACK_SIZE_BYTES(buffer_size),
        last_stat_time_(steady_clock::now()), last_msg_id_(0) {

    RCLCPP_INFO(this->get_logger(),
                "Subscriber启动 - 节点: %s, 主题: %s, 消息大小: %d KB",
                node_name.c_str(), topic_name.c_str(), buffer_size / 1024);

    // 重置统计计数器
    current_second_received_ = 0;
    current_second_lost_ = 0;
    current_second_total_latency_us_ = 0;
    current_second_bytes_received_ = 0;

    // 创建订阅者
    sub_ = this->create_subscription<std_msgs::msg::ByteMultiArray>(
        topic_name, 10,
        [this](const std_msgs::msg::ByteMultiArray::SharedPtr msg) {
          auto receive_time = steady_clock::now();

          // 从消息中提取ID和时间戳
          uint64_t msg_id = 0;
          int64_t send_time_ns = 0;

          if (msg->data.size() >= 16) {
            memcpy(&msg_id, &msg->data[0], sizeof(msg_id));
            memcpy(&send_time_ns, &msg->data[8], sizeof(send_time_ns));
          }

          // 计算延迟（单位：微秒）
          int64_t latency_us = 0;
          if (send_time_ns > 0) {
            auto send_time =
                steady_clock::time_point(nanoseconds(send_time_ns));
            latency_us =
                duration_cast<microseconds>(receive_time - send_time).count();

            // 更新延迟统计
            current_second_total_latency_us_ += latency_us;
          }

          // 检查消息丢失（基于ID连续性）
          if (msg_id > 0) {
            if (msg_id > last_msg_id_ + 1 && last_msg_id_ > 0) {
              // 检测到消息丢失
              uint64_t lost_count = msg_id - last_msg_id_ - 1;
              current_second_lost_ += lost_count;
            }
            last_msg_id_ = msg_id;
          }

          current_second_received_++;
          current_second_bytes_received_ += PACK_SIZE_BYTES;
        });

    // 创建定时器用于每秒统计
    timer_ = this->create_wall_timer(100ms, [this]() {
      auto now = steady_clock::now();
      auto elapsed_ms =
          duration_cast<milliseconds>(now - last_stat_time_).count();

      if (elapsed_ms >= 1000) {
        // 获取当前秒的统计值
        uint64_t received = current_second_received_.exchange(0);
        uint64_t lost = current_second_lost_.exchange(0);
        uint64_t total_latency = current_second_total_latency_us_.exchange(0);
        uint64_t bytes_received = current_second_bytes_received_.exchange(0);

        // 精确计算实际每秒的带宽
        double actual_elapsed_seconds = elapsed_ms / 1000.0;
        double actual_msg_per_sec = received / actual_elapsed_seconds;
        double actual_mbps =
            bytes_received / (1024.0 * 1024.0 * actual_elapsed_seconds);

        // 计算瞬时丢包率（基于当前秒）
        double instant_loss_rate = 0.0;
        if (received + lost > 0) {
          instant_loss_rate = lost * 100.0 / (received + lost);
        }

        // 计算平均延迟（基于当前秒）
        double avg_latency_us = 0.0;
        if (received > 0 && total_latency > 0) {
          avg_latency_us = static_cast<double>(total_latency) / received;
        }

        RCLCPP_INFO(this->get_logger(),
                    "[SUB STAT] 接收速率: %.0f msg/s, 带宽: %.2f MB/s, "
                    "丢包: %lu(%.2f%%), 平均延迟: %.1f us",
                    actual_msg_per_sec, actual_mbps, lost, instant_loss_rate,
                    avg_latency_us);

        last_stat_time_ = now;
      }
    });
  }

private:
  size_t PACK_SIZE_BYTES;
  std::atomic<uint64_t> current_second_received_;
  std::atomic<uint64_t> current_second_lost_;
  std::atomic<uint64_t> current_second_total_latency_us_;
  std::atomic<uint64_t> current_second_bytes_received_;
  std::atomic<uint64_t> last_msg_id_;
  steady_clock::time_point last_stat_time_;

  rclcpp::Subscription<std_msgs::msg::ByteMultiArray>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

void print_usage(const char *program_name) {
  std::cout << "用法: " << std::endl;
  std::cout << "  发布者: " << program_name
            << " pub <node_name> <topic_name> <buffer_size_KB> <rate>"
            << std::endl;
  std::cout << "  订阅者: " << program_name
            << " sub <node_name> <topic_name> <buffer_size_KB>" << std::endl;
  std::cout << "必需参数:" << std::endl;
  std::cout << "  pub/sub        模式选择 (pub: 发布者, sub: 订阅者)"
            << std::endl;
  std::cout << "  node_name      节点名称" << std::endl;
  std::cout << "  topic_name     主题名称" << std::endl;
  std::cout << "  buffer_size_KB 消息大小 (KB)" << std::endl;
  std::cout << "  rate           发送速率 (消息/批次, 仅pub模式使用)"
            << std::endl;
  std::cout << std::endl;
  std::cout << "示例:" << std::endl;
  std::cout << "  " << program_name << " pub pub_node1 test_topic 64 10"
            << std::endl;
  std::cout << "  " << program_name << " sub sub_node1 test_topic 64"
            << std::endl;
  std::cout << "  " << program_name << " pub pub_node2 big_topic 128 5"
            << std::endl;
}

bool parse_arguments(int argc, char *argv[]) {
  // 检查最少参数数量
  if (argc < 5) {
    std::cerr << "错误: 参数不足!" << std::endl;
    print_usage(argv[0]);
    return false;
  }

  // 解析必需参数
  g_mode = argv[1];
  if (g_mode != "pub" && g_mode != "sub") {
    std::cerr << "错误: 模式必须是 'pub' 或 'sub'" << std::endl;
    print_usage(argv[0]);
    return false;
  }

  g_node_name = argv[2];
  if (g_node_name.empty()) {
    std::cerr << "错误: 节点名称不能为空" << std::endl;
    print_usage(argv[0]);
    return false;
  }

  g_topic_name = argv[3];
  if (g_topic_name.empty()) {
    std::cerr << "错误: 主题名称不能为空" << std::endl;
    print_usage(argv[0]);
    return false;
  }

  try {
    int buffer_size_kb = std::stoi(argv[4]);
    if (buffer_size_kb <= 0) {
      std::cerr << "错误: buffer_size必须大于0" << std::endl;
      return false;
    }
    PACK_SIZE = buffer_size_kb * 1024; // KB转换为字节
  } catch (const std::exception &e) {
    std::cerr << "错误: 无效的buffer_size '" << argv[4] << "'" << std::endl;
    print_usage(argv[0]);
    return false;
  }

  // 发布者模式需要rate参数，订阅者模式不需要
  if (g_mode == "pub") {
    if (argc < 6) {
      std::cerr << "错误: 发布者模式需要rate参数!" << std::endl;
      print_usage(argv[0]);
      return false;
    }

    try {
      g_rate = std::stoi(argv[5]);
      if (g_rate <= 0) {
        std::cerr << "错误: 发送速率必须大于0" << std::endl;
        return false;
      }
    } catch (const std::exception &e) {
      std::cerr << "错误: 无效的发送速率 '" << argv[5] << "'" << std::endl;
      print_usage(argv[0]);
      return false;
    }
  }

  return true;
}

int main(int argc, char **argv) {
  std::cout << "=== ROS2 高性能测试程序 (简化版本) ===" << std::endl;

  // 解析命令行参数
  if (!parse_arguments(argc, argv)) {
    return 1;
  }

  // 显示配置信息
  std::cout << "模式: " << g_mode << std::endl;
  std::cout << "节点名称: " << g_node_name << std::endl;
  std::cout << "主题名称: " << g_topic_name << std::endl;
  std::cout << "消息大小: " << PACK_SIZE  << "KB" << std::endl;
  if (g_mode == "pub") {
    std::cout << "发送速率: " << g_rate << " 消息/批次" << std::endl;
  }
  std::cout << std::endl;

  // 初始化ROS2
  rclcpp::init(argc, argv);

  if (g_mode == "sub") {
    // 订阅者模式
    auto node =
        std::make_shared<BigSubscriber>(g_node_name, g_topic_name, PACK_SIZE);
    rclcpp::spin(node);
  } else {
    // 发布者模式
    auto node = std::make_shared<BigPublisher>(g_node_name, g_topic_name,
                                               PACK_SIZE, g_rate);
    rclcpp::spin(node);
  }

  rclcpp::shutdown();
  std::cout << "=== 测试程序结束 ===" << std::endl;
  return 0;
}