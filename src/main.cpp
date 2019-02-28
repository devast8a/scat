#include <scat/utils.hpp>
#include <scat/chain.hpp>
#include <scat/set_construction.hpp>
#include <scat/prime_probe.hpp>
#include <scat/timer.hpp>

#include <chrono>

int main(){
    using namespace scat;
    using namespace scat::prime_probe;
    using namespace scat::timer;

    chain_t chain;
    size_t failures = 0;
    for(size_t i = 0; i < 10; i += 1){
        cache backend;
        rdtscp32 timer;
        evicter<cache, rdtscp32> measure(&backend, &timer, chain);

        auto start = std::chrono::system_clock::now();
        auto sets = eviction_set_builder<evicter<cache, rdtscp32>>::build(measure);
        auto end = std::chrono::system_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cerr << "Time: " << duration.count() << "ms" << std::endl;
        std::cerr << std::endl;

        // Definitely not portable, but who cares right now
        if(sets.size() < 8000){
            failures += 1;
            std::cerr << "########################################" << std::endl;
        } else {
            std::cerr << "----------------------------------------" << std::endl;
        }
    }

    std::cerr << "Failures: " << failures << std::endl;
}
