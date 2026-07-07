#ifndef CC_VRWRAPPER_REPORT_SINKS_HPP
#define CC_VRWRAPPER_REPORT_SINKS_HPP

#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <systemc>

#if defined(__unix__)
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <unistd.h>
#  define VR_HAS_NETWORK 1
#else
#  define VR_HAS_NETWORK 0
#endif

#include "colors.hpp"
#include "utils.hpp"
#include "config.hpp"
#include "formatter.hpp"

namespace cc_vrwrapper::report::sinks
{

// ------------------------------------------------------------------------
// Sink interface
// ------------------------------------------------------------------------
struct Sink {
    virtual ~Sink() = default;
    virtual void write(const formatter::ReportInfo& info) = 0;
    virtual void flush() {}
};

// ------------------------------------------------------------------------
// Terminal sink — writes to stderr with colors (if TTY)
// ------------------------------------------------------------------------
class TerminalSink : public Sink {
public:
    void write(const formatter::ReportInfo& info) override
    {
        auto c = config::get_config();
        bool use_color = !c.force_no_color && utils::stderr_is_tty();

        std::string line = formatter::format_text(info);
        if (use_color) {
            std::cerr << colors::ansi_prefix(info.severity)
                      << line
                      << colors::RESET;
        } else {
            std::cerr << line;
        }

        if (info.severity == sc_core::SC_FATAL)
            std::cerr << " - Simulation will terminate!";
        std::cerr << "\n";
    }
    void flush() override { std::cerr.flush(); }
};

// ------------------------------------------------------------------------
// File sink — writes to file (text/json/logfmt, no colors), with rotation + gzip
// ------------------------------------------------------------------------
class FileSink : public Sink {
public:
    std::ofstream   file;
    std::string     path;
    std::size_t     current_size = 0;
    std::mutex      file_mutex;

    explicit FileSink(const std::string& p) : path(p)
    {
        if (!utils::ensure_parent_dirs(path)) {
            std::cerr << "[FATAL] FileSink: cannot create dirs for " << path << "\n";
            return;
        }
        file.open(path, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "[FATAL] FileSink: cannot open " << path << "\n";
        }
    }

    ~FileSink() override { if (file.is_open()) file.close(); }

    // NOTE: caller must hold file_mutex
    void maybe_rotate(std::size_t added)
    {
        auto c = config::get_config();
        if (c.max_file_size == 0) return;
        if (current_size + added < c.max_file_size) return;

        std::string backup = path + ".1";

        file.close();
        std::remove(backup.c_str());
        if (std::rename(path.c_str(), backup.c_str()) != 0) {
            // rotation failed; reopen original in append mode and keep going
            file.open(path, std::ios::out | std::ios::app);
            return;
        }

        if (c.compress_rotated) {
            // Compress the rotated file via external gzip (POSIX)
            std::string cmd = "gzip -f \"" + backup + "\" 2>/dev/null";
            int rc = std::system(cmd.c_str());
            (void)rc; // ignore failures
        }

        file.open(path, std::ios::out | std::ios::trunc);
        current_size = 0;
    }

    void write(const formatter::ReportInfo& info) override
    {
        auto c = config::get_config();
        std::string line = formatter::format(info, c.log_as_json, c.log_as_logfmt);
        line += "\n";

        std::lock_guard<std::mutex> g(file_mutex);
        if (!file.is_open()) return;

        maybe_rotate(line.size());
        file << line;
        current_size += line.size();
        file.flush();
    }

    void flush() override
    {
        std::lock_guard<std::mutex> g(file_mutex);
        if (file.is_open()) file.flush();
    }
};

// ------------------------------------------------------------------------
// Callback sink — user-registered lambdas
// ------------------------------------------------------------------------
using CallbackFn = std::function<void(const formatter::ReportInfo&)>;

class CallbackSink : public Sink {
public:
    std::vector<CallbackFn> callbacks;
    std::mutex cb_mutex;

    void add(CallbackFn fn)
    {
        std::lock_guard<std::mutex> g(cb_mutex);
        callbacks.push_back(std::move(fn));
    }

    void write(const formatter::ReportInfo& info) override
    {
        std::lock_guard<std::mutex> g(cb_mutex);
        for (auto& cb : callbacks) {
            try { cb(info); } catch (...) {}
        }
    }
};

// ------------------------------------------------------------------------
// Network sink — sends log lines over TCP (POSIX, basic)
// ------------------------------------------------------------------------
class NetworkSink : public Sink {
public:
#if VR_HAS_NETWORK
    int         sock_fd = -1;
    std::string host;
    int         port;
    std::mutex  net_mutex;

    NetworkSink(const std::string& h, int p) : host(h), port(p)
    {
        sock_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) return;

        hostent* he = ::gethostbyname(h.c_str());
        if (!he) { ::close(sock_fd); sock_fd = -1; return; }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(static_cast<std::uint16_t>(p));
        std::memcpy(&addr.sin_addr, he->h_addr,
                    static_cast<std::size_t>(he->h_length));

        if (::connect(sock_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(sock_fd);
            sock_fd = -1;
        }
    }

    ~NetworkSink() override {
        if (sock_fd >= 0) ::close(sock_fd);
    }

    void write(const formatter::ReportInfo& info) override
    {
        if (sock_fd < 0) return;
        std::string line = formatter::format_json(info) + "\n";

        std::lock_guard<std::mutex> g(net_mutex);
        std::size_t sent = 0;
        while (sent < line.size()) {
            ssize_t n = ::send(sock_fd, line.data() + sent, line.size() - sent,
#ifdef MSG_NOSIGNAL
                               MSG_NOSIGNAL
#else
                               0
#endif
            );
            if (n <= 0) {           // connection broken — give up, disable sink
                ::close(sock_fd);
                sock_fd = -1;
                return;
            }
            sent += static_cast<std::size_t>(n);
        }
    }
#else
    NetworkSink(const std::string&, int) {
        std::cerr << "[WARNING] NetworkSink: not supported on this platform\n";
    }
    void write(const formatter::ReportInfo&) override {}
#endif
};

// ------------------------------------------------------------------------
// Sink manager — holds all active sinks
// ------------------------------------------------------------------------
class SinkManager {
public:
    std::vector<std::shared_ptr<Sink>> sinks;
    std::mutex sinks_mutex;

    void add(std::shared_ptr<Sink> s) {
        std::lock_guard<std::mutex> g(sinks_mutex);
        sinks.push_back(std::move(s));
    }

    void clear() {
        std::lock_guard<std::mutex> g(sinks_mutex);
        sinks.clear();
    }

    void write_all(const formatter::ReportInfo& info) {
        std::lock_guard<std::mutex> g(sinks_mutex);
        for (auto& s : sinks) {
            try { s->write(info); } catch (...) {}
        }
    }

    void flush_all() {
        std::lock_guard<std::mutex> g(sinks_mutex);
        for (auto& s : sinks) {
            try { s->flush(); } catch (...) {}
        }
    }
};

inline SinkManager& sink_manager()
{
    static SinkManager inst;
    return inst;
}

} // namespace cc_vrwrapper::report::sinks

#endif // CC_VRWRAPPER_REPORT_SINKS_HPP
