#ifndef YOYO_ACE1110_QASSEMBLER_PREPROCESSOR_HPP
#define YOYO_ACE1110_QASSEMBLER_PREPROCESSOR_HPP

#include"forward.hpp"
#include"source.hpp"
#include"vocab_type.hpp"

class Preprocessor {
private:
    SrcFile* file;

    // function
    inline constexpr void replace_comments() const noexcept {
        // this function won't change the location of any char
        // comments will be replace with spaces of equal length
        size_t i = 0;
        char this_char, next_char;
        const size_t len = file->modified.size();
        // scan until the char before file->end()
        while (i < len-1) {
            this_char = file->modified[i];
            next_char = file->modified[i+1];
            // single line comments
            if (this_char == '/' && next_char == '/') {
                while (i < len && file->modified[i] != '\n') {
                    // replace with spaces
                    file->modified[i++] = ' ';
                }
            }
            // multiple line comments
            else if (i+1 < len && this_char == '/' && next_char == '*') {
                // replace "/*"
                file->modified[i+1] = ' ';
                file->modified[i] = ' ';
                i += 2;
                while (i < len) {
                    this_char = file->modified[i];
                    next_char = file->modified[i+1];
                    // end of comment
                    if (i + 1 < len && this_char == '*' && next_char == '/') {
                        file->modified[i+1] = ' ';
                        file->modified[i] = ' ';
                        i += 2;
                        break;
                    }
                    // replace with spaces
                    if (file->modified[i] != '\n') {
                        file->modified[i] = ' ';
                    }
                    ++i;
                }
            } 
            else ++i;
        }
    }
    inline constexpr void standardize_EOL_and_space() const noexcept {
        const size_t len = file->modified.size();
        for (size_t i = 0; i < len; ++i) {
            char& ch = file->modified[i];
            // standardize '\t'
            if (ch == '\t') { ch = ' '; } 
            else if (ch == '\r') {
                // "\r\n" => " \n" (Windows)
                if (i+1 < len && file->modified[i+1] == '\n') {
                    ch = ' ';
                } 
                // single \r => \n (Mac)
                else { ch = '\n'; }
            }
        }
    }

public:
    inline constexpr void preprocess(SrcFile* srcfile) noexcept {
        file = srcfile;
        file->compute_line_offsets();
        this->standardize_EOL_and_space();
        this->replace_comments();
    }
};

#endif
