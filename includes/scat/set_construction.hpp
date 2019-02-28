#ifndef SCAT_HEADER_SET_CONSTRUCTION
#define SCAT_HEADER_SET_CONSTRUCTION

#include <scat/chain.hpp>

#include <algorithm>
#include <random>
#include <vector>

namespace scat {

template<class T>
struct eviction_set_builder{
private:
    // TODO: Allow for configuration
    static const size_t CONTRACT_COUNT = 10;
    static const size_t ATTEMPT_COUNT = 20;

    // We accept a range of eviction set sizes.
    // The constants here are chosen through experimentation
    static const size_t EVICTION_SET_SIZE = T::backend_t::EVICTION_SET_SIZE;
    static const size_t EVICTION_SET_SIZE_LOWER = EVICTION_SET_SIZE - 1;
    static const size_t EVICTION_SET_SIZE_UPPER = EVICTION_SET_SIZE + 4;

public:
    typedef typename T::element_t element_t;

    static std::vector<std::vector<element_t>> build(T& primitive){
        std::vector<std::vector<element_t>> eviction_sets;
        element_t witness;

        std::random_device rd;
        std::mt19937 g(rd());
 
        std::vector<element_t> candidates = primitive.backend->get_elements();
        
        chain_t chain;

        for(size_t attempt = 1; attempt <= ATTEMPT_COUNT; ++attempt){
            std::vector<element_t> eviction_set;

            // Need at least T::EVICTION_SET_SIZE + 1 elements. The eviction set and the witness.
            if(candidates.size() <= EVICTION_SET_SIZE){
                break;
            }

            // Why shuffle?
            //   Without shuffling we will have to process on average most of the candidates before
            //   we find an eviction set. This is because elements that map to the same set will be
            //   evenly spread across the candidates set. If we shuffle then it is very likely that
            //   we will find an eviction set MUCH sooner.
            //
            // Why shuffle here?
            //   If we try to shuffle the candidates set once before entering the loop as opposed to
            //   each time we loop, we often run into a situation in which we continually attempt to
            //   construct the same failing eviction set over and over again. If we change how items
            //   are inserted back into the candidate set, it might be possible to move the shuffle
            //   to outside the loop.
            std::shuffle(candidates.begin(), candidates.end(), g);

            // -- Expand phase --
            if(!phase_expand(primitive, candidates, eviction_set, witness, chain)){
                candidates.push_back(witness);
                candidates.insert(candidates.end(), eviction_set.begin(), eviction_set.end());

                continue;
            }

            // -- Contract phase --
            // NOTE: On some platforms it is much faster to contract only once, and rerun the expand
            // phase when failing. On other platforms we need to perform multiple contract phases to
            // get any reasonable output. If we can reliably detect which strategy we should use, it
            // might be worth switching strategy based on the platform.
            for(size_t contract_count = 0; contract_count < CONTRACT_COUNT; contract_count += 1){
                // Early exit, eviction set is already correct size
                if(eviction_set.size() <= EVICTION_SET_SIZE_UPPER){
                    break;
                }
                phase_contract(primitive, candidates, eviction_set, witness, chain);
            }

            // Check contract is successful
            if(
                eviction_set.size() < EVICTION_SET_SIZE_LOWER ||
                eviction_set.size() > EVICTION_SET_SIZE_UPPER
            ){
                candidates.push_back(witness);
                candidates.insert(candidates.end(), eviction_set.begin(), eviction_set.end());

                continue;
            }

            // -- Collect phase --
            phase_collect(primitive, candidates, eviction_set, chain);
            eviction_sets.push_back(eviction_set);

            // Reset attempt count
            attempt = 0;
        }

        // Extend the eviction sets
        //  For some platforms we only need to discover a subset of the total eviction sets. We are
        //  able to generate the remaining eviction sets from the discovered subset. For a concrete
        //  example see l3::extend_elements.
        std::vector<std::vector<element_t>> all_eviction_sets;
        for(auto& set : eviction_sets){
            auto extended = primitive.backend->extend_elements(set);
            all_eviction_sets.insert(all_eviction_sets.end(), extended.begin(), extended.end());
        }

        // LE DEBUGGING INFO XDDD
        std::cerr << all_eviction_sets.size() << " sets constructed" << std::endl;
        std::cerr << std::endl;

        return all_eviction_sets;
    }

    // The purpose of the expand phase is to find a set of elements, known as an eviction set, that
    // completely fills a given resource thereby evicting any other elements that happen to be using
    // the resource. The expand phase is only concerned with finding an eviction set, it will find
    // eviction sets that contain extra elements that are mapped to other resources.
    static bool phase_expand(
        T& primitive,
        std::vector<element_t>& candidates,
        std::vector<element_t>& eviction_set,
        element_t& witness,
        chain_t& chain
    ) {
        for(size_t i = 0; i < EVICTION_SET_SIZE - 1; i += 1){
            eviction_set.push_back(candidates.back());
            candidates.pop_back();
        }

        // If the eviction set gets this big and we still have not found an eviction, assume we have
        // a problem and bailout.
        size_t bailout = candidates.size() / 2;

        // Bailing out early is detremental when we have very few eviction sets left to build, which
        // corresponds to a small candidate set size. Stop early bailout by allowing us to check the
        // entire candidate set.
        if(bailout < EVICTION_SET_SIZE * 10){
            bailout = candidates.size();
        }

        witness = candidates.back();
        candidates.pop_back();

        while(candidates.size() > 0 && eviction_set.size() < bailout){
            eviction_set.push_back(witness);
            witness = candidates.back();
            candidates.pop_back();

            if(primitive.set_evicts(eviction_set, witness, chain)){
                return true;
            }
        }

        return false;
    }

    // The purpose of the contract phase is to take an eviction set and remove elements that do not
    // contribute to the eviction.
    static void phase_contract(
        T& primitive,
        std::vector<element_t>& candidates,
        std::vector<element_t>& eviction_set,
        element_t& witness,
        chain_t& chain
    ) {
        size_t index = 0;

        while(eviction_set.size() > 0){
            auto element = eviction_set.back();
            eviction_set.pop_back();

            if(primitive.set_evicts(eviction_set, witness, chain)){
                candidates.push_back(element);
            } else {
                if(index >= eviction_set.size()){
                    eviction_set.push_back(element);
                    break;
                } else {
                    eviction_set.push_back(eviction_set[index]);
                    eviction_set[index] = element;
                    index += 1;
                }
            }
        }
    }

    // The purpose of the collect phase is to remove all other elements from the candidate set that
    // map to the same resource as a given eviction set.
    static void phase_collect(
        T& primitive,
        std::vector<element_t>& candidates,
        std::vector<element_t>& eviction_set,
        chain_t& chain
    ){
        size_t i = 0;
        while(i < candidates.size()){
            if(primitive.set_evicts(eviction_set, candidates[i], chain)){
                candidates[i] = candidates.back();
                candidates.pop_back();
            } else {
                i += 1;
            }
        }
    }
};

} // namespace scat

#endif // SCAT_HEADER_SET_CONSTRUCTION
