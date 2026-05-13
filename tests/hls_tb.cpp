// Test bench for nb_hls_top pipeline
// Verifies: parser → payload_action → checksum_action → deparser
#include "nb_hls.h"

#include <cstdio>

// ── bit-field positions in the 512-bit data word ────────────────────────────

static constexpr int TOTAL_LEN_HI          = 15;
static constexpr int TOTAL_LEN_LO          = 0;
static constexpr int COMPUTED_TOTAL_LEN_HI = 31;
static constexpr int COMPUTED_TOTAL_LEN_LO = 16;
static constexpr int CHECKSUM_HI           = 47;
static constexpr int CHECKSUM_LO           = 32;

// ── field accessors ─────────────────────────────────────────────────────────

static void set_total_len(ap_uint<NB_DATA_WIDTH>& data, ap_uint<16> val) {
    data.range(TOTAL_LEN_HI, TOTAL_LEN_LO) = val;
}
static ap_uint<16> get_total_len(const ap_uint<NB_DATA_WIDTH>& data) {
    return data.range(TOTAL_LEN_HI, TOTAL_LEN_LO);
}

static void set_computed_total_len(ap_uint<NB_DATA_WIDTH>& data, ap_uint<16> val) {
    data.range(COMPUTED_TOTAL_LEN_HI, COMPUTED_TOTAL_LEN_LO) = val;
}
static ap_uint<16> get_computed_total_len(const ap_uint<NB_DATA_WIDTH>& data) {
    return data.range(COMPUTED_TOTAL_LEN_HI, COMPUTED_TOTAL_LEN_LO);
}

static void set_checksum(ap_uint<NB_DATA_WIDTH>& data, ap_uint<16> val) {
    data.range(CHECKSUM_HI, CHECKSUM_LO) = val;
}
static ap_uint<16> get_checksum(const ap_uint<NB_DATA_WIDTH>& data) {
    return data.range(CHECKSUM_HI, CHECKSUM_LO);
}

// ── beat construction ───────────────────────────────────────────────────────

static axis_word make_header_beat(ap_uint<16> total_len,
                                  ap_uint<16> computed_total_len_in,
                                  ap_uint<16> checksum_in,
                                  bool         last) {
    axis_word w;
    w.data = 0;
    set_total_len(w.data, total_len);
    set_computed_total_len(w.data, computed_total_len_in);
    set_checksum(w.data, checksum_in);
    w.last = last ? 1 : 0;
    return w;
}

static axis_word make_payload_beat(ap_uint<32> id, bool last) {
    axis_word w;
    w.data = 0;
    w.data.range(63, 0) = (ap_uint<64>)id;
    w.last = last ? 1 : 0;
    return w;
}

// ── injection ───────────────────────────────────────────────────────────────

static void inject_single(axis_stream& rx,
                          ap_uint<16>  total_len,
                          ap_uint<16>  computed_total_len_in,
                          ap_uint<16>  checksum_in) {
    rx.write(make_header_beat(total_len, computed_total_len_in, checksum_in, true));
}

static void inject_multi(axis_stream& rx,
                         ap_uint<16>  total_len,
                         ap_uint<16>  computed_total_len_in,
                         ap_uint<16>  checksum_in,
                         int          n_payload) {
    rx.write(make_header_beat(total_len, computed_total_len_in, checksum_in,
                              n_payload == 0));
    for (int i = 0; i < n_payload; ++i) {
        rx.write(make_payload_beat(0xA000 + i, i == n_payload - 1));
    }
}

// ── drain ───────────────────────────────────────────────────────────────────

static int drain_packet(axis_stream& tx, axis_word* buf, int max_beats) {
    int n = 0;
    while (tx.size() > 0 && n < max_beats) {
        buf[n] = tx.read();
        ++n;
        if (buf[n - 1].last)
            break;
    }
    return n;
}

// ── drive the pipeline ──────────────────────────────────────────────────────

// Drive nb_hls_top until all expected output words appear.
// Returns total number of output beats drained.
static int drive_and_drain(axis_stream& rx, axis_stream& tx,
                           int max_cycles, int expected_out_words,
                           axis_word* buf, int buf_cap) {
    int cycles = 0;
    int drained_total = 0;
    while (cycles < max_cycles) {
        // Only call nb_hls_top if there is data queued or we haven't seen
        // all the expected outputs yet (pipeline may still have beats in flight).
        if (rx.size() > 0 || drained_total < expected_out_words) {
            nb_hls_top(rx, tx);
        }
        // drain whatever is on tx right now
        axis_word tmp[64];
        int n = drain_packet(tx, tmp, 64);
        for (int i = 0; drained_total + i < buf_cap && i < n; ++i) {
            buf[drained_total + i] = tmp[i];
        }
        drained_total += n;

        if (rx.size() == 0 && tx.size() == 0 && drained_total >= expected_out_words)
            break;

        ++cycles;
    }
    return drained_total;
}

// ── tests ───────────────────────────────────────────────────────────────────

static bool test_single_beat() {
    axis_stream rx, tx;
#pragma HLS STREAM variable = rx depth = 64
#pragma HLS STREAM variable = tx depth = 64

    inject_single(rx, 0xABCD, 0x5555, 0x1111);

    axis_word buf[16];
    int n = drive_and_drain(rx, tx, 128, 1, buf, 16);

    if (n != 1) {
        printf("[TB] single-beat: expected 1 beat, got %d\n", n);
        return false;
    }

    bool ok = true;
    if (get_total_len(buf[0].data) != 0xABCD) {
        printf("[TB] single-beat: total_len = 0x%04x, expected 0xABCD\n",
               (unsigned)get_total_len(buf[0].data));
        ok = false;
    }
    if (get_computed_total_len(buf[0].data) != 0xABCD) {
        printf("[TB] single-beat: computed_total_len = 0x%04x, expected 0xABCD\n",
               (unsigned)get_computed_total_len(buf[0].data));
        ok = false;
    }
    if (get_checksum(buf[0].data) != 0x0000) {
        printf("[TB] single-beat: checksum = 0x%04x, expected 0x0000\n",
               (unsigned)get_checksum(buf[0].data));
        ok = false;
    }
    return ok;
}

static bool test_multi_beat_passthrough() {
    axis_stream rx, tx;
#pragma HLS STREAM variable = rx depth = 64
#pragma HLS STREAM variable = tx depth = 64

    const int payload_beats = 3;
    inject_multi(rx, 0xBEEF, 0x9999, 0x2222, payload_beats);

    axis_word buf[64];
    int n = drive_and_drain(rx, tx, 256, payload_beats + 1, buf, 64);

    if (n != payload_beats + 1) {
        printf("[TB] multi-beat: expected %d beats, got %d\n", payload_beats + 1, n);
        return false;
    }

    bool ok = true;
    if (get_total_len(buf[0].data) != 0xBEEF)          ok = false;
    if (get_computed_total_len(buf[0].data) != 0xBEEF) ok = false;
    if (get_checksum(buf[0].data) != 0x0000)           ok = false;

    for (int i = 0; i < payload_beats; ++i) {
        ap_uint<32> expected = 0xA000 + i;
        ap_uint<32> actual   = buf[1 + i].data.range(31, 0);
        if (actual != expected) {
            printf("[TB] multi-beat: payload beat %d = 0x%08x, expected 0x%08x\n",
                   i, (unsigned)actual, (unsigned)expected);
            ok = false;
        }
    }
    if (buf[n - 1].last != 1) {
        printf("[TB] multi-beat: last flag missing\n");
        ok = false;
    }
    return ok;
}

static bool test_consecutive_packets() {
    axis_stream rx, tx;
#pragma HLS STREAM variable = rx depth = 64
#pragma HLS STREAM variable = tx depth = 64

    inject_single(rx, 0xAAAA, 0x0000, 0x0000);
    inject_single(rx, 0xBBBB, 0x0000, 0x0000);

    axis_word buf[16];
    int n = drive_and_drain(rx, tx, 256, 2, buf, 16);

    if (n != 2) {
        printf("[TB] consecutive: expected 2 beats, got %d\n", n);
        return false;
    }

    bool ok = true;
    if (get_total_len(buf[0].data) != 0xAAAA) ok = false;
    if (get_computed_total_len(buf[0].data) != 0xAAAA) ok = false;
    if (get_total_len(buf[1].data) != 0xBBBB) ok = false;
    if (get_computed_total_len(buf[1].data) != 0xBBBB) ok = false;
    return ok;
}

static bool test_large_payload() {
    axis_stream rx, tx;
#pragma HLS STREAM variable = rx depth = 64
#pragma HLS STREAM variable = tx depth = 64

    const int payload_beats = 32;
    inject_multi(rx, 0xCCCC, 0x8888, 0x3333, payload_beats);

    axis_word buf[64];
    int n = drive_and_drain(rx, tx, 512, payload_beats + 1, buf, 64);

    if (n != payload_beats + 1) {
        printf("[TB] large-payload: expected %d beats, got %d\n", payload_beats + 1, n);
        return false;
    }

    bool ok = true;
    if (get_total_len(buf[0].data) != 0xCCCC)          ok = false;
    if (get_computed_total_len(buf[0].data) != 0xCCCC) ok = false;
    if (get_checksum(buf[0].data) != 0x0000)           ok = false;
    if (buf[n - 1].last != 1) {
        printf("[TB] large-payload: last flag missing\n");
        ok = false;
    }
    return ok;
}

static bool test_mixed_packets() {
    // Alternating single-beat and multi-beat packets to stress the state machine.
    axis_stream rx, tx;
#pragma HLS STREAM variable = rx depth = 64
#pragma HLS STREAM variable = tx depth = 64

    // Packet 1: single-beat (0x1111)
    inject_single(rx, 0x1111, 0x0000, 0x0000);
    // Packet 2: multi-beat, 2 payload + 1 header = 3 beats (0x2222)
    inject_multi(rx, 0x2222, 0x0000, 0x0000, 2);
    // Packet 3: single-beat (0x3333)
    inject_single(rx, 0x3333, 0x0000, 0x0000);
    // Packet 4: multi-beat, 5 payload + 1 header = 6 beats (0x4444)
    inject_multi(rx, 0x4444, 0x0000, 0x0000, 5);

    int expected = 1 + 3 + 1 + 6; // 11 total beats
    axis_word buf[64];
    int n = drive_and_drain(rx, tx, 512, expected, buf, 64);

    if (n != expected) {
        printf("[TB] mixed-packets: expected %d beats, got %d\n", expected, n);
        return false;
    }

    int idx = 0;
    bool ok = true;

    // Packet 1: single-beat, total_len=0x1111
    if (get_total_len(buf[idx].data) != 0x1111)      ok = false;
    if (get_computed_total_len(buf[idx].data) != 0x1111) ok = false;
    if (!buf[idx].last) ok = false;
    idx += 1;

    // Packet 2: 3 beats, total_len=0x2222
    if (get_total_len(buf[idx].data) != 0x2222) ok = false;
    if (get_computed_total_len(buf[idx].data) != 0x2222) ok = false;
    if (buf[idx + 2].last != 1) ok = false;
    idx += 3;

    // Packet 3: single-beat, total_len=0x3333
    if (get_total_len(buf[idx].data) != 0x3333) ok = false;
    if (get_computed_total_len(buf[idx].data) != 0x3333) ok = false;
    if (!buf[idx].last) ok = false;
    idx += 1;

    // Packet 4: 6 beats, total_len=0x4444
    if (get_total_len(buf[idx].data) != 0x4444) ok = false;
    if (get_computed_total_len(buf[idx].data) != 0x4444) ok = false;
    if (buf[idx + 5].last != 1) ok = false;

    return ok;
}

// ── main ────────────────────────────────────────────────────────────────────

int main() {
    bool all_ok = true;

    printf("[TB] === net-blocks-hls2 pipeline test bench ===\n\n");

    bool r;

    r = test_single_beat();
    printf("[TB] single-beat packet .............. %s\n\n", r ? "PASS" : "FAIL");
    all_ok &= r;

    r = test_multi_beat_passthrough();
    printf("[TB] multi-beat passthrough .......... %s\n\n", r ? "PASS" : "FAIL");
    all_ok &= r;

    r = test_consecutive_packets();
    printf("[TB] consecutive packets .............. %s\n\n", r ? "PASS" : "FAIL");
    all_ok &= r;

    r = test_large_payload();
    printf("[TB] large payload (32 beats) ........ %s\n\n", r ? "PASS" : "FAIL");
    all_ok &= r;

    r = test_mixed_packets();
    printf("[TB] mixed packets stress test ....... %s\n\n", r ? "PASS" : "FAIL");
    all_ok &= r;

    if (all_ok) {
        printf("[TB] *** ALL TESTS PASSED ***\n");
        return 0;
    } else {
        printf("[TB] *** SOME TESTS FAILED ***\n");
        return 1;
    }
}
