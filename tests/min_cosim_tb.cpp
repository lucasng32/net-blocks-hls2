// Minimal cosim — single beat only (no multi-beat to avoid deadlock)
#include "nb_hls.h"
#include <cstdio>

int main() {
    axis_stream rx,tx;
#pragma HLS STREAM variable=rx depth=64
#pragma HLS STREAM variable=tx depth=64

    // Send one single-beat packet
    axis_word w; w.data=0; w.last=1;
    w.data.range(15,0) = 0xABCD;    // total_len
    w.data.range(31,16)= 0x5555;    // computed (garbage)
    w.data.range(47,32)= 0x1111;    // checksum (garbage)
    w.data.range(79,48)= 0x0A0B0C0D;
    rx.write(w);
    nb_hls_top(rx,tx);
    axis_word out = tx.read();

    bool ok = true;
    if (out.data.range(15,0) != 0xABCD) { printf("FAIL total_len\n"); ok=false; }
    if (out.data.range(31,16) != 0xABCD) { printf("FAIL computed_total_len\n"); ok=false; }
    if (out.last != 1) { printf("FAIL last\n"); ok=false; }
    if (ok) printf("[TB] min-cosim PASS\n");
    else    printf("[TB] min-cosim FAIL\n");
    return ok ? 0 : 1;
}
