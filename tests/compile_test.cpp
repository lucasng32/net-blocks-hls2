#define NB_HLS_CODEGEN
#include <cstdint>
#include <modules/payload_module.hpp>
#include <modules/checksum_module.hpp>
#include <modules/header_modules.hpp>

struct nb_metadata {
    ap_uint<16> total_len, computed_total_len, ethertype;
    ap_uint<32> ipv4_src, ipv4_dst;
    ap_uint<16> vlan_tci;
    ap_uint<32> dst_id, src_id, sequence_number, ack_sequence_number;
    ap_uint<8>  ttl, ip_protocol;
    ap_uint<32> src_addr, dst_addr;
    ap_uint<16> checksum;
};

int main() {
    hls::stream<nb_metadata> in, out;
    axis_streai d_in, d_out;
    nb::payload_module::process(in, out);
    nb::checksum_module::process(in, out);
    nb::identifier_module::process(in, out);
    nb::inorder_module::process(in, out);
    nb::routing_module::process(in, out);
    (void)d_in; (void)d_out;
    return 0;
}
