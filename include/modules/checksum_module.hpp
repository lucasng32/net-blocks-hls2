#pragma once

#include "nb_common.hpp"

namespace nb {

struct checksum_module {
    static constexpr field_def fields[] = {
        {"checksum", 16},
    };

    static constexpr const char* name      = "checksum";
    static constexpr const char* header    = "modules/checksum_module.hpp";

    template <typename Meta>
    static void process(hls::stream<Meta>& meta_in, hls::stream<Meta>& meta_out) {
#pragma HLS PIPELINE II = 1
        Meta m = meta_in.read();
        m.checksum = 0;
        meta_out.write(m);
    }
};

}  // namespace nb
