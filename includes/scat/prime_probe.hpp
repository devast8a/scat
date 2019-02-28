#ifndef SCAT_HEADER_PRIME_PROBE
#define SCAT_HEADER_PRIME_PROBE

#include <scat/chain.hpp>
#include <scat/utils.hpp>

#include <algorithm>
#include <iostream>
#include <vector>

namespace scat {
namespace prime_probe {

// TODO: Document this crap some more
struct cache {
public:
    struct element {
        uint32_t data;
        element* next;
        element* prev;
    } __attribute__((aligned(64)));

    using element_t = element*;
    using set_t = std::vector<element_t>;

    static const size_t EVICTION_SET_SIZE = 16;

    static const size_t CACHE_SIZE = 16 * 1024 * 1024;
    static const size_t VIRTUAL_ADDRESS_SIZE = (1 << 12) / sizeof(element);
    static const size_t CACHELINE_SIZE = (1 << 6) / sizeof(element);

private:
    std::vector<element> buffer;
    std::vector<element_t> elements;
    size_t size;

public:
    cache(size_t size = CACHE_SIZE){
        size /= sizeof(element);
        this->size = size;

        buffer.resize(size);

        for(size_t i = 0; i < size; i += 1){
            buffer[i].data = 1 + i;
        }

        // TODO: Explain why this works
        for(size_t i = 0; i < size; i += VIRTUAL_ADDRESS_SIZE){
            elements.push_back(&buffer[i]);
        }
    }

    inline void access_element(element_t element, chain_t& chain){
        chain.read(&element->data);
    }

    std::vector<element_t>& get_elements(){
        return elements;
    }

    std::vector<std::vector<element_t>> extend_elements(std::vector<element_t> const& elements){
        std::vector<std::vector<element_t>> extended;
        
        size_t size = elements.size();

        for(size_t offset = 0; offset < VIRTUAL_ADDRESS_SIZE; offset += CACHELINE_SIZE){
            std::vector<element_t> set;
            set.resize(size);

            std::transform(
                elements.begin(),
                elements.end(),
                set.begin(),
                [&](auto element){return element + offset;}
            );

            extended.push_back(set);
        }

        return extended;
    }
};

template<class Backend, class Timer>
struct evicter {
public:
    using ticks_t = typename Timer::ticks_t;
    using set_t = typename Backend::set_t;
    using element_t = typename Backend::element_t;
    using backend_t = Backend;

public:
    Backend* backend;
    Timer* timer;

    // TODO: Allow for configuration
    size_t sample_count = 5;
    float sample_point = 0.5;

    ticks_t threshold = 0;

    float calibration_separation = 0.2;
    size_t calibration_samples = 50;

public:
    evicter(Backend* backend, Timer* timer, chain_t& chain) :
        backend(backend), timer(timer){
        // TODO: Setup method of memoizing threshold configuration
        this->threshold = calibrate_threshold(chain);
    }

    // evict_and_time
    //  Given a set of elements from backend, access each element to try evict the witness.
    //  Then time how long it take to access the witness.
    ticks_t evict_and_time(set_t& set, element_t witness, chain_t& chain){
        return scat::utils::sample(sample_point, sample_count, [&]{
            // Access the element in case it's not in the cache to begin with
            backend->access_element(witness, chain);

            // For whatever reason accessing elements once isn't guaranteed to cache them, accessing
            // them twice seems to be sufficient in most of my tests (devast8a). If we could drop
            // this to a single read or write it would probably drop the eviction set expansion by
            // almost half.
            for(size_t i = 0; i < 2; i += 1){
                for(auto element : set){
                    backend->access_element(element, chain);
                }
            }

            auto start = timer->get_ticks(chain);
            backend->access_element(witness, chain);
            return timer->get_ticks(chain) - start;
        });
    }

    // set_evicts
    //  Given a set of elements from backend, determine if that set can evict the witness.
    ticks_t set_evicts(set_t& set, element_t witness, chain_t& chain){
        return evict_and_time(set, witness, chain) >= threshold;
    }

protected:
    // calibrate_threshold
    //  Automatically find a suitable value that will allow us to distinguish
    //  between a cache hit and a cache miss. So that given the time to access
    //  a piece of memory we can determine if 1t was cached or not.
    ticks_t calibrate_threshold(chain_t& chain){
        auto elements = backend->get_elements();

        // Run two experiments where we access each element in order and then access
        //  the last element, and hope it is still cached because we just accessed it
        //  the first element, and hope it is evicted because elements should fill the entire cache.
        auto hit = scat::utils::sample(1.0 - calibration_separation, calibration_samples, [&]{
            return evict_and_time(elements, elements.back(), chain);
        });
        auto miss = scat::utils::sample(calibration_separation, calibration_samples, [&]{
            return evict_and_time(elements, elements.front(), chain);
        });

        if(hit >= miss){
            std::cerr << "Could not calibrate an eviction threshold" << std::endl;
            // TODO: Communicate errors in a better way
            return 0;
        }

        auto threshold = hit + (miss - hit) / 2;

        // LE DEBUGGING INFO XDDD
        std::cerr << "Success, calibrated an eviction threshold" << std::endl;
        std::cerr << "   Cache Hit : " << hit << std::endl;
        std::cerr << "  Cache Miss : " << miss << std::endl;
        std::cerr << "   Threshold : " << threshold << std::endl;
        std::cerr << std::endl;
        
        return threshold;
    }

};

} // namespace prime_probe
} // namespace scat

#endif // SCAT_HEADER_PRIME_PROBE
