#define NB_HLS_CODEGEN

#include <cstdint>
#include <modules/payload_module.hpp>
#include <modules/checksum_module.hpp>

struct nb_metadata {
    uint16_t total_len;
    uint16_t computed_total_len;
    uint16_t checksum;
};

int main() {
    hls::stream<nb_metadata> in, out;
    nb::payload_module::process(in, out);
    nb::checksum_module::process(in, out);
    return 0;
}
