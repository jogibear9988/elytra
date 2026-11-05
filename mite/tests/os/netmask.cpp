#include "OSNetmask.h"
#include <array>
#include <cstdio>

int main()
{
    unsigned cases=0;
    for(size_t size:{4,16}){
        unsigned bits=static_cast<unsigned>(size*8);
        for(unsigned prefix=0;prefix<=bits;++prefix){
            std::array<unsigned char,16> bytes {};
            for(unsigned bit=0;bit<prefix;++bit)bytes[bit/8]|=static_cast<unsigned char>(128u>>(bit%8));
            auto result=Mite::os_netmask_prefix({bytes.data(),size});
            if(!result || *result!=prefix)return 1;
            ++cases;
            if(prefix+1<bits){
                auto bit=prefix+1;
                bytes[bit/8]|=static_cast<unsigned char>(128u>>(bit%8));
                if(Mite::os_netmask_prefix({bytes.data(),size}))return 1;
                ++cases;
            }
        }
    }
    std::printf("PASS %u native IPv4/IPv6 contiguous and noncontiguous mask cases\n",cases);
    return 0;
}
