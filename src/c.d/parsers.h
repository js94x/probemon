#ifndef PARSERS_H
#define PARSERS_H

#include <stdbool.h>
#include <stdint.h>

#include "logger_thread.h"

int8_t parse_radiotap_header(const uint8_t * packet, uint16_t * freq,
                                    int8_t * rssi);

void parse_probereq_frame(const uint8_t *packet, uint32_t header_len,
  int8_t offset, char **mac, uint8_t **ssid, uint8_t *ssid_len);

char *probereq_to_str(probereq_t pr);

bool is_utf8(const char * string);

#endif
