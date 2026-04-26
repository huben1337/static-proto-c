#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "../util/string_literal.hpp"

namespace escape_sequences {
    template<typename...>
    struct style;

    namespace cursor {
        template<size_t line, size_t column, bool use_format_effector = false>
        struct to;

        template<size_t by>
        struct up;

        template<size_t by>
        struct down;

        template<size_t by>
        struct right;

        template<size_t by>
        struct left;

        template<size_t by>
        struct down_to_begin;

        template<size_t n>
        struct to_column;

        template<size_t by>
        struct up_to_begin;
    } // namespace cursor

    namespace colors {
        struct _private;
    } // namespace colors

    struct _private {
        friend struct colors::_private;

        template<size_t, size_t, bool>
        friend struct cursor::to;

        template<size_t>
        friend struct cursor::up;

        template<size_t>
        friend struct cursor::down;

        template<size_t>
        friend struct cursor::right;

        template<size_t>
        friend struct cursor::left;

        template<size_t>
        friend struct cursor::down_to_begin;

        template<size_t>
        friend struct cursor::to_column;

        template<size_t>
        friend struct cursor::up_to_begin;

        template<typename...>
        friend struct style;

    private:
        static constexpr StringLiteral<2> bracket_prefix {"\033["};

        template<typename T>
        struct is_style_param {
            static constexpr bool value = false;
        };
    };

    namespace mode {
        struct bold          { struct set; struct reset; };
        struct dim           { struct set; struct reset; };
        struct italic        { struct set; struct reset; };
        struct underline     { struct set; struct reset; };
        struct blink         { struct set; struct reset; };
        struct reverse       { struct set; struct reset; };
        struct hide          { struct set; struct reset; };
        struct strikethrough { struct set; struct reset; };

        struct _private {
            friend struct bold;
            friend struct dim;
            friend struct italic;
            friend struct underline;
            friend struct blink;
            friend struct reverse;
            friend struct hide;
            friend struct strikethrough;

        private:
            template <StringLiteral v, typename Derived, typename... Incompatible>
            struct Mode {
                friend Derived;

                template<typename...>
                friend struct escape_sequences::style;

            private:
                consteval Mode() = default;

                template<typename Style>
                static consteval void assert_compatability () {
                    static_assert((!std::is_same_v<Style, Incompatible> && ...), "Incompatible stlye");
                }

                template<typename... OtherStyles>
                static consteval decltype(v) value () {
                    (assert_compatability<OtherStyles>(), ...);
                    static_assert(((std::is_same_v<Derived, OtherStyles> ? 1 : 0) + ...) == 1, "Duplicate modes are disallowed");
                    return v;
                }
            };
        };
    } // namespace mode

    namespace colors {
        struct color;
        struct foreground;
        struct background;

        namespace _256 {
            struct foreground;
            struct background;
        } // namespace _256

        namespace rgb {
            struct _private;
        } // namespace rgb

        struct _private {
            friend struct rgb::_private;
            friend struct foreground;
            friend struct background;
            friend struct _256::foreground;
            friend struct _256::background;

        private:
            template<typename T>
            static constexpr bool is_fg_color_v = false;

            template<StringLiteral v>
            struct FgColor {
                template<typename... Styles>
                friend struct escape_sequences::style;

            private:
                template<typename... AllStyles>
                static consteval decltype(v) value () {
                    static_assert(((is_fg_color_v<AllStyles> ? 1 : 0) + ...) == 1, "Multiple foreground colors are disallowed");
                    return v;
                }
            };

            template<StringLiteral v>
            static constexpr bool is_fg_color_v<FgColor<v>> = true;


            template<typename T>
            static constexpr bool is_bg_color_v = false;

            template<StringLiteral v>
            struct BgColor {
                template<typename... Styles>
                friend struct escape_sequences::style;

            private:
                template<typename... AllStyles>
                static consteval decltype(v) value () {
                    static_assert(((is_bg_color_v<AllStyles> ? 1 : 0) + ...) == 1, "Multiple background colors are disallowed");
                    return v;
                }
            };

            template<StringLiteral v>
            static constexpr bool is_bg_color_v<BgColor<v>> = true;

            using DefualtFgColor = FgColor<"39">;
            using DefualtBgColor = BgColor<"49">;

            template <size_t id>
            using fgc = FgColor<string_literal::concat_v<escape_sequences::_private::bracket_prefix, "38;5;"_sl, id>>;

            template <size_t id>
            using bgc = BgColor<string_literal::concat_v<escape_sequences::_private::bracket_prefix, "48;5;"_sl, id>>;

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
        };

        namespace rgb {
            struct _private {
                template <uint8_t r, uint8_t g, uint8_t b>
                using foreground = colors::_private::FgColor<string_literal::concat_v<"38;2;"_sl, r, ";"_sl, g, ";"_sl, b>>;

                template <uint8_t r, uint8_t g, uint8_t b>
                using background = colors::_private::BgColor<string_literal::concat_v<"48;2;"_sl, r, ";"_sl, g, ";"_sl, b>>;
            };
        } // namespace rgb

    } // namespace colors

    template<> struct _private::is_style_param<escape_sequences::mode::bold         ::set  > { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::bold         ::reset> { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::dim          ::set  > { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::dim          ::reset> { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::italic       ::set  > { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::italic       ::reset> { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::underline    ::set  > { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::underline    ::reset> { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::blink        ::set  > { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::blink        ::reset> { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::reverse      ::set  > { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::reverse      ::reset> { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::hide         ::set  > { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::hide         ::reset> { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::strikethrough::set  > { static constexpr bool value = true; };
    template<> struct _private::is_style_param<escape_sequences::mode::strikethrough::reset> { static constexpr bool value = true; };

    template<StringLiteral v>
    struct _private::is_style_param<colors::_private::FgColor<v>> {
        static constexpr bool value = true;
    };

    template<StringLiteral v>
    struct _private::is_style_param<colors::_private::BgColor<v>> {
        static constexpr bool value = true;
    };
} // namespace escape_sequences


namespace escape_sequences {
    namespace cursor {
        constexpr StringLiteral<3> to_home {"\033[H"};

        template<size_t line, size_t column, bool use_format_effector>
        struct to {
            static constexpr StringLiteral value = string_literal::concat_v<_private::bracket_prefix, line, ";"_sl, column, use_format_effector ? "f"_sl : "H"_sl>;
        };
        
        template<size_t by>
        struct up {
            static constexpr StringLiteral value = string_literal::concat_v<_private::bracket_prefix, by, "A"_sl>;
        };
        
        template<size_t by>
        struct down {
            static constexpr StringLiteral value = string_literal::concat_v<_private::bracket_prefix, by, "B"_sl>;
        };
        
        template<size_t by>
        struct right {
            static constexpr StringLiteral value = string_literal::concat_v<_private::bracket_prefix, by, "C"_sl>;
        };
        
        template<size_t by>
        struct left {
            static constexpr StringLiteral value = string_literal::concat_v<_private::bracket_prefix, by, "D"_sl>;
        };
        
        template<size_t by>
        struct down_to_begin {
            static constexpr StringLiteral value = string_literal::concat_v<_private::bracket_prefix, by, "E"_sl>;
        };
        
        template<size_t n>
        struct to_column {
            static constexpr StringLiteral value = string_literal::concat_v<_private::bracket_prefix, n, "G"_sl>;
        };

        template<size_t by>
        struct up_to_begin {
            static constexpr StringLiteral value = string_literal::concat_v<_private::bracket_prefix, by, "F"_sl>;
        };
        
        template<size_t line, size_t column, bool use_format_effector = false>
        constexpr StringLiteral to_v = to<line, column, use_format_effector>::value;

        template<size_t by>
        constexpr StringLiteral up_v = up<by>::value;

        template<size_t by>
        constexpr StringLiteral down_v = down<by>::value;

        template<size_t by>
        constexpr StringLiteral right_v = right<by>::value;

        template<size_t by>
        constexpr StringLiteral left_v = left<by>::value;

        template<size_t by>
        constexpr StringLiteral down_to_begin_v = down_to_begin<by>::value;
        
        template<size_t by>
        constexpr StringLiteral up_to_begin_v = up_to_begin<by>::value;
        
        template<size_t n>
        constexpr StringLiteral to_column_v = to_column<n>::value;

        constexpr StringLiteral<4> request      {"\033[6n"};
        constexpr StringLiteral<3> up_one       {"\033 M"};
        constexpr StringLiteral<3> save_pos_dec {"\033 7"};
        constexpr StringLiteral<3> load_pos_dec {"\033 8"};
        constexpr StringLiteral<3> save_pos_sco {"\033[s"};
        constexpr StringLiteral<3> load_pos_sco {"\033[u"};
    } // namespace cursor

    namespace erase {
        constexpr StringLiteral<3> in_screen    {"\033[J"};
        constexpr StringLiteral<4> sreen_end    {"\033[0J"};
        constexpr StringLiteral<4> screen_begin {"\033[1J"};
        constexpr StringLiteral<4> screen       {"\033[2J"};    
        constexpr StringLiteral<4> saved        {"\033[3J"};
        constexpr StringLiteral<3> in_line      {"\033[K"};
        constexpr StringLiteral<4> line_end     {"\033[0K"};
        constexpr StringLiteral<4> line_beginn  {"\033[1K"};
        constexpr StringLiteral<4> line         {"\033[2k"};
    } // namespace erase

    namespace mode {
        
        constexpr StringLiteral<4> reset = "\033[0m";

        // template<StringLiteral... modes>
        // requires (estd::are_distinct_v<modes...>)
        // static constexpr StringLiteral cell = string_literal::join<";", "\033[1;34;", "m">::apply<modes...>;

        struct bold           ::set   : _private::Mode<"1" , bold::set, bold::reset, dim::set, dim::reset> {};
        struct bold           ::reset : _private::Mode<"22", bold::reset, bold::set, dim::set, dim::reset> {};
        struct dim            ::set   : _private::Mode<"2" , dim::set, dim::reset, bold::set, bold::reset> {};
        struct dim            ::reset : _private::Mode<"22", dim::reset, dim::set, bold::set, bold::reset> {};
        struct italic         ::set   : _private::Mode<"3" , italic::set, italic::reset>  {};
        struct italic         ::reset : _private::Mode<"23", italic::reset, italic::set> {};
        struct underline      ::set   : _private::Mode<"4" , underline::set, underline::reset>  {};
        struct underline      ::reset : _private::Mode<"24", underline::reset, underline::set> {};
        struct blink          ::set   : _private::Mode<"5" , blink::set, blink::reset>  {};
        struct blink          ::reset : _private::Mode<"25", blink::reset, blink::set> {};
        struct reverse        ::set   : _private::Mode<"7" , reverse::set, reverse::reset>  {};
        struct reverse        ::reset : _private::Mode<"27", reverse::reset, reverse::set> {};
        struct hide           ::set   : _private::Mode<"8" , hide::set, hide::reset>  {};
        struct hide           ::reset : _private::Mode<"28", hide::reset, hide::set> {};
        struct strikethrough  ::set   : _private::Mode<"9" , strikethrough::set, strikethrough::reset>  {};
        struct strikethrough  ::reset : _private::Mode<"29", strikethrough::reset, strikethrough::set> {};
    } // namespace mode

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
            struct foreground : _private::ColorSpace<_private::fgc> {
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

            struct background : _private::ColorSpace<_private::bgc> {
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
            using foreground = _private::foreground<r, g, b>;

            template <uint8_t r, uint8_t g, uint8_t b>
            using background = _private::background<r, g, b>;
        }; // namespace rgb
    } // namespace colors

    template<typename... Styles>
    struct style {
        static_assert((_private::is_style_param<Styles>::value && ...), "Invalid style template paramter.");
        static constexpr StringLiteral value = string_literal::join<";", "\033[", "m">::apply<Styles::template value<Styles...>()...>::value;
    };

    template<typename... Styles>
    constexpr StringLiteral style_v = style<Styles...>::value;

    namespace screen {
        constexpr StringLiteral set_mode        {"\033[={value}h"};
        constexpr StringLiteral _40x25_mc       {"\033[=0h"};
        constexpr StringLiteral _40x25_col      {"\033[=1h"};
        constexpr StringLiteral _80x25_mc       {"\033[=2h"};
        constexpr StringLiteral _80x25_col      {"\033[=3h"};
        constexpr StringLiteral _320x200_col4   {"\033[=4h"};
        constexpr StringLiteral _320x200_mc     {"\033[=5h"};
        constexpr StringLiteral _640x200_mc     {"\033[=6h"};
        constexpr StringLiteral wrap_lines      {"\033[=7h"};
        constexpr StringLiteral _320x200_col    {"\033[=13h"};
        constexpr StringLiteral _640x200_col16  {"\033[=14h"};
        constexpr StringLiteral _640x350_mc2    {"\033[=15h"};
        constexpr StringLiteral _640x350_col16  {"\033[=16h"};
        constexpr StringLiteral _640x480_mc2    {"\033[=17h"};
        constexpr StringLiteral _640x480_col16  {"\033[=18h"};
        constexpr StringLiteral _320x200_col265 {"\033[=19h"};
        constexpr StringLiteral reset_mode      {"\033[={value}l"};
    } // namespace screen
} // namespace escape_sequences
