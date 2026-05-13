#pragma once

#include "nb_common.hpp"

namespace nb {

struct checksum_module {
    // Fields contributed to the metadata struct.
    // 'checksum' is serialized into the packet header.
    static constexpr field_def fields[] = {
        {"checksum", 16},
    };

    static constexpr const char* name   = "checksum";
    static constexpr const char* header = "modules/checksum_module.hpp";

    // Internet checksum helpers — one's complement, 16-bit words.
    //
    // Algorithm:
    //   1. Clear checksum field to 0
    //   2. Sum all header 16-bit words (this function) with wrap-around carry
    //   3. Take one's complement → store in checksum
    //
    // NOTE: The original net-blocks-hls also supports full_packet mode,
    // which requires scanning the payload data stream.  In the current
    // Parser-Match-Action-Deparser architecture the data stream bypasses
    // action modules, so only header_only is implemented here.  Full-packet
    // would route the data stream through the checksum module.

    // Add a 16-bit word into the running one's complement accumulator.
    static ap_uint<17> add_carry(ap_uint<17> sum, ap_uint<16> word) {
#pragma HLS INLINE
        ap_uint<17> r = sum + word;
        // wrap-around carry: if overflow, add 1 to low 16 bits
        if (r.bit(16)) r = r.range(15, 0) + 1;
        return r;
    }

    template <typename Meta>
    static void process(hls::stream<Meta>& meta_in, hls::stream<Meta>& meta_out) {
#pragma HLS PIPELINE II = 1
        Meta m = meta_in.read();

        // Save the original checksum (for ingress verification — future use)
        ap_uint<16> saved = m.checksum;

        // Clear checksum field before computing
        m.checksum = 0;

        // ── one's complement sum over all header 16-bit words ──────────
        ap_uint<17> sum = 0;

        // total_len         — bits 15:0,  1 word
        sum = add_carry(sum, m.total_len);

        // computed_total_len — bits 31:16, 1 word
        sum = add_carry(sum, m.computed_total_len);

        // checksum          — bits 47:32, 1 word (currently 0)
        sum = add_carry(sum, m.checksum);

        // ═══ additional fields from other modules go here ═══
        // When a new module adds metadata fields, add them below.
        // For fields wider than 16 bits, split into 16-bit chunks:
        //   sum = add_carry(sum, m.long_field.range(15, 0));
        //   sum = add_carry(sum, m.long_field.range(31, 16));

        // One's complement
        ap_uint<16> computed = ~sum.range(15, 0);

        // Write checksum into metadata
        m.checksum = computed;

        meta_out.write(m);
    }
};

}  // namespace nb
