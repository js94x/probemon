/*
worker thread that will process the queue filled by process_packet()
*/

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sqlite3.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>

#include "parsers.h"
#include "queue.h"
#include "logger_thread.h"
#include "db.h"
#include "manuf.h"

pthread_cond_t cv;
pthread_mutex_t mutex_queue;
extern queue_t *queue;
sqlite3 *db;
struct timespec start_ts_cache;
bool option_stdout;

manuf_t *ouidb;
size_t ouidb_size;

void free_probereq(probereq_t *pr)
{
    if (pr == NULL) return;

    free(pr->mac);
    pr->mac = NULL;
    free(pr->ssid);
    pr->ssid = NULL;
    if (pr->vendor) free(pr->vendor);
    pr->vendor = NULL;
    free(pr);
    pr = NULL;
    return;
}

void *process_queue(void *args)
{
  probereq_t *pr;
  probereq_t **prs;
  int qs;
  struct timespec now;

  while (1) {
    pthread_mutex_lock(&mutex_queue);
    pthread_cond_wait(&cv, &mutex_queue);

    qs = queue->size;
    prs = malloc(sizeof(probereq_t *) * qs);
    // off-load queue to a tmp array
    for (int i = 0; i < qs; i++) {
      prs[i] = (probereq_t *) dequeue(queue);
    }
    assert(queue->size == 0);
    pthread_mutex_unlock(&mutex_queue);

    // process the array after having unlock the queue
    for (int j = 0; j < qs; j++) {
      pr = prs[j];
      // look for vendor string in manuf
      int indx = lookup_oui(pr->mac, ouidb, ouidb_size);
      if (indx >= 0 && ouidb[indx].long_oui) {
        pr->vendor = strdup(ouidb[indx].long_oui);
      } else {
        pr->vendor = strdup("UNKNOWN");
      }
      insert_probereq(*pr, db);
      if (option_stdout) {
        char *pr_str = probereq_to_str(*pr);
        printf("%s\n", pr_str);
        free(pr_str);
      }
    }
    if (option_stdout) {
      fflush(stdout);
    }
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (now.tv_sec - start_ts_cache.tv_sec >= DB_CACHE_TIME) {
      // commit to db
      commit_txn(db);
      begin_txn(db);
      start_ts_cache = now;
    }
    for (int j = 0; j < qs; j++) {
      pr = prs[j];
      free_probereq(pr);
    }
    free(prs);
  }
  return NULL;
}
