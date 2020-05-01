#ifndef PARSERS_H
#define PARSERS_H

#include <stdbool.h>
#include <stdint.h>

#include "logger_thread.h"

extern int8_t parse_radiotap_header(const u_char * packet, uint16_t * freq,
                                    int8_t * rssi);

extern void parse_probereq_frame(const u_char *packet, uint32_t header_len,
  int8_t offset, char **mac, char **ssid, uint8_t *ssid_len);

char *probereq_to_str(probereq_t pr);

#endif
