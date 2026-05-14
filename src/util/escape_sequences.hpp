#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "../util/string_literal.hpp"

namespace escape_sequences {
    namespace cursor {
        template<size_t by>
        struct up;

        template<size_t by>
        struct down;

        template<size_t by>
        struct right;

        template<size_t by>
        struct left;

        template<size_t by>
        struct next_line;

        template<size_t by>
        struct prev_line;

        template<size_t n, bool use_alternative>
        struct to_column;

        template<size_t by>
        struct forward_tab;

        template<size_t by>
        struct backward_tab;

        template<size_t line, size_t column, bool use_alternative>
        struct to;

        template<size_t n>
        struct to_line;
    } // namespace cursor

    namespace erase {
        template<size_t n>
        struct characters;
    } // namespace erase

    namespace insert {
        template<size_t n>
        struct characters;

        template<size_t n>
        struct lines;
    } // namespace insert

    namespace _delete {
        template<size_t n>
        struct characters;

        template<size_t n>
        struct lines;
    } // namespace _delete

    namespace scroll {
        template<ssize_t n>
        struct by;

        template<size_t top, size_t bottom>
        struct set_region;
    } // namespace scroll

    namespace mode {
        struct keyboard_action   { struct set; struct reset; };
        struct insert            { struct set; struct reset; };
        struct rx_tx             { struct set; struct reset; };
        struct automatic_newline { struct set; struct reset; };

        template<typename... Modes>
        struct set;

        template<typename... Modes>
        struct reset;
    } // namespace mode

    namespace gr {
        struct bold          { struct set; struct reset; };
        struct dim           { struct set; struct reset; };
        struct italic        { struct set; struct reset; };
        struct underline     { struct set; struct reset; };
        struct blink         { struct set; struct reset; };
        struct reverse       { struct set; struct reset; };
        struct hide          { struct set; struct reset; };
        struct strikethrough { struct set; struct reset; };
    } // namespace gr

    namespace colors {
        struct foreground;
        struct background;

        namespace _256 {
            struct foreground;
            struct background;
        } // namespace _256
    } // namespace colors

    struct _private {
        template<ssize_t>
        friend struct scroll::by;

        template<typename...>
        friend struct sgr;

        template<size_t>
        friend struct cursor::up;

        template<size_t>
        friend struct cursor::down;

        template<size_t>
        friend struct cursor::right;

        template<size_t>
        friend struct cursor::left;

        template<size_t>
        friend struct cursor::next_line;

        template<size_t>
        friend struct cursor::prev_line;

        template<size_t, bool>
        friend struct cursor::to_column;

        template<size_t, size_t, bool>
        friend struct cursor::to;

        template<size_t n>
        friend struct cursor::to_line;

        template<size_t>
        friend struct cursor::forward_tab;

        template<size_t>
        friend struct cursor::backward_tab;

        template<size_t>
        friend struct erase::characters;

        template<size_t>
        friend struct insert::characters;

        template<size_t>
        friend struct insert::lines;

        template<size_t>
        friend struct _delete::characters;

        template<size_t>
        friend struct _delete::lines;

        template<size_t, size_t>
        friend struct scroll::set_region;

        friend struct mode::keyboard_action;
        friend struct mode::insert;
        friend struct mode::rx_tx;
        friend struct mode::automatic_newline;

        template<typename...>
        friend struct mode::set;

        template<typename...>
        friend struct mode::reset;

        friend struct gr::bold;
        friend struct gr::dim;
        friend struct gr::italic;
        friend struct gr::underline;
        friend struct gr::blink;
        friend struct gr::reverse;
        friend struct gr::hide;
        friend struct gr::strikethrough;

        friend struct colors::foreground;
        friend struct colors::background;
        friend struct colors::_256::foreground;
        friend struct colors::_256::background;

    private:
        static constexpr StringLiteral<2> csi {"\033["};

        template <StringLiteral v, typename Derived, typename... Incompatible>
        struct Attribute {
            friend Derived;

            Attribute() = delete;
        private:
            template<typename Attribute>
            static consteval void assert_compatability() {
                static_assert((!std::is_same_v<Attribute, Incompatible> && ...), "Incompatible stlye");
            }

            template<typename... AllAttributes>
            static consteval decltype(v) value() {
                (assert_compatability<AllAttributes>(), ...);
                static_assert(((std::is_same_v<Derived, AllAttributes> ? 1 : 0) + ...) == 1, "Duplicate attributes are disallowed");
                return v;
            }
        };

        template<StringLiteral v, typename Derived, typename... Incompatible>
        struct SetMode : Attribute<v, Derived, Incompatible...> {
            template<typename...>
            friend struct mode::set;
            
            SetMode() = delete;
        };

        template<typename T>
        static constexpr bool is_set_mode_v = false;

        template<StringLiteral v, typename Derived, typename... Incompatible>
        static constexpr bool is_set_mode_v<SetMode<v, Derived, Incompatible...>> = true;

        template<StringLiteral v, typename Derived, typename... Incompatible>
        struct ResetMode : Attribute<v, Derived, Incompatible...> {
            template<typename...>
            friend struct mode::reset;

            ResetMode() = delete;
        };

        template<typename T>
        static constexpr bool is_reset_mode_v = false;

        template<StringLiteral v, typename Derived, typename... Incompatible>
        static constexpr bool is_reset_mode_v<ResetMode<v, Derived, Incompatible...>> = true;

        static constexpr StringLiteral<1> mode_keyboard_action   {"2"};
        static constexpr StringLiteral<1> mode_insert            {"4"};
        static constexpr StringLiteral<2> mode_rx_tx             {"12"};
        static constexpr StringLiteral<2> mode_automatic_newline {"20"};

        template<StringLiteral v, typename Derived, typename... Incompatible>
        struct Gr : Attribute<v, Derived, Incompatible...> {
            template<typename...>
            friend struct escape_sequences::sgr;

            Gr() = delete;
        };

        template<typename T>
        struct is_gr_attribute {
            static constexpr bool value = false;
        };

        template <StringLiteral v, typename Derived, typename... Incompatible>
        struct is_gr_attribute<Gr<v, Derived, Incompatible...>> {
            static constexpr bool value = true;
        };

        template<StringLiteral v>
        struct FgColor {
            template<typename...>
            friend struct escape_sequences::sgr;

        private:
            template<typename... AllAttributes>
            static consteval decltype(v) value() {
                static_assert(((is_fg_color_v<AllAttributes> ? 1 : 0) + ...) == 1, "Multiple foreground colors are disallowed");
                return v;
            }
        };

        template<typename T>
        static constexpr bool is_fg_color_v = false;

        template<StringLiteral v>
        static constexpr bool is_fg_color_v<FgColor<v>> = true;

        template<StringLiteral v>
        struct BgColor {
            template<typename...>
            friend struct escape_sequences::sgr;

        private:
            template<typename... AllAttributes>
            static consteval decltype(v) value() {
                static_assert(((is_bg_color_v<AllAttributes> ? 1 : 0) + ...) == 1, "Multiple background colors are disallowed");
                return v;
            }
        };

        template<typename T>
        static constexpr bool is_bg_color_v = false;

        template<StringLiteral v>
        static constexpr bool is_bg_color_v<BgColor<v>> = true;

        template<StringLiteral v>
        struct is_gr_attribute<FgColor<v>> {
            static constexpr bool value = true;
        };

        template<StringLiteral v>
        struct is_gr_attribute<BgColor<v>> {
            static constexpr bool value = true;
        };

        template <size_t id>
        using make_fg_color_from_id = FgColor<string_literal::concat_v<escape_sequences::_private::csi, "38;5;"_sl, id>>;

        template <size_t id>
        using make_bg_color_from_id = BgColor<string_literal::concat_v<escape_sequences::_private::csi, "48;5;"_sl, id>>;

        template <uint8_t v>
        static constexpr uint8_t to_cube6 = v * (36.0L / 256.0L);

        template <uint8_t r, uint8_t g, uint8_t b>
        requires (r < 6 && g < 6 && b < 6)
        static constexpr size_t cube6_id = 16 + (36 * r) + (6 * g) + b;

        template <uint8_t r, uint8_t g, uint8_t b>
        static constexpr size_t closest_cube6_id = cube6_id<to_cube6<r>, to_cube6<g>, to_cube6<b>>;

        template <uint8_t v>
        requires (v < 24)
        static constexpr size_t gray_scale_id = 232 + v;

        template<
            template<size_t> typename ColorFromId
        >
        struct ColorSpace {
            template <uint8_t r, uint8_t g, uint8_t b>
            using closest_cube6 = ColorFromId<closest_cube6_id<r, g, b>>;

            template <uint8_t r, uint8_t g, uint8_t b>
            using cube6 = ColorFromId<cube6_id<r, g, b>>;

            template <uint8_t v>
            using gray_scale = ColorFromId<gray_scale_id<v>>::value;
        };

    public:
        using DefualtFgColor = FgColor<"39">;
        using DefualtBgColor = BgColor<"49">;

        template <uint8_t r, uint8_t g, uint8_t b>
        using rgb_foreground = FgColor<string_literal::concat_v<"38;2;"_sl, r, ";"_sl, g, ";"_sl, b>>;

        template <uint8_t r, uint8_t g, uint8_t b>
        using rgb_background = BgColor<string_literal::concat_v<"48;2;"_sl, r, ";"_sl, g, ";"_sl, b>>;
        
    private:
    };
} // namespace escape_sequences


namespace escape_sequences {
    namespace cursor {
        constexpr StringLiteral<3> home {"\033[H"};

        template<size_t by>
        struct up {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, by, "A"_sl>;
        };

        template<>
        struct up<1> {
            static constexpr StringLiteral<3> value {"\033[A"};
        };

        template<size_t by>
        struct down {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, by, "B"_sl>;
        };

        template <>
        struct down<1> {
            static constexpr StringLiteral<3> value {"\033[B"};
        };

        template<size_t by>
        struct right {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, by, "C"_sl>;
        };

        template <>
        struct right<1> {
            static constexpr StringLiteral<3> value {"\033[C"};
        };

        template<size_t by>
        struct left {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, by, "D"_sl>;
        };

        template <>
        struct left<1> {
            static constexpr StringLiteral<3> value {"\033[D"};
        };

        template<size_t by>
        struct next_line {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, by, "E"_sl>;
        };

        template <>
        struct next_line<1> {
            static constexpr StringLiteral<3> value {"\033[E"};
        };

        template<size_t by>
        struct prev_line {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, by, "F"_sl>;
        };

        template <>
        struct prev_line<1> {
            static constexpr StringLiteral<3> value {"\033[F"};
        };

        template<size_t n, bool use_alternative = false>
        struct to_column {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, n, "G"_sl>;
        };

        template<size_t n>
        struct to_column<n, true> {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, n, "m`"_sl>;
        };

        template <>
        struct to_column<1, false> {
            static constexpr StringLiteral<3> value {"\033[G"};
        };

        template <>
        struct to_column<1, true> : to_column<1, false> {};

        template<size_t line, size_t column, bool use_alternative = false>
        struct to {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, line, ";"_sl, column, use_alternative ? "f"_sl : "H"_sl>;
        };

        template<bool use_alternative>
        struct to<1, 1, use_alternative> {
            static constexpr StringLiteral<3> value = home;
        };

        template<size_t by>
        struct forward_tab {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, by, "I"_sl>;
        };

        template <>
        struct forward_tab<1> {
            static constexpr StringLiteral<3> value {"\033[I"};
        };
        
        template<size_t by>
        struct backward_tab {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, by, "Z"_sl>;
        };
        
        template <>
        struct backward_tab<1> {
            static constexpr StringLiteral<3> value {"\033[Z"};
        };

        template<size_t n>
        struct to_line {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, n, "d"_sl>;
        };

        template <>
        struct to_line<1> {
            static constexpr StringLiteral<3> value {"\033[G"};
        };

        template<size_t by>
        constexpr StringLiteral up_v = up<by>::value;

        template<size_t by>
        constexpr StringLiteral down_v = down<by>::value;

        template<size_t by>
        constexpr StringLiteral right_v = right<by>::value;

        template<size_t by>
        constexpr StringLiteral left_v = left<by>::value;

        template<size_t by>
        constexpr StringLiteral next_line_v = next_line<by>::value;
        
        template<size_t by>
        constexpr StringLiteral prev_line_v = prev_line<by>::value;

        template<size_t n, bool use_alternative = false>
        constexpr StringLiteral to_column_v = to_column<n, use_alternative>::value;

        template<size_t line, size_t column, bool use_alternative = false>
        constexpr StringLiteral to_v = to<line, column, use_alternative>::value;

        template<size_t n>
        constexpr StringLiteral to_line_v = to_line<n>::value;
        
        constexpr StringLiteral<2> save_pos_dec {"\0337"};
        constexpr StringLiteral<2> load_pos_dec {"\0338"};
        constexpr StringLiteral<2> to_bottom_left {"\033F"};

        constexpr StringLiteral<3> save_pos_sco {"\033[s"}; // ANSI.sys
        constexpr StringLiteral<3> load_pos_sco {"\033[u"}; // ANSI.sys
    } // namespace cursor

    namespace erase {
        namespace display {
            constexpr StringLiteral<3> below            {"\033[J"};
            constexpr StringLiteral<4> explicit_below   {"\033[0J"};
            constexpr StringLiteral<4> above            {"\033[1J"};
            constexpr StringLiteral<4> all              {"\033[2J"};    
            constexpr StringLiteral<4> saved            {"\033[3J"};

            namespace selective {
                constexpr StringLiteral<4> below            {"\033[?J"};
                constexpr StringLiteral<5> explicit_below   {"\033[?0J"};
                constexpr StringLiteral<5> above            {"\033[?1J"};
                constexpr StringLiteral<5> all              {"\033[?2J"};    
            } // namespace selective
        } // namespace display

        namespace line {
            constexpr StringLiteral<3> right            {"\033[K"};
            constexpr StringLiteral<4> explicit_right   {"\033[0K"};
            constexpr StringLiteral<4> left             {"\033[1K"};
            constexpr StringLiteral<4> all              {"\033[2k"};

            namespace selective {
                constexpr StringLiteral<4> right            {"\033[?K"};
                constexpr StringLiteral<5> explicit_right   {"\033[?0K"};
                constexpr StringLiteral<5> left             {"\033[?1K"};
                constexpr StringLiteral<5> all              {"\033[?2k"};
            } // namespace selective
        } // namespace line

        template<size_t n>
        struct characters {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, n, "X"_sl>;
        };

        template <>
        struct characters<1> {
            static constexpr StringLiteral<3> value {"\033[X"};
        };

        template<size_t n>
        constexpr StringLiteral characters_v = characters<n>::value;
    } // namespace erase

    namespace insert {
        template<size_t n>
        struct characters {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, n, "@"_sl>;
        };

        template <>
        struct characters<1> {
            static constexpr StringLiteral<3> value {"\033[@"};
        };

        template<size_t n>
        constexpr StringLiteral characters_v = characters<n>::value;

        template<size_t n>
        struct lines {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, n, "L"_sl>;
        };

        template <>
        struct lines<1> {
            static constexpr StringLiteral<3> value {"\033[L"};
        };

        template<size_t n>
        constexpr StringLiteral lines_v = lines<n>::value;
    } // namespace insert

    namespace _delete {
        template<size_t n>
        struct characters {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, n, "P"_sl>;
        };

        template <>
        struct characters<1> {
            static constexpr StringLiteral<3> value {"\033[P"};
        };

        template<size_t n>
        constexpr StringLiteral characters_v = characters<n>::value;

        template<size_t n>
        struct lines {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, n, "M"_sl>;
        };

        template <>
        struct lines<1> {
            static constexpr StringLiteral<3> value {"\033[M"};
        };

        template<size_t n>
        constexpr StringLiteral lines_v = lines<n>::value;
    } // namespace _delete

    namespace protection {
        constexpr StringLiteral<5> default_off {"\033[0\"q"};
        constexpr StringLiteral<5> on          {"\033[1\"q"};
        constexpr StringLiteral<5> off         {"\033[2\"q"};
    } // namespace protection

    namespace scroll {
        namespace lock {
            constexpr StringLiteral<2> above  {"\033l"};
            constexpr StringLiteral<2> remove {"\033m"};
        } // namespace lock

        constexpr size_t region_end = ~size_t{0};

        template<size_t top, size_t bottom>
        struct set_region {
            static_assert(top <= bottom, "Invalid scroll region");
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, top, ";"_sl, bottom, "r"_sl>;
        };

        template<>
        struct set_region<0, region_end> {
            static constexpr StringLiteral value {"\033[r"};
        };

        template<ssize_t n>
        struct by;

        template<>
        struct by<0> {
            static constexpr StringLiteral<0> value;
        };

        template<ssize_t n>
        requires (n > 1)
        struct by<n> {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, n, "T"_sl>;
        };

        template<>
        struct by<1> {
            static constexpr StringLiteral<3> value {"\033[T"};
        };

        template<ssize_t n>
        requires (n < -1)
        struct by<n> {
            static constexpr StringLiteral value = string_literal::concat_v<_private::csi, n, "S"_sl>;
        };

        template<>
        struct by<-1> {
            static constexpr StringLiteral<3> value {"\033[S"};
        };
    } // namespace scroll

    namespace mode {
        struct keyboard_action  ::set   : _private::SetMode  <_private::mode_keyboard_action,   keyboard_action::set>     {};
        struct keyboard_action  ::reset : _private::ResetMode<_private::mode_insert,            keyboard_action::reset>   {};
        struct insert           ::set   : _private::SetMode  <_private::mode_rx_tx,             insert::set>              {};
        struct insert           ::reset : _private::ResetMode<_private::mode_automatic_newline, insert::reset>            {};
        struct rx_tx            ::set   : _private::SetMode  <_private::mode_keyboard_action,   rx_tx::set>               {};
        struct rx_tx            ::reset : _private::ResetMode<_private::mode_insert,            rx_tx::reset>             {};
        struct automatic_newline::set   : _private::SetMode  <_private::mode_rx_tx,             automatic_newline::set>   {};
        struct automatic_newline::reset : _private::ResetMode<_private::mode_automatic_newline, automatic_newline::reset> {};

        template<typename... Modes>
        struct set {
            static_assert((_private::is_set_mode_v<Modes> && ...), "Invalid mode template paramter.");
            static constexpr StringLiteral value = string_literal::join<";", "\033[", "h">::apply<Modes::template value<Modes...>()...>::value;
        };

        template<typename... Modes>
        struct reset {
            static_assert((_private::is_reset_mode_v<Modes> && ...), "Invalid mode template paramter.");
            static constexpr StringLiteral value = string_literal::join<";", "\033[", "l">::apply<Modes::template value<Modes...>()...>::value;
        };
    } // namespace mode

    namespace gr {
        constexpr StringLiteral<4> reset {"\033[0m"};

        // template<StringLiteral... attributes>
        // requires (estd::are_distinct_v<attributes...>)
        // static constexpr StringLiteral cell = string_literal::join<";", "\033[1;34;", "m">::apply<attributes...>;

        struct bold           ::set   : _private::Gr<"1",  bold::set, bold::reset, dim::set, dim::reset> {};
        struct bold           ::reset : _private::Gr<"22", bold::reset, bold::set, dim::set, dim::reset> {};
        struct dim            ::set   : _private::Gr<"2",  dim::set, dim::reset, bold::set, bold::reset> {};
        struct dim            ::reset : _private::Gr<"22", dim::reset, dim::set, bold::set, bold::reset> {};
        struct italic         ::set   : _private::Gr<"3",  italic::set, italic::reset>                   {};
        struct italic         ::reset : _private::Gr<"23", italic::reset, italic::set>                   {};
        struct underline      ::set   : _private::Gr<"4",  underline::set, underline::reset>             {};
        struct underline      ::reset : _private::Gr<"24", underline::reset, underline::set>             {};
        struct blink          ::set   : _private::Gr<"5",  blink::set, blink::reset>                     {};
        struct blink          ::reset : _private::Gr<"25", blink::reset, blink::set>                     {};
        struct reverse        ::set   : _private::Gr<"7",  reverse::set, reverse::reset>                 {};
        struct reverse        ::reset : _private::Gr<"27", reverse::reset, reverse::set>                 {};
        struct hide           ::set   : _private::Gr<"8",  hide::set, hide::reset>                       {};
        struct hide           ::reset : _private::Gr<"28", hide::reset, hide::set>                       {};
        struct strikethrough  ::set   : _private::Gr<"9",  strikethrough::set, strikethrough::reset>     {};
        struct strikethrough  ::reset : _private::Gr<"29", strikethrough::reset, strikethrough::set>     {};
    } // namespace gr

    namespace colors {
        struct foreground {
            struct black    { using basic = _private::FgColor<"30">; using bright = _private::FgColor<"90">; };
            struct red      { using basic = _private::FgColor<"31">; using bright = _private::FgColor<"91">; };
            struct green    { using basic = _private::FgColor<"32">; using bright = _private::FgColor<"92">; };
            struct yellow   { using basic = _private::FgColor<"33">; using bright = _private::FgColor<"93">; };
            struct blue     { using basic = _private::FgColor<"34">; using bright = _private::FgColor<"94">; };
            struct magenta  { using basic = _private::FgColor<"35">; using bright = _private::FgColor<"95">; };
            struct cyan     { using basic = _private::FgColor<"36">; using bright = _private::FgColor<"96">; };
            struct white    { using basic = _private::FgColor<"37">; using bright = _private::FgColor<"97">; };
            using default_ = _private::DefualtFgColor;
        };

        struct background {
            struct black    { using basic = _private::BgColor<"40">; using bright = _private::BgColor<"100">; };
            struct red      { using basic = _private::BgColor<"41">; using bright = _private::BgColor<"101">; };
            struct green    { using basic = _private::BgColor<"42">; using bright = _private::BgColor<"102">; };
            struct yellow   { using basic = _private::BgColor<"43">; using bright = _private::BgColor<"103">; };
            struct blue     { using basic = _private::BgColor<"44">; using bright = _private::BgColor<"104">; };
            struct magenta  { using basic = _private::BgColor<"45">; using bright = _private::BgColor<"105">; };
            struct cyan     { using basic = _private::BgColor<"46">; using bright = _private::BgColor<"106">; };
            struct white    { using basic = _private::BgColor<"47">; using bright = _private::BgColor<"107">; };
            using default_ = _private::DefualtBgColor;
        };

        namespace _256 {
            struct foreground : _private::ColorSpace<_private::make_fg_color_from_id> {
                struct black    { using basic = _private::FgColor<"38;5;0">; using bright = _private::FgColor<"38;5;8">;  };
                struct red      { using basic = _private::FgColor<"38;5;1">; using bright = _private::FgColor<"38;5;9">;  };
                struct green    { using basic = _private::FgColor<"38;5;2">; using bright = _private::FgColor<"38;5;10">; };
                struct yellow   { using basic = _private::FgColor<"38;5;3">; using bright = _private::FgColor<"38;5;11">; };
                struct blue     { using basic = _private::FgColor<"38;5;4">; using bright = _private::FgColor<"38;5;12">; };
                struct magenta  { using basic = _private::FgColor<"38;5;5">; using bright = _private::FgColor<"38;5;13">; };
                struct cyan     { using basic = _private::FgColor<"38;5;6">; using bright = _private::FgColor<"38;5;14">; };
                struct white    { using basic = _private::FgColor<"38;5;7">; using bright = _private::FgColor<"38;5;15">; };
                using default_ = _private::DefualtFgColor;
            };

            struct background : _private::ColorSpace<_private::make_bg_color_from_id> {
                struct black    { using basic = _private::BgColor<"48;5;0">; using bright = _private::BgColor<"48;5;8">;  };
                struct red      { using basic = _private::BgColor<"48;5;1">; using bright = _private::BgColor<"48;5;9">;  };
                struct green    { using basic = _private::BgColor<"48;5;2">; using bright = _private::BgColor<"48;5;10">; };
                struct yellow   { using basic = _private::BgColor<"48;5;3">; using bright = _private::BgColor<"48;5;11">; };
                struct blue     { using basic = _private::BgColor<"48;5;4">; using bright = _private::BgColor<"48;5;12">; };
                struct magenta  { using basic = _private::BgColor<"48;5;5">; using bright = _private::BgColor<"48;5;13">; };
                struct cyan     { using basic = _private::BgColor<"48;5;6">; using bright = _private::BgColor<"48;5;14">; };
                struct white    { using basic = _private::BgColor<"48;5;7">; using bright = _private::BgColor<"48;5;15">; };
                using default_ = _private::DefualtBgColor;
            };
        } // namespace _256

        namespace rgb {
            template <uint8_t r, uint8_t g, uint8_t b>
            using foreground = _private::rgb_foreground<r, g, b>;

            template <uint8_t r, uint8_t g, uint8_t b>
            using background = _private::rgb_background<r, g, b>;
        }; // namespace rgb
    } // namespace colors

    template<typename... Grs>
    struct sgr {
        static_assert((_private::is_gr_attribute<Grs>::value && ...), "Invalid graphic rendition template paramter.");
        static constexpr StringLiteral value = string_literal::join<";", "\033[", "m">::apply<Grs::template value<Grs...>()...>::value;
    };

    template<typename... Grs>
    constexpr StringLiteral sgr_v = sgr<Grs...>::value;

    constexpr StringLiteral<4> soft_reset {"\033[!p"};

    namespace report {
        constexpr StringLiteral<4> status          {"\033[5n"}; // -> `CSI 0 n`
        constexpr StringLiteral<4> curser_position {"\033[6n"}; // -> `CSI r ; c R`
    }
} // namespace escape_sequences
