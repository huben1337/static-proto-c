#pragma once

#include <ranges>
#include "./dp_bitset_base.hpp"
#include "../variant_optimizer/data.hpp"

namespace sum_intersection_dp_bitset {

using dp_bitset_base::word_t, dp_bitset_base::num_t;

template <lexer::SIZE alignment>
[[gnu::always_inline, nodiscard]] inline const FixedLeaf* generate_bits_from_fixed_leafs (
    word_t* const bits,
    const num_t bitset_words,
    const num_t target, // only for assert since we should never have a num bigger then target as target is the sum of all nums
    const FixedLeaf* const fixed_leafs_buffer,
    const FixedLeaf* it,
    const VariantLeafMeta& meta
) {
    const FixedLeaf* const end = fixed_leafs_buffer + meta.fixed_leafs_ends.get<alignment>();
    for (; it != end; it++) {
        auto num = it->get_size();
        logger::debug("[generate_bits] num: ", num);
        if (num == 0) continue;
        BSSERT(num <= target);
        dp_bitset_base::apply_num_unsafe(num, bits, bitset_words);
    }
    return it;
}

template <lexer::SIZE alignment>
[[gnu::always_inline]] inline void generate_bits_from_variant_fields (
    word_t* const bits,
    const num_t bitset_words,
    const num_t target, // only for assert since we should never have a num bigger then target as target is the sum of all nums
    const std::ranges::subrange<const VariantField*> variant_fields
) {
    for (const VariantField& variant_field : variant_fields) {
        auto num = variant_field.sizes.get<alignment>();
        if (num == 0) continue;
        BSSERT(num <= target);
        dp_bitset_base::apply_num_unsafe(num, bits, bitset_words);
    }
}


inline void generate_bits (
    word_t* const bits,
    const num_t bitset_words,
    const num_t target,
    const FixedLeaf* const fixed_leafs_buffer,
    const VariantField* const variant_fields_buffer,
    const VariantLeafMeta& meta
) {

    dp_bitset_base::init_bits(bits, bitset_words);

    const std::ranges::subrange variant_fields {
        variant_fields_buffer + meta.variant_field_idxs.from,
        variant_fields_buffer + meta.variant_field_idxs.to
    };

    const FixedLeaf* it = fixed_leafs_buffer + meta.fixed_leafs_start;
    generate_bits_from_variant_fields  <lexer::SIZE::SIZE_8>(bits, bitset_words, target, variant_fields);
    it = generate_bits_from_fixed_leafs<lexer::SIZE::SIZE_8>(bits, bitset_words, target, fixed_leafs_buffer, it, meta);
    generate_bits_from_variant_fields  <lexer::SIZE::SIZE_4>(bits, bitset_words, target, variant_fields);
    it = generate_bits_from_fixed_leafs<lexer::SIZE::SIZE_4>(bits, bitset_words, target, fixed_leafs_buffer, it, meta);
    generate_bits_from_variant_fields  <lexer::SIZE::SIZE_2>(bits, bitset_words, target, variant_fields);
    it = generate_bits_from_fixed_leafs<lexer::SIZE::SIZE_2>(bits, bitset_words, target, fixed_leafs_buffer, it, meta);
    generate_bits_from_variant_fields  <lexer::SIZE::SIZE_1>(bits, bitset_words, target, variant_fields);
    std::ignore = generate_bits_from_fixed_leafs<lexer::SIZE::SIZE_1>(bits, bitset_words, target, fixed_leafs_buffer, it, meta);
}

} // namespace sum_intersection_dp_bitset