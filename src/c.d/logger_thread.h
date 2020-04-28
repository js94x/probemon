#ifndef WORKER_THREAD_H
#define WORKER_THREAD_H

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

struct probereq {
  time_t ts;
  char *mac;
  char *vendor;
  char *ssid;
  int rssi;
};
typedef struct probereq probereq_t;

void *process_queue(void *args);
void free_probereq(probereq_t *pr);

#endif
