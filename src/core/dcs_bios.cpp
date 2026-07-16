#include "dcs_bios.h"
#include <WiFi.h>
#include <AsyncUDP.h>

// ── Ring buffer ──────────────────────────────────────────────────────────────
// Single-producer (Core 0 AsyncUDP callback) / single-consumer (Core 1 loop).
// portMUX spinlock protects the push path only; pop advances s_tail on Core 1.
static constexpr uint8_t  RING_SLOTS    = 12;
static constexpr uint16_t RING_PKT_MAX  = 512;

struct RingSlot { uint8_t data[RING_PKT_MAX]; uint16_t len; IPAddress remoteIP; };
static RingSlot          s_ring[RING_SLOTS];
static volatile uint8_t  s_head = 0;   // written by Core 0
static volatile uint8_t  s_tail = 0;   // written by Core 1
static portMUX_TYPE      s_mux  = portMUX_INITIALIZER_UNLOCKED;

static AsyncUDP   s_udp;
static uint16_t   s_cmdPort;
static IPAddress  s_senderIp;
static unsigned long s_lastRx = 0;

// Decoded state — 0xFF = not yet received
static bool    s_mcLight = false;
static bool    s_rwrMslLaunch = false;
static bool    s_storesConfigLight = false;
static uint8_t s_storesConfigSw    = 0xFF;
static uint16_t s_fuel10K  = 0;
static uint16_t s_fuel1K   = 0;
static uint16_t s_fuel100  = 0;
static char s_chBuf[5] = "    ";
static bool     s_mwsOn        = false;
static bool     s_hdptL        = false;
static bool     s_hdptR        = false;
static uint8_t  s_cmdsModeKnob = 0xFF;   // 0xFF = not yet received
static bool     s_rwrPwrLight  = false;
static bool     s_ecm1S        = false;
static bool     s_ecmBtnsArmed = false;
static uint8_t  s_ecmPwSw      = 0xFF;   // 0xFF = not yet received
static uint8_t  s_ecmXmitSw    = 0xFF;   // 0xFF = not yet received
static bool     s_jmrSw        = false;

// Binary frame parser state machine
enum ParseState { SYNC0, SYNC1, SYNC2, SYNC3,
                  ADDR_LO, ADDR_HI, LEN_LO, LEN_HI, DATA };
static ParseState s_parse   = SYNC0;
static uint16_t   s_addr    = 0;
static uint16_t   s_len     = 0;
static uint16_t   s_dataIdx = 0;
static uint8_t    s_buf[512];

static void processWord(uint16_t addr, uint16_t word) {
  if (addr == DCSBIOS_ADDR_STORES_CONFIG_SW) {
    s_storesConfigSw = (word & DCSBIOS_MASK_STORES_CONFIG_SW) >> DCSBIOS_SHFT_STORES_CONFIG_SW;
  }
  if (addr == DCSBIOS_ADDR_MC_LIGHT) {
    s_mcLight = (word & DCSBIOS_MASK_MC_LIGHT) != 0;
  }
  if (addr == DCSBIOS_ADDR_RWR_MSL_LAUNCH) {
    s_rwrMslLaunch = (word & DCSBIOS_MASK_RWR_MSL_LAUNCH) != 0;
    s_ecm1S        = (word & DCSBIOS_MASK_ECM_1_S) != 0;
  }
  if (addr == DCSBIOS_ADDR_ECM_BTNS) {
    s_ecmBtnsArmed = (word & DCSBIOS_MASK_ECM_2_S) != 0
                  && (word & DCSBIOS_MASK_ECM_3_S) != 0
                  && (word & DCSBIOS_MASK_ECM_4_S) != 0
                  && (word & DCSBIOS_MASK_ECM_5_S) != 0;
  }
  if (addr == DCSBIOS_ADDR_ECM_PW_SW) {
    s_ecmPwSw   = (word & DCSBIOS_MASK_ECM_PW_SW)   >> DCSBIOS_SHFT_ECM_PW_SW;
    s_ecmXmitSw = (word & DCSBIOS_MASK_ECM_XMIT_SW) >> DCSBIOS_SHFT_ECM_XMIT_SW;
  }
  if (addr == DCSBIOS_ADDR_STORES_CONFIG_LIGHT) {
    s_storesConfigLight = (word & DCSBIOS_MASK_STORES_CONFIG_LIGHT) != 0;
  }

  if (addr == DCSBIOS_ADDR_FUEL_10K)  { s_fuel10K = word; }
  if (addr == DCSBIOS_ADDR_FUEL_1K)   { s_fuel1K  = word; }
  if (addr == DCSBIOS_ADDR_FUEL_100)  { s_fuel100 = word; }

  if (addr == DCSBIOS_ADDR_CH_AMT_0) {
    s_chBuf[0] = (char)(word & 0xFF);
    s_chBuf[1] = (char)(word >> 8);
  }
  if (addr == DCSBIOS_ADDR_CH_AMT_1) {
    s_chBuf[2] = (char)(word & 0xFF);
    s_chBuf[3] = (char)(word >> 8);
    s_chBuf[4] = '\0';
  }
  if (addr == DCSBIOS_ADDR_MWS_SW) {
    s_mwsOn = (word & DCSBIOS_MASK_MWS_SW) != 0;
    s_jmrSw = (word & DCSBIOS_MASK_JMR_SW) != 0;
  }
  if (addr == DCSBIOS_ADDR_HDPT) {
    s_hdptL = (word & DCSBIOS_MASK_HDPT_L) != 0;
    s_hdptR = (word & DCSBIOS_MASK_HDPT_R) != 0;
  }
  if (addr == DCSBIOS_ADDR_CMDS_MODE) {
    s_cmdsModeKnob = (word & DCSBIOS_MASK_CMDS_MODE) >> DCSBIOS_SHFT_CMDS_MODE;
  }
  if (addr == DCSBIOS_ADDR_RWR_PWR_LIGHT) {
    s_rwrPwrLight = (word & DCSBIOS_MASK_RWR_PWR_LIGHT) != 0;
  }
}

static void processBuf() {
  // Walk received data buffer in 2-byte words aligned to s_addr
  for (uint16_t i = 0; i + 1 < s_len; i += 2) {
    processWord(s_addr + i,
                (uint16_t)s_buf[i] | ((uint16_t)s_buf[i + 1] << 8));
  }
}

static void processByte(uint8_t b) {
  switch (s_parse) {
    case SYNC0: s_parse = (b == 0x55) ? SYNC1 : SYNC0; break;
    case SYNC1: s_parse = (b == 0x55) ? SYNC2 : SYNC0; break;
    case SYNC2: s_parse = (b == 0x55) ? SYNC3 : SYNC0; break;
    case SYNC3: s_parse = (b == 0x55) ? ADDR_LO : SYNC0; break;
    case ADDR_LO: s_addr  = b;          s_parse = ADDR_HI; break;
    case ADDR_HI: s_addr |= ((uint16_t)b << 8); s_parse = LEN_LO;  break;
    case LEN_LO:  s_len   = b;                  s_parse = LEN_HI;  break;
    case LEN_HI:
      s_len |= ((uint16_t)b << 8);
      if (s_len > sizeof(s_buf)) s_len = sizeof(s_buf);
      s_dataIdx = 0;
      if (s_addr == 0xFFFE || s_len == 0) {
        s_parse = ADDR_LO;
      } else {
        s_parse = DATA;
      }
      break;
    case DATA:
      if (s_dataIdx < sizeof(s_buf)) s_buf[s_dataIdx] = b;
      s_dataIdx++;
      if (s_dataIdx >= s_len) {
        processBuf();
        s_parse = ADDR_LO;
      }
      break;
  }
}

void DcsBios::begin(const char* mcastAddr, uint16_t listenPort,
                    const char* /*cmdHost*/, uint16_t cmdPort) {
  s_udp.close();  // close prior socket before re-bind on reconnect
  s_cmdPort  = cmdPort;
  s_senderIp = IPAddress(0, 0, 0, 0);

  // Reset ring buffer on re-init (e.g. after WiFi reconnect).
  portENTER_CRITICAL(&s_mux);
  s_head = s_tail = 0;
  portEXIT_CRITICAL(&s_mux);

  IPAddress mcast;
  mcast.fromString(mcastAddr);

  s_udp.listenMulticast(mcast, listenPort);

  s_udp.onPacket([](AsyncUDPPacket pkt) {
    // Core 0 — push raw bytes into ring buffer; do NOT parse here.
    portENTER_CRITICAL_ISR(&s_mux);
    uint8_t next = (s_head + 1) % RING_SLOTS;
    if (next != s_tail) {                          // drop if full
      uint16_t len = pkt.length() < RING_PKT_MAX ? (uint16_t)pkt.length() : RING_PKT_MAX;
      memcpy(s_ring[s_head].data, pkt.data(), len);
      s_ring[s_head].len      = len;
      s_ring[s_head].remoteIP = pkt.remoteIP();
      s_head = next;
    }
    portEXIT_CRITICAL_ISR(&s_mux);
  });
}

// Core 1 only — drains ring buffer, parses bytes, updates shared state.
static bool drainRing() {
  if (s_head == s_tail) return false;      // empty — fast path
  while (s_tail != s_head) {
    RingSlot& slot = s_ring[s_tail];
    s_lastRx   = millis();
    s_senderIp = slot.remoteIP;
    s_parse    = SYNC0;                    // each UDP packet is a fresh frame
    for (uint16_t i = 0; i < slot.len; i++) processByte(slot.data[i]);
    s_tail = (s_tail + 1) % RING_SLOTS;   // Core 1 owns tail; no lock needed
  }
  return true;
}

bool DcsBios::process() { return drainRing(); }

bool DcsBios::isConnected() {
  return s_lastRx > 0 && (millis() - s_lastRx < 3000);
}

void DcsBios::sendCommand(const char* id, uint16_t value) {
  if (s_senderIp == IPAddress(0, 0, 0, 0)) return;  // sender not yet known
  char buf[80];
  int  len = snprintf(buf, sizeof(buf), "%s %u\n", id, value);
  if (len > 0 && len < (int)sizeof(buf))
    s_udp.writeTo(reinterpret_cast<const uint8_t*>(buf), (size_t)len,
                  s_senderIp, s_cmdPort);
}

bool    DcsBios::masterCaution() { return s_mcLight; }
bool    DcsBios::rwrMslLaunch()  { return s_rwrMslLaunch; }
bool    DcsBios::storesConfigLight() { return s_storesConfigLight; }
uint8_t DcsBios::storesConfigSw()    { return s_storesConfigSw; }

static uint8_t dialDigit(uint16_t raw) {
  return (uint8_t)min(((uint32_t)raw * 10u + 32767u) / 65535u, (uint32_t)9u);
}

uint32_t DcsBios::fuelLbs() {
  uint32_t sub1K = (uint32_t)s_fuel100 * 1000u / 65535u;  // 0-999 continuous
  return dialDigit(s_fuel10K) * 10000u
       + dialDigit(s_fuel1K)  * 1000u
       + sub1K;
}

const char* DcsBios::chaffStr()     { return s_chBuf; }

bool    DcsBios::mwsOn()        { return s_mwsOn; }
bool    DcsBios::hdptLeft()     { return s_hdptL; }
bool    DcsBios::hdptRight()    { return s_hdptR; }
uint8_t DcsBios::cmdsModeKnob() { return s_cmdsModeKnob; }
bool    DcsBios::rwrPowerLight() { return s_rwrPwrLight; }
bool    DcsBios::ecmStandby()       { return s_ecm1S; }
bool    DcsBios::ecmBtns2to5Armed() { return s_ecmBtnsArmed; }
bool    DcsBios::ecmPowerOpr()      { return s_ecmPwSw == 2; }
bool    DcsBios::jmrSourceOn()      { return s_jmrSw; }
bool    DcsBios::ecmXmitAft()       { return s_ecmXmitSw == 2; }

