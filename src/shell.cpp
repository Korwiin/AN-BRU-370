#include "shell.h"

#ifdef DEV_BUILD
#include <WiFi.h>
#include "encoder.h"
#include "ui.h"
#include "wifi_mgr.h"

static Shell::Hooks s_hooks;
static char s_line[96];
static uint8_t s_lineLen = 0;

static void dispatch(char* line);

void Shell::begin(const Hooks& hooks) {
  s_hooks = hooks;
  Serial.println("#shell ready (type 'help')");
}

void Shell::poll() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      s_line[s_lineLen] = '\0';
      if (s_lineLen > 0) dispatch(s_line);
      s_lineLen = 0;
      continue;
    }
    if (s_lineLen < sizeof(s_line) - 1) s_line[s_lineLen++] = c;
    else s_lineLen = 0;  // overlong line: discard silently
  }
}

// Split "wifi conn full" into verb="wifi", rest="conn full" (rest may be "").
static void dispatch(char* line) {
  char* rest = strchr(line, ' ');
  if (rest) { *rest++ = '\0'; while (*rest == ' ') rest++; }
  else rest = line + strlen(line);

  if (strcmp(line, "ping") == 0) {
    Serial.println("#pong");
    Serial.println("#ok");
  } else if (strcmp(line, "help") == 0) {
    Serial.println("#ping|help|state?|enc <n>|enc sp|enc lp|fb?");
    Serial.println("#wifi?|wifi log?|wifi on|wifi off|wifi conn [full|soft]");
    Serial.println("#wifi auto on|off|wifi boot|wifi scan");
    Serial.println("#ok");
  } else {
    Serial.printf("#err unknown cmd '%s'\n", line);
  }
}
#endif  // DEV_BUILD
