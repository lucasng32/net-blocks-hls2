#pragma once

#include "modules/payload_module.hpp"
#include "modules/checksum_module.hpp"
#include "modules/header_modules.hpp"
#include "modules/reliable_module.hpp"

// Module registration order = field ordering in the metadata struct.
// The code generator assigns bit offsets in this order.
//
// Reliable runs after inorder (it depends on sequence_number).
// Checksum runs LAST so it sees the final header values.
//
// ── dynamic protocol demo ────────────────────────────────────────────
// identifier_module demonstrates conditional field extraction:
//   ethertype=0x0800  →  dst_id, src_id are IPv4 fields
//   ethertype=0x8100  →  (future: VLAN fields)
// (uncomment depends_on/match_value in identifier_module to enable)

using protocol_modules = nb::type_list<
    nb::payload_module,
    nb::identifier_module,
    nb::inorder_module,
    nb::reliable_module,     // stateful — tracks ACKs per connection
    nb::routing_module,
    nb::checksum_module      // LAST — sees final metadata
>;

constexpr int protocol_data_width = 512;
