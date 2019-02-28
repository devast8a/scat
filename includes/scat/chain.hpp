#ifndef SCAT_HEADER_CHAIN
#define SCAT_HEADER_CHAIN

#include <cstdint>
#include <iostream> // Maybe use some other way of generating a side effect?

namespace scat {

// chain_t attempts to serialize the execution of instructions in a light weight and cross platform
// manner. This is used for both avoiding optimization from the compiler and for avoiding reordering
// by the processor itself.
//
// The only guarantee we try to provide is that
//  chain.read(x);
//  chain.read(y);
// Will be executed in that order, and will not be reordered by the compiler or the processor.
struct chain_t {
    uint32_t value;

    chain_t(){
        value = 0;
    }

    template<class T>
    inline T read(T* p){
        this->value += *p;
        return *p;
    }

    ~chain_t(){
        if(((value << 1) | (value >> 1) | value) == 1){
            std::cerr << value << std::endl;
        }
    }
};

} // namespace scat


#endif // SCAT_HEADER_CHAIN
