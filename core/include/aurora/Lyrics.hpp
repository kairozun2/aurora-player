// Aurora Player - synced lyrics (.lrc) and plain text lyrics.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace aurora {

struct LyricLine {
    double timeSec = 0.0;   ///< -1 when the line has no timestamp
    std::string text;
};

class Lyrics {
public:
    bool loadFile(const std::string& path);
    void loadText(const std::string& text);
    void clear();

    bool empty() const { return lines_.empty(); }
    bool synced() const { return synced_; }
    const std::vector<LyricLine>& lines() const { return lines_; }
    const std::string& title() const { return title_; }
    const std::string& artist() const { return artist_; }
    double offsetSec() const { return offsetSec_; }

    /// Index of the line that should be highlighted at `seconds`, or -1.
    int indexAt(double seconds) const;
    std::string textAt(double seconds) const;
    std::string plainText() const;

private:
    std::vector<LyricLine> lines_;
    std::string title_;
    std::string artist_;
    double offsetSec_ = 0.0;
    bool synced_ = false;
};

} // namespace aurora
