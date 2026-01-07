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
#include "estd/ranges.hpp"
#include "estd/vector.hpp"
#include "math/mod1.hpp"
#include "nameof.hpp"
#include "parser/lexer_types.hpp"
#include "subset_sum_solving/dp_bitset_base.hpp"
#include "util/logger.hpp"


struct AlignedFields {
    std::vector<uint16_t> idxs;
    uint64_t size_sum = 0;
};

enum class STATE_TYPE : uint8_t {
    TOP_LEVEL,
    FIXED_ARRAY_LEVEL,
    FIXED_VARIANT_LEVEL
};


template<lexer::SIZE max_align = lexer::SIZE::SIZE_8>
using Fields = lexer::AlignMembersBase<AlignedFields, max_align>;

using PackFieldIdxs = lexer::AlignMembersBase<estd::integral_range<uint16_t>>;

struct Queued {
    fields_t fields;
    uint64_t modulated_field_size_sums = 0;
};

struct ConstStateBase {
    struct Shared {
        constexpr Shared (
            ReadOnlyBuffer                    ast_buffer,
            std::span<FixedOffset>            fixed_offsets,
            std::span<FixedOffset>            tmp_fixed_offsets,
            std::span<Buffer::View<uint64_t>> var_offsets,
            std::span<uint16_t>               idx_map,
            std::span<ArrayPackInfo>               pack_infos
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
        std::span<ArrayPackInfo>               pack_infos;
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
[[nodiscard]] inline fields_t extract_sum_subset_from_queue (const uint64_t target, Queued& queued) {
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    BSSERT(target != 0, "[subset_sum_perfect::solve] invalid target: ", target);
    constexpr uint16_t NO_CHAIN_VAL = -1;

    const std::unique_ptr<uint16_t[]> sum_chains = std::make_unique_for_overwrite<uint16_t[]>(target + 1);
    sum_chains[0] = uint16_t{0};
    std::uninitialized_fill_n(sum_chains.get() + 1, target, NO_CHAIN_VAL);
    
    const uint16_t queue_size = queued.fields.size();
    // console.debug("Extracting from queue with length: ", queue_size);
    for (uint16_t queue_idx = 0; queue_idx < queue_size; queue_idx++) {
        const uint64_t num = math::mod1(queued.fields[queue_idx].size, lexer::SIZE::MAX.byte_size());
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
    

        if (sum_chains[target] != NO_CHAIN_VAL) {

            fields_t used_fields;

            uint64_t chain_idx = target;
            do {
                uint16_t link = sum_chains[chain_idx];
                QueuedField& field = queued.fields[link];
                const auto modulated_field_size = math::mod1(field.size, lexer::SIZE::MAX.byte_size());
                queued.modulated_field_size_sums -= modulated_field_size;
                chain_idx -= modulated_field_size;
                used_fields.push_back(field);
                field.size = 0; // Mark for deletion from queue
            } while (chain_idx > 0);

            std::erase_if(queued.fields, [](const QueuedField& e) { return e.size == 0; });

            return used_fields;
        }
    }

    BSSERT(false, "[subset_sum_perfect::solve] reached unreachable. target: ", target);
    std::unreachable();
}

template <lexer::SIZE to_align, lexer::SIZE max_align, typename... T>
inline void add_to_align (uint16_t i, Fields<max_align>& fields, AlignedFields& first, T&... rest) {
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
        if constexpr (to_align == max_align) {
            estd::move_append(target.idxs, 0, first.idxs, rest.idxs..., i);
            target.size_sum += (first.size_sum + (0 + ... + rest.size_sum));
            first.size_sum = 0;
            ((rest.size_sum = 0), ...);
        } else {
            // NOLINTNEXTLINE(readability-suspicious-call-argument)
            add_to_align<lexer::next_bigger_size<to_align>>(i, fields, target, first, rest...);
        }
    }
};

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
            // self.template skip<alignment>();
            // return;
        }

        ArrayPackInfo& pack_info = self.const_state.shared().pack_infos[pack_info_idx];
        pack_info.size = size;

        self.template enqueue<alignment>(QueuedField{size, ArrayFieldPack{self.template move_to_tmp<alignment, true>(fixed_offset_idxs), pack_info_idx}});
    }

    template <lexer::SIZE alignment>
    void next_variant_pack (
        this const auto& self,
        const uint64_t size,
        const estd::integral_range<uint16_t> fixed_offset_idxs
    ) {
        // BSSERT(size != 0);
        if (size == 0) {
            BSSERT(fixed_offset_idxs.size() == 0);
            // self.template skip<alignment>();
            // return;
        }

        self.template enqueue<alignment>(QueuedField{size, VariantFieldPack{self.template move_to_tmp<alignment, false>(fixed_offset_idxs)}});
    }

    template <lexer::SIZE alignment>
    void next_simple (
        this const auto& self,
        const uint64_t count = 1
    ) {
        const uint16_t map_idx = self.next_map_idx();
        console.debug("using map_idx: ", map_idx);
        self.const_state.shared().idx_map[map_idx] = -1; // Mark as not set
        static_cast<const Derived&>(self).template enqueue<alignment>(QueuedField{alignment.byte_size() * count, SimpleField{map_idx}});
    }

    void next_simple (const lexer::SIZE alignment, const uint64_t count = 1) const {
        switch (alignment) {
            case lexer::SIZE::SIZE_8: return next_simple<lexer::SIZE::SIZE_8>(count);
            case lexer::SIZE::SIZE_4: return next_simple<lexer::SIZE::SIZE_4>(count);
            case lexer::SIZE::SIZE_2: return next_simple<lexer::SIZE::SIZE_2>(count);
            case lexer::SIZE::SIZE_1: return next_simple<lexer::SIZE::SIZE_1>(count);
            default: std::unreachable();
        }
    }

    template <lexer::SIZE alignment, bool array_mode>
    [[nodiscard]] estd::integral_range<uint16_t> move_to_tmp_ (
        const estd::integral_range<uint16_t> fixed_offset_idxs
    ) const {
        const std::span<FixedOffset>& fixed_offsets = const_state.shared().fixed_offsets;
        const std::span<FixedOffset>& tmp_fixed_offsets = const_state.shared().tmp_fixed_offsets;

        const uint16_t fixed_offset_idxs_count = fixed_offset_idxs.size();
        const uint16_t tmp_fixed_offset_idx_begin = mutable_state.level().tmp_fixed_offset_idx;
        const uint16_t tmp_fixed_offset_idx_end = tmp_fixed_offset_idx_begin + fixed_offset_idxs_count;
        mutable_state.level().tmp_fixed_offset_idx = tmp_fixed_offset_idx_end;


        const estd::integral_range tmp_fixed_offset_idxs {
            tmp_fixed_offset_idx_begin,
            tmp_fixed_offset_idx_end
        };

        for (const uint16_t idx : tmp_fixed_offset_idxs) {
            BSSERT(tmp_fixed_offsets[idx] == FixedOffset::empty(), idx);
        }

        console.debug("tmp_fixed_offsets[", *tmp_fixed_offset_idxs.begin(), " .. ", *tmp_fixed_offset_idxs.end(), "] = FixedOffset{?} alignment: ", alignment, " (tt)");

        std::copy(
            fixed_offsets.data() + *fixed_offset_idxs.begin(),
            fixed_offsets.data() + *fixed_offset_idxs.end(),
            tmp_fixed_offsets.data() + *tmp_fixed_offset_idxs.begin()
        );

        for (const uint16_t idx : fixed_offset_idxs) {
            fixed_offsets[idx] = FixedOffset::empty();
        }

        for (const uint16_t idx : tmp_fixed_offset_idxs) {
            if constexpr (array_mode) {
                BSSERT(tmp_fixed_offsets[idx].get_pack_align() <= alignment);
            } else {
                BSSERT(tmp_fixed_offsets[idx].get_pack_align() == alignment);
            }
            
        }

        return tmp_fixed_offset_idxs;
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

        const std::ranges::subrange target_tmp_fixed_offsets {
            const_state.shared().tmp_fixed_offsets.begin() + *tmp_fixed_offset_idxs.begin(),
            const_state.shared().tmp_fixed_offsets.begin() + *tmp_fixed_offset_idxs.end(),
        };

        for (const FixedOffset& fo : target_tmp_fixed_offsets) {
            BSSERT(fo == FixedOffset::empty());
        }

        console.debug("tmp_fixed_offsets[", *tmp_fixed_offset_idxs.begin(), " .. ", *tmp_fixed_offset_idxs.end(), "] = FixedOffset{?} alignment: ", alignment, " (tt)");

        std::swap_ranges(
            const_state.shared().fixed_offsets.begin() + *fixed_offset_idxs.begin(),
            const_state.shared().fixed_offsets.begin() + *fixed_offset_idxs.end(),
            target_tmp_fixed_offsets.begin()
        );

        for (const FixedOffset& fo : target_tmp_fixed_offsets) {
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
        self.mutable_state.level().queued.modulated_field_size_sums += math::mod1(field.size, lexer::SIZE::MAX.byte_size());
        self.mutable_state.level().queued.fields.emplace_back(field);
        static_cast<const Derived&>(self).template try_solve_queued<alignment>();
    }

    template <lexer::SIZE alignment>
    void skip (this const auto& self) {
        self.template skip<alignment>();
    }

    template<lexer::SIZE target_align>
    [[nodiscard]] constexpr uint64_t find_target () const {
        const uint64_t queued_sum = mutable_state.level().queued.modulated_field_size_sums;
        constexpr uint8_t target_align_byte_size = target_align.byte_size();
        if (queued_sum < target_align_byte_size) return 0;
        const dp_bitset_base::num_t bitset_words_count = dp_bitset_base::bitset_word_count(queued_sum);
        const std::unique_ptr<dp_bitset_base::word_t[]> bitset_words = std::make_unique_for_overwrite<dp_bitset_base::word_t[]>(bitset_words_count);
        dp_bitset_base::init_bits(bitset_words.get(), bitset_words_count);
        for (const auto& e : mutable_state.level().queued.fields) {
            dp_bitset_base::apply_num_unsafe(math::mod1(e.size, lexer::SIZE::MAX.byte_size()), bitset_words.get(), bitset_words_count);
        }
        uint64_t target = lexer::last_multiple(queued_sum, target_align);
        for (;;) {
            if (dp_bitset_base::bit_at(bitset_words.get(), target)) {
                return target;
            }
            if (target <= target_align_byte_size) return 0;
            target -= target_align_byte_size;
        }
    }

    template<lexer::SIZE target_align>
    requires (target_align != lexer::SIZE::SIZE_0)
    void try_solve_queued_for_align () const {
        MutableStateBase::TrivialLevel& level_mutable_state = mutable_state.level();
        
        const uint64_t target = find_target<target_align>();
        if (target == 0) return;
        console.debug("next_leaf enquing batch of size: ", target);
        const fields_t used_fields = extract_sum_subset_from_queue(target, level_mutable_state.queued);

        Fields<target_align> fields;
        
        const uint16_t used_fields_size = gsl::narrow_cast<uint16_t>(used_fields.size());
        for (uint16_t i = 0; i < used_fields_size; i++) {
            const QueuedField& field = used_fields[i];
            const uint64_t field_size = field.size;
            console.debug("used field: ", field_size, " target align: ", target_align);
            if constexpr (target_align >= lexer::SIZE::SIZE_8) {
                if (field_size % 8 == 0) {
                    fields.align8.size_sum += field_size;
                    fields.align8.idxs.emplace_back(i);
                    continue;
                }
            }
            if constexpr (target_align >= lexer::SIZE::SIZE_4) {
                if (field_size % 4 == 0) {
                    fields.align4.size_sum += field_size;
                    if constexpr (target_align >= lexer::SIZE::SIZE_8) {
                        if (fields.align4.size_sum % 8 == 0) {
                            BSSERT(fields.align4.idxs.size() != 0);
                            add_to_align<lexer::SIZE::SIZE_8>(i, fields, fields.align4);
                            continue;
                        }
                    }
                    BSSERT(fields.align4.idxs.size() == 0);
                    fields.align4.idxs.emplace_back(i);
                    continue;
                }
            }
            if constexpr (target_align >= lexer::SIZE::SIZE_2) {
                if (field_size % 2 == 0) {
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
                            add_to_align<lexer::SIZE::SIZE_8>(i, fields, fields.align2);
                            continue;
                        }
                    }
                    if constexpr (target_align >= lexer::SIZE::SIZE_8) {
                        if (fields.align2.size_sum % 4 == 0) {
                            add_to_align<lexer::SIZE::SIZE_4>(i, fields, fields.align2);
                            continue;
                        }
                    }
                    BSSERT(fields.align2.size_sum % 2 == 0);
                    fields.align2.idxs.emplace_back(i);
                    continue;
                }
            }
            fields.align1.size_sum += field_size;
            if constexpr (target_align >= lexer::SIZE::SIZE_8) {
                if (fields.align1.size_sum % 8 == 0) {
                    add_to_align<lexer::SIZE::SIZE_8>(i, fields, fields.align1);
                    continue;
                }
            }
            if constexpr (target_align >= lexer::SIZE::SIZE_4) {
                if (fields.align1.size_sum % 4 == 0) {
                    add_to_align<lexer::SIZE::SIZE_4>(i, fields, fields.align1);
                    continue;
                }
            }
            if constexpr (target_align >= lexer::SIZE::SIZE_2) {
                if (fields.align1.size_sum % 2 == 0) {
                    add_to_align<lexer::SIZE::SIZE_2>(i, fields, fields.align1);
                    continue;
                } 
            }
            fields.align1.idxs.emplace_back(i);
        }

        console.log("enqueueing for level: ", NAMEOF_ENUM(state_type), ", target align: ", target_align);
        for (auto idx : fields.template get<target_align>().idxs) {
            const QueuedField& field = used_fields[idx];
            std::visit([this, &field, &level_mutable_state]<typename T>(const T& arg) {
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
                } else if constexpr (std::is_same_v<SkippedField, T>) {
                    std::unreachable();
                } else {
                    static_assert(false);
                }
                level_mutable_state.current_offset += field.size;
            }, field.info);
        }

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
                // There will never be triggered a solve attampt for this alignment again so we need to trigger it manually.
                try_solve_queued<alignment>();
                return;
            }
            BSSERT(c != 0);
            c--;
        }

        template <lexer::SIZE alignment>
        void try_solve_queued () const {
            const lexer::SIZE largest_align = mutable_state.level().left_fields.largest_align();
            uint16_t& c = mutable_state.level().left_fields.get<alignment>();
            console.debug("[TopLevel::try_solve_queued] alignment: ", alignment, ", left_fields: ", mutable_state.level().left_fields);
            BSSERT(c != 0);
            c--;
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
        void try_solve_queued () const {
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
            lexer::LeafCounts::Counts queue_positions;
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
        void skip () const {
            const uint16_t idx = mutable_state.level().queue_positions.get<alignment>()++;
            const_state.level().queued[idx] = QueuedField{0, SkippedField{}};
        }

        template <lexer::SIZE alignment>
        void enqueue (const QueuedField field) const {
            console.debug("[FixedVariantLevel::enqueue] alignment: ", alignment.value);
            const uint16_t idx = mutable_state.level().queue_positions.get<alignment>()++;
            CSSERT(idx, <, const_state.level().queued.size());
            const_state.level().queued[idx] = field;
        }
    };
};
