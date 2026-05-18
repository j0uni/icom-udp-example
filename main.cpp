/**
 * Minimal ICOM Wi-Fi UDP example (vanilla C++).
 *
 * Connects to an ICOM radio (e.g. IC-705), completes control login,
 * opens CI-V and audio UDP streams, prints operating frequency and audio stats.
 *
 * Usage:
 *   ./icom_udp_example <radio_ip> [control_port] [user] [password]
 *
 * Defaults: port 50001, user "icom", password "icom"
 */

#include "icom_protocol.hpp"
#include "udp_socket.hpp"

#include <poll.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

using namespace icom;

constexpr long kTargetFreqHz = 3699000;  // 3699 kHz

struct Stream {
    UdpSocket sock;
    std::string name;
    uint32_t localId;

    Stream(const char* n, uint32_t id) : name(n), localId(id) {}
    uint32_t remoteId{0};
    uint16_t trackedSeq{1};
    uint16_t pingSeq{0};
    uint16_t innerSeq{0x30};
    uint16_t civSeq{0};
    bool ready{false};
    bool pingActive{false};
    std::map<uint16_t, std::vector<uint8_t>> txHistory;
    std::chrono::steady_clock::time_point lastAreYouThere;
    std::chrono::steady_clock::time_point lastPing;
    std::chrono::steady_clock::time_point lastFreqQuery;
};

static uint32_t nowMs() {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

static uint32_t makeId(uint32_t salt = 0) {
    auto ms = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    return ms ^ (salt * 0x9e3779b9u);
}

static void assignSeq(std::vector<uint8_t>& pkt, uint16_t seq) {
    if (pkt.size() >= 8) writeLe16(pkt, 6, seq);
}

static void sendTracked(Stream& s, const std::string& rigIp, int rigPort, std::vector<uint8_t> pkt) {
    assignSeq(pkt, s.trackedSeq);
    s.txHistory[s.trackedSeq] = pkt;
    s.sock.sendTo(pkt, rigIp, rigPort);
    s.trackedSeq++;
}

static void sendUntracked(Stream& s, const std::string& rigIp, int rigPort, const std::vector<uint8_t>& pkt) {
    s.sock.sendTo(pkt, rigIp, rigPort);
}

static void handlePing(Stream& s, const std::string& rigIp, int rigPort, const uint8_t* d, size_t len) {
    if (len != PING_SIZE || packetType(d, len) != CMD_PING) return;
    if (d[0x10] == 0x00)
        sendUntracked(s, rigIp, rigPort, pingReply(d, s.localId, s.remoteId));
    else if (readLe16(d, 6) == s.pingSeq)
        s.pingSeq++;
}

static void handleRetransmit(Stream& s, const std::string& rigIp, int rigPort, const uint8_t* d, size_t len) {
    if (packetType(d, len) != CMD_RETRANSMIT) return;
    if (len == CONTROL_SIZE) {
        uint16_t req = readLe16(d, 6);
        auto it = s.txHistory.find(req);
        if (it != s.txHistory.end())
            sendUntracked(s, rigIp, rigPort, it->second);
        else
            sendUntracked(s, rigIp, rigPort, controlPacket(CMD_NULL, 0, s.localId, s.remoteId));
    } else if (len > CONTROL_SIZE) {
        for (size_t i = 0x10; i + 1 < len; i += 2) {
            uint16_t req = readLe16(d, i);
            auto it = s.txHistory.find(req);
            if (it != s.txHistory.end())
                sendUntracked(s, rigIp, rigPort, it->second);
        }
    }
}

static bool handleCommon(Stream& s, const std::string& rigIp, int rigPort, const uint8_t* d, size_t len) {
    if (len < CONTROL_SIZE || rcvdId(d) != s.localId) return false;

    if (len == CONTROL_SIZE) {
        uint16_t t = packetType(d, len);
        if (t == CMD_I_AM_HERE) {
            s.remoteId = readLe32(d, 8);
            s.pingActive = true;
            sendUntracked(s, rigIp, rigPort, controlPacket(CMD_ARE_YOU_READY, 1, s.localId, s.remoteId));
            std::cout << "[" << s.name << "] I_AM_HERE remoteId=0x" << std::hex << s.remoteId << std::dec
                      << " (localId=0x" << std::hex << s.localId << ")" << std::dec << std::endl;
            return true;
        }
        if (t == CMD_I_AM_READY) {
            s.ready = true;
            std::cout << "[" << s.name << "] I_AM_READY" << std::endl;
            return true;
        }
        if (t == CMD_RETRANSMIT) {
            handleRetransmit(s, rigIp, rigPort, d, len);
            return true;
        }
    }
    if (len == PING_SIZE) {
        handlePing(s, rigIp, rigPort, d, len);
        return true;
    }
    if (len != CONTROL_SIZE && packetType(d, len) == CMD_RETRANSMIT) {
        handleRetransmit(s, rigIp, rigPort, d, len);
        return true;
    }
    return false;
}

enum class CtrlState { Discover, WaitLoginResp, WaitToken, WaitConnInfo, Done };

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <radio_ip> [control_port] [user] [password]\n";
        return 1;
    }

    std::string rigIp = argv[1];
    int controlPort = (argc > 2) ? std::atoi(argv[2]) : 50001;
    std::string user = (argc > 3) ? argv[3] : "icom";
    std::string pass = (argc > 4) ? argv[4] : "icom";

    Stream ctrl{"control", makeId(1)};
    Stream civ{"civ", makeId(2)};
    Stream audio{"audio", makeId(3)};

    if (!ctrl.sock.open() || !civ.sock.open() || !audio.sock.open()) {
        std::perror("socket");
        return 1;
    }

    uint16_t localToken = static_cast<uint16_t>(makeId() & 0xffff);
    uint32_t rigToken = 0;
    uint8_t rigMac[6]{};
    std::string rigName;
    int rigCivPort = 0;
    int rigAudioPort = 0;
    uint8_t civAddr = 0xA4;
    CtrlState cstate = CtrlState::Discover;
    bool authenticated = false;
    bool connInfoReplied = false;
    bool civOpened = false;

    uint64_t audioPackets = 0;
    uint64_t audioBytes = 0;
    long lastFreqHz = -1;

    auto t0 = std::chrono::steady_clock::now();
    ctrl.lastAreYouThere = ctrl.lastPing = civ.lastAreYouThere = audio.lastAreYouThere = t0;
    civ.lastFreqQuery = t0;

    std::cout << "ICOM UDP example\n"
              << "  Radio: " << rigIp << ":" << controlPort << "\n"
              << "  Local ports: control=" << ctrl.sock.localPort() << " civ=" << civ.sock.localPort()
              << " audio=" << audio.sock.localPort() << "\n";

    pollfd fds[3];
    fds[0] = {ctrl.sock.fd(), POLLIN, 0};
    fds[1] = {civ.sock.fd(), POLLIN, 0};
    fds[2] = {audio.sock.fd(), POLLIN, 0};

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);

    while (std::chrono::steady_clock::now() < deadline) {
        auto now = std::chrono::steady_clock::now();

        if (cstate == CtrlState::Discover &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - ctrl.lastAreYouThere).count() >= 500) {
            sendUntracked(ctrl, rigIp, controlPort, controlPacket(CMD_ARE_YOU_THERE, 0, ctrl.localId, 0));
            ctrl.lastAreYouThere = now;
        }
        if (ctrl.pingActive &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - ctrl.lastPing).count() >= 500) {
            sendUntracked(ctrl, rigIp, controlPort, pingSend(ctrl.localId, ctrl.remoteId, ctrl.pingSeq));
            ctrl.lastPing = now;
        }
        if (cstate == CtrlState::Done && !civ.ready &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - civ.lastAreYouThere).count() >= 500) {
            sendUntracked(civ, rigIp, rigCivPort, controlPacket(CMD_ARE_YOU_THERE, 0, civ.localId, 0));
            civ.lastAreYouThere = now;
        }
        if (civ.ready && civ.pingActive &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - civ.lastPing).count() >= 500) {
            sendUntracked(civ, rigIp, rigCivPort, pingSend(civ.localId, civ.remoteId, civ.pingSeq));
            civ.lastPing = now;
        }
        if (cstate == CtrlState::Done && !audio.ready &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - audio.lastAreYouThere).count() >= 500) {
            sendUntracked(audio, rigIp, rigAudioPort, controlPacket(CMD_ARE_YOU_THERE, 0, audio.localId, 0));
            audio.lastAreYouThere = now;
        }
        if (audio.ready && audio.pingActive &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - audio.lastPing).count() >= 500) {
            sendUntracked(audio, rigIp, rigAudioPort, pingSend(audio.localId, audio.remoteId, audio.pingSeq));
            audio.lastPing = now;
        }
        if (civOpened &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - civ.lastFreqQuery).count() >= 1000) {
            auto civCmd = civReadFreq(civAddr);
            auto pkt = wrapCiv(civ.localId, civ.remoteId, civ.civSeq++, civCmd);
            sendTracked(civ, rigIp, rigCivPort, pkt);
            civ.lastFreqQuery = now;
        }

        int pr = poll(fds, 3, 100);
        if (pr < 0) {
            std::perror("poll");
            break;
        }
        if (pr == 0) continue;

        auto process = [&](Stream& s, int pollIdx, int rigPort, bool isControl) {
            if (!(fds[pollIdx].revents & POLLIN)) return;
            std::vector<uint8_t> buf;
            std::string fromIp;
            int fromPort = 0;
            if (s.sock.recvFrom(buf, fromIp, fromPort) <= 0) return;

            if (isControl && fromIp.size()) rigIp = fromIp;

            if (handleCommon(s, rigIp, rigPort, buf.data(), buf.size())) {
                if (isControl && buf.size() == CONTROL_SIZE && packetType(buf.data(), buf.size()) == CMD_I_AM_READY &&
                    cstate == CtrlState::Discover) {
                    sendTracked(ctrl, rigIp, controlPort,
                                loginPacket(ctrl.localId, ctrl.remoteId, ctrl.innerSeq++, localToken, 0, user, pass,
                                            "icom_udp_example"));
                    cstate = CtrlState::WaitLoginResp;
                    std::cout << "[control] Sent login (0x80)" << std::endl;
                }
                if (!isControl && &s == &civ && s.ready && !civOpened) {
                    sendTracked(civ, rigIp, rigCivPort, openCloseCiv(civ.localId, civ.remoteId, civ.civSeq++, true));
                    civOpened = true;
                    civ.pingActive = true;
                    std::cout << "[civ] Open CI-V stream" << std::endl;
                    sendTracked(civ, rigIp, rigCivPort,
                                wrapCiv(civ.localId, civ.remoteId, civ.civSeq++,
                                        civSetFrequency(civAddr, kTargetFreqHz)));
                    lastFreqHz = kTargetFreqHz;
                    std::cout << "[civ] Set frequency to " << formatFrequency(kTargetFreqHz) << std::endl;
                }
                if (!isControl && &s == &audio && s.ready) {
                    audio.pingActive = true;
                }
                return;
            }

            if (isControl) {
                if (buf.size() == LOGIN_RESPONSE_SIZE && readLe32(buf.data(), 0) == LOGIN_RESPONSE_SIZE) {
                    if (readBe32(buf.data(), 0x30) == 0) {
                        rigToken = static_cast<int>(readBe32(buf.data(), 0x1c));
                        std::cout << "[control] Login OK, rigToken=0x" << std::hex << rigToken << std::dec << std::endl;
                        sendTracked(ctrl, rigIp, controlPort,
                                    tokenPacket(ctrl.localId, ctrl.remoteId, TOKEN_TYPE_CONFIRM, ctrl.innerSeq++,
                                                localToken, rigToken));
                        sendTracked(ctrl, rigIp, controlPort,
                                    tokenPacket(ctrl.localId, ctrl.remoteId, TOKEN_TYPE_RENEWAL, ctrl.innerSeq++,
                                                localToken, rigToken));
                        authenticated = true;
                        cstate = CtrlState::WaitToken;
                        std::cout << "[control] Sent token confirm + renewal" << std::endl;
                        // Proactively request connection (normally triggered by token response 0xffffffff)
                        sendTracked(ctrl, rigIp, controlPort,
                                    connectRequest(ctrl.localId, ctrl.remoteId, ctrl.innerSeq++, localToken,
                                                   static_cast<uint32_t>(rigToken), rigMac,
                                                   rigName.empty() ? "icom_udp_example" : rigName, user,
                                                   civ.sock.localPort(), audio.sock.localPort()));
                        std::cout << "[control] Sent connection request (0x90)" << std::endl;
                    } else {
                        std::cerr << "[control] Login failed error=0x" << std::hex << readBe32(buf.data(), 0x30)
                                  << std::dec << std::endl;
                    }
                    return;
                }
                if (buf.size() == TOKEN_SIZE && authenticated) {
                    uint8_t reqType = buf[21];
                    uint8_t reqReply = buf[20];
                    int response = static_cast<int>(readBe32(buf.data(), 0x30));
                    if (reqType == TOKEN_TYPE_RENEWAL && reqReply == 0x02 && response == static_cast<int>(0xffffffff)) {
                        std::memset(rigMac, 0, 6);
                        sendTracked(ctrl, rigIp, controlPort,
                                    connectRequest(ctrl.localId, ctrl.remoteId, ctrl.innerSeq++, localToken,
                                                   rigToken, rigMac, rigName.empty() ? "icom_udp_example" : rigName, user,
                                                   civ.sock.localPort(), audio.sock.localPort()));
                        cstate = CtrlState::WaitConnInfo;
                        std::cout << "[control] Sent connection request (0x90)" << std::endl;
                    }
                    return;
                }
                if (buf.size() == CONNINFO_SIZE && readLe32(buf.data(), 0) == CONNINFO_SIZE) {
                    bool busy = buf[0x60] != 0;
                    if (buf[0x27] == 0x80 && buf[0x26] == 0x10)
                        std::memcpy(rigMac, buf.data() + 0x2a, 6);
                    char nameBuf[33]{};
                    std::memcpy(nameBuf, buf.data() + 0x40, 32);
                    rigName = nameBuf;
                    std::cout << "[control] ConnInfo rig='" << rigName << "' busy=" << busy << std::endl;
                    if (!busy && !connInfoReplied) {
                        sendTracked(ctrl, rigIp, controlPort,
                                    connInfoReply(buf.data(), ctrl.localId, ctrl.remoteId, ctrl.innerSeq++,
                                                  localToken, static_cast<uint32_t>(rigToken), rigName, user,
                                                  civ.sock.localPort(), audio.sock.localPort()));
                        connInfoReplied = true;
                        std::cout << "[control] Sent conninfo reply (0x90)" << std::endl;
                    }
                    return;
                }
                if (buf.size() == STATUS_SIZE && readLe32(buf.data(), 0) == STATUS_SIZE) {
                    if (readBe32(buf.data(), 0x30) == 0 && buf[0x40] == 0) {
                        rigCivPort = readBe16(buf.data(), 0x42);
                        rigAudioPort = readBe16(buf.data(), 0x46);
                        std::cout << "[control] Status OK — rig CI-V port=" << rigCivPort
                                  << " audio port=" << rigAudioPort << std::endl;
                        cstate = CtrlState::Done;
                        civ.lastAreYouThere = audio.lastAreYouThere = std::chrono::steady_clock::now();
                        std::cout << "[civ/audio] Starting discovery on ports " << rigCivPort << "/"
                                  << rigAudioPort << std::endl;
                    }
                    return;
                }
                if (buf.size() >= 0xA8 && buf.size() > 0x94) {
                    civAddr = buf[0x42 + 0x52];  // RadioCap.civ in 0xA8 capabilities packet
                    std::cout << "[control] CI-V address from radio: 0x" << std::hex << (int)civAddr << std::dec
                              << std::endl;
                }
            }

            if (&s == &civ && isCivPayload(buf.data(), buf.size())) {
                const uint8_t* civ = buf.data() + 0x15;
                size_t civLen = buf.size() - 0x15;
                long hz = parseCivFrequency(civ, civLen);
                if (hz > 0 && hz != lastFreqHz) {
                    lastFreqHz = hz;
                    std::cout << "[civ] Frequency: " << formatFrequency(hz) << std::endl;
                }
            }

            if (&s == &audio && isAudioPayload(buf.data(), buf.size())) {
                size_t payload = buf.size() - AUDIO_HEAD_SIZE;
                audioPackets++;
                audioBytes += payload;
            }
        };

        process(ctrl, 0, controlPort, true);
        if (rigCivPort > 0) process(civ, 1, rigCivPort, false);
        if (rigAudioPort > 0) process(audio, 2, rigAudioPort, false);

        if (cstate == CtrlState::Done && civOpened && audio.ready) {
            static auto lastStats = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStats).count() >= 5) {
                std::cout << "[status] freq=" << formatFrequency(lastFreqHz)
                          << " | audio RX packets=" << audioPackets << " bytes=" << audioBytes
                          << " (~" << (audioBytes / 2) << " PCM16 samples @ 12kHz)" << std::endl;
                lastStats = now;
            }
        }
    }

    if (cstate == CtrlState::Done && civOpened)
        std::cout << "\nSuccess: control + CI-V + audio streams were active.\n";
    else
        std::cout << "\nTimed out or incomplete handshake (no radio reachable?).\n";

    return (cstate == CtrlState::Done && civOpened) ? 0 : 2;
}
