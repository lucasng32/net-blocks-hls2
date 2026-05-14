#pragma once

#ifdef NB_HLS_CODEGEN
// Minimal stubs so the code generator can parse module template declarations.
// These are never instantiated — we only read static constexpr members.
namespace hls {
template <typename T>
struct stream {
    T read();
    void write(const T&);
    bool empty() const;
};
}  // namespace hls

template <int W>
struct ap_uint {
    ap_uint() {}
    ap_uint(const ap_uint&) {}
    ap_uint(unsigned long long) {}
    template <int W2>
    ap_uint(const ap_uint<W2>&) {}
    ap_uint<W> range(int, int) const { return ap_uint<W>(); }
    ap_uint<W>& operator=(const ap_uint<W>&) { return *this; }
    ap_uint<W>& operator=(unsigned long long) { return *this; }
    template<int W2>
    ap_uint<W>& operator=(const ap_uint<W2>&) { return *this; }
    ap_uint<W> operator~() const { return ap_uint<W>(); }
    bool operator>(unsigned long long) const { return false; }
    ap_uint<W> operator-(unsigned long long) const { return ap_uint<W>(); }
    ap_uint<W>& operator++() { return *this; }
    operator unsigned int() const { return 0; }
    bool bit(int) const { return false; }
    bool operator==(unsigned long long) const { return false; }
    bool operator!=(unsigned long long) const { return true; }
};
template <int W1, int W2>
ap_uint<W1> operator+(const ap_uint<W1>&, const ap_uint<W2>&) { return {}; }
template <int W>
ap_uint<W> operator+(const ap_uint<W>&, int) { return {}; }
template <int W>
ap_uint<W> operator+(int, const ap_uint<W>&) { return {}; }
template <int W>
struct ap_int { ap_int() {} ap_int(const ap_int&) {} };

struct axis_wordi { ap_uint<512> data; ap_uint<1> last; axis_wordi() {} };
using axis_streai = hls::stream<axis_wordi>;
#else
#include <hls_stream.h>
#include <ap_axi_sdata.h>
#include <ap_int.h>
#endif

namespace nb {

struct field_def {
    const char* name;
    int         bit_width;
    const char* depends_on   = nullptr;   // parent field name for conditional extraction
    int         match_value  = 0;         // parent value that triggers this child
};

template <typename... Ts>
struct type_list {};

}  // namespace nb
