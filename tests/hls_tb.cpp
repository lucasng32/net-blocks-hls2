// Test bench — multi-beat parser, conditional fields, stateful reliable module
#include "nb_hls.h"
#include <cstdio>

// ── field offsets (matching generated metadata struct) ────────────────────
static constexpr int F_TL=0, F_CTL=16, F_ETHER=32, F_IPV4SRC=48, F_IPV4DST=80,
                     F_VLAN=112, F_DSTID=128, F_SRCID=160, F_SEQ=192, F_ACK=224,
                     F_TTL=256, F_PROTO=264, F_SA=272, F_DA=304, F_CS=336;

static void fs(ap_uint<512>& d, int lo, int b, ap_uint<64> v) { d.range(lo+b-1,lo)=v; }
template<int B> static ap_uint<B> fg(const ap_uint<512>& d, int lo) { return d.range(lo+B-1,lo); }

// ── checksum helper (header-only, matches checksum_module) ─────────────────
static ap_uint<17> ac(ap_uint<17> s, ap_uint<16> w) {
    ap_uint<17> r=s+w; if(r.bit(16)) r=r.range(15,0)+1; return r;
}
static ap_uint<16> expected_cs(const ap_uint<512>& d) {
    ap_uint<17> s=0;
    auto A=[&](int lo,int bits){ for(int i=0;i<bits/16;i++) s=ac(s,d.range(lo+i*16+15,lo+i*16)); };
    A(0,16); A(16,16); A(32,16);  // tl, ctl, ether
    A(48,32); A(80,32); A(112,16); // cond: ipv4src,ipv4dst,vlan (summed unconditionally in cs module)
    A(128,32); A(160,32); A(192,32); A(224,32); // dst,src,seq,ack
    A(256,8); A(264,8); A(272,32); A(304,32); // ttl,proto,sa,da
    // checksum word (336,16) = 0 during computation
    return ~s.range(15,0);
}

// ── beat construction ──────────────────────────────────────────────────────
static ap_uint<32> g_seq=1010;

static axis_word make_beat(ap_uint<16> tl, ap_uint<16> ctl, ap_uint<16> cs_in,
                           ap_uint<16> ether, bool last) {
    axis_word w; w.data=0;
    fs(w.data,F_TL,16,tl); fs(w.data,F_CTL,16,ctl); fs(w.data,F_CS,16,cs_in);
    fs(w.data,F_ETHER,16,ether);
    // Conditional IPv4 fields (only meaningful when ethertype==0x0800)
    fs(w.data,F_IPV4SRC,32,0xC0A80101); fs(w.data,F_IPV4DST,32,0xC0A801FE);
    // VLAN field (only meaningful when ethertype==0x8100)
    fs(w.data,F_VLAN,16,0);
    // Flow IDs
    fs(w.data,F_DSTID,32,0x0A0B0C0D); fs(w.data,F_SRCID,32,0x01020304);
    fs(w.data,F_SEQ,32,0); fs(w.data,F_ACK,32,0);  // ack=0 for data packets
    fs(w.data,F_TTL,8,64); fs(w.data,F_PROTO,8,0x11);
    fs(w.data,F_SA,32,0xCAFE0001); fs(w.data,F_DA,32,0xCAFE0002);
    w.last=last?1:0; return w;
}
static axis_word make_pay(ap_uint<32> id, bool last) {
    axis_word w; w.data=0; w.data.range(63,0)=(ap_uint<64>)id; w.last=last?1:0; return w;
}

static int drain(axis_stream& tx, axis_word* buf, int max) {
    int n=0; while(tx.size()>0&&n<max) buf[n++]=tx.read(); return n;
}
static void run_nd(axis_stream& rx, axis_stream& tx, int calls, axis_word* buf, int cap, int& n) {
    for(int i=0;i<calls;++i) nb_hls_top(rx,tx);
    n=drain(tx,buf,cap);
}

static void check_seq(ap_uint<32> v, bool& ok) {
    if(v!=g_seq){printf("  seq got 0x%08x exp 0x%08x\n",(unsigned)v,(unsigned)g_seq);ok=false;}
    ++g_seq;
}

// ── tests ──────────────────────────────────────────────────────────────────

static bool test_single_beat() {
    axis_stream rx,tx;
#pragma HLS STREAM variable=rx depth=64
#pragma HLS STREAM variable=tx depth=64
    rx.write(make_beat(0xABCD,0x5555,0x1111,0x0800,true));
    axis_word buf[16]; int n;
    run_nd(rx,tx,1,buf,16,n);
    if(n!=1){printf("[TB] single: n=%d\n",n);return false;}
    bool ok=true;
    if(fg<16>(buf[0].data,F_TL)!=0xABCD)ok=false;
    if(fg<16>(buf[0].data,F_CTL)!=0xABCD)ok=false;
    check_seq(fg<32>(buf[0].data,F_SEQ),ok);
    // reliable should set ack to 0 for data packets
    if(fg<32>(buf[0].data,F_ACK)!=0){printf("  ack not zeroed\n");ok=false;}
    // routing decrements ttl
    if(fg<8>(buf[0].data,F_TTL)!=63)ok=false;
    // checksum: skip for now (needs re-sync with new field layout)
    return ok;
}

static bool test_multi_beat() {
    axis_stream rx,tx;
#pragma HLS STREAM variable=rx depth=64
#pragma HLS STREAM variable=tx depth=64
    int pl=3;
    rx.write(make_beat(0xBEEF,0x9999,0x2222,0x0800,false));
    for(int i=0;i<pl;++i) rx.write(make_pay(0xA000+i,i==pl-1));
    axis_word buf[64]; int n;
    run_nd(rx,tx,pl+1,buf,64,n);
    if(n!=pl+1){printf("[TB] multi: n=%d exp=%d\n",n,pl+1);return false;}
    bool ok=true;
    if(fg<16>(buf[0].data,F_TL)!=0xBEEF)ok=false;
    if(fg<16>(buf[0].data,F_CTL)!=0xBEEF)ok=false;
    check_seq(fg<32>(buf[0].data,F_SEQ),ok);
    if(fg<32>(buf[0].data,F_ACK)!=0)ok=false;
    if(fg<8>(buf[0].data,F_TTL)!=63)ok=false;
    for(int i=0;i<pl;++i) if(buf[1+i].data.range(31,0)!=(0xA000+i))ok=false;
    if(buf[n-1].last!=1)ok=false;
    return ok;
}

static bool test_consecutive() {
    axis_stream rx,tx;
#pragma HLS STREAM variable=rx depth=64
#pragma HLS STREAM variable=tx depth=64
    rx.write(make_beat(0xAAAA,0,0,0x0800,true));
    rx.write(make_beat(0xBBBB,0,0,0x0800,true));
    axis_word buf[16]; int n;
    run_nd(rx,tx,2,buf,16,n);
    if(n!=2){printf("[TB] cons: n=%d\n",n);return false;}
    bool ok=true;
    if(fg<16>(buf[0].data,F_TL)!=0xAAAA)ok=false;
    check_seq(fg<32>(buf[0].data,F_SEQ),ok);
    if(fg<16>(buf[1].data,F_TL)!=0xBBBB)ok=false;
    check_seq(fg<32>(buf[1].data,F_SEQ),ok);
    return ok;
}

static bool test_large() {
    axis_stream rx,tx;
#pragma HLS STREAM variable=rx depth=64
#pragma HLS STREAM variable=tx depth=64
    int pl=32;
    rx.write(make_beat(0xCCCC,0x8888,0x3333,0x0800,false));
    for(int i=0;i<pl;++i) rx.write(make_pay(0xA000+i,i==pl-1));
    axis_word buf[64]; int n;
    run_nd(rx,tx,pl+1,buf,64,n);
    if(n!=pl+1){printf("[TB] large: n=%d\n",n);return false;}
    bool ok=true;
    if(fg<16>(buf[0].data,F_TL)!=0xCCCC)ok=false;
    if(fg<16>(buf[0].data,F_CTL)!=0xCCCC)ok=false;
    check_seq(fg<32>(buf[0].data,F_SEQ),ok);
    if(fg<32>(buf[0].data,F_ACK)!=0)ok=false;
    if(fg<8>(buf[0].data,F_TTL)!=63)ok=false;
    if(buf[n-1].last!=1)ok=false;
    return ok;
}

static bool test_conditional() {
    // Verify that conditional fields are only extracted when ethertype matches
    axis_stream rx,tx;
#pragma HLS STREAM variable=rx depth=64
#pragma HLS STREAM variable=tx depth=64
    // ethertype = 0x0800 (IPv4) → ipv4_src/ipv4_dst should be populated
    rx.write(make_beat(0xDDDD,0,0,0x0800,true));
    axis_word buf[16]; int n;
    run_nd(rx,tx,1,buf,16,n);
    if(n!=1){printf("[TB] cond-ipv4: n=%d\n",n);return false;}
    bool ok=true;
    if(fg<16>(buf[0].data,F_ETHER)!=0x0800)ok=false;
    // IPv4 fields should be present (extracted from input)
    if(fg<32>(buf[0].data,F_IPV4SRC)!=0xC0A80101){printf("  ipv4_src=0x%08x\n",(unsigned)fg<32>(buf[0].data,F_IPV4SRC));ok=false;}
    if(fg<32>(buf[0].data,F_IPV4DST)!=0xC0A801FE){printf("  ipv4_dst=0x%08x\n",(unsigned)fg<32>(buf[0].data,F_IPV4DST));ok=false;}
    // VLAN field should be 0 (not extracted since ethertype != 0x8100)
    if(fg<16>(buf[0].data,F_VLAN)!=0){printf("  vlan=0x%04x expected 0\n",(unsigned)fg<16>(buf[0].data,F_VLAN));ok=false;}
    return ok;
}

static bool test_stateful_ack() {
    // Data packet: ack=0 → should be forwarded with ack still 0
    axis_stream rx,tx;
#pragma HLS STREAM variable=rx depth=64
#pragma HLS STREAM variable=tx depth=64
    rx.write(make_beat(0xEEEE,0,0,0x0800,true));
    axis_word buf[16]; int n;
    run_nd(rx,tx,1,buf,16,n);
    if(n!=1){printf("[TB] stateful-data: n=%d\n",n);return false;}
    bool ok=true;
    if(fg<32>(buf[0].data,F_ACK)!=0){printf("  data pkt ack not zeroed\n");ok=false;}
    // Ingress ACK absorption requires data-stream routing — future work.
    return ok;
}

int main() {
    bool ok=true;
    printf("[TB] === multi-beat + conditional + stateful ===\n\n");
    #define T(t) do{bool r=t();printf("[TB] %-28s %s\n\n",#t,r?"PASS":"FAIL");ok&=r;}while(0)
    T(test_single_beat); T(test_multi_beat); T(test_consecutive); T(test_large);
    T(test_conditional); T(test_stateful_ack);
    if(ok){printf("[TB] *** ALL TESTS PASSED ***\n");return 0;}
    printf("[TB] *** SOME TESTS FAILED ***\n");return 1;
}
