#include "can_collector.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

CanCollector::CanCollector() = default;

CanCollector::~CanCollector() {
    close_socket();
}

bool CanCollector::init(const std::string& interface_name, const std::vector<CanFilterConfig>& filters) {
    close_socket();
    interface_name_ = interface_name;

    socket_fd_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_fd_ < 0) {
        std::cerr << "[CanCollector] Error creating SocketCAN raw socket: " << strerror(errno) << std::endl;
        return false;
    }

    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
    if (ioctl(socket_fd_, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "[CanCollector] ioctl SIOCGIFINDEX failed for interface " << interface_name_
                  << ": " << strerror(errno) << " (Interface might be down or not created yet)" << std::endl;
        close_socket();
        return false;
    }

    // Apply kernel-level CAN ID filters to save CPU cycles
    if (!filters.empty()) {
        std::vector<struct can_filter> rfilter(filters.size());
        for (size_t i = 0; i < filters.size(); ++i) {
            rfilter[i].can_id = filters[i].can_id;
            rfilter[i].can_mask = filters[i].mask ? filters[i].mask : CAN_SFF_MASK;
        }
        if (setsockopt(socket_fd_, SOL_CAN_RAW, CAN_RAW_FILTER, 
                       rfilter.data(), rfilter.size() * sizeof(struct can_filter)) < 0) {
            std::cerr << "[CanCollector] Warning: Failed to apply CAN raw filter: " << strerror(errno) << std::endl;
        } else {
            std::cout << "[CanCollector] Applied " << filters.size() << " kernel-level CAN filters." << std::endl;
        }
    }

    struct sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[CanCollector] Bind failed on " << interface_name_ << ": " << strerror(errno) << std::endl;
        close_socket();
        return false;
    }

    std::cout << "[CanCollector] Successfully bound to Linux SocketCAN interface: " << interface_name_ 
              << " (fd: " << socket_fd_ << ")" << std::endl;
    return true;
}

std::vector<nlohmann::json> CanCollector::read_frames(int timeout_ms) {
    std::vector<nlohmann::json> frames;
    if (socket_fd_ < 0) return frames;

    struct pollfd pfd{};
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;

    int poll_res = poll(&pfd, 1, timeout_ms);
    if (poll_res <= 0 || !(pfd.revents & POLLIN)) {
        return frames;
    }

    // Read available frames (up to 20 per cycle to avoid blocking main loop)
    int read_count = 0;
    while (read_count++ < 20) {
        struct can_frame frame{};
        ssize_t nbytes = read(socket_fd_, &frame, sizeof(struct can_frame));
        if (nbytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            break;
        }
        if (nbytes < static_cast<ssize_t>(sizeof(struct can_frame))) {
            break;
        }

        nlohmann::json f;
        bool is_eff = (frame.can_id & CAN_EFF_FLAG) != 0;
        bool is_rtr = (frame.can_id & CAN_RTR_FLAG) != 0;
        uint32_t clean_id = is_eff ? (frame.can_id & CAN_EFF_MASK) : (frame.can_id & CAN_SFF_MASK);

        f["can_id"] = clean_id;
        f["extended"] = is_eff;
        f["rtr"] = is_rtr;
        f["dlc"] = frame.can_dlc;

        std::ostringstream hex_ss;
        for (int i = 0; i < frame.can_dlc && i < 8; ++i) {
            hex_ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(frame.data[i]);
        }
        f["data_hex"] = hex_ss.str();

        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        f["timestamp_ms"] = now_ms;

        frames.push_back(std::move(f));

        // Peek if more data is ready
        struct pollfd check_pfd{};
        check_pfd.fd = socket_fd_;
        check_pfd.events = POLLIN;
        if (poll(&check_pfd, 1, 0) <= 0) break;
    }

    return frames;
}

bool CanCollector::send_frame(uint32_t can_id, const std::vector<uint8_t>& data) {
    if (socket_fd_ < 0 || data.size() > 8) return false;

    struct can_frame frame{};
    frame.can_id = can_id;
    frame.can_dlc = static_cast<__u8>(data.size());
    std::memcpy(frame.data, data.data(), data.size());

    ssize_t n = write(socket_fd_, &frame, sizeof(struct can_frame));
    return (n == sizeof(struct can_frame));
}

void CanCollector::close_socket() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}
