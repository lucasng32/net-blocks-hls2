#pragma once

#include "nb_common.hpp"

namespace nb {

struct reliable_module {
    // Fields contributed to the packet header
    static constexpr field_def fields[] = {
        {"ack_sequence_number", 32},
    };

    static constexpr const char* name   = "reliable";
    static constexpr const char* header = "modules/reliable_module.hpp";

    // Stateful BRAM/URAM configuration
    static constexpr int max_connections  = 16;
    static constexpr int redelivery_depth = 32;

    template <typename Meta>
    static void process(hls::stream<Meta>& meta_in,
                        hls::stream<Meta>& meta_out) {
#pragma HLS PIPELINE II = 1
        if (meta_in.empty()) return;

        // ── per-connection state (Vitis HLS infers BRAM) ─────────────
        // seq_table[conn][slot] stores the sequence number pending ACK
        static ap_uint<32> seq_table[max_connections][redelivery_depth];
#pragma HLS BIND_STORAGE variable = seq_table type = RAM_T2P impl = BRAM
#pragma HLS DEPENDENCE variable = seq_table inter false

        // valid flag: 1 = slot occupied, 0 = free
        static ap_uint<1> slot_valid[max_connections][redelivery_depth];
#pragma HLS BIND_STORAGE variable = slot_valid impl = LUTRAM

        // ── process packet ───────────────────────────────────────────
        Meta m = meta_in.read();

        ap_uint<32> ack_seq = m.ack_sequence_number;
        ap_uint<32> conn    = ap_uint<32>(m.src_id);   // connection index

        if (ack_seq != 0) {
            // ── ingress: ACK packet ─────────────────────────────────
            // Look up the redelivery slot for this ack sequence
            ap_uint<5> slot = ack_seq.range(4, 0);   // slot = seq % 32
            if (slot_valid[conn.range(3,0)][slot]) {
                seq_table[conn.range(3,0)][slot] = 0;
                slot_valid[conn.range(3,0)][slot] = 0;
            }
            // ACK packets are not forwarded downstream (consumed)
        } else {
            // ── send path: data packet ───────────────────────────────
            // Record this packet for potential retransmission
            ap_uint<32> seq = m.sequence_number;
            ap_uint<5> slot = seq.range(4, 0);
            seq_table[conn.range(3,0)][slot] = seq;
            slot_valid[conn.range(3,0)][slot] = 1;

            // Set ack_sequence_number to 0 for data packets
            m.ack_sequence_number = 0;

            meta_out.write(m);
        }
    }
};

}  // namespace nb
