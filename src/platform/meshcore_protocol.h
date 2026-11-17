/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>

// Minimal, allocation-free codec for the MeshCore packets the toolkit needs:
// zero-hop NODE_DISCOVER_REQ/RESP and ADVERT (node name) payloads.
//
// Wire reference: MeshCore src/Packet.h / src/Mesh.cpp. A zero-hop direct
// packet is  [header][path_len=0][payload...]  and the header is
//   bits 0..1 route (2 = direct), bits 2..5 payload type (0x0B control),
//   bits 6..7 version (0).
namespace platform::meshcore {

constexpr uint8_t kRouteTransportFlood  = 0x00;
constexpr uint8_t kRouteFlood           = 0x01;
constexpr uint8_t kRouteDirect          = 0x02;
constexpr uint8_t kRouteTransportDirect = 0x03;

constexpr uint8_t kPayloadControl = 0x0B;
constexpr uint8_t kPayloadAdvert  = 0x04;

constexpr uint8_t kCtlTypeDiscoverReq  = 0x80;
constexpr uint8_t kCtlTypeDiscoverResp = 0x90;

constexpr uint8_t kAdvTypeNone     = 0;
constexpr uint8_t kAdvTypeChat     = 1;
constexpr uint8_t kAdvTypeRepeater = 2;
constexpr uint8_t kAdvTypeRoom     = 3;
constexpr uint8_t kAdvTypeSensor   = 4;

constexpr size_t kPublicKeySize = 32;
constexpr size_t kDiscoverRequestSize = 12;
constexpr size_t kDiscoverPayloadSize = 10;

struct PacketView {
    uint8_t header{0};
    uint8_t route{0};
    uint8_t payload_type{0};
    uint8_t path_len{0};      // encoded byte: bits 0..5 count, bits 6..7 hash size - 1
    uint8_t hops{0};          // decoded path count (0 = heard directly)
    const uint8_t* path{nullptr};
    const uint8_t* payload{nullptr};
    uint8_t payload_len{0};
};

// Parses the MeshCore wire header/path and points `payload` into `data`.
bool parse_packet(const uint8_t* data, size_t len, PacketView& out);

// Builds  [0x2E][0x00][0x80][type_filter][tag:4 LE][since:4 LE].
// type_filter 0 means every advertised node type.
size_t build_discover_request(uint8_t type_filter, uint32_t tag, uint8_t* out, size_t capacity);

struct DiscoverResponse {
    uint32_t tag{0};
    uint8_t node_type{kAdvTypeNone};
    int8_t their_snr_q4{0};
    uint8_t pubkey[kPublicKeySize]{};
    uint8_t pubkey_len{0};
};

bool parse_discover_response(const PacketView& packet, DiscoverResponse& out);

struct Advert {
    uint8_t pubkey[kPublicKeySize]{};
    uint8_t node_type{kAdvTypeNone};
    char name[32]{};
    bool has_name{false};
};

bool parse_advert(const PacketView& packet, Advert& out);

const char* node_type_name(uint8_t type);
const char* node_type_short(uint8_t type);
uint8_t all_node_type_filter();

// Recolor-safe copy of a node name: collapses control bytes, escapes '#'
// (LVGL recolor delimiter) and caps at `max_chars` UTF-8 codepoints.
void copy_display_name(char* out, size_t out_capacity, const char* source, size_t max_bytes);

} // namespace platform::meshcore
