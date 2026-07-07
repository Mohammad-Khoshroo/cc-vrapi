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
#include <algorithm>   // ADDED: for std::remove (used in SinkManager::remove)
#include <filesystem>
#include <systemc>

#if defined(__unix__)
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <sys/wait.h>   // ADDED: for waitpid
#  include <fcntl.h>      // ADDED: for open/creat in gzip_file
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
// ADDED: Helper — gzip a file WITHOUT invoking a shell (POSIX only).
//
// Previously the code used std::system("gzip -f \"path\"") which was
// vulnerable to shell injection if the log path contained shell
// metacharacters (e.g.  logs/"; rm -rf /; ").
//
// This implementation uses fork() + execlp() so the path is passed as a
// proper argv element, never interpreted by a shell.
//
// Creates `src + ".gz"` and removes the original on success.
// Returns true on success, false on any failure.
// ------------------------------------------------------------------------
inline bool gzip_file(const std::string& src)
{
#if defined(__unix__)
    std::string dst = src + ".gz";
    pid_t pid = ::fork();
    if (pid < 0) return false;
    if (pid == 0) {
        // Child: redirect stdout → dst file, then exec gzip -c src
        int fd = ::open(dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) ::_exit(127);
        ::dup2(fd, STDOUT_FILENO);
        ::close(fd);
        ::execlp("gzip", "gzip", "-c", src.c_str(), (char*)nullptr);
        ::_exit(127);  // only reached if execlp failed
    }
    // Parent: wait for child
    int status = 0;
    ::waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        std::remove(src.c_str());  // remove uncompressed original
        return true;
    }
    return false;
#else
    (void)src;
    return false;  // non-POSIX: compression not supported
#endif
}

// ------------------------------------------------------------------------
// Terminal sink — writes to stderr with colors (if TTY)
// ------------------------------------------------------------------------
class TerminalSink : public Sink {
public:
    void write(const formatter::ReportInfo& info) override
    {
        auto c = config::get_config();
        bool use_color = !c.force_no_color && utils::stderr_is_tty();

        // Terminal always uses human-readable text format (not JSON/logfmt)
        // for readability. If you need structured output, use FileSink.
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
            // Rotation failed — reopen original in append mode and sync
            // current_size to the actual file size on disk.
            //
            // CHANGED: previously current_size was NOT recalculated here,
            // so every subsequent write would retry (and fail) rotation
            // in an infinite loop. Now we query the real file size so the
            // next rotation attempt only happens when genuinely needed.
            file.open(path, std::ios::out | std::ios::app);
            std::error_code ec;
            auto sz = std::filesystem::file_size(path, ec);
            current_size = ec ? 0 : static_cast<std::size_t>(sz);
            return;
        }

        if (c.compress_rotated) {
            // CHANGED: use gzip_file() instead of std::system() to avoid
            // shell injection via crafted file paths.
            gzip_file(backup);  // creates backup.gz, removes backup
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

    // CHANGED: replaced gethostbyname (thread-unsafe, deprecated) with
    // getaddrinfo (thread-safe, modern). Also added reconnect() so a
    // dropped connection can be re-established on the next write.
    void reconnect()
    {
        if (sock_fd >= 0) {
            ::close(sock_fd);
            sock_fd = -1;
        }

        addrinfo hints{};
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* res = nullptr;
        std::string port_str = std::to_string(port);
        if (::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) {
            return;
        }

        // Try each resolved address until one connects
        for (addrinfo* rp = res; rp; rp = rp->ai_next) {
            sock_fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (sock_fd < 0) continue;
            if (::connect(sock_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
                break;  // success
            }
            ::close(sock_fd);
            sock_fd = -1;
        }
        ::freeaddrinfo(res);
    }

    NetworkSink(const std::string& h, int p) : host(h), port(p)
    {
        reconnect();
    }

    ~NetworkSink() override {
        if (sock_fd >= 0) ::close(sock_fd);
    }

    void write(const formatter::ReportInfo& info) override
    {
        std::string line = formatter::format_json(info) + "\n";

        std::lock_guard<std::mutex> g(net_mutex);

        // CHANGED: reconnect logic moved inside the lock to prevent
        // two threads from reconnecting simultaneously.
        if (sock_fd < 0) {
            reconnect();
            if (sock_fd < 0) return;
        }

        std::size_t sent = 0;
        while (sent < line.size()) {
            ssize_t n = ::send(sock_fd, line.data() + sent, line.size() - sent,
#ifdef MSG_NOSIGNAL
                               MSG_NOSIGNAL
#else
                               0
#endif
            );
            if (n <= 0) {
                // Connection broken — close and give up on this write.
                // Next write() will attempt reconnect().
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

    // ADDED: remove a specific sink by pointer identity.
    // Previously only clear() existed — no way to remove individual sinks.
    void remove(std::shared_ptr<Sink> s) {
        std::lock_guard<std::mutex> g(sinks_mutex);
        sinks.erase(
            std::remove(sinks.begin(), sinks.end(), s),
            sinks.end()
        );
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