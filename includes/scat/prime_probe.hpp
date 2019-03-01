#ifndef SCAT_HEADER_PRIME_PROBE
#define SCAT_HEADER_PRIME_PROBE

#include <scat/chain.hpp>
#include <scat/utils.hpp>
#include <scat/timer.hpp>
#include <scat/set_construction.hpp>
#include <scat/signal.hpp>

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

namespace scat {
namespace prime_probe {

// TODO: Find a home for channel_t
using channel_t = size_t;

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
    // Use shared_ptr here?
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
        threshold = calibrate_threshold(chain);
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
        return threshold;
    }

};

// reader_eviction_count
//  Counts the number of elements in a provided set that were evicted since the previous sample.
template<class State>
struct reader_eviction_count {
public:
    using ticks_t = typename State::timer_t::ticks_t;
    using sample_t = int16_t;

    static const sample_t MISSED_TIME_SLOT = -1;

public:
    // TODO: Support configuration
    size_t sample_count = 10000;
    ticks_t slot_length = 3000;
    ticks_t threshold = 130;

protected:
    // probe
    //  Return the number of elements in the provided range that have been evicted then busy wait
    //  until the end of the timeslot.
    //
    //  Returns MISSED_TIME_SLOT if probe is called and the timeslot has already been missed or if
    //  element accessing took longer than the timeslot.
    template<class Iterator>
    inline sample_t probe(
        State& state,
        Iterator begin,
        Iterator end,
        ticks_t slot_start,
        chain_t& chain
    ){
        sample_t count = 0;
        auto time_start = state.timer->get_ticks(chain);
        auto time_end = time_start;

        // Check if previous timeslot overran and consumed out timeslot
        if((time_end - slot_start) > slot_length){
            return MISSED_TIME_SLOT;
        }

        for(auto it = begin; it != end; ++it){
            state.backend->access_element(*it, chain);
            time_end = state.timer->get_ticks(chain);

            // Check if element was evicted
            if((time_end - time_start) >= threshold){
                count += 1;
            }

            time_start = time_end;
        }

        // We might have missed our timeslot if our code was interrupted
        if((time_end - slot_start) > slot_length){
            return MISSED_TIME_SLOT;
        }

        // Spin until the end of our time slot
        while((time_end - slot_start) < slot_length){
            time_end = state.timer->get_ticks(chain);
        }

        return count;
    }

public:
    // read_channel
    //  Return a vector of samples obtained by repeatedly calling probe on a given channel.
    //
    //  This function isn't intended to be directly used by client code, instead clients should use
    //  scat::signal::source or scat::signal::source_group, which wraps this class.
    std::vector<sample_t> read_channel(
        State& state,
        channel_t channel,
        chain_t& chain
    ){
        std::vector<sample_t> samples;
        samples.reserve(sample_count);

        size_t iterations = sample_count / 2;
        bool odd_sample_count = sample_count % 2 == 1;

        auto& set = state.sets[channel];

        // We don't want to probe in the same order every time.
        // Usually this would not be a problem, the sets we are given are defined as being the
        // minimal number of elements to evict every other element. That is the set perfectly fits
        // in the shared resource. However set construction is imperfect and accessing the entire
        // set, may result in some of the set's elements being evicted.
        //
        // In the event that a set evicts its own elements, accessing the elements of the set in the
        // same order will cause each new accessed element to evict another member of the set, this
        // artificially raises the number of evicted elements.
        auto slot_start = state.timer->get_ticks(chain);
        for(size_t i = 0; i < iterations; i += 1){
            samples.push_back(probe(state,  set.begin(),  set.end(), slot_start, chain));
            slot_start += slot_length;

            samples.push_back(probe(state, set.rbegin(), set.rend(), slot_start, chain));
            slot_start += slot_length;
        }
        if(odd_sample_count){
            samples.push_back(probe(state,  set.begin(),  set.end(), slot_start, chain));
        }

        return samples;
    }

    // read_channels
    //  Return a vector of samples obtained by repeatedly calling probe for each given channel.
    //
    //  This function isn't intended to be directly used by client code, instead clients should use
    //  scat::signal::source or scat::signal::source_group, which wraps this class.
    std::vector<std::vector<sample_t>> read_channels(
        State& state,
        std::vector<channel_t>& channels,
        chain_t& chain
    ){
        std::vector<std::vector<sample_t>> samples;
        for(auto channel: channels){
            samples.push_back(read_channel(channel, chain));
        }
        return samples;
    }
};

// TODO: Clean this all up
template<class Backend, class Timer, class Evicter>
struct state {
    using timer_t = Timer;

    std::unique_ptr<Backend> backend;
    std::unique_ptr<Timer> timer;
    std::unique_ptr<Evicter> evicter;
    std::vector<typename Backend::set_t> sets;
};

template<
    class Backend = cache,
    class Timer = timer::rdtscp32,
    class Evicter = evicter<Backend, Timer>
>
signal::source_group<
    state<Backend, Timer, Evicter>,
    reader_eviction_count<state<Backend, Timer, Evicter>>
> create(){
    using state_t = state<Backend, Timer, Evicter>;

    auto s = std::make_shared<state_t>();
    chain_t chain;

    s->backend = std::make_unique<Backend>();
    s->timer = std::make_unique<Timer>();

    s->evicter = std::make_unique<Evicter>(
        s->backend.get(),
        s->timer.get(),
        chain
    );

    s->sets = eviction_set_builder<Evicter>::build(*s->evicter);

    reader_eviction_count<state_t> r;
    r.threshold = s->evicter->threshold;

    signal::source_group<state_t, reader_eviction_count<state_t>> g(s, r);

    return g;
}

} // namespace prime_probe
} // namespace scat

#endif // SCAT_HEADER_PRIME_PROBE
