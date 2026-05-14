#pragma once

#include "nb_common.hpp"

namespace nb {

struct reliable_module {
    static constexpr field_def fields[] = {
        {"ack_sequence_number", 32},
    };
    static constexpr const char* name   = "reliable";
    static constexpr const char* header = "modules/reliable_module.hpp";
    static constexpr int max_connections  = 16;
    static constexpr int redelivery_depth = 32;

    template <typename Meta>
    static void process(hls::stream<Meta>& meta_in,
                        hls::stream<Meta>& meta_out) {
#pragma HLS PIPELINE II = 1

        static ap_uint<32> seq_table[max_connections][redelivery_depth];
#pragma HLS BIND_STORAGE variable = seq_table type = RAM_T2P impl = BRAM
#pragma HLS DEPENDENCE variable = seq_table inter false
        static ap_uint<1> slot_valid[max_connections][redelivery_depth];
#pragma HLS BIND_STORAGE variable = slot_valid impl = LUTRAM

        if (meta_in.empty()) return;
        Meta m = meta_in.read();
        ap_uint<32> ack_seq = m.ack_sequence_number;
        ap_uint<32> conn    = ap_uint<32>(m.src_id);

        if (ack_seq != 0) {
            ap_uint<5> slot = ack_seq.range(4, 0);
            if (slot_valid[conn.range(3,0)][slot]) {
                seq_table[conn.range(3,0)][slot] = 0;
                slot_valid[conn.range(3,0)][slot] = 0;
            }
        } else {
            ap_uint<32> seq = m.sequence_number;
            ap_uint<5> slot = seq.range(4, 0);
            seq_table[conn.range(3,0)][slot] = seq;
            slot_valid[conn.range(3,0)][slot] = 1;
            m.ack_sequence_number = 0;
            meta_out.write(m);
        }
    }
};

}  // namespace nb
