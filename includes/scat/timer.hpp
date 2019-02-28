#ifndef SCAT_HEADER_TIMER
#define SCAT_HEADER_TIMER

#include <cstdint>

#include <scat/chain.hpp>

namespace scat {
namespace timer {

struct rdtscp32 {
public:
    typedef uint32_t ticks_t;

    inline ticks_t get_ticks(chain_t& chain){
        ticks_t time;
        asm volatile ("rdtscp": "=a" (time) :: "edx", "ecx");
        return time;
    }
};

struct rdtscp64 {
public:
    typedef uint64_t ticks_t;

    inline ticks_t get_ticks(chain_t& chain){
        uint32_t eax, edx;
        asm volatile ("rdtscp": "=a" (eax), "=d" (edx) :: "ecx");
        return ((ticks_t)edx << 32) | eax;
    }
};

} // namespace timer
} // namespace scat

#endif // SCAT_HEADER_TIMER
