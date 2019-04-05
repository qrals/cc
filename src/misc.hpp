#pragma once

#include <vector>
#include <algorithm>

template <typename t>
auto contains(const std::vector<t>& vec, const t& elt) {
    return std::find(vec.begin(), vec.end(), elt) != vec.end();
}
