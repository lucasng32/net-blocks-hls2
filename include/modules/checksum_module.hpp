#pragma once

#include "nb_common.hpp"

namespace nb {

struct checksum_module {
    static constexpr field_def fields[] = {
        {"checksum", 16},
    };

    static constexpr const char* name   = "checksum";
    static constexpr const char* header = "modules/checksum_module.hpp";
    static constexpr bool needs_data    = false;

    // ── one's complement helpers ────────────────────────────────────

    static ap_uint<17> add_carry(ap_uint<17> sum, ap_uint<16> word) {
#pragma HLS INLINE
        ap_uint<17> r = sum + word;
        if (r.bit(16)) r = r.range(15, 0) + ap_uint<17>(1);
        return r;
    }

    static void add_field(ap_uint<17>& sum, ap_uint<16> f) {
#pragma HLS INLINE
        sum = add_carry(sum, f);
    }

    template <int W>
    static void add_wide(ap_uint<17>& sum, const ap_uint<W>& f) {
#pragma HLS INLINE
        for (int i = 0; i < W / 16; ++i)
            sum = add_carry(sum, f.range(i * 16 + 15, i * 16));
    }

    // ── process (header_only, metadata only) ────────────────────────
    //
    // Runs LAST in the metadata chain (after all header mutations).
    // Computes the Internet checksum over all header 16-bit words
    // with the checksum field treated as zero.
    //
    // Full-packet mode (summing payload data) requires routing the
    // data stream through this module — deferred to avoid the
    // metadata/data timing deadlock in the DATAFLOW pipeline.

    template <typename Meta>
    static void process(hls::stream<Meta>& meta_in,
                        hls::stream<Meta>& meta_out) {
#pragma HLS PIPELINE II = 1
        Meta m = meta_in.read();

        ap_uint<17> sum = 0;

        add_field(sum, m.total_len);
        add_field(sum, m.computed_total_len);
        add_field(sum, ap_uint<16>(0));                // checksum word
        add_field(sum, m.ethertype);
        // conditional — present if ethertype==0x0800
        add_wide (sum, m.ipv4_src);
        add_wide (sum, m.ipv4_dst);
        // conditional — present if ethertype==0x8100
        add_field(sum, m.vlan_tci);
        // flow identifiers
        add_wide (sum, m.dst_id);
        add_wide (sum, m.src_id);
        add_wide (sum, m.sequence_number);
        add_wide (sum, m.ack_sequence_number);           // reliable
        {
            ap_uint<16> rw = (ap_uint<16>(m.ip_protocol) << 8)
                           |  ap_uint<16>(m.ttl);
            add_field(sum, rw);
        }
        add_wide (sum, m.src_addr);
        add_wide (sum, m.dst_addr);

        m.checksum = ~sum.range(15, 0);
        meta_out.write(m);
    }
};

}  // namespace nb
