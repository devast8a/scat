#include <scat/prime_probe.hpp>
#include <scat/signal.hpp>

int main(){
    auto pp = scat::prime_probe::create();

    auto signal = scat::signal::find_first(
        scat::signal::repeat({1, 0, 1, 0, 1, 1, 1, 0, 0, 0}, 3),
        pp
    );

    if(!signal){
        std::cout << "Could not find signal" << std::endl;
        return 1;
    }

    auto data = scat::signal::decode_binary(*signal, 256);
    for(auto bit : data){
        std::cout << (bit ? "1" : "0");
    }
    std::cout << std::endl;

    return 0;
}
