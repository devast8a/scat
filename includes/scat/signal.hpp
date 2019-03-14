#ifndef SCAT_HEADER_SET_SIGNAL
#define SCAT_HEADER_SET_SIGNAL

#include <memory>
#include <vector>

// TODO: Make signal interface nicer

namespace scat {
namespace signal {

// TODO: Find a home for channel_t
using channel_t = size_t;

template<
    class State,
    class Reader
>
struct source {
public:
    using sample_t = typename Reader::sample_t;

private:
    chain_t chain;  // TODO: Support universal chain?
    channel_t channel;
    Reader reader;
    std::shared_ptr<State> state;

public:
    source(
        std::shared_ptr<State> state,
        Reader reader,
        channel_t channel
    ) : state(state),
        reader(reader),
        channel(channel)
    {
    }

    std::vector<sample_t> read(){
        return reader.read_channel(*state, channel, chain);
    }

    channel_t get_channel(){
        return channel;
    }
};

template<
    class State,
    class Reader
>
struct source_group {
public:
    using sample_t = typename Reader::sample_t;
    using source_t = source<State, Reader>;

private:
    chain_t chain;  // TODO: Support universal chain?
    std::vector<channel_t> channels;
    std::shared_ptr<State> state;

public:
    Reader reader;

    source_group(
        std::shared_ptr<State> state,
        Reader reader
    ) : state(state),
        reader(reader)
    {
        // Register for all channels
        // TODO: Abstract away from prime_probe specific state
        channels.reserve(state->sets.size());
        for(channel_t channel = 0; channel < state->sets.size(); channel += 1){
            channels.push_back(channel);
        }
    }

    std::vector<std::vector<sample_t>> read(
    ){
        return reader.read_channels(*state, channels, chain);
    };

    std::vector<sample_t> read_channel(
        channel_t channel
    ){
        return reader.read_channel(*state, channel, chain);
    }

    std::vector<channel_t>& get_channels(){
        return channels;
    }

    source_t channel_to_source(channel_t channel){
        return source_t(state, reader, channel);
    }


    std::vector<source_t> get_sources(){
        std::vector<source_t> sources;
        sources.reserve(channels.size());

        for(auto channel : channels){
            sources.emplace_back(state, reader, channel);
        }

        return sources;
    }
};



template<typename T>
struct length {
    T value;
    size_t length;
    size_t start;
};

template<typename T>
std::vector<length<T>> samples_to_lengths(
    std::vector<T> const& samples,
    size_t const minimum_gap = 0
){
    std::vector<length<T>> output;

    size_t index = 1;
    size_t length = 1;
    size_t start = 0;
    T value = samples[0];

    while(index < samples.size()){
        if(value != samples[index]){
            if(length <= minimum_gap && output.size() > 0){
                auto prev = output.back();

                value = prev.value;
                start = prev.start;
                length += prev.length;

                output.pop_back();
            } else {
                output.push_back({value, length, start});

                start = index;
                length = 0;
                value = samples[index];
            }
        }

        index += 1;
        length += 1;
    }

    output.push_back({value, length, start});

    return output;
}

template<typename T>
std::vector<T> lengths_to_samples(
    std::vector<length<T>> const& lengths
){
    std::vector<T> samples;

    for(auto length : lengths){
        for(size_t i = 0; i < length.length; i += 1){
            samples.push_back(length.value);
        }
    }

    return samples;
}

template<typename T>
std::vector<T> low_pass(std::vector<T> samples, size_t freq){
    auto lengths = samples_to_lengths(samples, freq);
    return lengths_to_samples(lengths);
}

template<typename T>
std::vector<T> threshold_samples(
    std::vector<T>& samples,
    T high = 1
){
    // Find threshold
    T optimal_threshold = 0;
    size_t value = 100000000;

    for(T threshold = 1; threshold < 16; threshold++){
        size_t zero = 0;
        size_t one = 0;

        for(auto value : samples){
            if(value >= threshold){
                one += 1;
            } else {
                zero += 1;
            }
        }

        size_t difference = (zero > one) ? (zero - one) : (one - zero);
        if(difference < value){
            value = difference;
            optimal_threshold = threshold;
        }
    }

    // Apply threshold
    for(size_t i = 0; i < samples.size(); ++i){
        samples[i] = (samples[i] >= optimal_threshold) ? high : 0;
    }
    return samples;
}

struct signal {
    size_t start;
    size_t end;
    std::vector<length<int16_t>> data;
    size_t one_timestep;
    size_t zero_timestep;
};

std::vector<bool> decode_binary(signal const& signal, size_t bits){
    std::vector<bool> results;

    for(size_t index = signal.end; index < signal.data.size(); ++index){
        auto length = signal.data[index];
        
        bool value = (length.value == 1);
        float c = ((float)length.length) / (value ? signal.one_timestep : signal.zero_timestep);
        size_t count = std::lround(c);

        for(size_t i = 0; i < count; ++i){
            results.push_back(value);

            if(results.size() >= bits){
                return results;
            }
        }
    }

    return results;
}

template<typename Sources>
std::unique_ptr<signal> find_first(
    std::vector<int16_t> known,
    Sources& sources
){
    auto signal_lengths = samples_to_lengths(known);

    size_t minimum_gap = 6;

    size_t zero_signal_sum = 0;
    size_t one_signal_sum = 0;

    for(auto length : signal_lengths){
        if(length.value == 0){
            zero_signal_sum += length.length;
        } else {
            one_signal_sum += length.length;
        }
    }

    for(auto source : sources.get_channels()){
        auto data = sources.read_channel(source);
        data = threshold_samples(data);
        auto lengths = samples_to_lengths(data, minimum_gap);

        // Find singal from lengths
        size_t window_start = 0;
        size_t window_end = signal_lengths.size();

        while(window_end < lengths.size()){
            size_t zero_window_sum = 0;
            size_t one_window_sum = 0;

            for(size_t index = window_start; index < window_end; ++index){
                if(lengths[index].value == 0){
                    zero_window_sum += lengths[index].length;
                } else {
                    one_window_sum += lengths[index].length;
                }
            }

            size_t one_timestep = one_window_sum / one_signal_sum;
            size_t zero_timestep = zero_window_sum / zero_signal_sum;
            float max_tolerance = 0;

            // Compare the window with our signal
            for(size_t index = window_start; index < window_end; ++index){
                size_t timestep = (lengths[index].value == 0) ? zero_timestep : one_timestep;
                size_t s = signal_lengths[index - window_start].length * timestep;
                size_t w = lengths[index].length;

                size_t difference = (s > w) ? (s - w) : (w - s);
                float tolerance = ((float)difference) / s;
                if(tolerance >= max_tolerance){
                    max_tolerance = tolerance;
                }
            }

            if(max_tolerance <= 0.4){
                auto result = std::make_unique<signal>();
                result->start = window_start;
                result->end = window_end;
                result->data = lengths;
                result->one_timestep = one_timestep;
                result->zero_timestep = zero_timestep;
                return result;
            }

            window_start += 1;
            window_end += 1;
        }
    }

    return nullptr;
}

std::vector<int16_t> repeat(std::vector<int16_t>&& input, size_t count){
    std::vector<int16_t> output;

    for(size_t i = 0; i < count; ++i){
        output.insert(output.end(), input.begin(), input.end());
    }

    return output;
}

} // namespace signal
} // namespace scat


#endif // SCAT_HEADER_SET_SIGNAL
