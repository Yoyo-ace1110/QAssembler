#ifndef YOYO_ACE1110_QASSEMBLER_AST_HPP
#define YOYO_ACE1110_QASSEMBLER_AST_HPP

#include"forward.hpp"
#include"source.hpp"
#include"vocab_type.hpp"

#include"statement.hpp"

struct ASTNode {
    uint8_t type_id = 255;
    size_t front_index = npos;

    [[nodiscard]] inline constexpr size_t offset() const noexcept {
        return front_index;
    }
    [[nodiscard]] inline constexpr size_t length() const noexcept {
        if (front_index == npos || type_id == 255) [[unlikely]] return 0;
        // cast type_id to Type
        switch (type_id) {
            case astOPENQASM::ID: return astOPENQASM::serialized_size;
            case astInclude ::ID: return astInclude ::serialized_size;
            case astQRegDecl::ID: return astQRegDecl::serialized_size;
            case astCRegDecl::ID: return astCRegDecl::serialized_size;
            case astMeasure ::ID: return astMeasure ::serialized_size;
            case astReset   ::ID: return astReset   ::serialized_size;
            case astBarrier ::ID: return astBarrier ::serialized_size;
            case astGateCall::ID: return astGateCall::serialized_size;
            case astOpaque  ::ID: return astOpaque  ::serialized_size;
            default: return 0;
        }
    }
};

class AST {
private:
    friend struct ASTNode;
    std::vector<ASTNode>   nodes;
    std::vector<std::byte> bytes;

public:
    template<typename T, typename... Args>
    requires (is_Statement_c<T>)
    inline void emplace_back(Args&&... args) {
        nodes.emplace_back(bytes.size(), T::ID);
        T::serialize_to(bytes, std::forward<Args>(args)...);
    }
    
    template<typename T>
    requires (is_Statement_c<T>)
    inline const T get(size_t i) const {
        size_t offset = nodes[i].offset();
        size_t length = nodes[i].length();
        const std::byte* start = bytes.data()+offset;
        return T::deserialized(std::span(start, length));
    }
};

#endif
