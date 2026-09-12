#ifndef YOYO_ACE1110_QASSEMBLER_LEXER_HPP
#define YOYO_ACE1110_QASSEMBLER_LEXER_HPP

#include"forward.hpp"
#include"source.hpp"
#include"vocab_type.hpp"

class Lexer {
private:
    const SrcFile* file;
    ErrorManager* errman;
    TokenManager* tokman;

    // function
    inline constexpr bool _is_number(char c) const noexcept {
        return (c >= '0' && c <= '9');
    }
    inline constexpr void _ensure_index(size_t index) const {
        if (index < file->modified.size()) return;
        throw std::out_of_range("file->modified index out of range");
    }
    inline constexpr bool _has_close_quote(size_t index) const {
        size_t count = 0;
        this->_ensure_index(index);
        if (index == 0) return false;
        if (file->modified[index] != '"') return false;
        --index;
        // count the "\" before quote
        while (true) {
            if (file->modified[index] != '\\') break;
            ++count;
            if (index == 0) break;
            --index;
        }
        // (count % 2 == 0);
        return !(count & 1);
    }
    inline constexpr void set_file(const SrcFile* srcfile) noexcept {
        file = srcfile;
    }
    inline constexpr void _add_error(const Token& token, const std::string& msg) const {
        this->errman->emplace_back(token, msg);
    }
    inline constexpr void _add_error(const SrcLoc& srcloc, size_t len, const std::string& msg) const {
        this->errman->emplace_back(srcloc, len, msg);
    }
    inline constexpr bool _can_be_identifier(char c) const noexcept {
        bool is_uppercase = (c >= 'A' && c <= 'Z');
        bool is_lowercase = (c >= 'a' && c <= 'z');
        return (is_uppercase || is_lowercase || this->_is_number(c) || c == '_');
    }
    inline constexpr bool _is_end_of_keyword(size_t next_index) const noexcept {
        if (next_index >= file->modified.size()) return true;
        return !this->_can_be_identifier(file->modified[next_index]);
    }
    [[nodiscard]] inline constexpr std::string_view _get_subview(size_t index, size_t length) const {
        this->_ensure_index(index);
        return std::string_view(file->modified).substr(index, length);
    }

    [[nodiscard]] inline Token _match_symbol(const SrcLoc& srcloc) const {
        const size_t index = srcloc.offset;
        this->_ensure_index(index);
        std::string_view subview;
        // 2-char symbols
        if (index+1 < file->modified.size()) {
            subview = this->_get_subview(index, 2);
            if (subview == "->") return Token(tokman, srcloc, subview, Token::Kind::Arrow);
            if (subview == "==") return Token(tokman, srcloc, subview, Token::Kind::Equal);
        }
        // 1-char symbols
        switch (file->modified[index]) {
            case ',': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::Comma);}
            case ';': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::Semicolon);}
            case '[': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::LeftBracket);}
            case ']': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::RightBracket);}
            case '{': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::LeftBrace);}
            case '}': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::RightBrace);}
            case '(': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::LeftParen);}
            case ')': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::RightParen);}
            case '+': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::Plus);}
            case '-': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::Minus);}
            case '*': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::Times);}
            case '/': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::Devide);}
            case '^': {return Token(tokman, srcloc, this->_get_subview(index, 1), Token::Kind::Power);}
            default : break;
        }
        // default
        return Token(tokman, srcloc, subview, Token::Kind::Invalid);
    }
    [[nodiscard]] inline Token _match_keyword(const SrcLoc& srcloc) const {
        const size_t index = srcloc.offset;
        this->_ensure_index(index);
        std::string_view subview;
        Token token(tokman, srcloc, subview, Token::Kind::Invalid);
        const size_t remaining_length = file->modified.size()-index;
        // lambda
        auto match = [&] (std::string_view keyword, Token::Kind kind) -> bool {
            size_t length = keyword.size();
            subview = this->_get_subview(index, length);
            if (subview == keyword && this->_is_end_of_keyword(index+length)) {
                token = Token(tokman, srcloc, subview, kind);
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
    inline void _add_dynamic_token_or_fix_error(const SrcLoc& srcloc, size_t length) const {
        const size_t index = srcloc.offset;
        const size_t end = index+length-1;
        this->_ensure_index(end);
        std::string string_buffer;
        std::string_view subview = this->_get_subview(index, length);
        // Token token(tokman, srcloc, subview, Token::Kind::Invalid);
        // string literal
        if (file->modified[index] == '"') {
            if (length < 2 || !this->_has_close_quote(end)) {
                // Fix: close quote
                subview.remove_prefix(1);
                tokman->add_fixed_token(srcloc, std::string(subview), Token::Kind::StringLiteral);
                // Error: Unclosed quote
                this->_add_error(tokman->tokens.back(), "Unclosed quote");
            }
            // strip the quotes
            subview.remove_prefix(1);
            subview.remove_suffix(1);
            tokman->emplace_back(srcloc, subview, Token::Kind::StringLiteral);
        }
        // real number & interger
        else if (this->_is_number(file->modified[index]) || file->modified[index] == '.') {
            string_buffer += file->modified[index];
            bool has_exponent = false, has_decimal_point = false;
            for (size_t i = index+1; i < index+length; ++i) {
                if (this->_is_number(file->modified[i])) {
                    string_buffer += file->modified[i];
                    continue;
                }
                else if (file->modified[i] == '.') {
                    if (has_exponent) {
                        // Fix: skip this character
                        std::string msg = "decimal point must not follow Exponent";
                        this->_add_error(srcloc, length, msg);
                        continue;
                    }
                    if (has_decimal_point) {
                        // Fix: skip this character
                        std::string msg = "More than one decimal points";
                        this->_add_error(srcloc, length, msg);
                        continue;
                    }
                    // decimal point must be adjacent to a number
                    bool last_char_is_number = ((i > 0)   && this->_is_number(file->modified[i-1]));
                    bool next_char_is_number = ((i < end) && this->_is_number(file->modified[i+1]));
                    if (!(last_char_is_number || next_char_is_number)) {
                        // Error: decimal point must be adjacent to a number
                        std::string msg = "decimal point must be adjacent to a number";
                        this->_add_error(srcloc, length, msg);
                        // Fix: add a zero after it
                        string_buffer += file->modified[i];
                        has_decimal_point = true;
                        string_buffer += '0';
                        continue;
                    }
                    string_buffer += file->modified[i];
                    has_decimal_point = true;
                    continue;
                }
                else if (file->modified[i] == 'e' || file->modified[i] == 'E') {
                    if (has_exponent) {
                        // Fix: skip this character
                        this->_add_error(srcloc, length, "More than one exponent");
                        continue;
                    }
                    has_exponent = true;
                    string_buffer += file->modified[i];
                    // skip "+" or "-"
                    if (i < end && (file->modified[i+1] == '+' || file->modified[i+1] == '-')) {
                        string_buffer += file->modified[++i];
                    }
                    continue;
                }
                else {
                    // Fix: skip this character
                    this->_add_error(srcloc, length, "Invalid character");
                    continue;
                }
            }
            // file->modified[end] cannot be exponent 
            if (file->modified[end] == 'e' || file->modified[end] == 'E') {
                // Fix: delete this character
                this->_add_error(srcloc, length, "Exponent connot be the last character");
                string_buffer.pop_back();
            }
            // distinguish nnInteger & real number
            Token::Kind kind = (
                (has_decimal_point || has_exponent) ? 
                Token::Kind::RealNumber : 
                Token::Kind::nnInteger
            );
            if (string_buffer != subview) {
                tokman->add_fixed_token(srcloc, string_buffer, kind);
            } else { tokman->emplace_back(srcloc, subview, kind); }
        }
        // identifier (this must be placed after numbers)
        else if (this->_can_be_identifier(file->modified[index])) {
            string_buffer += file->modified[index];
            for (size_t i = index+1; i < index+length; ++i) {
                // got invalid character
                if (!this->_can_be_identifier(file->modified[i])) {
                    // Error: Invalid character
                    this->_add_error(srcloc, length, "Invalid character");
                    // Fix: skip this character
                    continue;
                }
                string_buffer += file->modified[i];
            }
            // Identifier
            if (string_buffer != subview) {
                tokman->add_fixed_token(srcloc, string_buffer, Token::Kind::Identifier);
            } else { tokman->emplace_back(srcloc, subview, Token::Kind::Identifier); }
        }
        // Error: Cannot identify this token
        else { this->_add_error(srcloc, length, "Cannot identify this token"); }
    }

public:
    // constructor
    inline constexpr Lexer(ErrorManager* errman_, TokenManager* tokman_) noexcept 
    : file(nullptr), errman(errman_), tokman(tokman_) {}

    inline void tokenize(const SrcFile* srcfile) {
        this->set_file(srcfile);
        const std::string& text = file->modified;
        if (text.empty()) return;
        // initialize
        size_t length;
        bool is_in_quote = false;
        Token token  (tokman, file->srcman);
        Token dynamic(tokman, file->srcman);
        SrcLoc start  (file->srcman, 0, file->id);
        SrcLoc current(file->srcman, 0, file->id);
        tokman->tokens.reserve(tokman->tokens.size()+text.size()/4);
        // tokenize loop
        while (current.offset < text.size()) {
            length = current.offset-start.offset+1;
            // match quote
            if (is_in_quote) {
                // quote closed
                if (this->_has_close_quote(current.offset)) {
                    this->_add_dynamic_token_or_fix_error(current, length);
                    start.offset = (++current.offset);
                    is_in_quote = false;
                    continue;
                }
                // keep going
                (++current.offset);
                continue;
            }
            // match dynamic
            if (start.offset != current.offset) {
                token = this->_match_symbol(current);
                // only these four condition will end up this dynamic token
                bool is_EOL   = text[current.offset] == '\n';
                bool is_space = text[current.offset] == ' ' ;
                bool is_quote = text[current.offset] == '"' ;
                if (is_EOL || is_space || is_quote || token.is_valid()) {
                    this->_add_dynamic_token_or_fix_error(start, length-1);
                    start.offset = current.offset;
                }
                // otherwise this dynamic token still extend
                else {
                    ++current.offset;
                    continue;
                }
            }
            // quote opened
            if (text[current.offset] == '"') {
                is_in_quote = true;
                start.offset = (current.offset++);
                continue;
            }
            // match useless spaces
            if (text[current.offset] == ' ' || text[current.offset] == '\n') {
                start.offset = (++current.offset);
                continue;
            }
            // match symbols
            token = this->_match_symbol(current);
            if (token.is_valid()) {
                tokman->push_back(token);
                current.offset += token.length();
                start.offset = current.offset;
                continue;
            }
            // match keywords
            token = this->_match_keyword(current);
            if (token.is_valid()) {
                tokman->push_back(token);
                current.offset += token.length();
                start.offset = current.offset;
                continue;
            }
            // new dynamic token starts
            start.offset = (current.offset++);
        }
        // the last token
        if (start.offset != current.offset) {
            length = current.offset-start.offset+1;
            this->_add_dynamic_token_or_fix_error(start, length);
        }
    }
};

#endif
