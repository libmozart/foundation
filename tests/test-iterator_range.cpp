/**
 * Mozart++ Template Library
 * Licensed under MIT License
 * Copyright (c) 2020 Covariant Institute
 * Website: https://covariant.cn/
 * Github:  https://github.com/covariant-institute/
 */

#include <cstdio>
#include <mozart++/iterator_range>
#include <vector>

int main() {
    std::vector<int> a{1, 2, 3, 34, 5, 6, 7, 8, 1, 231, 2, 1, 31};
    for (auto i : mpp::make_range(a)) {
        printf("%d\n", i);
    }
}
