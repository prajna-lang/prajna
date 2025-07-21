#include <chrono>
#include <iostream>
#include <vector>

void Perf(const std::vector<int32_t>& array0, const std::vector<int32_t>& array1,
          std::vector<int32_t>& array2) {
    size_t size = array0.size();
    for (size_t i = 0; i < size; ++i) {
        array2[i] = array0[i] + array1[i];
    }
}

int main() {
    std::cout << "enter main" << std::endl;

    size_t size = 1000000000;
    std::vector<int32_t> array0(size, 0);
    std::vector<int32_t> array1(size, 0);
    std::vector<int32_t> array2(size);

    std::cout << "start" << std::endl;
    auto t0 = std::chrono::high_resolution_clock::now();

    Perf(array0, array1, array2);

    auto t1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = t1 - t0;

    double gb_per_second = (size * sizeof(int32_t)) / 1e9 / duration.count();
    std::cout << gb_per_second << " GB/s" << std::endl;

    return 0;
}
