#include "nb_hls.h"
#include <cstdio>

int main() {
    bool ok=true;
    printf("[TB] === single-phase deparser + depth64 ===\n\n");
    
    auto test1=[&](int nbeats){
        axis_stream rx,tx;
#pragma HLS STREAM variable=rx depth=64
#pragma HLS STREAM variable=tx depth=64
        
        axis_word w; 
        w.data=0; 
        w.last=(nbeats==1); 
        w.data.range(15,0)=0xABCD;
        w.keep=-1; // Required for valid RTL input
        w.strb=-1; // Required for valid RTL input
        rx.write(w);
        
        for(int i=1;i<nbeats;i++){
            axis_word p;
            p.data=0;
            p.data.range(31,0)=0xA000+i;
            p.last=(i==nbeats-1);
            p.keep=-1; // Required for valid RTL input
            p.strb=-1; // Required for valid RTL input
            rx.write(p);
        }
        
        int n=0;
        // Fixed 512-cycle pump without early exit
        for(int i=0; i<512; ++i) {
            nb_hls_top(rx,tx);
            
            while(!tx.empty()) {
                axis_word out_word = tx.read();
                ++n;
            }
        }
        
        printf("[TB] %dbeats: n=%d exp=%d %s\n",nbeats,n,nbeats,n==nbeats?"PASS":"FAIL");
        return n==nbeats;
    };
    
    ok&=test1(1); ok&=test1(4); ok&=test1(32);
    
    printf("[TB] %s\n",ok?"*** ALL PASS ***":"*** FAIL ***");
    return ok?0:1;
}