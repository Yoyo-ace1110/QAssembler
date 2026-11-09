#ifndef YOYO_ACE1110_QASSEMBLER_STATEMENT_HPP
#define YOYO_ACE1110_QASSEMBLER_STATEMENT_HPP

#include "forward.hpp"
#include "source.hpp"
#include "vocab_type.hpp"

template<typename Derived>
struct Statement {
    // CRTP [start, end]
    size_t start = npos;
    size_t end   = npos;

    // static member
    inline static constexpr uint8_t ID = Derived::ID;
    inline static constexpr std::string_view name = Derived::name;
    inline static constexpr size_t serialized_size = sizeof(start)+sizeof(end);
    inline static void serialize_to(std::vector<std::byte>& buffer, size_t start, size_t end) {
        size_t len = sizeof(size_t);
        size_t offset = buffer.size();
        buffer.resize(offset + serialized_size);
        // copy to the end of buffer
        std::memcpy(buffer.data() + offset, &start, len);
        offset += len;
        std::memcpy(buffer.data() + offset, &end, len);
    }
    [[nodiscard]] inline static Derived deserialized(std::span<const std::byte> bytes) {
        if (bytes.size() < serialized_size) [[unlikely]] {
            std::string msg = "std::vector<std::byte> is too small for ";
            throw std::invalid_argument(to_str(msg, Derived::name));
        }
        Derived obj;
        size_t len = sizeof(size_t);
        std::memcpy(&obj.start, bytes.data()    , len);
        std::memcpy(&obj.end  , bytes.data()+len, len);
        return obj;
    }

    // function
    inline constexpr void ensure(size_t index) const {
        if (index < this->length()) [[likely]] return;
        std::string msg = to_str("::ensure(", index, ") out of range");
        throw std::out_of_range(to_str(Statement::name, msg));
    } 
    [[nodiscard]] inline constexpr size_t length() const noexcept {
        if (start == npos || end == npos || start > end) [[unlikely]] return 0;
        return end - start + 1;
    }
    [[nodiscard]] inline constexpr Token& operator[] (size_t index) {
        return tokman->tokens[start+index];
    }
    [[nodiscard]] inline constexpr const Token& operator[] (size_t index) const {
        return tokman->tokens[start+index];;
    }

    // ostream operator
    friend inline std::ostream& operator<< (std::ostream& os, const Statement& statement) {
        os << "\tStatement<" << Derived::name << "> { ";
        bool is_first_token = true;
        for (size_t i = 0; i < statement.length(); ++i) {
            const Token& token = statement[i];
            if (is_first_token) { is_first_token = false; } 
            else if (token.sep_by_space()) { os << ' '; }
            os << token.text;
        }
        return (os << " }");
    }
};

struct astOPENQASM : public Statement<astOPENQASM> {
    inline static constexpr uint8_t ID = 1;
    inline static constexpr std::string_view name = "astOPENQASM";
};
struct astInclude  : public Statement<astInclude > {
    inline static constexpr uint8_t ID = 2;
    inline static constexpr std::string_view name = "astInclude";
};
struct astQRegDecl : public Statement<astQRegDecl> {
    inline static constexpr uint8_t ID = 3;
    inline static constexpr std::string_view name = "astQRegDecl";
};
struct astCRegDecl : public Statement<astCRegDecl> {
    inline static constexpr uint8_t ID = 4;
    inline static constexpr std::string_view name = "astCRegDecl";
};
struct astMeasure  : public Statement<astMeasure > {
    inline static constexpr uint8_t ID = 5;
    inline static constexpr std::string_view name = "astMeasure";
};
struct astReset    : public Statement<astReset   > {
    inline static constexpr uint8_t ID = 6;
    inline static constexpr std::string_view name = "astReset";
};
struct astBarrier  : public Statement<astBarrier > {
    inline static constexpr uint8_t ID = 7;
    inline static constexpr std::string_view name = "astBarrier";
};
struct astGateCall : public Statement<astGateCall> {
    inline static constexpr uint8_t ID = 8;
    inline static constexpr std::string_view name = "astGateCall";
};
struct astOpaque   : public Statement<astOpaque  > {
    inline static constexpr uint8_t ID = 9;
    inline static constexpr std::string_view name = "astOpaque";
};

#endif
