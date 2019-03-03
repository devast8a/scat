#ifndef SCAT_HEADER_TIMER
#define SCAT_HEADER_TIMER

#include <scat/chain.hpp>

#include <chrono>
#include <cstdint>

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

// realtime_calibration<Timer>
//  Attempt to calibrate Timer against high_resolution_clock, so that Timer ticks can be converted
//  to realtime and back.
template<class Timer>
struct realtime_calibration {
    struct settings_t {
        float ratio;
        std::chrono::nanoseconds realtime;
        typename Timer::ticks_t ticks;

        // We need to support less than operator so that settings_t can be ordered (required for
        //  scat::utils::sample, or any other sorted container of settings_t)
        bool operator<(settings_t other){
            return ratio < other.ratio;
        }
    };

    inline static bool calibrated = false;
    inline static settings_t settings;

    // calibrate
    //  Forcefully calibrate the timer against high_resolution_clock, writes the results to settings
    //  and returns it.
    static settings_t& calibrate(
        Timer& timer,
        std::chrono::nanoseconds calibration_length,
        float sample_point,
        size_t sample_count,
        chain_t& chain
    ){
        settings = scat::utils::sample(sample_point, sample_count, [&]{
            auto clock_start = std::chrono::high_resolution_clock::now();
            auto timer_start = timer.get_ticks(chain);

            auto clock_end = clock_start;
            auto timer_end = timer_start;

            while(clock_end - clock_start < calibration_length){
                clock_end = std::chrono::high_resolution_clock::now();
                timer_end = timer.get_ticks(chain);
            }

            settings_t result;
            result.realtime = (clock_end - clock_start);
            result.ticks = (timer_end - timer_start);
            result.ratio = (float)result.realtime.count() / result.ticks;
            return result;
        });

        calibrated = true;
        return settings;
    }

    // calibrate
    //  Forcefully calibrate the timer against high_resolution_clock, writes the results to settings
    //  and returns it.
    static settings_t& calibrate(){
        if(calibrated){
            return settings;
        }

        Timer timer;
        chain_t chain;

        return calibrate(timer, std::chrono::milliseconds(1), 0.5, 5, chain);
    }
};

// realtime_to_ticks
//  Converts std::chrono duration into ticks_t duration
template<class Timer>
typename Timer::ticks_t realtime_to_ticks(std::chrono::nanoseconds realtime){
    auto settings = realtime_calibration<Timer>::calibrate();
    return realtime * settings.ticks / settings.realtime;
}

// ticks_to_realtime
//  Converts ticks_t duration into std::chrono duration
template<class Timer>
std::chrono::nanoseconds ticks_to_realtime(typename Timer::ticks_t ticks){
    auto settings = realtime_calibration<Timer>::calibrate();
    return (int64_t)ticks * settings.realtime / settings.ticks;
}


} // namespace timer
} // namespace scat

#endif // SCAT_HEADER_TIMER
