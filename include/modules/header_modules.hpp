#pragma once

#include "nb_common.hpp"

namespace nb {

// Macro-style guard: skip this call if the metadata stream is empty.
// Necessary because the parser produces one metadata entry per packet
// but N data beats; action modules are called once per nb_hls_top
// invocation and must be no-ops during payload beats.
#define NB_GUARD_EMPTY(stream) if ((stream).empty()) return

struct identifier_module {
    static constexpr field_def fields[] = {
        {"ethertype", 16},
        // Conditional: IPv4 fields (only extracted when ethertype == 0x0800)
        {"ipv4_src",  32, "ethertype", 0x0800},
        {"ipv4_dst",  32, "ethertype", 0x0800},
        // Conditional: VLAN fields (only when ethertype == 0x8100)
        {"vlan_tci",  16, "ethertype", 0x8100},
        // Always present (root) — flow identifiers
        {"dst_id",    32},
        {"src_id",    32},
    };
    static constexpr const char* name="identifier",*header="modules/header_modules.hpp";
    template<typename Meta>
    static void process(hls::stream<Meta>& mi, hls::stream<Meta>& mo) {
#pragma HLS PIPELINE II=1
        NB_GUARD_EMPTY(mi); Meta m=mi.read(); mo.write(m);
    }
};

struct inorder_module {
    static constexpr field_def fields[] = {{"sequence_number",32}};
    static constexpr const char* name="inorder",*header="modules/header_modules.hpp";
    template<typename Meta>
    static void process(hls::stream<Meta>& mi, hls::stream<Meta>& mo) {
#pragma HLS PIPELINE II=1
        NB_GUARD_EMPTY(mi);
        Meta m=mi.read(); static ap_uint<32> seq=1010; m.sequence_number=seq; ++seq; mo.write(m);
    }
};

struct routing_module {
    static constexpr field_def fields[] = {{"ttl",8},{"ip_protocol",8},{"src_addr",32},{"dst_addr",32}};
    static constexpr const char* name="routing",*header="modules/header_modules.hpp";
    template<typename Meta>
    static void process(hls::stream<Meta>& mi, hls::stream<Meta>& mo) {
#pragma HLS PIPELINE II=1
        NB_GUARD_EMPTY(mi);
        Meta m=mi.read(); if(m.ttl>0)m.ttl=m.ttl-1; mo.write(m);
    }
};

} // namespace nb
