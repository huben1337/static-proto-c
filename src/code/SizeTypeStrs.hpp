#pragma once

#include "../core/SIZE.hpp"

namespace decode_code {
    struct SizeTypeStrs;

    namespace _detail {
        struct SizeTypeStrsTable;

        struct SizeTypeStrsTable {
            friend struct decode_code::SizeTypeStrs;

        private:
            template <SIZE size>
            static constexpr StringLiteral size_type_str_v = string_literal::concat_v<"uint"_sl, size.byte_size() * 8, "_t"_sl>;

            template <SIZE... sizes>
            struct size_type_strs_size {
                static constexpr size_t value = (size_type_str_v<sizes>.size() + ...);
            };

            static constexpr size_t types_count = SIZE::MAX.ordinal() + 1;

            char data[SIZE::enums::template apply<size_type_strs_size>::value];
            std::string_view views[types_count];

        public:
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
            consteval SizeTypeStrsTable () {
                char* data_pos = data;
                SIZE::enums::foreach([&, this]<SIZE size>() {
                    constexpr StringLiteral type_str = size_type_str_v<size>;
                    std::copy_n(type_str.begin(), type_str.size(), data_pos);
                    views[size.ordinal()] = {data_pos, type_str.size()};
                    data_pos += type_str.size();
                });
            }
        };

        constexpr SizeTypeStrsTable size_type_strs_table {};
    } // namespace _detail

    
    struct SizeTypeStrs {
        [[nodiscard]] static constexpr std::string_view get (const SIZE size) {
            BSSERT(size <= SIZE::MAX);
            return _detail::size_type_strs_table.views[size.ordinal()];
        }
    };

} // decode_code