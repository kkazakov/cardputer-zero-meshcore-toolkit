/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "meshcore_protocol.h"

#include <cstring>

namespace platform::meshcore {
namespace {

uint32_t read_u32_le(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8U) |
           (static_cast<uint32_t>(data[2]) << 16U) |
           (static_cast<uint32_t>(data[3]) << 24U);
}

void write_u32_le(uint8_t* data, uint32_t value) {
    data[0] = static_cast<uint8_t>(value & 0xFFU);
    data[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    data[2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    data[3] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

bool is_continuation(uint8_t byte) {
    return (byte & 0xC0U) == 0x80U;
}

} // namespace

bool parse_packet(const uint8_t* data, size_t len, PacketView& out) {
    if (!data || len < 2) {
        return false;
    }

    size_t index = 0;
    out.header = data[index++];
    out.route = out.header & 0x03U;
    out.payload_type = static_cast<uint8_t>((out.header >> 2U) & 0x0FU);

    // Transport routes carry two 16-bit transport codes before the path.
    if (out.route == kRouteTransportFlood || out.route == kRouteTransportDirect) {
        if (index + 4 > len) {
            return false;
        }
        index += 4;
    }

    if (index >= len) {
        return false;
    }
    out.path_len = data[index++];
    const size_t hash_size = static_cast<size_t>((out.path_len >> 6U) & 0x03U) + 1;
    out.hops = out.path_len & 0x3FU;
    const size_t path_bytes = hash_size * out.hops;
    if (index + path_bytes > len) {
        return false;
    }
    out.path = path_bytes ? &data[index] : nullptr;
    index += path_bytes;

    out.payload = &data[index];
    out.payload_len = static_cast<uint8_t>(len - index);
    return true;
}

size_t build_discover_request(uint8_t type_filter, uint32_t tag, uint8_t* out, size_t capacity) {
    if (!out || capacity < kDiscoverRequestSize) {
        return 0;
    }

    out[0] = static_cast<uint8_t>((kPayloadControl << 2U) | kRouteDirect);
    out[1] = 0x00; // zero-hop: direct route with an empty path
    out[2] = kCtlTypeDiscoverReq;
    out[3] = type_filter ? type_filter : all_node_type_filter();
    write_u32_le(&out[4], tag);
    write_u32_le(&out[8], 0); // since = 0: answer regardless of freshness
    return kDiscoverRequestSize;
}

bool parse_discover_response(const PacketView& packet, DiscoverResponse& out) {
    if (packet.payload_type != kPayloadControl || !packet.payload) {
        return false;
    }
    if ((packet.payload[0] & 0xF0U) != kCtlTypeDiscoverResp || packet.payload_len < 6 + 8) {
        return false;
    }

    const size_t key_len = packet.payload_len >= 6 + kPublicKeySize ? kPublicKeySize : 8;
    out.tag = read_u32_le(&packet.payload[2]);
    out.node_type = packet.payload[0] & 0x0FU;
    out.their_snr_q4 = static_cast<int8_t>(packet.payload[1]);
    out.pubkey_len = static_cast<uint8_t>(key_len);
    std::memset(out.pubkey, 0, sizeof(out.pubkey));
    std::memcpy(out.pubkey, &packet.payload[6], key_len);
    return true;
}

bool parse_advert(const PacketView& packet, Advert& out) {
    // pubkey(32) + timestamp(4) + signature(64) + app_data(1..)
    if (packet.payload_type != kPayloadAdvert || !packet.payload ||
        packet.payload_len < kPublicKeySize + 4 + 64 + 1) {
        return false;
    }

    out = {};
    std::memcpy(out.pubkey, packet.payload, kPublicKeySize);
    const uint8_t* app = &packet.payload[kPublicKeySize + 4 + 64];
    const size_t app_len = packet.payload_len - (kPublicKeySize + 4 + 64);
    const uint8_t flags = app[0];
    out.node_type = flags & 0x0FU;

    size_t index = 1;
    if (flags & 0x10U) index += 8; // latitude/longitude
    if (flags & 0x20U) index += 2; // feature 1
    if (flags & 0x40U) index += 2; // feature 2
    if ((flags & 0x80U) == 0 || index >= app_len) {
        return true; // valid advert, just no name
    }

    const size_t available = app_len - index;
    std::memset(out.name, 0, sizeof(out.name));
    size_t written = 0;
    for (size_t i = 0; i < available && written + 1 < sizeof(out.name); ++i) {
        const uint8_t byte = app[index + i];
        if (byte < 0x20U || byte == 0x7FU) {
            continue;
        }
        out.name[written++] = static_cast<char>(byte);
    }
    out.name[written] = '\0';
    out.has_name = written > 0;
    return true;
}

const char* node_type_name(uint8_t type) {
    switch (type) {
        case kAdvTypeChat: return "companion";
        case kAdvTypeRepeater: return "repeater";
        case kAdvTypeRoom: return "room";
        case kAdvTypeSensor: return "sensor";
        default: return "node";
    }
}

const char* node_type_short(uint8_t type) {
    switch (type) {
        case kAdvTypeChat: return "C";
        case kAdvTypeRepeater: return "R";
        case kAdvTypeRoom: return "Rm";
        case kAdvTypeSensor: return "S";
        default: return "?";
    }
}

uint8_t all_node_type_filter() {
    return static_cast<uint8_t>((1U << kAdvTypeChat) | (1U << kAdvTypeRepeater) |
                                (1U << kAdvTypeRoom) | (1U << kAdvTypeSensor));
}

void copy_display_name(char* out, size_t out_capacity, const char* source, size_t max_bytes) {
    if (!out || out_capacity == 0) {
        return;
    }
    out[0] = '\0';
    if (!source || max_bytes == 0) {
        return;
    }

    size_t written = 0;
    size_t codepoints = 0;
    size_t index = 0;
    while (source[index] != '\0' && index < max_bytes && written + 2 < out_capacity) {
        const uint8_t lead = static_cast<uint8_t>(source[index]);
        if (lead < 0x20U || lead == 0x7FU) {
            out[written++] = ' ';
            ++index;
            continue;
        }

        size_t length = 1;
        if ((lead & 0xE0U) == 0xC0U) {
            length = 2;
        } else if ((lead & 0xF0U) == 0xE0U) {
            length = 3;
        } else if ((lead & 0xF8U) == 0xF0U) {
            length = 4;
        }
        for (size_t i = 1; i < length; ++i) {
            if (source[index + i] == '\0' ||
                !is_continuation(static_cast<uint8_t>(source[index + i]))) {
                length = 1;
                break;
            }
        }
        if (codepoints >= 18 || index + length > max_bytes) {
            break;
        }

        if (length == 1 && lead == '#') {
            if (written + 2 >= out_capacity) {
                break;
            }
            out[written++] = '#';
            out[written++] = '#';
        } else {
            if (written + length >= out_capacity) {
                break;
            }
            for (size_t i = 0; i < length; ++i) {
                out[written++] = static_cast<char>(source[index + i]);
            }
        }
        ++codepoints;
        index += length;
    }

    while (written > 0 && out[written - 1] == ' ') {
        --written;
    }
    out[written] = '\0';
}

} // namespace platform::meshcore
