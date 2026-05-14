#pragma once

#include "nb_common.hpp"

namespace nb {

struct payload_module {
    static constexpr field_def fields[] = {
        {"total_len", 16},
        {"computed_total_len", 16},
    };

    static constexpr const char* name      = "payload";
    static constexpr const char* header    = "modules/payload_module.hpp";

    template <typename Meta>
    static void process(hls::stream<Meta>& meta_in, hls::stream<Meta>& meta_out) {
#pragma HLS PIPELINE II = 1
        if (meta_in.empty()) return;
        Meta m = meta_in.read();
        m.computed_total_len = m.total_len;
        meta_out.write(m);
    }
};

}  // namespace nb
