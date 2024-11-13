#include <type_traits>

template<typename T>
inline constexpr bool is_char_ptr_t = std::is_same_v<T, char*> || std::is_same_v<T, const char*>;