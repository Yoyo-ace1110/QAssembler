#ifndef YOYO_ACE1110_QASSEMBLER_VOCAB_TYPE_HPP
#define YOYO_ACE1110_QASSEMBLER_VOCAB_TYPE_HPP

#include"forward.hpp"
#include"source.hpp"

struct Token {
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
    SrcLoc location;
    std::string_view text;
    Kind kind = Kind::Invalid;

    // function
    [[nodiscard]] inline constexpr size_t length() const noexcept {
        return text.size();
    }
    [[nodiscard]] inline constexpr bool is_valid() const noexcept {
        return (kind != Kind::Invalid);
    } 
    [[nodiscard]] inline constexpr bool sep_by_space() const noexcept {
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

    // operator
    friend inline std::ostream& operator << (std::ostream& os, const Token& token) {
        return (os << token.text);
    }
};

struct Error {
    // member
    size_t length;
    SrcLoc location;
    std::string message;

    // constructor
    inline Error(const Token& token, const std::string& msg) noexcept
    : length(token.length()), location(token.location), message(msg) {}
    inline Error(const SrcLoc& srcloc, size_t len, const std::string& msg) noexcept
    : length(len), location(srcloc), message(msg) {}
    
    // operator
    friend inline std::ostream& operator << (std::ostream& os, const Error& error) {
        // store the flags before output anything to os
        std::ios_base::fmtflags os_flag(os.flags());
        // calculate the line ranges of error
        SrcLoc begin_loc = error.location, end_loc = error.location;
        if (error.length > 0) { end_loc.offset += error.length-1; }
        SrcCoord begin_coord(begin_loc), end_coord(end_loc);
        // prepare for padding
        size_t line_num_width = std::to_string(end_coord.row).length();
        const SrcFile* file = error.location.file_ptr();
        std::string padding(line_num_width, ' ');
        // print error message
        os << "In " << begin_coord.filepath() << ": " << error.message << '\n';
        // print source code and '^' tags
        for (size_t line = begin_coord.row; line <= end_coord.row; ++line) {
            // print source code
            std::string_view line_content = file->get_line(line);
            os << std::setw(line_num_width) << line << " | " << line_content << "\n";
            // prepare for tags: only the first line or the last line may have currect tokens
            size_t start_col = (line == begin_coord.row) ? begin_coord.col : 1;
            size_t end_col   = (line == end_coord.row  ) ? end_coord.col   : line_content.length();
            if (start_col > line_content.length()) { start_col = line_content.length()+1; }
            // padding before tags were printed
            os << padding << " | ";
            for (size_t i = 1; i < start_col; ++i) {
                // padding for the \t char in source code
                if (i <= line_content.length() && line_content[i-1] == '\t') { os << '\t'; } 
                else { os << ' '; }
            }
            // print the error tags
            size_t mark_len = (end_col >= start_col) ? (end_col - start_col + 1) : 1;
            os << std::string(mark_len, '^') << "\n";
        }
        // restore the flags
        os.flags(os_flag);
        return os;
    }
};

struct ErrorManager {
    std::vector<Error> errors;

    // function
    [[nodiscard]] inline constexpr size_t size() const noexcept {
        return errors.size();
    }
    inline constexpr void push_back(const Error& error) {
        errors.push_back(error);
    }
    inline constexpr void emplace_back(const Token& token, const std::string& msg) {
        errors.emplace_back(token, msg);
    }
    inline constexpr void emplace_back(const SrcLoc& srcloc, size_t len, const std::string& msg) {
        errors.emplace_back(srcloc, len, msg);
    }
};

struct TokenManager {
    std::vector<Token> tokens;
    std::deque<std::string> fixed_str;
    
    // constructor
    inline TokenManager() noexcept = default;

    // function
    [[nodiscard]] inline constexpr size_t size() const noexcept {
        return tokens.size();
    }
    inline void push_back(const Token& token) {
        tokens.push_back(token);
    }
    inline void emplace_back(const SrcLoc& srcloc, std::string_view str, Token::Kind kind_) {
        tokens.emplace_back(srcloc, str, kind_);
    }
    inline void add_fixed_token(const SrcLoc& srcloc, const std::string& str, Token::Kind kind_) {
        fixed_str.push_back(str);
        std::string_view sv = fixed_str.back();
        this->emplace_back(srcloc, sv, kind_);
    }
};

#endif
