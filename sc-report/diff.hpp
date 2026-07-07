#ifndef CC_VRWRAPPER_REPORT_DIFF_HPP
#define CC_VRWRAPPER_REPORT_DIFF_HPP

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace cc_vrwrapper::report::diff
{

// Strip the timestamp portion " @ <time>" so two runs with different
// simulation timing can still be compared.
inline std::string normalize_line(const std::string& line)
{
    std::string out = line;
    auto pos = out.find(" @ ");
    if (pos != std::string::npos) {
        // Remove from " @ " until the next " (" which marks file:line
        auto end = out.find(" (", pos);
        if (end != std::string::npos) {
            out.erase(pos, end - pos);
        }
    }
    return out;
}

inline std::vector<std::string> read_lines(const std::string& path)
{
    std::vector<std::string> lines;
    std::ifstream f(path);
    if (!f) return lines;
    std::string line;
    while (std::getline(f, line)) {
        lines.push_back(normalize_line(line));
    }
    return lines;
}

struct DiffResult {
    int  added    = 0;
    int  removed  = 0;
    bool identical = true;
};

// ------------------------------------------------------------------------
// NOTE: This is a simple line-by-line positional diff.
// If a line is inserted or removed in the MIDDLE of the log, ALL subsequent
// lines will appear as changed (added+removed). For a smarter diff
// (LCS / Myers algorithm), replace this implementation. This is sufficient
// for detecting additions/removals at the END, which is the common case
// for log regression testing.
// ------------------------------------------------------------------------
inline DiffResult compare(const std::string& baseline_path,
                          const std::string& current_path,
                          std::ostream& os = std::cerr)
{
    auto base = read_lines(baseline_path);
    auto curr = read_lines(current_path);

    DiffResult r;
    std::size_t n = std::max(base.size(), curr.size());
    for (std::size_t i = 0; i < n; ++i) {
        bool b_ok = i < base.size();
        bool c_ok = i < curr.size();
        if (b_ok && c_ok) {
            if (base[i] != curr[i]) {
                os << "~~ line " << (i+1) << " differs\n"
                   << "- " << base[i] << "\n"
                   << "+ " << curr[i] << "\n";
                r.identical = false;
            }
        } else if (b_ok) {
            os << "-- line " << (i+1) << " removed\n"
               << "- " << base[i] << "\n";
            r.removed++;
            r.identical = false;
        } else {
            os << "++ line " << (i+1) << " added\n"
               << "+ " << curr[i] << "\n";
            r.added++;
            r.identical = false;
        }
    }
    return r;
}

} // namespace cc_vrwrapper::report::diff

#endif