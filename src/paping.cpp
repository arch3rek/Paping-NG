/*
 * Paping-NG - Modern cross-platform TCP port testing tool
 * Copyright (C) 2026 Oliver (arch3rek, www.arch3r.eu)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  pragma comment(lib, "ws2_32.lib")
using SockLen = int;
#else
#  include <sys/socket.h>
#  include <netdb.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <fcntl.h>
using SOCKET = int;
using SockLen = socklen_t;
constexpr SOCKET INVALID_SOCKET = -1;
inline void closesocket(SOCKET s) { ::close(s); }
#endif

#include <iostream>
#include <string>
#include <string_view>
#include <format>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <charconv>
#include <limits>
#include <optional>
#include <cstring>
#include <algorithm>

static constexpr std::string_view VERSION = "1.0.1";

namespace col {
    constexpr std::string_view rst = "\033[0m";
    constexpr std::string_view red = "\033[91m";
    constexpr std::string_view green = "\033[92m";
    constexpr std::string_view yellow = "\033[93m";
    constexpr std::string_view cyan = "\033[96m";
    constexpr std::string_view blue = "\033[94m";
}

static std::string colored(std::string_view text, std::string_view color) {
    return std::string(color) + std::string(text) + std::string(col::rst);
}

static void platform_init() {
#ifdef _WIN32
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;
    if (GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

static void platform_cleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

struct Config {
    std::string host;
    int port = 0;
    int count = -1;
    int timeout = 1000;
};

struct HostInfo {
    std::string input;
    std::string canonical;
    std::string ip;
    bool        is_ip;
};

struct Stats {
    int    attempts = 0;
    int    connects = 0;
    int    failures = 0;
    double total = 0.0;
    double min_ms = std::numeric_limits<double>::max();
    double max_ms = 0.0;

    void record(double ms) {
        total += ms;
        if (ms < min_ms) min_ms = ms;
        if (ms > max_ms) max_ms = ms;
    }
    [[nodiscard]] double avg() const {
        return connects > 0 ? total / connects : 0.0;
    }
};

static std::optional<HostInfo> resolve(const std::string& hostname) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    addrinfo* res = nullptr;
    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &res) != 0 || !res)
        return std::nullopt;

    char ip_buf[INET6_ADDRSTRLEN]{};
    void* addr_ptr = res->ai_family == AF_INET
        ? static_cast<void*>(&reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr)
        : static_cast<void*>(&reinterpret_cast<sockaddr_in6*>(res->ai_addr)->sin6_addr);
    inet_ntop(res->ai_family, addr_ptr, ip_buf, sizeof(ip_buf));

    HostInfo info;
    info.input = hostname;
    info.canonical = res->ai_canonname ? res->ai_canonname : hostname;
    info.ip = ip_buf;
    info.is_ip = (hostname == std::string(ip_buf));
    freeaddrinfo(res);
    return info;
}

enum class Probe { Connected, TimedOut, Failed };

static Probe tcp_probe(const std::string& ip, int port, int timeout_ms, double& elapsed_ms) {
    elapsed_ms = 0.0;

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST;

    addrinfo* res = nullptr;
    auto      port_str = std::to_string(port);
    if (getaddrinfo(ip.c_str(), port_str.c_str(), &hints, &res) != 0 || !res)
        return Probe::Failed;

    SOCKET sock = ::socket(res->ai_family, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { freeaddrinfo(res); return Probe::Failed; }

    // non-blocking connect
#ifdef _WIN32
    ULONG nonblocking = 1;
    ioctlsocket(sock, FIONBIO, &nonblocking);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    auto t0 = std::chrono::steady_clock::now();
    ::connect(sock, res->ai_addr, static_cast<SockLen>(res->ai_addrlen));
    freeaddrinfo(res);

    fd_set wfds, efds;
    FD_ZERO(&wfds); FD_ZERO(&efds);
    FD_SET(sock, &wfds);
    FD_SET(sock, &efds);

    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    // windows ignores nfds on linux must be max_fd+1
    int nfds = 0;
#ifndef _WIN32
    nfds = static_cast<int>(sock) + 1;
#endif
    int r = select(nfds, nullptr, &wfds, &efds, &tv);

    elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0).count();

    Probe result = Probe::Connected;
    if (r <= 0) {
        result = Probe::TimedOut;
    }
    else if (FD_ISSET(sock, &efds)) {
        result = Probe::Failed;
    }
    else {
        int     err = 0;
        SockLen opt_len = static_cast<SockLen>(sizeof(err));
        getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &opt_len);
        if (err != 0) result = Probe::Failed;
    }

    closesocket(sock);
    return result;
}

static void print_banner() {
    std::cout << std::format("Paping-NG v{} - Copyright (c) 2026 Oliver (arch3r.eu)\n\n", VERSION);
}

static void print_usage() {
    std::cout <<
        "Usage: paping <host> -p <port> [options]\n\n"
        "Options:\n"
        "  -p, --port N      TCP port to probe (required)\n"
        "  -c, --count N     stop after N probes (default: run forever)\n"
        "  -t, --timeout N   connection timeout in ms (default: 1000)\n"
        "  -h, --help        show this help\n";
}

static void print_target(const HostInfo& host, int port) {
    if (host.is_ip) {
        std::cout << std::format("Connecting to {} on TCP port {}:\n\n",
            colored(host.ip, col::yellow),
            colored(std::to_string(port), col::yellow));
    }
    else {
        std::cout << std::format("Connecting to {} [{}] on TCP port {}:\n\n",
            colored(host.canonical, col::yellow),
            colored(host.ip, col::yellow),
            colored(std::to_string(port), col::yellow));
    }
}

static void print_probe(const HostInfo& host, int port, Probe result, double ms) {
    switch (result) {
    case Probe::Connected:
        std::cout << std::format("Connected to {}: time={} protocol={} port={}\n",
            colored(host.ip, col::green),
            colored(std::format("{:.2f}ms", ms), col::green),
            colored("TCP", col::green),
            colored(std::to_string(port), col::green));
        break;
    case Probe::TimedOut:
        std::cout << colored("Connection timed out\n", col::red);
        break;
    case Probe::Failed:
        std::cout << colored("Connection failed\n", col::red);
        break;
    }
}

static void print_stats(const Stats& s) {
    if (s.attempts == 0) return;
    const double fail_pct = 100.0 * s.failures / s.attempts;
    const double min_v = s.connects > 0 ? s.min_ms : 0.0;
    const double max_v = s.connects > 0 ? s.max_ms : 0.0;
    std::cout << std::format(
        "\n{}  Probed = {}, Connected = {}, Failed = {} ({:.2f}%)\n"
        "Approximate connection times:\n"
        "  Minimum = {:.2f}ms, Maximum = {:.2f}ms, Average = {:.2f}ms\n",
        colored("Connection statistics:\n", col::blue),
        colored(std::to_string(s.attempts), col::cyan),
        colored(std::to_string(s.connects), col::green),
        colored(std::to_string(s.failures), col::red),
        fail_pct, min_v, max_v, s.avg());
}

static std::optional<Config> parse_args(int argc, char* argv[]) {
    Config cfg;
    bool   got_host = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        auto next_int = [&](int& out) -> bool {
            if (++i >= argc) return false;
            const char* p = argv[i];
            auto [end, ec] = std::from_chars(p, p + std::strlen(p), out);
            return ec == std::errc{};
            };

        if (arg == "-p" || arg == "--port") { if (!next_int(cfg.port))    return std::nullopt; }
        else if (arg == "-c" || arg == "--count") { if (!next_int(cfg.count))   return std::nullopt; }
        else if (arg == "-t" || arg == "--timeout") { if (!next_int(cfg.timeout)) return std::nullopt; }
        else if (arg == "-h" || arg == "--help" || arg == "-?") { return std::nullopt; }
        else if (!got_host) { cfg.host = argv[i]; got_host = true; }
        else { return std::nullopt; }
    }

    if (!got_host || cfg.port <= 0 || cfg.port > 65535 || cfg.timeout <= 0)
        return std::nullopt;
    return cfg;
}

static std::atomic<bool> g_stop{ false };
static void sig_handler(int) { g_stop = true; }

int main(int argc, char* argv[]) {
    platform_init();
    print_banner();

    auto cfg_opt = parse_args(argc, argv);
    if (!cfg_opt) {
        print_usage();
        platform_cleanup();
        return 1;
    }
    const Config& cfg = *cfg_opt;

    auto host_opt = resolve(cfg.host);
    if (!host_opt) {
        std::cerr << colored("Cannot resolve host: ", col::red) << cfg.host << '\n';
        platform_cleanup();
        return 1;
    }
    const HostInfo& host = *host_opt;

    print_target(host, cfg.port);
    std::signal(SIGINT, sig_handler);

    Stats stats;
    int   exit_code = 0;
    bool  infinite = (cfg.count < 0);

    for (int i = 0; (infinite || i < cfg.count) && !g_stop; ++i) {
        double elapsed = 0.0;
        Probe  result = tcp_probe(host.ip, cfg.port, cfg.timeout, elapsed);

        ++stats.attempts;
        if (result == Probe::Connected) {
            ++stats.connects;
            stats.record(elapsed);
        }
        else {
            ++stats.failures;
            exit_code = 1;
        }

        print_probe(host, cfg.port, result, elapsed);

        const int  sleep_ms = 1000 - static_cast<int>(std::min(elapsed, 1000.0));
        const bool more = infinite || (i + 1 < cfg.count);
        if (sleep_ms > 0 && more && !g_stop)
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }

    print_stats(stats);
    platform_cleanup();
    return exit_code;
}