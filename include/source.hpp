#ifndef YOYO_ACE1110_QASSEMBLER_SOURCE_HPP
#define YOYO_ACE1110_QASSEMBLER_SOURCE_HPP

#include"forward.hpp"

struct SrcLoc {
    size_t offset;
    size_t file_id;
    SrcManager* srcman;

    // constructor
    inline constexpr explicit SrcLoc(SrcManager* srcman_) noexcept
    : offset(npos), file_id(npos), srcman(srcman_) {}
    inline constexpr explicit SrcLoc(SrcManager* srcman_, size_t offset_, size_t file_id) noexcept
    : offset(offset_), file_id(file_id), srcman(srcman_) {}

    // function 
    [[nodiscard]] SrcFile* file_ptr();
    [[nodiscard]] const SrcFile* file_ptr() const;
    [[nodiscard]] std::string_view filepath() const;
};

struct SrcFile {
    size_t id = npos;
    SrcManager* srcman;
    std::string content;
    std::string modified;
    std::vector<size_t> line_offsets;

    // constructor
    inline explicit SrcFile(SrcManager* srcman_, size_t id_, const std::string& content_)
    : id(id_), srcman(srcman_), content(content_), modified(content_) { this->compute_line_offsets(); }
    
    // content-based function
    inline constexpr void compute_line_offsets() {
        // offset start with 0
        line_offsets = {0};
        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] != '\n') continue;
            // the first char of next line
            line_offsets.push_back(i+1);
        }
    }
    [[nodiscard]] inline constexpr size_t length() const noexcept {
        return content.size();
    }
    [[nodiscard]] inline constexpr Coordinate get_coord(size_t offset) const {
        using Iterator = std::vector<size_t>::const_iterator;
        if (offset >= this->length()) [[unlikely]] {
            throw std::out_of_range("SrcFile offset out of range");
        }
        Iterator it = std::upper_bound(line_offsets.begin(), line_offsets.end(), offset);
        if (it == line_offsets.begin()) [[unlikely]] {
            throw std::invalid_argument("SrcFile line_offsets is invalid");
        }
        --it; // it that >= offset
        size_t col = offset - (*it) + 1;
        size_t row = std::distance(line_offsets.begin(), it) + 1;
        return Coordinate(row, col);
    }
    [[nodiscard]] inline constexpr std::string_view get_line(size_t line) const {
        if (line == 0 || line > line_offsets.size()) [[unlikely]] {
            throw std::out_of_range("SrcFile line number out of range");
        }
        // calculate the offset
        size_t start_offset = line_offsets[line-1];
        size_t end_offset = (line < line_offsets.size() ? line_offsets[line] : content.size());
        // get the string view and strip
        std::string_view line_view(content.data() + start_offset, end_offset - start_offset);
        if (!line_view.empty() && line_view.back() == '\n') line_view.remove_suffix(1);
        if (!line_view.empty() && line_view.back() == '\r') line_view.remove_suffix(1);
        return line_view;
    } 
};

struct SrcCoord {
    size_t file_id;
    size_t row, col;
    SrcManager* srcman;

    // constructor
    inline explicit SrcCoord(const SrcLoc& srcloc) noexcept
    : file_id(srcloc.file_id), srcman(srcloc.srcman) {
        Coordinate coord = this->file_ptr()->get_coord(srcloc.offset);
        row = coord.row; 
        col = coord.col;
    }

    // function 
    [[nodiscard]] SrcFile* file_ptr();
    [[nodiscard]] const SrcFile* file_ptr() const;
    [[nodiscard]] std::string_view filepath() const;

    // operator
    friend inline std::ostream& operator << (std::ostream& os, const SrcCoord& coord) {
        return (os << coord.filepath() << ':' << coord.row << ':' << coord.col);
    } 
};

class SrcManager {
private:
    // index => id, value => filepath
    std::deque<std::string> paths;
    std::deque<SrcFile>     files;

    // function
    inline void check_index(size_t index) const {
        if (index < this->size()) [[likely]] return;
        throw std::out_of_range("SrcFile id out of range");
    }

public:
    [[nodiscard]] inline size_t size() const noexcept {
        return paths.size();
    }
    [[nodiscard]] inline std::string_view get_path(size_t id) const {
        this->check_index(id);
        return paths[id];
    }
    [[nodiscard]] inline const SrcFile& get_file(size_t id) const {
        this->check_index(id);
        return files[id];
    }
    [[nodiscard]] inline SrcFile& get_file(size_t id) {
        this->check_index(id);
        return files[id];
    }
    [[nodiscard]] inline size_t add_file(const std::string& path, const std::string& content) {
        size_t id = this->size();
        files.emplace_back(this, id, content);
        paths.push_back(path);
        return id;
    }
    [[nodiscard]] inline bool load_from(const std::string& filepath) {
        // try to read the file
        std::ifstream ifs(filepath.c_str());
        if (!ifs) [[unlikely]] return false;
        std::ostringstream ss;
        ss << ifs.rdbuf();
        // add it to file & path list
        size_t id = this->size();
        paths.push_back(filepath);
        files.emplace_back(this, id, ss.str());
        return true;
    }
};

// SrcLoc
inline SrcFile* SrcLoc::file_ptr() {
    return &(srcman->get_file(file_id));
}
inline const SrcFile* SrcLoc::file_ptr() const {
    return &(srcman->get_file(file_id));
}
inline std::string_view SrcLoc::filepath() const {
    return srcman->get_path(file_id);
}

// SrcCoord
inline SrcFile* SrcCoord::file_ptr() {
    return &(srcman->get_file(file_id));
}
inline const SrcFile* SrcCoord::file_ptr() const {
    return &(srcman->get_file(file_id));
}
inline std::string_view SrcCoord::filepath() const {
    return srcman->get_path(file_id);
}

#endif
