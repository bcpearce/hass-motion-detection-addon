#include "Util/Tools.h"

#include <algorithm>
#include <ranges>
#include <string_view>

namespace util {

bool NoCaseCmp(const char *s1, const char *s2) {
  if (s1 && s2) {
    auto sv1 = std::string_view(s1);
    auto sv2 = std::string_view(s2);
    return sv1.size() == sv2.size() &&
           std::ranges::all_of(std::views::zip_transform(
                                   [](auto c1, auto c2) -> bool {
                                     return std::tolower(c1) ==
                                            std::tolower(c2);
                                   },
                                   sv1, sv2),
                               [](auto b) { return b; });
  }
  return false;
}

} // namespace util