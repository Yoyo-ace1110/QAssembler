#ifndef YOYO_ACE1110_QASSEMBLER_STATEMENT_HPP
#define YOYO_ACE1110_QASSEMBLER_STATEMENT_HPP

#include"forward.hpp"
#include"source.hpp"
#include"vocab_type.hpp"

template<typename Derived>
struct Statement { // CRTP
private:
    [[nodiscard]] inline constexpr Derived* derived_ptr() noexcept {
        return static_cast<Derived*>(this);
    }
    [[nodiscard]] inline constexpr const Derived* derived_ptr() const noexcept {
        return static_cast<const Derived*>(this);
    }

public:
    // static member
    inline static constexpr std::string_view name = Derived::name;
    inline static constexpr size_t serialized_size = Derived::serialized_size;
    [[nodiscard]] inline static Derived deserialized(const std::vector<std::byte>& bytes) {
        return Derived::deserialized(bytes);
    }

    // function
    inline constexpr void ensure(size_t index) const {
        if (index < this->size()) [[likely]] return;
        throw std::out_of_range(to_str(Derived::name, "::ensure index out of range"));
    } 
    [[nodiscard]] inline constexpr size_t size() const noexcept {
        return this->derived_ptr()->size();
    }
    [[nodiscard]] inline constexpr Token& operator[] (size_t index) {
        return this->derived_ptr()->operator[](index);
    }
    [[nodiscard]] inline constexpr const Token& operator[] (size_t index) const {
        return this->derived_ptr()->operator[](index);
    }
    [[nodiscard]] inline constexpr size_t to_tokens_index(size_t index) const {
        return this->derived_ptr()->to_tokens_index(index);
    }
    [[nodiscard]] inline std::vector<std::byte> serialized() const {
        return this->derived_ptr()->serialized();
    }

protected:
    // os operator template
    friend inline std::ostream& operator<< (std::ostream& os, const Statement& statement) {
        // Print the Type and the Kind
        os << "\tStatement(Kind=" << Derived::name << ") { ";
        bool is_first_token = true;
        // Print the tokens
        for (size_t i = 0; i < statement.size(); ++i) {
            const Token& token = statement[i];
            if (is_first_token) { is_first_token = false; } 
            else if (token.sep_by_space()) { os << ' '; }
            os << token.text;
        }
        // Close the brace
        return (os << " }");
    }
};

struct astOPENQASM : public Statement<astOPENQASM> {
    size_t openqasm  = npos;
    size_t version   = npos;
    size_t semicolon = npos;

    // static member
    inline static constexpr std::string_view name = "OPENQASM";
    inline static constexpr size_t serialized_size = sizeof(openqasm) + sizeof(version) + sizeof(semicolon);
    [[nodiscard]] inline static astOPENQASM deserialized(const std::vector<std::byte>& bytes) {
        // throw if the vector is too small
        if (bytes.size() < serialized_size) [[unlikely]] {
            throw std::invalid_argument("std::vector<std::byte> is too small for astOPENQASM");
        }
        // memcpy into obj
        size_t length;
        astOPENQASM obj;
        size_t offset = 0;
        length = sizeof(size_t);
        // deserialize in the same order
        std::memcpy(&obj.openqasm, bytes.data()+offset, length);
        offset += length;
        std::memcpy(&obj.version, bytes.data() + offset, length);
        offset += length;
        std::memcpy(&obj.semicolon, bytes.data() + offset, length);
        return obj;
    }

    // function
    [[nodiscard]] inline constexpr size_t size() const noexcept { return 3; }
    [[nodiscard]] inline constexpr Token& operator[] (size_t index) {
        return tokman->tokens[this->to_tokens_index(index)];
    }
    [[nodiscard]] inline constexpr const Token& operator[] (size_t index) const {
        return tokman->tokens[this->to_tokens_index(index)];
    }
    [[nodiscard]] inline constexpr size_t to_tokens_index(size_t index) const {
        this->ensure(index);
        switch (index) {
            case 0: return openqasm ;
            case 1: return version  ;
            case 2: return semicolon;
            default: throw std::out_of_range("unknown error");
        }
    }
    [[nodiscard]] inline std::vector<std::byte> serialized() const {
        size_t length;
        size_t offset = 0;
        length = sizeof(size_t);
        // allocate memory and memcpy
        std::vector<std::byte> bytes;
        bytes.resize(serialized_size);
        // three members (non-static)
        std::memcpy(bytes.data()+offset, &openqasm, length);
        offset += length;
        std::memcpy(bytes.data()+offset, &version, length);
        offset += length;
        std::memcpy(bytes.data()+offset, &semicolon, length);
        return bytes;
    }
};

#endif
