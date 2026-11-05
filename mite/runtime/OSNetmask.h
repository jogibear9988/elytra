#pragma once
#include <optional>
#include <span>

namespace Mite {
// Native sockaddr masks contain bytes in network order, independent of CPU endian.
inline std::optional<unsigned> os_netmask_prefix(std::span<unsigned char const> bytes)
{
    unsigned prefix=0;
    bool ended=false;
    for(auto byte:bytes){
        for(unsigned bit=128;bit;bit>>=1){
            if(byte & bit){if(ended)return {}; ++prefix;}
            else ended=true;
        }
    }
    return prefix;
}
}
