#pragma once

#include "nb_common.hpp"

namespace nb {

struct identifier_module {
    static constexpr field_def fields[] = {
        {"ethertype", 16},
        {"ipv4_src",  32, "ethertype", 0x0800},
        {"ipv4_dst",  32, "ethertype", 0x0800},
        {"vlan_tci",  16, "ethertype", 0x8100},
        {"dst_id",    32},
        {"src_id",    32},
    };
    static constexpr const char* name="identifier",*header="modules/header_modules.hpp";
    template<typename Meta>
    static void process(hls::stream<Meta>& mi, hls::stream<Meta>& mo) {
#pragma HLS PIPELINE II=1
        if (mi.empty()) return;
        Meta m=mi.read(); mo.write(m);
    }
};

struct inorder_module {
    static constexpr field_def fields[] = {{"sequence_number",32}};
    static constexpr const char* name="inorder",*header="modules/header_modules.hpp";
    template<typename Meta>
    static void process(hls::stream<Meta>& mi, hls::stream<Meta>& mo) {
#pragma HLS PIPELINE II=1
        if (mi.empty()) return;
        Meta m=mi.read();
        static ap_uint<32> seq=1010; m.sequence_number=seq; ++seq; mo.write(m);
    }
};

struct routing_module {
    static constexpr field_def fields[] = {{"ttl",8},{"ip_protocol",8},{"src_addr",32},{"dst_addr",32}};
    static constexpr const char* name="routing",*header="modules/header_modules.hpp";
    template<typename Meta>
    static void process(hls::stream<Meta>& mi, hls::stream<Meta>& mo) {
#pragma HLS PIPELINE II=1
        if (mi.empty()) return;
        Meta m=mi.read(); if (m.ttl>0) m.ttl=m.ttl-1; mo.write(m);
    }
};

} // namespace nb
