// version: OPENQASM 2.0
// Only U and CX are built-in gates
// Third-party library: see NOTICE file

#include<array>
#include<deque>
#include<string>
#include<vector>
#include<memory>
#include<limits>
#include<fstream>
#include<cstdint>
#include<iomanip>
#include<utility>
#include<charconv>
#include<iostream>
#include<optional>
#include<stdexcept>
#include<filesystem>
#include<string_view>

namespace {
    using size_t = std::size_t;
    static const size_t npos = std::string::npos;
    using context_type = std::vector<std::string>;

    template <typename... Args>
    inline std::string str(const Args&... args) {
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

    inline std::filesystem::path get_current_dir() {
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
    std::filesystem::path get_program_dir() {
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

    inline bool convert_sv_to_int64(std::string_view sv, std::int64_t& val) {
        // return whether it was overflow
        std::from_chars_result result = std::from_chars(sv.data(), sv.data() + sv.size(), val);
        // convert successfully
        if      (result.ec == std::errc{}) { return false; }
        else if (result.ec == std::errc::result_out_of_range) { return true; }
        // invalid std::string_view
        else throw std::invalid_argument("invalid std::string_view to convert to std::int64_t");
    }
}

class Range {
public:
    // [begin, end)
    size_t begin, end;
    // ----- constructor -----
    inline Range(size_t begin_, size_t end_) : begin(begin_), end(end_) {}
    inline Range(const std::string& string) : begin(0), end(string.length()) {}
    inline Range(const Range& other) : begin(other.begin), end(other.end) {}
    // ----- assignment operator -----
    inline Range& operator = (const Range& other) {
        begin = other.begin;
        end = other.end;
        return (*this);
    }
    // ----- comparison operator -----
    friend inline bool operator == (const Range& a, const Range& b) {
        return (a.begin == b.begin && a.end == b.end);
    }
    friend inline bool operator != (const Range& a, const Range& b) {
        return !(a == b);
    }
    // ----- function -----
    inline size_t length() const {
        return (end > begin) ? (end - begin) : 0;
    }
};

class Token {
public:
    enum class Kind : std::uint8_t {
        Invalid,

        // keywords
        OpenQASM,
        Include,
        If,
        QReg,
        CReg,
        Gate,
        Reset,
        Opaque,
        Measure,
        Barrier,

        // built-in
        Const_pi,   // pi
        Func_ln,    // ln
        Func_sin,   // sin
        Func_cos,   // cos
        Func_tan,   // tan
        Func_exp,   // exp
        Func_sqrt,  // sqrt

        // operators
        Plus,        // +
        Minus,       // -
        Times,       // *
        Devide,      // /
        Power,       // ^

        // symbols
        Arrow,          // ->
        Equal,          // ==
        Comma,          // ,
        Semicolon,      // ;
        LeftBracket,    // [
        RightBracket,   // ]
        LeftBrace,      // {
        RightBrace,     // }
        LeftParen,      // (
        RightParen,     // )

        // dynamic
        nnInteger,
        Identifier,
        RealNumber,
        StringLiteral,
    };
    // member
    size_t line = npos;
    std::string_view text;
    Kind kind = Kind::Invalid;
    // constructor
    inline Token() {}
    inline Token(size_t line_, std::string_view text_, Kind kind_) noexcept
        : line(line_), text(text_), kind(kind_) {}
    inline Token(const Token& other) : line(other.line), text(other.text), kind(other.kind) {}
    // function
    inline bool is_valid() const {
        return (kind != Kind::Invalid);
    } 
    inline size_t length() const {
        return text.size();
    }
    inline bool sep_by_space() const {
        return (
            kind == Kind::OpenQASM      || 
            kind == Kind::Include       || 
            kind == Kind::If            || 
            kind == Kind::QReg          || 
            kind == Kind::CReg          || 
            kind == Kind::Reset         || 
            kind == Kind::Opaque        || 
            kind == Kind::Measure       || 
            kind == Kind::Barrier       || 
            kind == Kind::Arrow         || 
            kind == Kind::Identifier    || 
            kind == Kind::StringLiteral
        );
    }
    // assignment operator
    inline Token& operator = (const Token& other) {
        line = other.line;
        text = other.text;
        kind = other.kind;
        return (*this);
    }
    // ostream operator
    friend std::ostream& operator<< (std::ostream& os, const Token& token) {
        return (os << "In line " << token.line << ": \"" << token.text << '"');
    }
};

class Error {
public:
    // member
    Range lines{0, 1}; // lines here starts with 1
    std::string message;
    std::string_view text;
    using pointer_type = std::shared_ptr<context_type>;
    inline static pointer_type context_ptr = nullptr;
    // constructor
    inline Error(Range lines_, const std::string& msg, std::string_view text_) 
        : lines(lines_), message(msg), text(text_) {}
    inline Error(const Token& token, const std::string& msg) : message(msg), text(token.text) {
        // single line token
        if (token.kind != Token::Kind::StringLiteral) {
            lines.begin = token.line;
            lines.end   = token.line + 1;
            return;
        }
        // multiple lines (string literal only)
        size_t index = 0;
        size_t lines_count = 1;
        // find until nothing
        while (true) {
            index = token.text.find('\n', index);
            // \n not found
            if (index == npos) break;
            ++lines_count;
            ++index;
            // reach the end of text
            if (index == token.text.size()) break;
        }
        lines.begin = token.line;
        lines.end   = token.line + lines_count;
    }
    // ostream support
    friend inline std::ostream& operator<< (std::ostream& os, const Error& error) {
        if (!context_ptr) throw std::invalid_argument("invalid context");
        std::ios_base::fmtflags f(os.flags());
        // single line
        if (error.lines.length() == 1) [[likely]] {
            os << "In line " << error.lines.begin << ": \n";
            os << '\t' << std::setw(4) << error.lines.begin << " | " << (*error.context_ptr)[error.lines.end-1] << '\n';
        } 
        // multiple lines
        else {
            size_t index = error.lines.begin;
            size_t max_line = error.lines.end - 1;
            int width = (max_line > 9999) ? 6 : 4;
            os << "In lines " << error.lines.begin << '-' << max_line << ": \n";
            while (index < error.lines.end) {
                os << "\t" << std::setw(width) << index << " | " << (*error.context_ptr)[index] << '\n';
                ++index;
            }
        }
        os << "Error: " << error.message << "\n" << error.text << '\n';
        os.flags(f);
        return os;
    }
};

class TextProcessor {
protected:
    inline Range _default_range() const {
        return Range(0, text.size());
    }
public:
    std::string text;
    // ----- constructor -----
    inline TextProcessor(std::string text_) : text(text_) {}
    inline TextProcessor(const TextProcessor& other) : text(other.text) {}
    // ----- assignment operator -----
    inline TextProcessor& operator = (const TextProcessor& other) {
        text = other.text;
        return (*this);
    }
    // ----- function -----
    inline void clear() {text.clear();}
    inline bool empty() const {return text.empty();}
    inline void ensure(const Range& range) const {
        if ((range.begin > range.end) || (range.end > text.length())) {
            throw std::invalid_argument("Invalid range object received!");
        }
    }
    inline void erase(const Range& range) {
        this->ensure(range);
        text.erase(range.begin, range.length());
    }
    inline void insert(size_t pos, const std::string& str) {
        text.insert(pos, str);
    }
    
    inline void replace(const Range& range, const std::string& str) {
        this->ensure(range);
        text.replace(range.begin, range.length(), str);
    }
    inline void replace_all(const std::string& from, const std::string& to) {
        this->replace_all(from, to, this->_default_range());
    }
    inline void replace_all(const std::string& from, const std::string& to, Range range) {
        this->ensure(range);
        if (from.empty()) return;
        while (range.begin < range.end) {
            size_t pos = text.find(from, range.begin);
            if (pos == std::string::npos || pos + from.length() > range.end) break;
            text.replace(pos, from.length(), to);
            if (to.length() >= from.length()) { range.end += (to.length() - from.length()); } 
            else                              { range.end -= (from.length() - to.length()); }
            range.begin = pos + to.length();
        }
    }
    
    inline size_t find(const std::string& str) const {
        return this->find(str, this->_default_range());
    }
    inline size_t find(const std::string& str, size_t pos) const {
        if (str.empty() || pos >= text.size()) return npos;
        return text.find(str, pos);
    }
    inline size_t find(const std::string& str, const Range& range) const {
        if (str.empty()) return npos;
        size_t pos = text.find(str, range.begin);
        if (pos != npos && pos+str.length() <= range.end) {
            return pos;
        }
        return npos;
    }
    
    inline std::vector<std::string> split_by(const std::string& sep) const {
        return this->split_by(sep, _default_range());
    }
    inline std::vector<std::string> split_by(const std::string& sep, const Range& range) const {
        this->ensure(range);
        std::vector<std::string> result;
        if (this->empty() || range.length() == 0) return result;
        // separator is nothing
        if (sep.empty()) [[unlikely]] {
            result.push_back(text.substr(range.begin, range.length()));
            return result;
        }
        // go through the text
        size_t start = range.begin;
        size_t end = range.begin + range.length();
        size_t sep_len = sep.length();
        size_t i = start;
        while (i < end) {
            // match the separator
            if (i + sep_len <= end && text.compare(i, sep_len, sep) == 0) {
                result.push_back(text.substr(start, i - start));
                i += sep_len;
                start = i;
            } else {++i;}
        }
        // the last part after all separators
        if (start <= end) result.push_back(text.substr(start, end - start));
        return result;
    }
};

class Preprocessor : public TextProcessor {
public:
    // ----- function -----
    inline void erase_comments() {
        // this won't change the line number
        size_t comment_pos, end_of_line;
        while (true) {
            comment_pos = this->find("//", comment_pos);
            end_of_line = this->find("\n", comment_pos);
            if (comment_pos == npos) break;
            if (end_of_line == npos) text.erase(comment_pos);
            // erase the comment line without '\n'
            else this->erase(Range(comment_pos++, end_of_line));
        }
    }
    inline void standardize_EOL() {
        // replace all EOL to "\n"
        this->replace_all("\r\n", "\n");
        this->replace_all("\r"  , "\n");
    }
    inline void standardize_space() {
        // replace all spaces to " "
        this->replace_all("\t", " ");
    }
};

class Tokenizer {
public:
    // ----- member -----
    std::string_view text;
    std::vector<Token> tokens;
    std::vector<Error> errors;
    // this container will not reallocate when you call push_back()
    // it will allocate a new chuck and append the element in
    // it store the pointer of every chuck instead of element
    std::deque<std::string> fixed_str;
private:
    // ----- function -----
    inline bool _is_number(char c) const {
        return (c >= '0' && c <= '9');
    }
    inline void _ensure_index(size_t index) const {
        if (index < text.size()) return;
        throw std::out_of_range("Tokenizer[] index out of range");
    }
    inline bool _has_close_quote(size_t index) const {
        size_t count = 0;
        this->_ensure_index(index);
        if (index == 0) return false;
        if (text[index] != '"') return false;
        --index;
        // count the "\" before quote
        while (true) {
            if (text[index] != '\\') break;
            ++count;
            if (index == 0) break;
            --index;
        }
        // (count % 2 == 0);
        return !(count & 1);
    }
    inline void _add_error(const Token& token, const std::string& str) {
        errors.push_back(Error(token, str));
    }
    
    inline bool _can_be_identifier(char c) const {
        bool is_uppercase = (c >= 'A' && c <= 'Z');
        bool is_lowercase = (c >= 'a' && c <= 'z');
        return (is_uppercase || is_lowercase || this->_is_number(c) || c == '_');
    }
    inline bool _is_end_of_keyword(size_t next_index) const {
        if (next_index >= text.size()) return true;
        return !this->_can_be_identifier(text[next_index]);
    }
    inline std::string _get_substr(size_t index, size_t length) const {
        this->_ensure_index(index);
        return std::string(this->_get_subview(index, length));
    }
    inline std::string_view _get_subview(size_t index, size_t length) const {
        this->_ensure_index(index);
        return text.substr(index, length);
    }
    
    inline Token _match_symbol(size_t line, size_t index) const {
        this->_ensure_index(index);
        std::string_view subview;
        // 2-char symbols
        if (index+1 < text.size()) {
            subview = this->_get_subview(index, 2);
            if (subview == "->") {return Token(line, subview, Token::Kind::Arrow);}
            if (subview == "==") {return Token(line, subview, Token::Kind::Equal);}
        }
        // 1-char symbols
        switch (text[index]) {
            case ',': {return Token(line, this->_get_subview(index, 1), Token::Kind::Comma);}
            case ';': {return Token(line, this->_get_subview(index, 1), Token::Kind::Semicolon);}
            case '[': {return Token(line, this->_get_subview(index, 1), Token::Kind::LeftBracket);}
            case ']': {return Token(line, this->_get_subview(index, 1), Token::Kind::RightBracket);}
            case '{': {return Token(line, this->_get_subview(index, 1), Token::Kind::LeftBrace);}
            case '}': {return Token(line, this->_get_subview(index, 1), Token::Kind::RightBrace);}
            case '(': {return Token(line, this->_get_subview(index, 1), Token::Kind::LeftParen);}
            case ')': {return Token(line, this->_get_subview(index, 1), Token::Kind::RightParen);}
            case '+': {return Token(line, this->_get_subview(index, 1), Token::Kind::Plus);}
            case '-': {return Token(line, this->_get_subview(index, 1), Token::Kind::Minus);}
            case '*': {return Token(line, this->_get_subview(index, 1), Token::Kind::Times);}
            case '/': {return Token(line, this->_get_subview(index, 1), Token::Kind::Devide);}
            case '^': {return Token(line, this->_get_subview(index, 1), Token::Kind::Power);}
            default : break;
        }
        // default
        return Token(line, subview, Token::Kind::Invalid);
    }
    inline Token _match_keyword(size_t line, size_t index) const {
        this->_ensure_index(index);
        std::string_view subview;
        Token token(line, subview, Token::Kind::Invalid);
        const size_t remaining_length = text.size()-index;
        // lambda
        auto match = [&] (std::string_view keyword, Token::Kind kind) -> bool {
            size_t length = keyword.size();
            subview = this->_get_subview(index, length);
            if (subview == keyword && this->_is_end_of_keyword(index+length)) {
                token = Token(line, subview, kind);
                return true;
            }
            return false;
        };
        // keywords, must be ordered by length
        if (remaining_length < 2) return token;
        if (match("if", Token::Kind::If))            return token;
        if (match("ln", Token::Kind::Func_ln))        return token;
        if (match("pi", Token::Kind::Const_pi))       return token;
        if (remaining_length < 3) return token;
        if (match("sin", Token::Kind::Func_sin))      return token;
        if (match("cos", Token::Kind::Func_cos))      return token;
        if (match("tan", Token::Kind::Func_tan))      return token;
        if (match("exp", Token::Kind::Func_exp))      return token;
        if (remaining_length < 4) return token;
        if (match("qreg", Token::Kind::QReg))         return token;
        if (match("creg", Token::Kind::CReg))         return token;
        if (match("gate", Token::Kind::Gate))         return token;
        if (match("sqrt", Token::Kind::Func_sqrt))    return token;
        if (remaining_length < 5) return token;
        if (match("reset", Token::Kind::Reset))       return token;
        if (remaining_length < 6) return token;
        if (match("opaque", Token::Kind::Opaque))     return token;
        if (remaining_length < 7) return token;
        if (match("include", Token::Kind::Include))   return token;
        if (match("measure", Token::Kind::Measure))   return token;
        if (match("barrier", Token::Kind::Barrier))   return token;
        if (remaining_length < 8) return token;
        if (match("OPENQASM", Token::Kind::OpenQASM)) return token;
        return token;
    }
    inline void _add_dynamic_token_or_fix_error(size_t line, size_t index, size_t length) {
        const size_t end = index+length-1;
        this->_ensure_index(end);
        std::string string_buffer;
        std::string_view subview = this->_get_subview(index, length);
        Token token(line, subview, Token::Kind::Invalid);
        // string literal
        if (text[index] == '"') {
            if (length < 2 || !this->_has_close_quote(end)) {
                // Error: Unclosed quote
                this->_add_error(token, "Unclosed quote");
                // Fix: close quote
                fixed_str.push_back(str(token.text, "\""));
                token.text = std::string_view(fixed_str.back());
                // strip the quotes
                token.text.remove_prefix(1);
                token.text.remove_suffix(1);
            }
            // string literal
            token.kind = Token::Kind::StringLiteral;
            // strip the quotes
            token.text.remove_prefix(1);
            token.text.remove_suffix(1);
            tokens.push_back(token);
            return;
        }
        // real number & interger
        else if (this->_is_number(text[index]) || text[index] == '.') {
            string_buffer += text[index];
            bool has_exponent = false, has_decimal_point = false;
            for (size_t i = index+1; i < index+length; ++i) {
                if (this->_is_number(text[i])) {
                    string_buffer += text[i];
                    continue;
                }
                else if (text[i] == '.') {
                    if (has_exponent) {
                        this->_add_error(token, "decimal point must not follow Exponent");
                        // Fix: skip this character
                        continue;
                    }
                    if (has_decimal_point) {
                        // Error: More than one decimal points
                        this->_add_error(token, "More than one decimal points");
                        // Fix: skip this character
                        continue;
                    }
                    // decimal point must be adjacent to a number
                    bool last_char_is_number = ((i > 0)   && this->_is_number(text[i-1]));
                    bool next_char_is_number = ((i < end) && this->_is_number(text[i+1]));
                    if (!(last_char_is_number || next_char_is_number)) {
                        // Error: decimal point must be adjacent to a number
                        this->_add_error(token, "decimal point must be adjacent to a number");
                        // Fix: add a zero after it
                        has_decimal_point = true;
                        string_buffer += text[i];
                        string_buffer += '0';
                        continue;
                    }
                    string_buffer += text[i];
                    has_decimal_point = true;
                    continue;
                }
                else if (text[i] == 'e' || text[i] == 'E') {
                    if (has_exponent) {
                        // Error: More than one exponent
                        this->_add_error(token, "More than one exponent");
                        // Fix: skip this character
                        continue;
                    }
                    has_exponent = true;
                    string_buffer += text[i];
                    // skip "+" or "-"
                    if (i < end && (text[i+1] == '+' || text[i+1] == '-')) {
                        string_buffer += text[++i];
                    }
                    continue;
                }
                else {
                    // Error: Invalid character
                    this->_add_error(token, "Invalid character");
                    // Fix: skip this character
                    continue;
                }
            }
            // text[end] cannot be exponent 
            if (text[end] == 'e' || text[end] == 'E') {
                // Error: Exponent connot be the last character
                this->_add_error(token, "Exponent connot be the last character");
                // Fix: delete this character
                string_buffer.pop_back();
            }
            // distinguish nnInteger & real number
            token.kind = (
                (has_decimal_point || has_exponent) ? 
                Token::Kind::RealNumber : 
                Token::Kind::nnInteger
            );
            if (string_buffer != subview) {
                fixed_str.push_back(string_buffer);
                token.text = fixed_str.back();
            }
            tokens.push_back(token);
            return;
        }
        // identifier (this must be placed after numbers)
        else if (this->_can_be_identifier(text[index])) {
            string_buffer += text[index];
            for (size_t i = index+1; i < index+length; ++i) {
                // got invalid character
                if (!this->_can_be_identifier(text[i])) {
                    // Error: Invalid character
                    this->_add_error(token, "Invalid character");
                    // Fix: skip this character
                    continue;
                }
                string_buffer += text[i];
            }
            // Identifier
            if (string_buffer != subview) {
                fixed_str.push_back(string_buffer);
                token.text = fixed_str.back();
            }
            token.kind = Token::Kind::Identifier;
            tokens.push_back(token);
            return;
        }
        // Error: Cannot identify this token
        else { this->_add_error(token, "Cannot identify this token"); }
    }
public:
    // ----- constructor -----
    inline Tokenizer() = default;
    inline Tokenizer(std::string_view sv) : text(sv) {}
    inline Tokenizer(const Tokenizer& other) : text(other.text) {}
    // ----- function -----
    inline void tokenize() {
        // initialize
        size_t length;
        size_t line = 1;
        size_t start = 0;
        size_t index = 0;
        fixed_str.clear();
        Token token, dynamic;
        bool is_in_quote = false;
        // skip empty text
        if (text.empty()) {
            tokens.clear();
            return;
        }
        tokens.reserve(text.size()/4);
        // tokenize loop
        while (index < text.size()) {
            length = index-start+1;
            // match quote
            if (is_in_quote) {
                // quote closed
                if (this->_has_close_quote(index)) {
                    this->_add_dynamic_token_or_fix_error(line, start, length);
                    is_in_quote = false;
                    start = (++index);
                    continue;
                }
                // keep going
                (++index);
                continue;
            }
            // match dynamic
            if (start != index) {
                token = this->_match_symbol(line, index);
                // only these four condition will end up this dynamic token
                if (text[index] == ' ' || text[index] == '\n' || text[index] == '"' || token.is_valid()) {
                    this->_add_dynamic_token_or_fix_error(line, start, length-1);
                    start = index;
                }
                // otherwise this dynamic token still extend
                else {
                    ++index;
                    continue;
                }
            }
            // quote opened
            if (text[index] == '"') {
                is_in_quote = true;
                start = (index++);
                continue;
            }
            // match useless spaces
            if (text[index] == ' ' || text[index] == '\n') {
                if (text[index] == '\n') ++line;
                start = (++index);
                continue;
            }
            // match symbols
            token = this->_match_symbol(line, index);
            if (token.is_valid()) {
                tokens.push_back(token);
                index += token.length();
                start = index;
                continue;
            }
            // match keywords
            token = this->_match_keyword(line, index);
            if (token.is_valid()) {
                tokens.push_back(token);
                index += token.length();
                start = index;
                continue;
            }
            // new dynamic token starts
            start = (index++);
        }
        // the last token
        if (start != index) this->_add_dynamic_token_or_fix_error(line, start, length);
    }
};

struct Statement {
private:
    inline static constexpr std::array<std::string_view, 9> kind_map = {
        "OPENQASM", 
        "QRegDecl", 
        "CRegDecl", 
        "GateCall", 
        "Include" , 
        "Measure" ,
        "Barrier" ,
        "Opaque"  ,
        "Reset"   ,
    };
public:
    enum class Kind : std::uint8_t {
        OpenQASM = 0,
        QRegDecl = 1,
        CRegDecl = 2,
        GateCall = 3,
        Include  = 4, 
        Measure  = 5, 
        Barrier  = 6, 
        Opaque   = 7, 
        Reset    = 8, 
    };
    // always endswith ';' => omit it
    Kind kind;
    std::vector<Token> leafs;
    // function
    inline void add_tokens(const std::vector<Token>& target, Range range) {
        leafs.insert(leafs.end(), target.begin()+range.begin, target.begin()+range.end);
    }
    // ostream support
    friend inline std::ostream& operator<< (std::ostream& os, const Statement& statement) {
        // Print the Type and the Kind
        size_t kind_index = static_cast<size_t>(statement.kind);
        std::string_view kind_sv = std::string_view("Unknown");
        if (kind_index < Statement::kind_map.size()) {
            kind_sv = Statement::kind_map[kind_index];
        }
        os << "\tStatement(Kind=" << kind_sv << ") { ";
        bool is_first_token = true;
        // Print the tokens
        for (const Token& token : statement.leafs) {
            if (is_first_token) {
                is_first_token = false;
            } else if (token.sep_by_space()) {
                os << ' ';
            }
            os << token.text;
        }
        // Close the brace
        return (os << " }");
    }
};

struct Expression {
private:
    inline static constexpr std::array<std::string_view, 4> kind_map = {
        "Variable", 
        "BinaryOp", 
        "UnaryOp" , 
        "Literal" , 
    };
public:
    enum class Kind : std::uint8_t {
        Variable,
        BinaryOp,
        UnaryOp, 
        Literal
    };
    Kind kind;
    std::vector<Token> leafs;
    // ostream support
    friend inline std::ostream& operator<< (std::ostream& os, const Expression& expression) {
        // Print the Type and the Kind
        size_t kind_index = static_cast<size_t>(expression.kind);
        std::string_view kind_sv = std::string_view("Unknown");
        if (kind_index < Expression::kind_map.size()) {
            kind_sv = Expression::kind_map[kind_index];
        }
        os << "\tExpression(Kind=" << kind_sv << ") { ";
        bool is_first_token = true;
        // Print the tokens
        for (const Token& token : expression.leafs) {
            if (is_first_token) {
                is_first_token = false;
            } else if (token.sep_by_space()) {
                os << ' ';
            }
        }
        // Close the brace
        return (os << " }");
    }
};

struct IfCondition {
    // if (creg==val) qop
    size_t val;
    Token creg;
    Statement statemant;
    // ostream support
    friend inline std::ostream& operator<< (std::ostream& os, const IfCondition& if_cond) {
        // Print the condition
        os << "\tIfCondition { if (" << if_cond.creg.text << "==" << if_cond.val << ") ";
        // Print the statement
        return (os << if_cond.statemant << " }");
    }
};

struct GateDeclaration {
    // name (cparams) qparams
    Token name;
    std::vector<Token> cparams;
    std::vector<Token> qparams;
    std::vector<Statement> body;
    // ostream support
    friend inline std::ostream& operator<< (std::ostream& os, const GateDeclaration& gate_decl) {
        bool is_first_token = true;
        os << "\tGateDeclaration {\n\t\t";
        // Print the name of gate and classical params
        os << "gate " << gate_decl.name.text << '(';
        for (const Token& token : gate_decl.cparams) {
            if (!is_first_token) { (os << ", "); }
            else { is_first_token = false; }
            os << token.text;
        }
        os << ") {";
        // Print the body inside
        for (const Statement& statement : gate_decl.body) {
            os << "\n\t\t\t" << statement;
        }
        os << "\n\t\t}" << "\n\t}";
        return os;
    }
};

class AST {
public:
    struct Node {
        enum class Kind : std::uint8_t {
            VecStatement, 
            VecIfCondition, 
            VecGateDeclaration, 
        };
        Kind kind;
        size_t index;
        inline Node() = default;
        inline Node(Kind kind_, size_t index_) : kind(kind_), index(index_) {}
    };
    // ----- member -----
    std::vector<Statement>          statements;
    std::vector<IfCondition>        if_conditions;
    std::vector<GateDeclaration>    gate_declarations;
    std::vector<Node> node_handler; // handle level1 nodes
    // ----- function -----
    inline size_t size() const noexcept {
        return node_handler.size();
    } 
    inline void push_back(const Statement& statement) {
        Node node(Node::Kind::VecStatement, statements.size());
        statements.push_back(statement);
        node_handler.push_back(node);
    }
    inline void push_back(const IfCondition& if_condition) {
        Node node(Node::Kind::VecIfCondition, if_conditions.size());
        if_conditions.push_back(if_condition);
        node_handler.push_back(node);
    }
    inline void push_back(const GateDeclaration& gate_declaration) {
        Node node(Node::Kind::VecGateDeclaration, gate_declarations.size());
        gate_declarations.push_back(gate_declaration);
        node_handler.push_back(node);
    }
    // ------ operator -----
    inline Node& operator[](size_t index) {
        if (index >= node_handler.size()) {
            std::string msg = "AST::operator[] index out of range: ";
            throw std::out_of_range(str(msg, index));
        }
        return node_handler[index];
    }
    inline const Node& operator[](size_t index) const {
        if (index >= node_handler.size()) {
            std::string msg = "AST::operator[] index out of range: ";
            throw std::out_of_range(str(msg, index));
        }
        return node_handler[index];
    }
    friend inline std::ostream& operator<< (std::ostream& os, const AST& ast) {
        os << "AST {\n";
        using NodeKind = AST::Node::Kind;
        for (int i = 0; i < ast.size(); ++i) {
            const AST::Node& node = ast[i];
            if        (node.kind == NodeKind::VecStatement) {
                os << ast.statements[node.index] << '\n';
            } else if (node.kind == NodeKind::VecIfCondition) {
                os << ast.if_conditions[node.index] << '\n';
            } else if (node.kind == NodeKind::VecGateDeclaration) {
                os << ast.gate_declarations[node.index] << '\n';
            } else throw std::invalid_argument("Unknown AST::Node::Kind");
        }
        return (os << '}');
    }
};

class Parser {
public:
    // ----- member -----
    AST ast;
    std::vector<Token> tokens;
    std::vector<Error> errors;
    std::deque<std::string> fixed_str;
    
    // ----- constructor -----
    inline Parser(Tokenizer& tk) 
      : tokens(std::move(tk.tokens)), 
        errors(std::move(tk.errors)),
        fixed_str(std::move(tk.fixed_str)) {}
private:
    // ----- structure -----
    template<typename T>
    struct ParsedObject {
        size_t index = npos;
        std::optional<T> node_opt = std::nullopt;
        inline ParsedObject() noexcept = default;
        inline ParsedObject(size_t i, std::optional<T> ptr) : index(i), node_opt(ptr) {}
    };
    
    // ----- function ----- 
    template <typename... Args>
    inline void _add_error(size_t index, const Args&... args) {
        errors.push_back(Error(tokens[index], str(args...)));
    }
    inline void _add_error(const Token& token, const std::string& str) {
        errors.push_back(Error(token, str));
    }
    inline void _ensure_index(size_t index) const {
        if (index < tokens.size()) return;
        throw std::out_of_range("Parser::tokens index out of range");
    }
    
    inline void _attach_token(size_t index, Token::Kind kind, const std::string& str) {
        if (index >= tokens.size()) return;
        fixed_str.push_back(str);
        size_t line = (index == 0 ? 1 : tokens[index-1].line);
        std::string_view sv = std::string_view(fixed_str.back());
        tokens.insert(tokens.begin()+index, Token(line, sv, kind));
    }
    inline bool _check_token_kind(size_t index, Token::Kind kind) noexcept {
        return (index < tokens.size() && tokens[index].kind == kind);
    }
    inline bool _ensure_token_exist(size_t index, const std::string& expected, Token::Kind kind) {
        // token: the token before expected
        // expected: expected token type
        // next: next_token.text
        if (!this->_check_token_kind(index, kind)) {
            this->_add_error(index, "Expected ", expected, " before next token or EOF");
            return false;
        }
        return true;
    }
    inline void _ensure_or_attach(size_t& current, const std::string& expected, Token::Kind kind, const std::string& str) {
        if (current != std::numeric_limits<size_t>::max()) { ++current; }
        if (this->_ensure_token_exist(current-1, expected, kind)) return; 
        this->_attach_token(current-1, kind, str);
    }
    
    inline std::optional<Statement>  _parse_qop(size_t& current) {
        // Update current and parse qop if there is one
        // else Do not update current, and then return false
        if (!this->_check_token_kind(current, Token::Kind::Identifier)) return {std::nullopt};
        // prepare
        Statement statement;
        size_t index = current;
        // gate_name (classical_param) qubit_param;
        this->_parse_classical_param(current);
        this->_parse_qubit_param(current);
        this->_ensure_or_attach(current, "\";\"", Token::Kind::Semicolon, ";");
        statement.add_tokens(tokens, Range(index, current));
        return {statement};
    }
    inline std::optional<Expression> _parse_expression(size_t& current) {// TODO:
        // Update current and parse expression if there is one
        // else Do not update current, and then return false
    }
    inline void _parse_qubit_param(size_t& current) {
        if (current >= tokens.size()) return;
        // make sure the first qubit exist
        this->_ensure_or_attach(current, "the name of qreg", Token::Kind::Identifier, "demmy qreg");
        this->_ensure_or_attach(current, "\"[\"", Token::Kind::LeftBracket, "[");
        this->_ensure_or_attach(current, "the index of qubit", Token::Kind::nnInteger, "0");
        this->_ensure_or_attach(current, "\"]\"", Token::Kind::RightBracket, "]");
        // parse more qubit if there are some
        size_t last_intact_qubit = current;
        bool comma_exist, qreg_exist, left_exist, index_exist, right_exist;
        while (true) {
            // ..., qreg[index]
            comma_exist = this->_check_token_kind(current++, Token::Kind::Comma       );
            qreg_exist  = this->_check_token_kind(current++, Token::Kind::Identifier  );
            left_exist  = this->_check_token_kind(current++, Token::Kind::LeftBracket );
            index_exist = this->_check_token_kind(current++, Token::Kind::nnInteger   );
            right_exist = this->_check_token_kind(current++, Token::Kind::RightBracket);
            // successfully get an intact qubit, eat it
            if (comma_exist && qreg_exist && left_exist && index_exist && right_exist) {
                last_intact_qubit = current;
                continue;
            }
            // something loss, stop parsing and left things behind there
            current = last_intact_qubit;
            return;
        }
    }
    inline void _parse_classical_param(size_t& current) {
        if (current >= tokens.size()) return;
        // no classical parameter
        if (!this->_check_token_kind(current, Token::Kind::LeftParen)) return;
        // make sure the first param exist, otherwise () is redundant
        if (!this->_parse_expression(++current).has_value()) {
            // Error: empty parameter list
            this->_add_error(current, "Empty parameter list is redundant");
            this->_ensure_or_attach(current, "\")\"", Token::Kind::RightParen, ")");
            return;
        }
        // parameters separated by ','
        size_t last_intact_param = current;
        bool has_comma, has_exprs;
        while (true) {
            // ..., Identifier
            has_comma = this->_check_token_kind(current++, Token::Kind::Comma);
            has_exprs = this->_parse_expression(current).has_value();
            // successfully parse a param
            if (has_comma && has_exprs) {
                last_intact_param = current;
                continue;
            }
            // something wrong, close Paren and break
            this->_ensure_or_attach(current, "\")\"", Token::Kind::RightParen, ")");
            current = last_intact_param;
            return;
        }
    }
    inline void _parse_qubit_param_decl(size_t& current) {
        if (current >= tokens.size()) return;
        bool has_camma = false;
        while (true) {
            // parse the name of qubit
            if (!this->_check_token_kind(current, Token::Kind::Identifier)) {
                // move back to previous qubit
                if (has_camma) --current;
                // qubit not found
                else {
                    // Error: empty qubit param list
                    this->_add_error(current, "Expect qubit parameter list");
                    // Fix: attach a dummy param 
                    this->_attach_token(current++, Token::Kind::Identifier, "dummy param");
                    // the space make it impossible for user to use this param name
                }
                return;
            }
            // return if there's no camma after qubit
            if (!this->_check_token_kind(current, Token::Kind::Comma)) return;
            ++current;
        }
    }
    inline void _parse_classical_param_decl(size_t& current) {
        if (current >= tokens.size()) return;
        // no classical parameter
        if (!this->_check_token_kind(current, Token::Kind::LeftParen)) return;
        if (!this->_check_token_kind(++current, Token::Kind::Identifier)) {
            // Error: empty parameter list
            this->_add_error(current, "Empty parameter list is redundant");
            this->_ensure_or_attach(current, "\")\"", Token::Kind::RightParen, ")");
            return;
        }
        ++current; // Eat Identifier
        // parameters separated by ','
        bool has_comma, has_idtfr;
        while (true) {
            // ..., Identifier
            has_comma = this->_check_token_kind(current  , Token::Kind::Comma);
            has_idtfr = this->_check_token_kind(current+1, Token::Kind::Identifier);
            // successfully parse a param
            if (has_comma && has_idtfr) {
                current += 2;
                continue;
            }
            // something wrong, close Paren and break
            this->_ensure_or_attach(current, "\")\"", Token::Kind::RightParen, ")");
            return;
        }
    }

    // Fix error automatically and add them to errors
    // If there isn't any valid RETURN_VAL, return {npos, std::nullopt}
    // Scan the RETURN_TYPE after index, return {new index, the RETURN_VAL}
    inline ParsedObject<Statement> _parse_statement(size_t index) {
        // Include path will be converted into abs path
        // If there aren't any param parameter list, attach a "dummy ..." after it
        // If the size of register is not specified or is too large, its size will be set to 0
        this->_ensure_index(index);
        Statement statement;
        size_t current = index+1;
        switch (tokens[index].kind) {
            case Token::Kind::Semicolon : {
                return {current, statement};
            }
            case Token::Kind::OpenQASM  : {
                // ensure version is 2.0
                if (this->_ensure_token_exist(current, "version number", Token::Kind::RealNumber)) {
                    if (tokens[current].text != "2.0") {
                        // Error: Unsupported version
                        std::string msg = str();
                        this->_attach_token(current, Token::Kind::RealNumber, "2.0");
                        this->_add_error(current, "Unsupported version of OPENQASM: ");
                    }
                } 
                else this->_attach_token(current++, Token::Kind::RealNumber, "2.0");
                statement.kind = Statement::Kind::OpenQASM; break;
            }
            case Token::Kind::Include   : {
                // ensure string of header path exist
                if (!this->_ensure_token_exist(current, "header path", Token::Kind::StringLiteral)) {
                    // Fix: skip this Include token
                    return {current, statement};
                }
                // include path: current_dir & program_dir
                std::filesystem::path header_path = "";
                std::filesystem::path raw_filepath = tokens[current].text;
                std::filesystem::path try_program_dir = get_program_dir() / raw_filepath;
                std::filesystem::path try_current_dir = get_current_dir() / raw_filepath;
                if (std::filesystem::exists(try_program_dir)) { header_path = try_program_dir;}
                if (std::filesystem::exists(try_current_dir)) { header_path = try_current_dir;}
                if (std::filesystem::exists(raw_filepath))    { header_path = raw_filepath;   }
                // this file does not exist
                if (header_path.empty()) {
                    // Error: No such file or directory
                    this->_add_error(current, "No such file or directory");
                    // Fix: skip this Include token
                    return {++current, statement};
                }
                // cannot read this file
                if (!std::ifstream(header_path).good()) {
                    // Error: Permission denied
                    this->_add_error(current++, "Permission denied");
                    // Fix: skip this Include token
                    return {current, statement};
                }
                // rewrite the path into abs path
                fixed_str.push_back(header_path.string());
                tokens[current].text = std::string_view(fixed_str.back());
                statement.kind = Statement::Kind::Include; 
                ++current; break;
            }
            case Token::Kind::QReg      : {
                // parse the name of register
                this->_ensure_or_attach(current, "the name of qreg", Token::Kind::Identifier, "dummy qreg decl");
                // the '[' before size of qreg
                this->_ensure_or_attach(current, "\"[\"", Token::Kind::LeftBracket, "[");
                // the size of qreg
                this->_ensure_or_attach(current, "the size of qreg", Token::Kind::nnInteger, "0");
                // go back to checkout the size of register
                --current; std::int64_t register_size;
                bool is_overflow = convert_sv_to_int64(tokens[current].text, register_size);
                if (is_overflow) {
                    // Error: qreg is too big
                    this->_add_error(current, "The size of qreg is too large: ", tokens[current].text);
                    // Fix: let the size zero
                    fixed_str.push_back("0");
                    tokens[current].text = std::string_view(fixed_str.back());
                }
                ++current;
                // the ']' before size of qreg
                this->_ensure_or_attach(current, "\"]\"", Token::Kind::RightBracket, "]");
                statement.kind = Statement::Kind::QRegDecl; break;
            }
            case Token::Kind::CReg      : {
                // parse the name of register
                this->_ensure_or_attach(current, "the name of qreg", Token::Kind::Identifier, "dummy creg decl");
                // the '[' before size of qreg
                this->_ensure_or_attach(current, "\"[\"", Token::Kind::LeftBracket, "[");
                // the size of qreg
                this->_ensure_or_attach(current, "the size of creg", Token::Kind::nnInteger, "0");
                // go back to checkout the size of register
                --current; std::int64_t register_size;
                bool is_overflow = convert_sv_to_int64(tokens[current].text, register_size);
                if (is_overflow) {
                    // Error: qreg is too big
                    this->_add_error(current, "The size of creg is too large: ", tokens[current].text);
                    // Fix: let the size zero
                    fixed_str.push_back("0");
                    tokens[current].text = std::string_view(fixed_str.back());
                }
                ++current;
                // the ']' before size of qreg
                this->_ensure_or_attach(current, "\"]\"", Token::Kind::RightBracket, "]");
                statement.kind = Statement::Kind::CRegDecl; break;
            }
            case Token::Kind::Opaque    : {
                // parse the name of opaque
                this->_ensure_or_attach(current, "the name of opaque", Token::Kind::Identifier, "dummy opaque");
                // parameter declarations
                this->_parse_classical_param_decl(current);
                this->_parse_qubit_param_decl    (current);
                statement.kind = Statement::Kind::Opaque; break;
            }
            case Token::Kind::Reset     : {
                // the Qreg to reset
                this->_ensure_or_attach(current, "a qreg name", Token::Kind::Identifier, "dummy qreg");
                // reset the whole qreg
                if (!this->_check_token_kind(current, Token::Kind::LeftBracket)) {
                    statement.kind = Statement::Kind::Reset; break;
                } 
                // reset single qubit
                else { ++current; }
                // the index of qubit
                this->_ensure_or_attach(current, "the index of qubit", Token::Kind::nnInteger, "0");
                // the "]" after qreg index
                this->_ensure_or_attach(current, "\"]\"", Token::Kind::RightBracket, "]");
                statement.kind = Statement::Kind::Reset; break;
            }
            case Token::Kind::Barrier   : {
                // ``barrier qreg_name`` 
                this->_ensure_or_attach(current, "a qreg name", Token::Kind::Identifier, "dummy qreg");
                // param are qubits, not the whole qreg
                if (this->_check_token_kind(current, Token::Kind::LeftBracket)) {
                    // barrier ``qreg_name[index], ...``
                    this->_parse_qubit_param(--current);
                }
                statement.kind = Statement::Kind::Barrier; break;
            }
            case Token::Kind::Measure   : {
                // ``measure qreg_name`` ...
                this->_ensure_or_attach(current, "a qreg name", Token::Kind::Identifier, "dummy qreg");
                // measure the whole qreg
                if (!this->_check_token_kind(current, Token::Kind::LeftBracket)) {
                    // measure qreg_name ``-> creg_name``
                    this->_ensure_or_attach(current, "\"->\"", Token::Kind::Arrow, "->");
                    this->_ensure_or_attach(current, "a creg name", Token::Kind::Identifier, "dummy creg");
                    statement.kind = Statement::Kind::Measure; break;
                }
                // measure single qubit
                else { ++current; }
                // measure qreg_name ``[index] -> creg_name[index]``
                this->_ensure_or_attach(current, "\"[\"", Token::Kind::LeftBracket, "[");
                this->_ensure_or_attach(current, "the index of qubit", Token::Kind::nnInteger, "0");
                this->_ensure_or_attach(current, "\"]\"", Token::Kind::RightBracket, "]");
                // the arrow "->" between qubit and classical bit 
                this->_ensure_or_attach(current, "\"->\"", Token::Kind::Arrow, "->");
                // the creg to store the result
                this->_ensure_or_attach(current, "a creg name", Token::Kind::Identifier, "dummy creg");
                this->_ensure_or_attach(current, "\"[\"", Token::Kind::LeftBracket, "[");
                this->_ensure_or_attach(current, "the index of classical bit", Token::Kind::nnInteger, "0");
                this->_ensure_or_attach(current, "\"]\"", Token::Kind::RightBracket, "]");
                statement.kind = Statement::Kind::Measure; break;
            }
            case Token::Kind::Identifier: {
                // gate_name (classical_param) qubit_param
                this->_parse_classical_param(current);
                this->_parse_qubit_param(current);
            }
            default:                      {
                std::string msg = "Unknown Token::Kind";
                throw std::invalid_argument(msg);
            }
        }
        // current == index+1 means statement is invalid
        if (current == index+1) return {npos, std::nullopt};
        // assign kind & leafs for statement
        this->_ensure_or_attach(current, "\";\"", Token::Kind::Semicolon, ";");
        statement.add_tokens(tokens, Range(index, current));
        return {current, statement};
    }
    inline ParsedObject<IfCondition> _parse_if_condition(size_t index) {
        this->_ensure_index(index);
        // skip if token is not "if"
        if (tokens[index].kind != Token::Kind::If) return {npos, std::nullopt}; 
        IfCondition if_cond;
        size_t current = index + 1;
        // `if (creg == int)` qop;
        this->_ensure_or_attach(current, "\"(\"", Token::Kind::LeftParen, "(");
        this->_ensure_or_attach(current, "a creg name", Token::Kind::Identifier, "dummy_creg");
        if_cond.creg = tokens[current-1];
        this->_ensure_or_attach(current, "\"==\"", Token::Kind::Equal, "==");
        this->_ensure_or_attach(current, "an integer value", Token::Kind::nnInteger, "0");
        // limit the val to compare 
        std::int64_t raw_val = 0;
        bool is_overflow = convert_sv_to_int64(tokens[current-1].text, raw_val);
        if (is_overflow) {
            if_cond.val = 0;
            std::string msg = "The condition value is too large: ";
            this->_add_error(current-1, msg, tokens[current-1].text);
        } else { if_cond.val = static_cast<size_t>(raw_val); }
        this->_ensure_or_attach(current, "\")\"", Token::Kind::RightParen, ")");
        // if (creg == int) `qop`;
        std::optional<Statement> parsed_qop = this->_parse_qop(current);
        if (parsed_qop.has_value()) {
            if_cond.statemant = parsed_qop.value();
            return {current, if_cond};
        }
        // Error: qop not found
        this->_ensure_or_attach(current, "\";\"", Token::Kind::Semicolon, ";");
        this->_add_error(current, "Expected a valid qop after if condition");
        if_cond.statemant = Statement();
        return {current, if_cond};
    }
    inline ParsedObject<GateDeclaration> _parse_gate_declaration(size_t index) {// TODO:
        this->_ensure_index(index);
        // skip if token is not "gate"
        if (tokens[index].kind != Token::Kind::Gate) return {index+1, std::nullopt}; 
    }

public:
    inline void parse() {
        std::string msg;
        size_t index = 0;
        // index may increase more than 1 here
        while (index < tokens.size()) {
            // case 1: gate declaration
            ParsedObject<GateDeclaration> parsed_gate_decl = this->_parse_gate_declaration(index);
            if (parsed_gate_decl.node_opt.has_value()) {
                ast.push_back(std::move(*parsed_gate_decl.node_opt));
                index = parsed_gate_decl.index;
                continue;
            }
            // case 2: if condition
            ParsedObject<IfCondition> parsed_if_cond = this->_parse_if_condition(index);
            if (parsed_if_cond.node_opt.has_value()) {
                ast.push_back(std::move(*parsed_if_cond.node_opt));
                index = parsed_if_cond.index;
                continue;
            }
            // case 3: statement
            ParsedObject<Statement> parsed_statement = this->_parse_statement(index);
            if (parsed_statement.node_opt.has_value()) {
                ast.push_back(std::move(*parsed_statement.node_opt));
                index = parsed_statement.index;
                continue;
            }
            // case 4: invalid
            this->_add_error(index, "Invalid token: ", tokens[index].text);
            index += 1;
        }
    }
};

int main(int argc, char* argv[]) {
    std::cout << "QAssembler is now working\n";
    // ----- load file -----
    // there should be two arguments
    // QAssemblerIBM.exe target.qasm
    // argv[0] is QAssemblerIBM.exe
    // argv[1] is target.qasm
    if (argc != 2) [[unlikely]] {
        std::cout << "Error: Unable to parse arguments\n";
        std::cout << "Usage: QAssemblerIBM <File.qasm>\n";
        return 1;
    }
    // get target asm filepath
    std::string asm_path(argv[1]);
    // filename should end with ".inc" or ".qasm"
    bool ends_with_inc =  (asm_path.size() > 4 && asm_path.ends_with(".inc") );
    bool ends_with_qasm = (asm_path.size() > 5 && asm_path.ends_with(".qasm"));
    if (!ends_with_inc && !ends_with_qasm) [[unlikely]] {
        std::cout << "Error: invalid filepath: " << asm_path << "\n";
        std::cout << "Usage: QAssemblerIBM <File.qasm>\n";
        return 1;
    }
    // open the file from path
    std::ifstream asm_file(asm_path.c_str());
    if (!asm_file) [[unlikely]] {
        std::cout << "Error: Cannot open the file" << asm_path << "\n";
        return 1;
    }
    // convert ifs to TextProcessor
    std::string raw_asm_str((std::istreambuf_iterator<char>(asm_file)), std::istreambuf_iterator<char>());

    // ----- preprocess -----
    std::cout << "preprocessing\n";
    Preprocessor preprocessor(raw_asm_str);
    preprocessor.erase_comments();
    preprocessor.standardize_EOL();
    preprocessor.standardize_space();

    // ----- tokenize -----
    std::cout << "tokenizing\n";
    Tokenizer tokenizer(preprocessor.text);
    context_type context = preprocessor.split_by("\n");
    Error::context_ptr = std::make_shared<context_type>(context);
    tokenizer.tokenize();

    // ----- parser -----
    std::cout << "parsing\n";
    Parser parser(tokenizer);
    
    // DEBUG
    std::cout << "----- context -----\n";
    print_vector(context);
    std::cout << "----- tokens -----\n";
    print_vector(tokenizer.tokens);
    std::cout << "----- errors -----\n";
    print_vector(tokenizer.errors);
    std::cout << "----- AST -----\n";
    std::cout << parser.ast << '\n';
    std::cout << "----- errors -----\n";
    print_vector(parser.errors);

    // 語意分析(Gate展開, 常數計算, 檢查語意是否合理)
    // 基本區塊劃分與控制流圖建立(分成 Basic Block)
    // 量子線路優化(相消, 合併, 交換, Dead Code)
    // 做成可以模擬的資料格式

    // ----- grammer -----
    // OPENQASM 2.0;
    // include "qelib1.inc";
    // qreg q[n];
    // creg c[n];
    // measure q[n] -> c[n];
    // if (expression) statement;
    // expression: only c==<const>
    // opaque
    // reset
    // barrier q[n];
    // gate_name params;
    // gate HGate param { 
    //     u2(0, pi) param; 
    // }
    // gate rz(phi) a { 
    //     u1(phi) a; 
    // }
    return 0;
}

// g++ QAssembler.cpp -o QAssembler.exe -O3 -Wall -Wextra -g3 -std=c++20 -static -static-libgcc -static-libstdc++
// QAssembler.exe Example.qasm || QAssembler.exe qelib1.inc

// TODO : more friendly error message
// FIXME: error should also record witch file it is in (test.cpp:1:8)
// FIXME: it might be confusing if error occurs on fixed token
// FIXME: using same template of error message for all kinds of error
// SOLUTION: store all the raw token, and then every token point to a raw token
// FIXME: there might be some expression in Expression, IfCondition, and GateDeclaration
// SOLUTION: use std::byte* and index table to store data in AST and so on
// SOLUTION: and then we dont need Statement::Kind and Expression::Kind
// SOLUTION: inherit instead, parse it and return {std::byte*, size}
