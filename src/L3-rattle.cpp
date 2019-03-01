#include <iostream>

volatile char buffer[4096];

int main(int ac, char **av) {
    size_t STEP = 15000;
    size_t signal[] = {
        1, 0, 1, 0, 1, 1, 1, 0, 0, 0,
        1, 0, 1, 0, 1, 1, 1, 0, 0, 0,
        1, 0, 1, 0, 1, 1, 1, 0, 0, 0,

        1, 0,
        1, 0, 0,
        1, 0, 0, 0,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0,
    };
    size_t signal_length = sizeof(signal) / sizeof(signal[0]);

    // Transform 1, 0 signal into addresses in buffer
    //  Use addresses in the middle to avoid accidentally sharing a
    //  cache-line with something else in the program.
    for(size_t i = 0; i < signal_length; ++i){
        if(signal[i] == 0){
            signal[i] = 800;
        } else {
            signal[i] = 1800;
        }
    }

    while(true){
        for(size_t index = 0; index < signal_length; ++index){
            size_t address = signal[index];
            for(size_t i = 0; i < STEP; ++i){
                buffer[address] += 1;
            }
        }
    }
}
