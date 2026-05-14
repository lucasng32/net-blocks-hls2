// Debug cosim — prints actual output fields
#include "nb_hls.h"
#include <cstdio>

int main() {
    axis_stream rx,tx;
#pragma HLS STREAM variable=rx depth=64
#pragma HLS STREAM variable=tx depth=64

    // Inject one single-beat packet with known values
    axis_word w; w.data=0; w.last=1;
    w.data.range(15,0)  = 0xABCD;   // total_len
    w.data.range(31,16) = 0x5555;   // computed (garbage, should be overwritten)
    w.data.range(47,32) = 0x1111;   // checksum (garbage, should be overwritten)
    rx.write(w);

    // Drive the pipeline enough times
    for (int i = 0; i < 128; ++i)
        nb_hls_top(rx, tx);

    // Read and print output
    while (tx.size() > 0) {
        axis_word out = tx.read();
        printf("OUT: total_len=0x%04x computed=0x%04x checksum=0x%04x last=%d\n",
               (unsigned)out.data.range(15,0),
               (unsigned)out.data.range(31,16),
               (unsigned)out.data.range(47,32),
               (int)out.last);
    }
    if (tx.size() == 0) printf("(no output)\n");

    printf("[TB] debug-cosim done\n");
    return 0;
}
