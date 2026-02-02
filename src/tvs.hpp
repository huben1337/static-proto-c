#pragma once

#include <algorithm>
#include <cstdint>
#include <gsl/pointers>
#include <gsl/util>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>


#include "./parser/lexer_types.hpp"
#include "./variant_optimizer/data.hpp"
#include "container/memory.hpp"
#include "estd/class_constraints.hpp"
#include "estd/empty.hpp"
#include "estd/ranges.hpp"
#include "estd/vector.hpp"
#include "math/mod1.hpp"
#include "nameof.hpp"
#include "parser/lexer_types.hpp"
#include "subset_sum_solving/dp_bitset_base.hpp"
#include "util/logger.hpp"
#include "helper/alloca.hpp"

struct AlignedFields {
    std::vector<uint16_t> idxs;
    uint64_t size_sum = 0;
};

enum class STATE_TYPE : uint8_t {
    TOP_LEVEL,
    FIXED_ARRAY_LEVEL,
    FIXED_VARIANT_LEVEL
};


template<lexer::SIZE max_align>
struct Fields : lexer::AlignMembersBase<AlignedFields, max_align.next_smaller(), Fields<max_align>> {};

template<>
struct Fields<lexer::SIZE::SIZE_1> : estd::empty {};

template <lexer::SIZE target_align>
void enqueueing_for_level (auto& level, const uint16_t idx) {
    level.template enqueue_for_level<target_align>(idx);    
}

template <lexer::SIZE target_align>
void enqueueing_for_level (auto& level, const std::vector<uint16_t>& idxs) {
    for (const uint16_t idx : idxs) {
        level.template enqueue_for_level<target_align>(idx);    
    }
}

template <lexer::SIZE to_align, lexer::SIZE max_align, typename... T>
static void add_to_align (
    auto& level,
    const uint16_t i,
    // QueuedField& field,
    Fields<max_align>& fields,
    AlignedFields& first,
    T&... rest
) {
    if constexpr (to_align == max_align) {
        enqueueing_for_level<max_align>(level, first.idxs);
        (enqueueing_for_level<max_align>(level, rest.idxs), ...);
        enqueueing_for_level<max_align>(level, i);
    } else {
        AlignedFields& target = fields.template get<to_align>();
        if (target.size_sum == 0) {
            BSSERT(target.idxs.size() == 0);
            if constexpr (sizeof...(T) > 0) {
                estd::move_append(first.idxs, 1, rest.idxs...);
                first.size_sum += (rest.size_sum + ...);
                ((rest.size_sum = 0), ...);
            }
            first.idxs.emplace_back(i);
            std::swap(target.idxs, first.idxs);
            std::swap(target.size_sum, first.size_sum);
        } else {
            BSSERT(target.idxs.size() != 0);
            add_to_align<lexer::next_bigger_size<to_align>>(level, i, fields, target, first, rest...);
        }
    }
};

// TODO: Make it possible to pass already dereferenced/indexed field instead of the index only
template<lexer::SIZE target_align>
static void add_field (
    auto& level,
    const uint16_t field_idx,
    Fields<target_align>& fields,
    const uint64_t field_size
) {
    if constexpr (target_align >= lexer::SIZE::SIZE_8) {
        if (field_size % 8 == 0) {
            if constexpr (target_align == lexer::SIZE::SIZE_8) {
                enqueueing_for_level<target_align>(level, field_idx);
            } else {
                fields.align8.size_sum += field_size;
                fields.align8.idxs.emplace_back(field_idx);
            }
            return;
        }
    }
    if constexpr (target_align >= lexer::SIZE::SIZE_4) {
        if (field_size % 4 == 0) {
            if constexpr (target_align == lexer::SIZE::SIZE_4) {
                enqueueing_for_level<target_align>(level, field_idx);
            } else {
                fields.align4.size_sum += field_size;
                if constexpr (target_align >= lexer::SIZE::SIZE_8) {
                    if (fields.align4.size_sum % 8 == 0) {
                        BSSERT(fields.align4.idxs.size() != 0);
                        add_to_align<lexer::SIZE::SIZE_8>(level, field_idx, fields, fields.align4);
                        return;
                    }
                }
                BSSERT(fields.align4.idxs.size() == 0);
                fields.align4.idxs.emplace_back(field_idx);
            }
            return;
        }
    }
    if constexpr (target_align >= lexer::SIZE::SIZE_2) {
        if (field_size % 2 == 0) {
            if constexpr (target_align == lexer::SIZE::SIZE_2) {
                enqueueing_for_level<target_align>(level, field_idx);
            } else {
                fields.align2.size_sum += field_size;
                if constexpr (target_align >= lexer::SIZE::SIZE_8) {
                    if (fields.align2.size_sum % 8 == 0) {
                        /*
                        In this case we could apply the field directly.
                        But better would be to check if we can split off a align4 part of the combination.
                        But i dont think that should ever be the case since the (new_size_sum % 4) branch should have caught that combintion ??!
                        Im not really sure. The align4 subset combination is not caught if the change bubbles up into here and adds a align4 combination as a whole,
                        which should not really happen.
                        */
                        add_to_align<lexer::SIZE::SIZE_8>(level, field_idx, fields, fields.align2);
                        return;
                    }
                }
                if constexpr (target_align >= lexer::SIZE::SIZE_4) {
                    if (fields.align2.size_sum % 4 == 0) {
                        add_to_align<lexer::SIZE::SIZE_4>(level, field_idx, fields, fields.align2);
                        return;
                    }
                }
                BSSERT(fields.align2.size_sum % 2 == 0);
                fields.align2.idxs.emplace_back(field_idx);
            }
            return;
        }
    }
    if constexpr (target_align == lexer::SIZE::SIZE_1) {
        enqueueing_for_level<target_align>(level, field_idx);
    } else {
        fields.align1.size_sum += field_size;
        if constexpr (target_align >= lexer::SIZE::SIZE_8) {
            if (fields.align1.size_sum % 8 == 0) {
                add_to_align<lexer::SIZE::SIZE_8>(level, field_idx, fields, fields.align1);
                return;
            }
        }
        if constexpr (target_align >= lexer::SIZE::SIZE_4) {
            if (fields.align1.size_sum % 4 == 0) {
                add_to_align<lexer::SIZE::SIZE_4>(level, field_idx, fields, fields.align1);
                return;
            }
        }
        if constexpr (target_align >= lexer::SIZE::SIZE_2) {
            if (fields.align1.size_sum % 2 == 0) {
                add_to_align<lexer::SIZE::SIZE_2>(level, field_idx, fields, fields.align1);
                return;
            } 
        }
        fields.align1.idxs.emplace_back(field_idx);
    }
}

struct Queued {
    std::vector<QueuedField> fields;
    uint64_t field_size_sum = 0;
    uint64_t modulated_field_size_sum = 0;

    void increment_sum (const uint64_t size) {
        field_size_sum += size;
        modulated_field_size_sum += math::mod1(size, lexer::SIZE::MAX.byte_size());
    } 
};

struct ConstStateBase {
    struct Shared {
        constexpr Shared (
            ReadOnlyBuffer                    ast_buffer,
            std::span<FixedOffset>            fixed_offsets,
            std::span<FixedOffset>            tmp_fixed_offsets,
            std::span<Buffer::View<uint64_t>> var_offsets,
            std::span<uint16_t>               idx_map,
            std::span<ArrayPackInfo>          pack_infos
        ) : ast_buffer        (ast_buffer),
            fixed_offsets     (fixed_offsets),
            tmp_fixed_offsets (tmp_fixed_offsets),
            var_offsets       (var_offsets),
            idx_map           (idx_map),
            pack_infos        (pack_infos) {}

        ReadOnlyBuffer                    ast_buffer;    // Buffer containing the AST
        std::span<FixedOffset>            fixed_offsets; // Represets the offset of each fixed size leaf.
        std::span<FixedOffset>            tmp_fixed_offsets;
        std::span<Buffer::View<uint64_t>> var_offsets;   // Represents the size of the variable size leaf. Used for genrating the offset calc strings
        std::span<uint16_t>               idx_map;       // Maps occurence in the AST to a stored leaf
        std::span<ArrayPackInfo>          pack_infos;
    };
};

struct MutableStateBase {
    struct Shared : estd::no_copy {
        Buffer   var_offset_buffer;             // Stores the size chains for variable size leaf offsets
        // uint16_t fixed_offset_idx_base = 0;  // The current base index for fixed sized leafs (maybe can be moved into LevelConstState if we know the total fixed leaf count including nested levels)
        uint16_t current_map_idx = 0;           // The current index into ConstState::idx_map
        uint16_t current_pack_info_idx = 0;

        constexpr explicit Shared (
            Buffer&& var_offset_buffer
        ) : var_offset_buffer(std::move(var_offset_buffer)) {}
    };

    struct TrivialLevel : estd::unique_only {
        Queued queued;
        uint64_t current_offset = 0;
        uint16_t fixed_offset_idx;
        uint16_t tmp_fixed_offset_idx;

        constexpr TrivialLevel (const uint16_t fixed_offset_idx, const uint16_t tmp_fixed_offset_idx)
            : fixed_offset_idx(fixed_offset_idx) ,tmp_fixed_offset_idx(tmp_fixed_offset_idx) {}

        [[nodiscard]] uint16_t next_fixed_offset_idx() {
            return fixed_offset_idx++;
        }
    };
};


// O(n * t) | 0 < t < MAX_SUM
inline void generate_sum_subset_chains (const uint64_t target, const std::vector<QueuedField>& queued_fields, const std::span<uint16_t> sum_chains) {
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    BSSERT(target != 0, "[subset_sum_perfect::solve] invalid target: ", target);
    constexpr uint16_t NO_CHAIN_VAL = -1;

    // const std::unique_ptr<uint16_t[]> sum_chains = std::make_unique_for_overwrite<uint16_t[]>(target + 1);
    sum_chains[0] = uint16_t{0};
    std::uninitialized_fill_n(sum_chains.data() + 1, target, NO_CHAIN_VAL);
    
    const uint16_t queue_size = queued_fields.size();
    // console.debug("Extracting from queue with length: ", queue_size);
    for (uint16_t queue_idx = 0; queue_idx < queue_size; queue_idx++) {
        const uint64_t num = math::mod1(queued_fields[queue_idx].size, lexer::SIZE::MAX.byte_size());
        // console.debug("Adding num ", num, " to sum subset");
        BSSERT(/*num != 0 && */num <= target, "num: ", num, ", target: ", target);
        if (num == 0) continue;
        // #pragma clang loop vectorize(enable)
        for (uint64_t i = target - num; ;) {
            const uint16_t old_chain_entry = sum_chains[i];
            if (old_chain_entry != NO_CHAIN_VAL) {
                uint16_t& new_chain_entry = sum_chains[i + num];
                if (new_chain_entry == NO_CHAIN_VAL) {
                    new_chain_entry = queue_idx;
                }
            }
            if (i == 0) break;
            i--;
        }
    
        if (sum_chains[target] != NO_CHAIN_VAL) return;
    }

    BSSERT(false, "[subset_sum_perfect::solve] reached unreachable. target: ", target);
    std::unreachable();
}


template <typename Derived, STATE_TYPE state_type, typename ConstState, typename MutableState>
struct AnyLevelStateBase {
    friend Derived;

    ConstState const_state;
    MutableState mutable_state;

private:
    constexpr AnyLevelStateBase (
        const ConstState& const_state,
        const MutableState& mutable_state
    ) : const_state(const_state),
        mutable_state(mutable_state) {}

public:
    [[nodiscard]] uint16_t next_map_idx () const {
        return mutable_state.shared().current_map_idx++;
    }

    [[nodiscard]] uint16_t next_pack_info_base_idx () const {
        uint16_t& current_pack_info_idx = mutable_state.shared().current_pack_info_idx;
        const uint16_t& base_idx = current_pack_info_idx;
        current_pack_info_idx +=4;
        return base_idx;
    }

    template <lexer::SIZE alignment>
    void next_array_pack (
        this const auto& self,
        const uint64_t size,
        const estd::integral_range<uint16_t> fixed_offset_idxs,
        const uint16_t pack_info_idx
    ) {
        if (size == 0) {
            BSSERT(fixed_offset_idxs.size() == 0);
            self.template skip<alignment>();
            return;
        }

        ArrayPackInfo& pack_info = self.const_state.shared().pack_infos[pack_info_idx];
        pack_info.size = size;

        self.template enqueue<alignment>(QueuedField{size, ArrayFieldPack{self.template move_to_tmp<alignment, true>(fixed_offset_idxs), pack_info_idx}});
    }

    template <lexer::SIZE alignment>
    void next_variant_packs (
        this const auto& self,
        const PendingVariantFieldPacks packs
    ) {
        self.template next_variant_pack<lexer::SIZE::SIZE_8>(packs.get<lexer::SIZE::SIZE_8>());
        self.template next_variant_pack<lexer::SIZE::SIZE_4>(packs.get<lexer::SIZE::SIZE_4>());
        self.template next_variant_pack<lexer::SIZE::SIZE_2>(packs.get<lexer::SIZE::SIZE_2>());
        self.template next_variant_pack<lexer::SIZE::SIZE_1>(packs.get<lexer::SIZE::SIZE_1>());
    }

    template <lexer::SIZE alignment>
    void next_variant_pack (
        this const auto& self,
        const std::pair<uint64_t, estd::integral_range<uint16_t>> pack
    ) {
        self.template next_variant_pack<alignment>(pack.first, pack.second);
    }

    template <lexer::SIZE alignment>
    void next_variant_pack (
        this const auto& self,
        const uint64_t size,
        const estd::integral_range<uint16_t> fixed_offset_idxs
    ) {
        // console.debug("Next variant pack: ", alignment, ", size: ", size);
        // BSSERT(size != 0);
        if (size == 0) {
            CSSERT(fixed_offset_idxs.size(), ==, 0, " ", alignment);
            self.template skip<alignment>();
            return;
        }

        self.template enqueue<alignment>(QueuedField{size, VariantFieldPack{self.template move_to_tmp<alignment, false>(fixed_offset_idxs), alignment}});
    }

    template <lexer::SIZE alignment>
    void next_simple (
        this const auto& self,
        const uint64_t count = 1
    ) {
        const uint16_t map_idx = self.next_map_idx();
        console.debug("using map_idx: ", map_idx);
        self.const_state.shared().idx_map[map_idx] = -1; // Mark as not set
        self.template enqueue<alignment>(QueuedField{alignment.byte_size() * count, SimpleField{map_idx, alignment}});
    }

    void next_simple (
        this const auto& self,
        const lexer::SIZE alignment, const uint64_t count = 1
    ) {
        switch (alignment) {
            case lexer::SIZE::SIZE_8: return self.template next_simple<lexer::SIZE::SIZE_8>(count);
            case lexer::SIZE::SIZE_4: return self.template next_simple<lexer::SIZE::SIZE_4>(count);
            case lexer::SIZE::SIZE_2: return self.template next_simple<lexer::SIZE::SIZE_2>(count);
            case lexer::SIZE::SIZE_1: return self.template next_simple<lexer::SIZE::SIZE_1>(count);
            default: std::unreachable();
        }
    }

    template <lexer::SIZE alignment, bool array_mode>
    [[nodiscard]] estd::integral_range<uint16_t> move_to_tmp (
        const estd::integral_range<uint16_t> fixed_offset_idxs
    ) const {
        const estd::integral_range tmp_fixed_offset_idxs {
            mutable_state.level().tmp_fixed_offset_idx,
            fixed_offset_idxs.wrapped_size()
        };
        mutable_state.level().tmp_fixed_offset_idx = *tmp_fixed_offset_idxs.end();

        const std::ranges::subrange target_tmp_fixed_offsets = tmp_fixed_offset_idxs.access_subrange(const_state.shared().tmp_fixed_offsets);

        console.debug("tmp_fixed_offsets[", *tmp_fixed_offset_idxs.begin(), " .. ", *tmp_fixed_offset_idxs.end(), "] = fixed_offsets[", *fixed_offset_idxs.begin(), " .. ", *fixed_offset_idxs.end(), "] alignment: ", alignment, " (tt)");

        for (const uint16_t idx : tmp_fixed_offset_idxs) {
            BSSERT(const_state.shared().tmp_fixed_offsets[idx] == FixedOffset::empty(), idx);
        }

        std::ranges::swap_ranges(
            fixed_offset_idxs.access_subrange(const_state.shared().fixed_offsets),
            target_tmp_fixed_offsets
        );

        for (const FixedOffset& fo : target_tmp_fixed_offsets) {
            // if (fo.get_pack_align() > alignment) {
            //     console.warn("Detected possible downgrade of field: ", fo.get_pack_align(), " to ", alignment, " (tt)");
            // }
            if constexpr (array_mode) {
                BSSERT(fo.get_pack_align() <= alignment);
            } else {
                BSSERT(fo.get_pack_align() <= alignment);
            }
            
        }

        return tmp_fixed_offset_idxs;
    }
};

template <typename Derived, STATE_TYPE state_type, typename ConstState, typename MutableState>
struct TrivialLevelStateBase : AnyLevelStateBase<TrivialLevelStateBase<Derived, state_type, ConstState, MutableState>, state_type, ConstState, MutableState> {
    friend Derived;

    using Base = AnyLevelStateBase<TrivialLevelStateBase<Derived, state_type, ConstState, MutableState>, state_type, ConstState, MutableState>;

private:
    constexpr TrivialLevelStateBase (
        const ConstState& const_state,
        const MutableState& mutable_state
    ) : Base{const_state, mutable_state} {}
    
public:
    using Base::const_state;
    using Base::mutable_state;

    [[nodiscard]] const uint16_t& get_fixed_offset_idx() const {
        return mutable_state.level().fixed_offset_idx;
    }

    template <lexer::SIZE alignment>
    void enqueue (
        this const auto& self,
        const QueuedField field
    ) {
        if constexpr (alignment == lexer::SIZE::MAX) {
            self.template enqueue_for_level_<alignment, false>(field);
            self.template decrement_left_fields<alignment>();
        } else {
            self.mutable_state.level().queued.increment_sum(field.size);
            self.mutable_state.level().queued.fields.emplace_back(field);
            self.template try_solve_queued<alignment>();
        }
    }

    template <lexer::SIZE alignment>
    void skip (this const auto& self) {
        self.template skip<alignment>();
    }

    template<lexer::SIZE target_align>
    [[nodiscard]] constexpr uint64_t find_target () const {
        constexpr uint8_t target_align_byte_size = target_align.byte_size();
        const uint64_t queued_sum = mutable_state.level().queued.field_size_sum;
        if (queued_sum < target_align_byte_size) return 0;
        const uint64_t modulated_queued_sum = mutable_state.level().queued.modulated_field_size_sum;
        const dp_bitset_base::num_t bitset_words_count = dp_bitset_base::bitset_word_count(modulated_queued_sum);
        ALLOCA_UNSAFE_SPAN(bitset_words, dp_bitset_base::word_t, bitset_words_count)
        dp_bitset_base::init_bits(bitset_words.data(), bitset_words_count);
        for (const auto& e : mutable_state.level().queued.fields) {
            BSSERT(e.size != 0);
            dp_bitset_base::apply_num_unsafe(math::mod1(e.size, lexer::SIZE::MAX.byte_size()), bitset_words.data(), bitset_words_count);
        }
        uint64_t target = lexer::last_multiple(modulated_queued_sum, target_align);
        for (;;) {
            if (dp_bitset_base::bit_at(bitset_words.data(), target)) {
                return target;
            }
            if (target <= target_align_byte_size) return 0;
            target -= target_align_byte_size;
        }
    }

    template <lexer::SIZE target_align, bool set_field_size_zero>
    void enqueue_for_level_ (estd::conditional_const_t<!set_field_size_zero, QueuedField>& field) const {
        MutableStateBase::TrivialLevel& level_mutable_state = mutable_state.level();
        std::visit([this, &level_mutable_state]<typename T>(const T& arg) {
            const ConstStateBase::Shared& shared_const_state = const_state.shared();
            if constexpr (std::is_same_v<SimpleField, T>) {
                const uint16_t map_idx = arg.map_idx;
                const uint16_t fixed_offset_idx = level_mutable_state.next_fixed_offset_idx();
                const FixedOffset fo {level_mutable_state.current_offset, map_idx, target_align};
                console.debug("fixed_offsets[", fixed_offset_idx, "] = ", fo);
                FixedOffset& out = shared_const_state.fixed_offsets[fixed_offset_idx];
                BSSERT(out == FixedOffset::empty());
                out = fo;
                if constexpr (state_type == STATE_TYPE::TOP_LEVEL) {
                    console.debug("idx_map[", map_idx, "] = ", fixed_offset_idx);
                    uint16_t& idx_out = shared_const_state.idx_map[map_idx];
                    BSSERT(idx_out == static_cast<uint16_t>(-1));
                    idx_out = fixed_offset_idx;
                }
            } else if constexpr (std::is_same_v<ArrayFieldPack, T> || std::is_same_v<VariantFieldPack, T>) {
                if constexpr (std::is_same_v<ArrayFieldPack, T>) {
                    ArrayPackInfo& pack_info = shared_const_state.pack_infos[arg.pack_info_idx];
                    if constexpr (state_type == STATE_TYPE::TOP_LEVEL) {
                        pack_info.parent_idx = static_cast<uint16_t>(-1);
                    } else {
                        pack_info.parent_idx = const_state.level().pack_info_base_idx + target_align.value;
                    }
                }
                const estd::integral_range<uint16_t>& tmp_fixed_offset_idxs = arg.tmp_fixed_offset_idxs;
                console.debug(estd::conditionally<std::is_same_v<ArrayFieldPack, T>>("ArrayFieldPack "_sl, "VariantFieldPack "_sl) + "idxs: {from: "_sl, *tmp_fixed_offset_idxs.begin(),
                    ", to: ", *tmp_fixed_offset_idxs.end(), "}, target align: ", target_align);
                console.debug("tmp_fixed_offsets[", *tmp_fixed_offset_idxs.begin(), " .. ", *tmp_fixed_offset_idxs.end(), "] = FixedOffset::empty() target_align: ", target_align, " (sq)");
                for (const uint16_t idx : tmp_fixed_offset_idxs) {
                    FixedOffset& tmp = shared_const_state.tmp_fixed_offsets[idx];
                    CSSERT(tmp.pack_align, <=, target_align, "Cant downgrade alignment");
                    // if (tmp.pack_align > target_align) {
                    //     console.warn("Detected possible downgrade of field: ", tmp.pack_align, " to ", target_align, " (sq)");
                    // }
                    const uint16_t map_idx = tmp.map_idx;
                    const uint16_t fixed_offset_idx = level_mutable_state.next_fixed_offset_idx();
                    const FixedOffset fo {tmp.offset + level_mutable_state.current_offset, map_idx, tmp.pack_align};
                    console.debug("fixed_offsets[", fixed_offset_idx, "] = ", fo);
                    FixedOffset& out = shared_const_state.fixed_offsets[fixed_offset_idx];
                    BSSERT(out == FixedOffset::empty());
                    out = fo;
                    if constexpr (state_type == STATE_TYPE::TOP_LEVEL) {
                        console.debug("idx_map[", map_idx, "] = ", fixed_offset_idx, " (sq)");
                        uint16_t& idx_out = shared_const_state.idx_map[map_idx];
                        BSSERT(idx_out == static_cast<uint16_t>(-1));
                        idx_out = fixed_offset_idx;
                    }
                    tmp = FixedOffset::empty();
                }
            } else {
                static_assert(false);
            }
        }, field.info);
        BSSERT(field.size != 0);
        level_mutable_state.current_offset += field.size;
        // TODO: Maybe implement this functionality without this NTTP
        if constexpr (set_field_size_zero) {
            // Mark for deletion from queue.
            field.size = 0;
        }
    }

    template <lexer::SIZE target_align>
    void enqueue_for_level (const uint16_t idx) const {
        MutableStateBase::TrivialLevel& level_mutable_state = mutable_state.level();
        enqueue_for_level_<target_align, true>(level_mutable_state.queued.fields[idx]);
    }


    template<lexer::SIZE target_align>
    requires (target_align != lexer::SIZE::SIZE_0)
    void try_solve_queued_for_align () const {
        MutableStateBase::TrivialLevel& level_mutable_state = mutable_state.level();
        
        const uint64_t target = find_target<target_align>();
        if (target == 0) return;
        console.debug("next_leaf enquing batch of size: ", target);

        ALLOCA_UNSAFE_SPAN(sum_chains, uint16_t, target + 1);
        generate_sum_subset_chains(target, level_mutable_state.queued.fields, sum_chains);

        Fields<target_align> fields;

        uint64_t chain_idx = target;
        do {
            const uint16_t field_idx = sum_chains[chain_idx];
            QueuedField& field = level_mutable_state.queued.fields[field_idx];
            const uint64_t field_size = field.size;
            console.debug("used field: ", field_size, " target align: ", target_align);
            add_field(*this, field_idx, fields, field_size);
            const auto modulated_field_size = math::mod1(field_size, lexer::SIZE::MAX.byte_size());
            level_mutable_state.queued.field_size_sum -= field_size;
            level_mutable_state.queued.modulated_field_size_sum -= modulated_field_size;
            chain_idx -= modulated_field_size;
        } while (chain_idx > 0);

        console.debug("enqueueing for level: ", NAMEOF_ENUM(state_type), ", target align: ", target_align);
        // BSSERT(fields.template get<target_align>().idxs.size() == 0);
        // for (const uint16_t idx : fields.template get<target_align>().idxs) {
        //     
        // }

        std::erase_if(level_mutable_state.queued.fields, [](const QueuedField& e) { return e.size == 0; });

        // We should have inserted all possible field from queue
        BSSERT(find_target<target_align>() == 0);
    }

    void try_solve_queued_for_align (const lexer::SIZE target_align) const {
        switch (target_align) {
            case lexer::SIZE::SIZE_8: return try_solve_queued_for_align<lexer::SIZE::SIZE_8>();
            case lexer::SIZE::SIZE_4: return try_solve_queued_for_align<lexer::SIZE::SIZE_4>();
            case lexer::SIZE::SIZE_2: return try_solve_queued_for_align<lexer::SIZE::SIZE_2>();
            case lexer::SIZE::SIZE_1: return try_solve_queued_for_align<lexer::SIZE::SIZE_1>();
            default: std::unreachable();
        }
    }
};

struct TopLevel {
    struct ConstState : ConstStateBase {
        struct Level {
            std::span<uint64_t> var_leaf_sizes;
            std::span<uint16_t> size_leafe_idxs;    // Since varirable sized leafs also are sorted by alignment we need this mapping to their insertion order

            [[nodiscard]] static constexpr uint16_t get_pack_info_base_idx() {
                return static_cast<uint16_t>(-1);
            }
        };
    private:
        Shared shared_data;
        Level level_data;
    public:
        constexpr ConstState (const Shared& shared, const Level& level)
            : shared_data(shared), level_data(level) {}

        [[nodiscard]] const Shared& shared() const { return shared_data; }
        [[nodiscard]] const Level& level() const { return level_data; }
    };

    struct MutableState : MutableStateBase {
        struct Level : TrivialLevel {
            constexpr Level (
                const lexer::LeafCounts::Counts left_fields,
                lexer::LeafCounts::Counts var_leaf_positions
            ) : TrivialLevel{0, 0}, left_fields(left_fields), var_leaf_positions(var_leaf_positions) {}
            
            lexer::LeafCounts::Counts left_fields;
            lexer::LeafCounts::Counts var_leaf_positions;
            uint16_t current_size_leaf_idx = 0;
        };

        struct Data {
            Shared shared;
            Level level;
        };

    private:
        gsl::not_null<Data*> data;
        
    public:
        constexpr explicit MutableState (Data& data)
            : data(&data) {}

        [[nodiscard]] Shared& shared() const { return data->shared; }
        [[nodiscard]] Level& level() const { return data->level; }
    };
    
    struct State : TrivialLevelStateBase<State, STATE_TYPE::TOP_LEVEL, ConstState, MutableState> {

        constexpr State (
            const ConstState& const_state,
            const MutableState& mutable_state
        ) : TrivialLevelStateBase{const_state, mutable_state} {}

        template <lexer::SIZE alignment>
        void skip () const {
            uint16_t& c = mutable_state.level().left_fields.get<alignment>();
            if (c == 1) {
                // There will never be triggered a solve attempt for this alignment again so we need to trigger it manually.
                try_solve_queued<alignment>();
                return;
            }
            BSSERT(c != 0);
            c--;
        }

        template <lexer::SIZE alignment>
        void decrement_left_fields () const {
            uint16_t& c = mutable_state.level().left_fields.get<alignment>();
            BSSERT(c != 0);
            c--;
        }

        template <lexer::SIZE alignment>
        void try_solve_queued () const {
            const lexer::SIZE largest_align = mutable_state.level().left_fields.largest_align();
            console.debug("[TopLevel::try_solve_queued] alignment: ", alignment, ", left_fields: ", mutable_state.level().left_fields);
            decrement_left_fields<alignment>();
            console.debug("[TopLevel::try_solve_queued] largest_align: ", largest_align);
            try_solve_queued_for_align(largest_align);
        }

        template <lexer::SIZE alignment>
        [[nodiscard]] uint16_t next_var_leaf_idx () const {
            const uint16_t idx = mutable_state.level().var_leaf_positions.get<alignment>()++;
            const uint16_t size_leaf_idx = mutable_state.level().current_size_leaf_idx;
            console.debug("size_leafe_idxs[", idx, "] = ", size_leaf_idx);
            const_state.level().size_leafe_idxs[idx] = size_leaf_idx;
            return idx;
        }

        template <lexer::SIZE alignment>
        void next_simple_var () const {
            const uint16_t idx = next_var_leaf_idx<alignment>();
            const_state.level().var_leaf_sizes[idx] = alignment.byte_size();
            const_state.shared().idx_map[next_map_idx()] = idx;
        }
    };
};


struct FixedArrayLevel {
    struct ConstState : ConstStateBase {
        struct Level {
            uint16_t pack_info_base_idx;

            [[nodiscard]] const uint16_t& get_pack_info_base_idx() const {
                return pack_info_base_idx;
            }
        };

    private:
        Shared shared_data;
        Level level_data;
    
    public:
        constexpr ConstState (const Shared& shared, const Level& level)
            : shared_data(shared), level_data(level) {}

        [[nodiscard]] const Shared& shared() const { return shared_data; }
        [[nodiscard]] const Level& level() const { return level_data; }
    };

    struct MutableState : MutableStateBase {
        struct Level : TrivialLevel {
            using TrivialLevel::TrivialLevel;
        };

    private:
        gsl::not_null<Shared*> shared_data;
        gsl::not_null<Level*> level_data;

    public:
        constexpr MutableState (Shared& shared, Level& level)
            : shared_data(&shared), level_data(&level) {}

        [[nodiscard]] Shared& shared() const { return *shared_data; }
        [[nodiscard]] Level& level() const { return *level_data; }
    };

    struct State : TrivialLevelStateBase<State, STATE_TYPE::FIXED_ARRAY_LEVEL, ConstState, MutableState> {
        constexpr State (
            const ConstState& const_state,
            const MutableState& mutable_state
        ) : TrivialLevelStateBase{const_state, mutable_state} {}

        template <lexer::SIZE alignment>
        void skip () const {}

        template <lexer::SIZE>
        consteval void decrement_left_fields () const {}

        template <lexer::SIZE>
        void try_solve_queued () const {
            console.debug("[FixedArrayLevel::try_solve_queued]");
            try_solve_queued_for_align<lexer::SIZE::SIZE_8>();
        }
    };
};

struct FixedVariantLevel {
    struct ConstState : ConstStateBase {
        struct Level {
            std::span<QueuedField> queued; // Only used in variants
            uint16_t fixed_offset_idx;
            uint16_t pack_info_base_idx;

            [[nodiscard]] const uint16_t& get_pack_info_base_idx() const {
                return pack_info_base_idx;
            }
        };

    private:
        Shared shared_data;
        Level level_data;

    public:
        constexpr ConstState (const Shared& shared, const Level& level)
            : shared_data(shared), level_data(level) {}

        [[nodiscard]] const Shared& shared() const { return shared_data; }
        [[nodiscard]] const Level& level() const { return level_data; }
    };

    struct MutableState : MutableStateBase {
        struct Level {
            lexer::LeafSizes used_spaces;
            lexer::LeafCounts::Counts non_zero_fields_counts;
            uint16_t queue_position;
            uint16_t tmp_fixed_offset_idx;
        };

    private:
        gsl::not_null<Shared*> shared_data;
        gsl::not_null<Level*> level_data;
    
    public:
        constexpr MutableState (Shared& shared, Level& level)
            : shared_data(&shared), level_data(&level) {}
    
        [[nodiscard]] Shared& shared() const { return *shared_data; }
        [[nodiscard]] Level& level() const { return *level_data; }
    };
    
    struct State : AnyLevelStateBase<State, STATE_TYPE::FIXED_VARIANT_LEVEL, ConstState, MutableState> {
        constexpr State (
            const ConstState& const_state,
            const MutableState& mutable_state
        ) : AnyLevelStateBase{const_state, mutable_state} {}

        [[nodiscard]] const uint16_t& get_fixed_offset_idx() const {
            return const_state.level().fixed_offset_idx;
        }

        template <lexer::SIZE alignment>
        void skip () const {}

        template <lexer::SIZE alignment>
        void enqueue (const QueuedField field) const {
            console.debug("[FixedVariantLevel::enqueue] alignment: ", alignment);
            const uint16_t idx = mutable_state.level().queue_position++;
            CSSERT(idx, <, const_state.level().queued.size());
            const_state.level().queued[idx] = field;
            mutable_state.level().used_spaces.get<alignment>() += field.size;
            mutable_state.level().non_zero_fields_counts.get<alignment>()++;
        }
    };
};
