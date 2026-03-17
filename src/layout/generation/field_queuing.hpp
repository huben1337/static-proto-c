#pragma once

#include "../../estd/empty.hpp"
#include "../../estd/vector.hpp"
#include "../../core/SIZE.hpp"
#include "../../core/AlignMembersBase.hpp"

namespace layout::generation {

struct AlignedFields : estd::no_copy {
    std::vector<uint16_t> idxs;
    uint64_t size_sum = 0;
};

template<SIZE max_align>
struct Fields : AlignMembersBase<AlignedFields, max_align.next_smaller(), SIZE::SIZE_1, Fields<max_align>> {
    using Base = AlignMembersBase<AlignedFields, max_align.next_smaller(), SIZE::SIZE_1, Fields>;
    using Base::Base;

private:
    template <SIZE new_max_align, SIZE... alignments>
    requires (new_max_align < max_align)
    constexpr Fields<new_max_align> extract_ (this Fields&& self, estd::variadic_v<alignments...> /*unused*/) {
        return Fields<new_max_align>{std::move(self.template get<alignments>())...};
    }

public:
    template <SIZE new_max_align>
    requires (new_max_align < max_align)
    constexpr Fields<new_max_align> extract (this Fields&& self) {
        if constexpr (new_max_align > SIZE::SIZE_1) {
            return std::move(self).template extract_<new_max_align>(
                make_size_range<SIZE::SIZE_1, new_max_align.next_smaller()>{});
        } else {
            return Fields<new_max_align>{};
        }
    }
};

template<>
struct Fields<SIZE::SIZE_1> : estd::empty {
    using empty::empty;
};

template <SIZE target_align>
void enqueueing_for_level (auto& level, const uint16_t idx) {
    level.template enqueue_for_level<target_align>(idx);    
}

template <SIZE target_align>
void enqueueing_for_level (auto& level, const std::vector<uint16_t>& idxs) {
    for (const uint16_t idx : idxs) {
        level.template enqueue_for_level<target_align>(idx);    
    }
}

template <SIZE to_align, SIZE max_align, typename... T>
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
            add_to_align<to_align.next_bigger()>(level, i, fields, target, first, rest...);
        }
    }
};

// TODO: Make it possible to pass already dereferenced/indexed field instead of the index only
template<SIZE target_align>
static void add_field (
    auto& level,
    const uint16_t field_idx,
    Fields<target_align>& fields,
    const uint64_t field_size
) {
    if constexpr (target_align >= SIZE::SIZE_8) {
        if (field_size % 8 == 0) {
            if constexpr (target_align == SIZE::SIZE_8) {
                enqueueing_for_level<target_align>(level, field_idx);
            } else {
                fields.template get<SIZE::SIZE_8>().size_sum += field_size;
                fields.template get<SIZE::SIZE_8>().idxs.emplace_back(field_idx);
            }
            return;
        }
    }
    if constexpr (target_align >= SIZE::SIZE_4) {
        if (field_size % 4 == 0) {
            if constexpr (target_align == SIZE::SIZE_4) {
                enqueueing_for_level<target_align>(level, field_idx);
            } else {
                fields.template get<SIZE::SIZE_4>().size_sum += field_size;
                if constexpr (target_align >= SIZE::SIZE_8) {
                    if (fields.template get<SIZE::SIZE_4>().size_sum % 8 == 0) {
                        BSSERT(fields.template get<SIZE::SIZE_4>().idxs.size() != 0);
                        add_to_align<SIZE::SIZE_8>(level, field_idx, fields, fields.template get<SIZE::SIZE_4>());
                        return;
                    }
                }
                BSSERT(fields.template get<SIZE::SIZE_4>().idxs.size() == 0);
                fields.template get<SIZE::SIZE_4>().idxs.emplace_back(field_idx);
            }
            return;
        }
    }
    if constexpr (target_align >= SIZE::SIZE_2) {
        if (field_size % 2 == 0) {
            if constexpr (target_align == SIZE::SIZE_2) {
                enqueueing_for_level<target_align>(level, field_idx);
            } else {
                fields.template get<SIZE::SIZE_2>().size_sum += field_size;
                if constexpr (target_align >= SIZE::SIZE_8) {
                    if (fields.template get<SIZE::SIZE_2>().size_sum % 8 == 0) {
                        /*
                        In this case we could apply the field directly.
                        But better would be to check if we can split off a align4 part of the combination.
                        But i dont think that should ever be the case since the (new_size_sum % 4) branch should have caught that combintion ??!
                        Im not really sure. The align4 subset combination is not caught if the change bubbles up into here and adds a align4 combination as a whole,
                        which should not really happen.
                        */
                        add_to_align<SIZE::SIZE_8>(level, field_idx, fields, fields.template get<SIZE::SIZE_2>());
                        return;
                    }
                }
                if constexpr (target_align >= SIZE::SIZE_4) {
                    if (fields.template get<SIZE::SIZE_2>().size_sum % 4 == 0) {
                        add_to_align<SIZE::SIZE_4>(level, field_idx, fields, fields.template get<SIZE::SIZE_2>());
                        return;
                    }
                }
                BSSERT(fields.template get<SIZE::SIZE_2>().size_sum % 2 == 0);
                fields.template get<SIZE::SIZE_2>().idxs.emplace_back(field_idx);
            }
            return;
        }
    }
    if constexpr (target_align == SIZE::SIZE_1) {
        enqueueing_for_level<target_align>(level, field_idx);
    } else {
        fields.template get<SIZE::SIZE_1>().size_sum += field_size;
        if constexpr (target_align >= SIZE::SIZE_8) {
            if (fields.template get<SIZE::SIZE_1>().size_sum % 8 == 0) {
                add_to_align<SIZE::SIZE_8>(level, field_idx, fields, fields.template get<SIZE::SIZE_1>());
                return;
            }
        }
        if constexpr (target_align >= SIZE::SIZE_4) {
            if (fields.template get<SIZE::SIZE_1>().size_sum % 4 == 0) {
                add_to_align<SIZE::SIZE_4>(level, field_idx, fields, fields.template get<SIZE::SIZE_1>());
                return;
            }
        }
        if constexpr (target_align >= SIZE::SIZE_2) {
            if (fields.template get<SIZE::SIZE_1>().size_sum % 2 == 0) {
                add_to_align<SIZE::SIZE_2>(level, field_idx, fields, fields.template get<SIZE::SIZE_1>());
                return;
            } 
        }
        fields.template get<SIZE::SIZE_1>().idxs.emplace_back(field_idx);
    }
}

} // namespace layout::generation