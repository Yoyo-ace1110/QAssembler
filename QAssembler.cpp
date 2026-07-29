// version: OPENQASM 2.0
// Only U and CX are built-in gates

#include<array>
#include<deque>
#include<string>
#include<vector>
#include<memory>
#include<fstream>
#include<cstdint>
#include<iomanip>
#include<iostream>
#include<stdexcept>
#include<string_view>

namespace {
    using size_t = std::size_t;
    static const size_t npos = std::string::npos;
    using context_type = std::vector<std::string>;

    template <typename... Args>
    inline std::string to_str(const Args&... args) {
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
}

class Range {
public:
    // [begin, end)
    size_t begin, end;
    // ----- constructor -----
    inline constexpr Range(size_t begin_, size_t end_) noexcept : begin(begin_), end(end_) {}
    inline constexpr Range(const std::string& string) noexcept : begin(0), end(string.length()) {}
    inline constexpr Range(const Range& other) noexcept : begin(other.begin), end(other.end) {}
    // ----- assignment operator -----
    inline constexpr Range& operator = (const Range& other) noexcept {
        begin = other.begin;
        end = other.end;
        return (*this);
    }
    // ----- comparison operator -----
    friend inline constexpr bool operator == (const Range& a, const Range& b) noexcept {
        return (a.begin == b.begin && a.end == b.end);
    }
    friend inline constexpr bool operator != (const Range& a, const Range& b) noexcept {
        return !(a == b);
    }
    // ----- function -----
    inline constexpr size_t length() const noexcept {
        return (end > begin) ? (end - begin) : 0;
    }
};

class TextProcessor {
protected:
    inline constexpr Range _default_range() const noexcept {
        return Range(0, text.size());
    }
public:
    std::string text = "";
    // ----- constructor -----
    inline constexpr TextProcessor(std::string text_ = "") noexcept : text(text_) {}
    inline constexpr TextProcessor(const TextProcessor& other) noexcept : text(other.text) {}
    // ----- assignment operator -----
    inline constexpr TextProcessor& operator = (const TextProcessor& other) noexcept {
        text = other.text;
        return (*this);
    }
    // ----- function -----
    inline constexpr void clear() noexcept {text.clear();}
    inline constexpr bool empty() const noexcept {return text.empty();}
    inline constexpr void ensure(const Range& range) const {
        if ((range.begin > range.end) || (range.end > text.length())) {
            throw std::invalid_argument("Invalid range object received!");
        }
    }
    inline constexpr void erase(const Range& range) noexcept {
        this->ensure(range);
        text.erase(range.begin, range.length());
    }
    inline constexpr void insert(size_t pos, const std::string& str) noexcept {
        text.insert(pos, str);
    }
    inline constexpr void replace(const Range& range, const std::string& str) noexcept {
        this->ensure(range);
        text.replace(range.begin, range.length(), str);
    }
    inline constexpr void replace_all(const std::string& from, const std::string& to) noexcept {
        this->replace_all(from, to, this->_default_range());
    }
    inline constexpr void replace_all(const std::string& from, const std::string& to, Range range) noexcept {
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
    inline constexpr size_t find(const std::string& str) const noexcept {
        return this->find(str, this->_default_range());
    }
    inline constexpr size_t find(const std::string str, size_t pos) const noexcept {
        if (str.empty() || pos >= text.size()) return npos;
        return text.find(str, pos);
    }
    inline constexpr size_t find(const std::string& str, const Range& range) const noexcept {
        if (str.empty()) return npos;
        size_t pos = text.find(str, range.begin);
        if (pos != npos && pos+str.length() <= range.end) {
            return pos;
        }
        return npos;
    }
    inline constexpr std::vector<std::string> split_by(const std::string& sep) const noexcept {
        return this->split_by(sep, _default_range());
    }
    inline constexpr std::vector<std::string> split_by(const std::string& sep, const Range& range) const noexcept {
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
    inline constexpr void standardize_EOL() noexcept {
        // replace all EOL to "\n"
        this->replace_all("\r\n", "\n");
        this->replace_all("\r"  , "\n");
    }
    inline constexpr void standardize_space() noexcept {
        // replace all spaces to " "
        this->replace_all("\t", " ");
    }
    inline constexpr void erase_comments() noexcept {
        // ... \n
        while (true) {
            // this won't change the line number
            size_t comment_pos = this->find("//");
            size_t end_of_line = this->find("\n", comment_pos);
            if (comment_pos == npos) break;
            // erase the comment line without '\n'
            this->erase(Range(comment_pos, (end_of_line == npos) ? 0 : end_of_line));
        }
    }
};

enum class TokenType : std::uint8_t {
    invalid,
    // keywords
    openqasm,
    include,
    if_,
    qreg,
    creg,
    gate,
    reset,
    opaque,
    measure,
    barrier,
    // built-in constant
    const_pi,
    // built-in functions
    func_ln,     // ln
    func_sin,    // sin
    func_cos,    // cos
    func_tan,    // tan
    func_exp,    // exp
    func_sqrt,   // sqrt
    // operators
    plus,        // +
    minus,       // -
    times,    // *
    devide,      // /
    power,       // ^
    // symbols
    comma,       // ,
    semicolon,   // ;
    l_bracket,   // [
    r_bracket,   // ]
    l_brace,     // {
    r_brace,     // }
    l_paren,     // (
    r_paren,     // )
    arrow,       // ->
    equal,       // ==
    // dynamic
    nninteger,
    identifier,
    real_number,
    string_literal,
};

class Token {
public:
    // member
    size_t line = npos;
    std::string_view text = "";
    TokenType type = TokenType::invalid;
    // constructor
    inline constexpr Token() noexcept {}
    inline constexpr Token(const Token& other) noexcept : line(other.line), text(other.text), type(other.type) {}
    inline constexpr Token(size_t line_, std::string_view token_str, TokenType token_type) noexcept
        : line(line_), text(token_str), type(token_type) {}
    // function
    inline constexpr bool is_valid() const noexcept {
        return (type != TokenType::invalid);
    } 
    inline constexpr size_t length() const noexcept {
        return text.size();
    }
    // assignment operator
    inline constexpr Token& operator = (const Token& other) noexcept {
        line = other.line;
        text = other.text;
        type = other.type;
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
    Range lines{0, 1};          // lines here is from human's point of view
    std::string message = "";
    std::string_view text = "";
    using pointer_type = std::shared_ptr<context_type>;
    inline static pointer_type context_ptr = nullptr;
    // constructor
    inline constexpr Error(Range lines_, const std::string& msg, std::string_view text_) noexcept 
        : lines(lines_), message(msg), text(text_) {}
    inline constexpr Error(const Token& token, const std::string& msg) noexcept 
        : message(msg), text(token.text) {
        // single line token
        if (token.type != TokenType::string_literal) {
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
    inline void print() const { std::cout << (*this) << '\n'; }
    friend std::ostream& operator<< (std::ostream& os, const Error& error) {
        if (!context_ptr) throw std::invalid_argument("invalid context");
        std::ios_base::fmtflags f(os.flags());
        // single line
        if (error.lines.length() == 1) [[likely]] {
            os << "In single line " << error.lines.begin << ": \n";
            os << '\t' << std::setw(4) << error.lines.begin << " | " << (*error.context_ptr)[error.lines.begin-1] << '\n';
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

class Tokenizer {
public:
    // ----- member -----
    std::string_view text = "";
    std::vector<Token> tokens{};
    std::vector<Error> errors{};
    // this container will not reallocate when you call push_back()
    // it will allocate a new chuck and append the element in
    // it store the pointer of every chuck instead of element  
    std::deque<std::string> fixed_str = {};
private:
    // ----- function -----
    inline constexpr void _ensure_index(size_t index) const {
        if (index < text.size()) return;
        throw std::out_of_range("Tokenizer[] index out of range");
    }
    inline constexpr bool _has_close_quote(size_t index) const {
        size_t count = 0;
        this->_ensure_index(index);
        if (text[index] != '"') return false;
        // count the "\" before quote
        do {
            --index;
            if (text[index] == '\\') {
                ++count;
            } else break;
        } while (index > 0);
        // (count % 2 == 0);
        return !(count & 1);
    }
    inline constexpr bool _is_number(char c) const noexcept {
        return (c >= '0' && c <= '9');
    }
    inline constexpr void _add_error(const Token& token, const std::string& str) {
        errors.push_back(Error(token, str));
    }
    inline constexpr bool _can_be_identifier(char c) const noexcept {
        bool is_uppercase = (c >= 'A' && c <= 'Z');
        bool is_lowercase = (c >= 'a' && c <= 'z');
        return (is_uppercase || is_lowercase || this->_is_number(c) || c == '_');
    }
    inline constexpr bool _is_end_of_keyword(size_t next_index) const noexcept {
        if (next_index >= text.size()) return true;
        return !this->_can_be_identifier(text[next_index]);
    }
    inline constexpr std::string _get_substr(size_t index, size_t length) const {
        this->_ensure_index(index);
        return std::string(this->_get_subview(index, length));
    }
    inline constexpr std::string_view _get_subview(size_t index, size_t length) const {
        this->_ensure_index(index);
        return text.substr(index, length);
    }
    inline constexpr Token _match_symbol(size_t line, size_t index) const {
        this->_ensure_index(index);
        std::string_view subview = "";
        // 2-char symbols
        if (index+1 < text.size()) {
            subview = this->_get_subview(index, 2);
            if (subview == "->") {return Token(line, subview, TokenType::arrow);}
            if (subview == "==") {return Token(line, subview, TokenType::equal);}
        }
        // 1-char symbols
        switch (text[index]) {
            case ',': {return Token(line, this->_get_subview(index, 1), TokenType::comma);}
            case ';': {return Token(line, this->_get_subview(index, 1), TokenType::semicolon);}
            case '[': {return Token(line, this->_get_subview(index, 1), TokenType::l_bracket);}
            case ']': {return Token(line, this->_get_subview(index, 1), TokenType::r_bracket);}
            case '{': {return Token(line, this->_get_subview(index, 1), TokenType::l_brace);}
            case '}': {return Token(line, this->_get_subview(index, 1), TokenType::r_brace);}
            case '(': {return Token(line, this->_get_subview(index, 1), TokenType::l_paren);}
            case ')': {return Token(line, this->_get_subview(index, 1), TokenType::r_paren);}
            case '+': {return Token(line, this->_get_subview(index, 1), TokenType::plus);}
            case '-': {return Token(line, this->_get_subview(index, 1), TokenType::minus);}
            case '*': {return Token(line, this->_get_subview(index, 1), TokenType::times);}
            case '/': {return Token(line, this->_get_subview(index, 1), TokenType::devide);}
            case '^': {return Token(line, this->_get_subview(index, 1), TokenType::power);}
            default : break;
        }
        // default
        return Token(line, subview, TokenType::invalid);
    }
    inline constexpr Token _match_keyword(size_t line, size_t index) const {
        this->_ensure_index(index);
        std::string_view subview = "";
        Token token(line, subview, TokenType::invalid);
        const size_t remaining_length = text.size()-index;
        // lambda
        auto match = [&] (std::string_view keyword, TokenType type) -> bool {
            size_t length = keyword.size();
            subview = this->_get_subview(index, length);
            if (subview == keyword && this->_is_end_of_keyword(index+length)) {
                token = Token(line, subview, type);
                return true;
            }
            return false;
        };
        // keywords, must be ordered by length
        if (remaining_length < 2) return token;
        if (match("if", TokenType::if_))            return token;
        if (match("ln", TokenType::func_ln))        return token;
        if (match("pi", TokenType::const_pi))       return token;
        if (remaining_length < 3) return token;
        if (match("sin", TokenType::func_sin))      return token;
        if (match("cos", TokenType::func_cos))      return token;
        if (match("tan", TokenType::func_tan))      return token;
        if (match("exp", TokenType::func_exp))      return token;
        if (remaining_length < 4) return token;
        if (match("qreg", TokenType::qreg))         return token;
        if (match("creg", TokenType::creg))         return token;
        if (match("gate", TokenType::gate))         return token;
        if (match("sqrt", TokenType::func_sqrt))    return token;
        if (remaining_length < 5) return token;
        if (match("reset", TokenType::reset))       return token;
        if (remaining_length < 6) return token;
        if (match("opaque", TokenType::opaque))     return token;
        if (remaining_length < 7) return token;
        if (match("include", TokenType::include))   return token;
        if (match("measure", TokenType::measure))   return token;
        if (match("barrier", TokenType::barrier))   return token;
        if (remaining_length < 8) return token;
        if (match("OPENQASM", TokenType::openqasm)) return token;
        return token;
    }
    inline constexpr void _add_dynamic_token_or_fix_error(size_t line, size_t index, size_t length) noexcept {
        const size_t end = index+length-1;
        this->_ensure_index(end);
        std::string string_buffer = "";
        std::string_view subview = this->_get_subview(index, length);
        Token token(line, subview, TokenType::invalid);
        // string literal
        if (text[index] == '"') {
            if (length < 2 || !this->_has_close_quote(end)) {
                // Error: Unclosed quote
                this->_add_error(token, "Unclosed quote");
                // Fix: close quote
                fixed_str.push_back(to_str(token.text, "\""));
                token.text = std::string_view(fixed_str.back());
                // strip the quotes
                token.text.remove_prefix(1);
                token.text.remove_suffix(1);
            }
            // string literal
            token.type = TokenType::string_literal;
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
            // distinguish nninteger & real number
            token.type = (
                (has_decimal_point || has_exponent) ? 
                TokenType::real_number : 
                TokenType::nninteger
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
            // identifier
            if (string_buffer != subview) {
                fixed_str.push_back(string_buffer);
                token.text = fixed_str.back();
            }
            token.type = TokenType::identifier;
            tokens.push_back(token);
            return;
        }
        // Error: Cannot identify this token
        else { this->_add_error(token, "Cannot identify this token"); }
    }
public:
    // ----- constructor -----
    inline Tokenizer() noexcept = default;
    inline Tokenizer(std::string_view str) noexcept : text(str) {}
    inline Tokenizer(const Tokenizer& other) noexcept : text(other.text) {}
    // ----- function -----
    inline constexpr void tokenize() noexcept {
        // initialize
        size_t length;
        size_t line = 1;
        size_t start = 0;
        size_t index = 0;
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
    // ----- operators -----
    inline constexpr Token& operator[] (size_t index) {
        if (index >= tokens.size()) throw std::out_of_range("Tokenizer[] index out of range");
        return tokens[index];
    }
    inline constexpr const Token operator[] (size_t index) const {
        if (index >= tokens.size()) throw std::out_of_range("Tokenizer[] index out of range");
        return tokens[index];
    }
};

class ClassAST {
    struct Expression {
        
    };
};

class Parser {
public:
    // ----- member -----
    Tokenizer tokenizer;
    std::vector<ClassAST> AST = {};
    std::vector<Token>& tokens = tokenizer.tokens;
    std::vector<Error>& errors = tokenizer.errors;
    std::deque<std::string>& fixed_str = tokenizer.fixed_str;
    // ----- constructor -----
    inline Parser(const Tokenizer& tokenizer_) noexcept : tokenizer(tokenizer_) {}
private:
    // ----- functions -----
    inline constexpr void _add_error(const Token& token, const std::string& str) {
        tokenizer.errors.push_back(Error(token, str));
    }
public:
    // inline constexpr void check_beginning_tokens() noexcept {
    //     std::string msg = "";
    //     // the beginning tokenizer must be "OPENQASM 2.0;"
    //     if (tokenizer[0].type != TokenType::openqasm || tokenizer[0].text != "OPENQASM") {
    //         msg = "The first token must be 'OPENQASM'";
    //         this->_add_error(tokenizer[0], msg);
    //         return;
    //     }
    //     if (tokenizer[1].type != TokenType::real_number) {
    //         msg = "The second token must be version number";
    //         this->_add_error(tokenizer[1], msg);
    //         return;
    //     }
    //     if (tokenizer[2].type != TokenType::semicolon) {
    //         msg = to_str("Expected ';' before '", tokenizer[2].text, "' token");
    //         this->_add_error(tokenizer[2], msg);
    //         return;
    //     }
    //     if (tokenizer[1].text != "2.0") {
    //         msg = to_str("Unsupported version: ", tokenizer[1].text);
    //         this->_add_error(tokenizer[1], msg);
    //         return;
    //     }
    // }
    inline constexpr void parse() noexcept {

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
    TextProcessor processer(raw_asm_str);

    // ----- preprocess -----
    std::cout << "preprocessing\n";
    Preprocessor preprocessor(raw_asm_str);
    std::cout << "erase_comments\n";
    preprocessor.erase_comments();
    std::cout << "standardize_EOL\n";
    preprocessor.standardize_EOL();
    std::cout << "standardize_space\n";
    preprocessor.standardize_space();
    std::cout << "split_by\n";
    context_type context = preprocessor.split_by("\n");
    Error::context_ptr = std::make_shared<context_type>(context);

    // ----- tokenize -----
    std::cout << "tokenizing\n";
    Tokenizer tokenizer(preprocessor.text);
    tokenizer.tokenize();

    // ----- parser -----
    std::cout << "parsing\n";
    Parser parser(tokenizer);
    
    // DEBUG
    std::cout << "----- tokens -----\n";
    print_vector(tokenizer.tokens);
    std::cout << "----- errors -----\n";
    print_vector(tokenizer.errors);
    std::cout << "----- context -----\n";
    print_vector(context);

    // 語意分析(Gate展開, 常數計算, 檢查語意是否合理)
    // 基本區塊劃分與控制流圖建立(分成 Basic Block)
    // 量子線路優化(相消, 合併, 交換, Dead Code)
    // 做成可以模擬的資料格式

    // ----- grammer -----
    // OPENQASM 2.0;
    // include "qelib1.inc";
    // qreg[n]
    // creg[n]
    // gate ...parameters
    // measure q[n] -> c[n]
    // if (expression) statement
    // expression: only c==<const>
    // opaque
    // reset
    // barrier
    // gate_name params
    // gate HGate param { 
    //     u2(0, pi) param; 
    // }
    // gate rz(phi) a { 
    //     u1(phi) a; 
    // }
    return 0;
}

// g++ QAssembler.cpp -o QAssembler.exe -O3 -Wall -Wextra -g3 -std=c++20 -static -static-libgcc -static-libstdc++
// QAssembler.exe Example.qasm
// QAssembler.exe qelib1.inc
// Parse tokens into an AST
