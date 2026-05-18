#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace icom {

inline constexpr int CONTROL_SIZE = 0x10;
inline constexpr int PING_SIZE = 0x15;
inline constexpr int OPENCLOSE_SIZE = 0x16;
inline constexpr int TOKEN_SIZE = 0x40;
inline constexpr int STATUS_SIZE = 0x50;
inline constexpr int LOGIN_RESPONSE_SIZE = 0x60;
inline constexpr int LOGIN_SIZE = 0x80;
inline constexpr int CONNINFO_SIZE = 0x90;
inline constexpr int AUDIO_HEAD_SIZE = 0x18;
inline constexpr int TX_BUFFER_SIZE = 0xf0;
inline constexpr int AUDIO_SAMPLE_RATE = 12000;

inline constexpr uint16_t CMD_NULL = 0x00;
inline constexpr uint16_t CMD_RETRANSMIT = 0x01;
inline constexpr uint16_t CMD_ARE_YOU_THERE = 0x03;
inline constexpr uint16_t CMD_I_AM_HERE = 0x04;
inline constexpr uint16_t CMD_DISCONNECT = 0x05;
inline constexpr uint16_t CMD_ARE_YOU_READY = 0x06;
inline constexpr uint16_t CMD_I_AM_READY = 0x06;
inline constexpr uint16_t CMD_PING = 0x07;

inline constexpr uint8_t TOKEN_TYPE_CONFIRM = 0x02;
inline constexpr uint8_t TOKEN_TYPE_RENEWAL = 0x05;
inline constexpr uint8_t LPCM_16BIT = 0x04;

inline void writeLe32(std::vector<uint8_t>& p, size_t off, uint32_t v) {
    p[off] = v & 0xff;
    p[off + 1] = (v >> 8) & 0xff;
    p[off + 2] = (v >> 16) & 0xff;
    p[off + 3] = (v >> 24) & 0xff;
}

inline void writeLe16(std::vector<uint8_t>& p, size_t off, uint16_t v) {
    p[off] = v & 0xff;
    p[off + 1] = (v >> 8) & 0xff;
}

inline void writeBe32(std::vector<uint8_t>& p, size_t off, uint32_t v) {
    p[off] = (v >> 24) & 0xff;
    p[off + 1] = (v >> 16) & 0xff;
    p[off + 2] = (v >> 8) & 0xff;
    p[off + 3] = v & 0xff;
}

inline void writeBe16(std::vector<uint8_t>& p, size_t off, uint16_t v) {
    p[off] = (v >> 8) & 0xff;
    p[off + 1] = v & 0xff;
}

inline uint32_t readLe32(const uint8_t* d, size_t off = 0) {
    return (uint32_t)d[off] | ((uint32_t)d[off + 1] << 8) | ((uint32_t)d[off + 2] << 16) |
           ((uint32_t)d[off + 3] << 24);
}

inline uint32_t readBe32(const uint8_t* d, size_t off = 0) {
    return ((uint32_t)d[off] << 24) | ((uint32_t)d[off + 1] << 16) | ((uint32_t)d[off + 2] << 8) |
           (uint32_t)d[off + 3];
}

inline uint16_t readLe16(const uint8_t* d, size_t off = 0) {
    return (uint16_t)d[off] | ((uint16_t)d[off + 1] << 8);
}

inline uint16_t readBe16(const uint8_t* d, size_t off = 0) {
    return ((uint16_t)d[off] << 8) | (uint16_t)d[off + 1];
}

inline uint16_t packetType(const uint8_t* d, size_t len) {
    if (len < 6) return 0xffff;
    return readLe16(d, 4);
}

inline uint32_t rcvdId(const uint8_t* d) { return readLe32(d, 12); }

inline std::vector<uint8_t> controlPacket(uint16_t type, uint16_t seq, uint32_t sentId, uint32_t recvId) {
    std::vector<uint8_t> p(CONTROL_SIZE, 0);
    writeLe32(p, 0, CONTROL_SIZE);
    writeLe16(p, 4, type);
    writeLe16(p, 6, seq);
    writeLe32(p, 8, sentId);
    writeLe32(p, 12, recvId);
    return p;
}

inline std::vector<uint8_t> pingSend(uint32_t localId, uint32_t remoteId, uint16_t pingSeq) {
    std::vector<uint8_t> p(PING_SIZE, 0);
    writeLe32(p, 0, PING_SIZE);
    writeLe16(p, 4, CMD_PING);
    writeLe16(p, 6, pingSeq);
    writeLe32(p, 8, localId);
    writeLe32(p, 12, remoteId);
    p[0x10] = 0x00;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
    writeLe32(p, 0x11, static_cast<uint32_t>(ms));
    return p;
}

inline std::vector<uint8_t> pingReply(const uint8_t* in, uint32_t localId, uint32_t remoteId) {
    std::vector<uint8_t> p(PING_SIZE, 0);
    writeLe32(p, 0, PING_SIZE);
    writeLe16(p, 4, CMD_PING);
    p[6] = in[6];
    p[7] = in[7];
    writeLe32(p, 8, localId);
    writeLe32(p, 12, remoteId);
    p[0x10] = 0x01;
    p[0x11] = in[0x11];
    p[0x12] = in[0x12];
    p[0x13] = in[0x13];
    p[0x14] = in[0x14];
    return p;
}

inline std::array<uint8_t, 16> passCode(const std::string& pass) {
    // ICOM Wi-Fi password scrambling lookup table (unused indices zero)
    static const uint8_t sequence[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0x47, 0x5d, 0x4c, 0x42, 0x66, 0x20, 0x23, 0x46, 0x4e, 0x57, 0x45, 0x3d, 0x67, 0x76, 0x60, 0x41, 0x62, 0x39,
        0x59, 0x2d, 0x68, 0x7e, 0x7c, 0x65, 0x7d, 0x49, 0x29, 0x72, 0x73, 0x78, 0x21, 0x6e, 0x5a, 0x5e, 0x4a, 0x3e,
        0x71, 0x2c, 0x2a, 0x54, 0x3c, 0x3a, 0x63, 0x4f, 0x43, 0x75, 0x27, 0x79, 0x5b, 0x35, 0x70, 0x48, 0x6b, 0x56,
        0x6f, 0x34, 0x32, 0x6c, 0x30, 0x61, 0x6d, 0x7b, 0x2f, 0x4b, 0x64, 0x38, 0x2b, 0x2e, 0x50, 0x40, 0x3f, 0x55,
        0x33, 0x37, 0x25, 0x77, 0x24, 0x26, 0x74, 0x6a, 0x28, 0x53, 0x4d, 0x69, 0x22, 0x5c, 0x44, 0x31, 0x36, 0x58,
        0x3b, 0x7a, 0x51, 0x5f, 0x52};
    std::array<uint8_t, 16> out{};
    for (size_t i = 0; i < pass.size() && i < 16; ++i) {
        int p = (static_cast<uint8_t>(pass[i]) + static_cast<int>(i)) & 0xff;
        if (p > 126) p = 32 + p % 127;
        out[i] = sequence[p];
    }
    return out;
}

inline void putString(std::vector<uint8_t>& p, size_t off, const std::string& s, size_t len) {
    std::memset(p.data() + off, 0, len);
    std::memcpy(p.data() + off, s.data(), std::min(s.size(), len));
}

inline std::vector<uint8_t> loginPacket(uint32_t localId, uint32_t remoteId, uint16_t innerSeq,
                                      uint16_t localToken, int rigToken, const std::string& user,
                                      const std::string& pass, const std::string& appName) {
    std::vector<uint8_t> p(LOGIN_SIZE, 0);
    writeLe32(p, 0, LOGIN_SIZE);
    writeLe16(p, 6, 0);
    writeLe32(p, 8, localId);
    writeLe32(p, 12, remoteId);
    writeBe16(p, 18, LOGIN_SIZE - 0x10);
    p[20] = 0x01;
    p[21] = 0x00;
    writeBe16(p, 22, innerSeq);
    writeBe16(p, 26, localToken);
    writeBe32(p, 28, static_cast<uint32_t>(rigToken));
    auto u = passCode(user);
    auto pw = passCode(pass);
    std::memcpy(p.data() + 64, u.data(), 16);
    std::memcpy(p.data() + 80, pw.data(), 16);
    putString(p, 96, appName, 16);
    return p;
}

inline std::vector<uint8_t> tokenPacket(uint32_t localId, uint32_t remoteId, uint8_t requestType,
                                        uint16_t innerSeq, uint16_t localToken, uint32_t rigToken) {
    std::vector<uint8_t> p(TOKEN_SIZE, 0);
    writeLe32(p, 0, TOKEN_SIZE);
    writeLe32(p, 8, localId);
    writeLe32(p, 12, remoteId);
    writeBe16(p, 18, TOKEN_SIZE - 0x10);
    p[20] = 0x01;
    p[21] = requestType;
    writeBe16(p, 22, innerSeq);
    writeBe16(p, 26, localToken);
    writeBe32(p, 28, rigToken);
    return p;
}

inline std::vector<uint8_t> connectRequest(uint32_t localId, uint32_t remoteId, uint16_t innerSeq,
                                           uint16_t localToken, uint32_t rigToken,
                                           const uint8_t mac[6], const std::string& rigName,
                                           const std::string& user, int civLocalPort,
                                           int audioLocalPort) {
    std::vector<uint8_t> p(CONNINFO_SIZE, 0);
    writeLe32(p, 0, CONNINFO_SIZE);
    writeLe32(p, 8, localId);
    writeLe32(p, 12, remoteId);
    writeBe16(p, 18, CONNINFO_SIZE - 0x10);
    p[20] = 0x01;
    p[21] = 0x03;
    writeBe16(p, 22, innerSeq);
    writeBe16(p, 26, localToken);
    writeBe32(p, 28, rigToken);
    p[0x26] = 0x10;
    p[0x27] = 0x80;
    std::memcpy(p.data() + 0x28, mac, 6);
    putString(p, 64, rigName, 32);
    auto u = passCode(user);
    std::memcpy(p.data() + 96, u.data(), 16);
    p[0x70] = 0x01;
    p[0x71] = 0x01;
    p[0x72] = LPCM_16BIT;
    p[0x73] = LPCM_16BIT;
    writeBe32(p, 0x74, AUDIO_SAMPLE_RATE);
    writeBe32(p, 0x78, AUDIO_SAMPLE_RATE);
    writeBe32(p, 0x7c, static_cast<uint32_t>(civLocalPort));
    writeBe32(p, 0x80, static_cast<uint32_t>(audioLocalPort));
    writeBe32(p, 0x84, TX_BUFFER_SIZE);
    p[0x88] = 0x01;
    return p;
}

inline std::vector<uint8_t> connInfoReply(const uint8_t* rigData, uint32_t localId, uint32_t remoteId,
                                          uint16_t innerSeq, uint16_t localToken, uint32_t rigToken,
                                          const std::string& rigName, const std::string& user, int civLocalPort,
                                          int audioLocalPort) {
    std::vector<uint8_t> p(CONNINFO_SIZE, 0);
    writeLe32(p, 0, CONNINFO_SIZE);
    writeLe32(p, 8, localId);
    writeLe32(p, 12, remoteId);
    writeBe16(p, 18, CONNINFO_SIZE - 0x10);
    p[20] = 0x01;
    p[21] = 0x03;
    writeBe16(p, 22, innerSeq);
    writeBe16(p, 26, localToken);
    writeBe32(p, 28, rigToken);
    std::memcpy(p.data() + 32, rigData + 32, 32);
    putString(p, 64, rigName, 32);
    auto u = passCode(user);
    std::memcpy(p.data() + 96, u.data(), 16);
    p[0x70] = 0x01;
    p[0x71] = 0x01;
    p[0x72] = LPCM_16BIT;
    p[0x73] = LPCM_16BIT;
    writeBe32(p, 0x74, AUDIO_SAMPLE_RATE);
    writeBe32(p, 0x78, AUDIO_SAMPLE_RATE);
    writeBe32(p, 0x7c, static_cast<uint32_t>(civLocalPort));
    writeBe32(p, 0x80, static_cast<uint32_t>(audioLocalPort));
    writeBe32(p, 0x84, TX_BUFFER_SIZE);
    p[0x88] = 0x01;
    return p;
}

inline std::vector<uint8_t> civReadFreq(uint8_t rigAddr, uint8_t ctrlAddr = 0xE0) {
    return {0xFE, 0xFE, rigAddr, ctrlAddr, 0x03, 0xFD};
}

/** CI-V 0x05 — set operating frequency (Hz). */
inline std::vector<uint8_t> civSetFrequency(uint8_t rigAddr, long freqHz, uint8_t ctrlAddr = 0xE0) {
    std::vector<uint8_t> data(11);
    data[0] = 0xFE;
    data[1] = 0xFE;
    data[2] = rigAddr;
    data[3] = ctrlAddr;
    data[4] = 0x05;
    data[5] = static_cast<uint8_t>(((freqHz % 100 / 10) << 4) + (freqHz % 10));
    data[6] = static_cast<uint8_t>(((freqHz % 10000 / 1000) << 4) + (freqHz % 1000 / 100));
    data[7] = static_cast<uint8_t>(((freqHz % 1000000 / 100000) << 4) + (freqHz % 100000 / 10000));
    data[8] = static_cast<uint8_t>(((freqHz % 100000000 / 10000000) << 4) + (freqHz % 10000000 / 1000000));
    data[9] = static_cast<uint8_t>(((freqHz / 1000000000) << 4) + (freqHz % 1000000000 / 100000000));
    data[10] = 0xFD;
    return data;
}

inline std::string formatFrequency(long hz) {
    if (hz <= 0) return "n/a";
    char buf[64];
    if (hz >= 1000000L)
        std::snprintf(buf, sizeof(buf), "%.3f MHz (%ld Hz)", hz / 1e6, hz);
    else
        std::snprintf(buf, sizeof(buf), "%.3f kHz (%ld Hz)", hz / 1e3, hz);
    return buf;
}

inline std::vector<uint8_t> wrapCiv(uint32_t localId, uint32_t remoteId, uint16_t civSeq,
                                    const std::vector<uint8_t>& civ) {
    std::vector<uint8_t> p(0x15 + civ.size(), 0);
    writeLe32(p, 0, static_cast<uint32_t>(p.size()));
    writeLe32(p, 8, localId);
    writeLe32(p, 12, remoteId);
    p[0x10] = 0xc1;
    writeLe16(p, 0x11, static_cast<uint16_t>(civ.size()));
    writeBe16(p, 0x13, civSeq);
    std::memcpy(p.data() + 0x15, civ.data(), civ.size());
    return p;
}

inline std::vector<uint8_t> openCloseCiv(uint32_t localId, uint32_t remoteId, uint16_t civSeq, bool open) {
    std::vector<uint8_t> p(OPENCLOSE_SIZE, 0);
    writeLe32(p, 0, OPENCLOSE_SIZE);
    writeLe32(p, 8, localId);
    writeLe32(p, 12, remoteId);
    p[0x10] = 0xc0;
    writeLe16(p, 0x11, 1);
    writeBe16(p, 0x13, civSeq);
    p[0x15] = open ? 0x04 : 0x00;
    return p;
}

inline bool isCivPayload(const uint8_t* d, size_t len) {
    if (len <= 0x15) return false;
    uint16_t civLen = readLe16(d, 0x11);
    return len - 0x15 == civLen && d[0x10] == 0xc1 && packetType(d, len) != CMD_RETRANSMIT;
}

inline bool isAudioPayload(const uint8_t* d, size_t len) {
    if (len < AUDIO_HEAD_SIZE) return false;
    return len - AUDIO_HEAD_SIZE == readBe16(d, 0x16);
}

inline long freqFromBcd(const uint8_t* bcd, size_t n = 5) {
    long f = 0;
    long mul = 1;
    for (size_t i = 0; i < n && i < 5; ++i) {
        f += (bcd[i] & 0x0f) * mul;
        f += ((bcd[i] >> 4) & 0x0f) * mul * 10;
        mul *= 100;
    }
    return f;
}

inline long parseCivFrequency(const uint8_t* civ, size_t len) {
    for (size_t i = 0; i + 6 < len; ++i) {
        if (civ[i] == 0xFE && civ[i + 1] == 0xFE && i + 5 < len) {
            uint8_t cmd = civ[i + 4];
            if (cmd == 0x00 || cmd == 0x03) {
                size_t fd = i + 5;
                while (fd < len && civ[fd] != 0xFD) ++fd;
                if (fd > i + 5 && fd - (i + 5) >= 5)
                    return freqFromBcd(civ + i + 5, fd - (i + 5));
            }
        }
    }
    return -1;
}

}  // namespace icom
