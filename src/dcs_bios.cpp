#include "dcs_bios.h"
#include <WiFi.h>

static WiFiUDP   s_udp;
static char      s_cmdHost[20];
static uint16_t  s_cmdPort;
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
static char s_flBuf[5] = "    ";
static bool     s_ecmTx    = false;

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
  if (addr == DCSBIOS_ADDR_FL_AMT_0) {
    s_flBuf[0] = (char)(word & 0xFF);
    s_flBuf[1] = (char)(word >> 8);
  }
  if (addr == DCSBIOS_ADDR_FL_AMT_1) {
    s_flBuf[2] = (char)(word & 0xFF);
    s_flBuf[3] = (char)(word >> 8);
    s_flBuf[4] = '\0';
  }

  if (addr == DCSBIOS_ADDR_ECM_TX) { s_ecmTx = (word & DCSBIOS_MASK_ECM_TX) != 0; }
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
                    const char* cmdHost,   uint16_t cmdPort) {
  strlcpy(s_cmdHost, cmdHost, sizeof(s_cmdHost));
  s_cmdPort = cmdPort;
  IPAddress mcast;
  mcast.fromString(mcastAddr);
  s_udp.beginMulticast(mcast, listenPort);
}

bool DcsBios::update() {
  int pktSize = s_udp.parsePacket();
  if (pktSize <= 0) return false;
  s_lastRx = millis();
  s_parse = SYNC0;  // each UDP packet starts fresh with the 0x55×4 sync header
  while (s_udp.available()) processByte((uint8_t)s_udp.read());
  return true;
}

bool DcsBios::isConnected() {
  return s_lastRx > 0 && (millis() - s_lastRx < 3000);
}

bool DcsBios::hasData() {
  return isConnected();
}

void DcsBios::sendCommand(const char* id, uint16_t value) {
  s_udp.beginPacket(s_cmdHost, s_cmdPort);
  s_udp.printf("%s %u\n", id, value);
  s_udp.endPacket();
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
const char* DcsBios::flareStr()     { return s_flBuf; }
bool        DcsBios::ecmTransmitting() { return s_ecmTx; }

