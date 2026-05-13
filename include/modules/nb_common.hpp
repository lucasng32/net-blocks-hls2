#pragma once

#ifdef NB_HLS_CODEGEN
// Minimal stubs so the code generator can parse module template declarations.
// These are never instantiated — we only read static constexpr members.
namespace hls {
template <typename T>
struct stream {
    T read();
    void write(const T&);
};
}  // namespace hls
#else
#include <hls_stream.h>
#include <ap_axi_sdata.h>
#include <ap_int.h>
#endif

namespace nb {

struct field_def {
    const char* name;
    int bit_width;
};

template <typename... Ts>
struct type_list {};

}  // namespace nb
