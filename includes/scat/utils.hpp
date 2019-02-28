#ifndef SCAT_HEADER_UTILS
#define SCAT_HEADER_UTILS

#include <algorithm>
#include <cmath>
#include <vector>

namespace scat {
namespace utils {

/* sample(percentile, values)
 *  Indexes into values using percentile, a floating point number between zero and one.
 *  Don't rely on specific behavior, the intended use case is to roughly sample a given measurement.
 *
 *  Examples
 *      sample(0.0, {1, 2, 3, 4}) == 1
 *      sample(0.3, {1, 2, 3, 4}) == 2
 *      sample(0.6, {1, 2, 3, 4}) == 3
 *      sample(1.0, {1, 2, 3, 4}) == 4
 */
template<class T>
T sample(
    float percentile,
    std::vector<T>& values
){
    size_t size = values.size();
    size_t index = std::llround(size * percentile);
    //size_t index = (size_t)std::floor(size * percentile);
    return values[index >= size ? index = size - 1 : index];
}

template<class Fn, class... Args>
typename std::result_of<Fn(Args...)>::type sample(
    float percentile,
    size_t count,
    Fn&& fn,
    Args&&... args
){
    std::vector<typename std::result_of<Fn(Args...)>::type> results;
    results.resize(count);

    for(size_t offset = 0; offset < count; offset += 1){
        results[offset] = fn(std::forward<Args>(args)...);
    }

    std::sort(results.begin(), results.end());
    return sample(percentile, results);
}

template<class Fn, class... Args>
std::vector<typename std::result_of<Fn(Args...)>::type> sample(
    std::vector<float> percentiles,
    size_t count,
    Fn&& fn,
    Args&&... args
){
    std::vector<typename std::result_of<Fn(Args...)>::type> results;
    results.resize(count);

    for(size_t offset = 0; offset < count; offset += 1){
        results[offset] = fn(std::forward<Args>(args)...);
    }

    std::sort(results.begin(), results.end());

    std::vector<typename std::result_of<Fn(Args...)>::type> outputs;
    for(auto percentile : percentiles){
        outputs.push_back(sample(percentile, results));
    }
    return outputs;
}

} // namespace utils
} // namespace scat


#endif // SCAT_HEADER_UTILS
