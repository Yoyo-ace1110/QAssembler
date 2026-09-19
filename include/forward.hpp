#ifndef YOYO_ACE1110_QASSEMBLER_FORWARD_HPP
#define YOYO_ACE1110_QASSEMBLER_FORWARD_HPP

#include<array>
#include<deque>
#include<string>
#include<vector>
#include<memory>
#include<limits>
#include<fstream>
#include<cstring>
#include<cstdint>
#include<cstddef>
#include<iomanip>
#include<utility>
#include<charconv>
#include<iostream>
#include<optional>
#include<stdexcept>
#include<algorithm>
#include<filesystem>
#include<string_view>

using size_t = std::size_t;
static const size_t npos = std::string::npos;
// static const size_t ptr_size = sizeof(void*);

// declare forwarding
struct [[nodiscard]] SrcLoc;
struct [[nodiscard]] SrcFile;
struct [[nodiscard]] SrcCoord;
class  [[nodiscard]] SrcManager;
struct [[nodiscard]] Token;
struct [[nodiscard]] Error;
struct [[nodiscard]] ErrorManager;
struct [[nodiscard]] TokenManager;
class  [[nodiscard]] Preprocessor;
class  [[nodiscard]] Lexer;

// declare pointers and assign as nullptr
SrcManager*   srcman = nullptr; 
ErrorManager* errman = nullptr;
TokenManager* tokman = nullptr;

// utilitys
struct [[nodiscard]] Coordinate {
    // starts with 1
    size_t row = npos;
    size_t col = npos;
};

template <typename... Args>
[[nodiscard]] inline std::string to_str(const Args&... args) {
    static_assert(
        (std::is_convertible<Args, std::string_view>::value && ...), 
        "All arguments must be convertible to std::string_view"
    );
    size_t total_length = (std::string_view(args).size() + ... + 0);
    std::string result;
    result.reserve(total_length);
    (result.append(std::string_view(args)), ...);
    return result;
}

template <typename T>
inline void print_vector(const std::vector<T>& arr) {
    if (arr.empty()) return;
    for (size_t i = 0; i < arr.size(); ++i) {
        std::cout << arr[i] << "\n";
    }
}

[[nodiscard]] inline std::filesystem::path get_current_dir() {
    return std::filesystem::current_path();
}

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <limits.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#endif
[[nodiscard]] inline std::filesystem::path get_program_dir() {
    #if defined(_WIN32)
        wchar_t path[MAX_PATH] = {0};
        GetModuleFileNameW(NULL, path, MAX_PATH);
        return std::filesystem::path(path).parent_path();
    #elif defined(__linux__)
        char path[PATH_MAX] = {0};
        ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
        if (count != -1) return std::filesystem::path(std::string(path, count)).parent_path();
    #elif defined(__APPLE__)
        char path[PATH_MAX] = {0};
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            return std::filesystem::canonical(std::filesystem::path(path)).parent_path();
        }
    #else
        return std::filesystem::current_path(); // Fallback
    #endif
}

[[nodiscard]] inline bool convert_sv_to_int64(std::string_view sv, std::int64_t& val) {
    // return whether it was overflow
    std::from_chars_result result = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    // convert successfully
    if      (result.ec == std::errc{}) { return false; }
    else if (result.ec == std::errc::result_out_of_range) { return true; }
    // invalid std::string_view
    else throw std::invalid_argument("invalid std::string_view to convert to std::int64_t");
}

#endif
