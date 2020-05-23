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
#include "config_yaml.h"
#include "lruc.h"

#define MAX_QUEUE_SIZE 128 // watch out: this is already defined in probemon.c; keep them in sync
#define MAC_CACHE_SIZE 64
#define SSID_CACHE_SIZE 64

extern pthread_cond_t cv;
extern pthread_mutex_t mutex_queue;
extern queue_t *queue;
extern sqlite3 *db;
struct timespec start_ts_cache;
extern bool option_stdout;

extern manuf_t *ouidb;
extern size_t ouidb_size;

extern uint64_t *ignored;
extern int ignored_count;

lruc *ssid_pk_cache = NULL, *mac_pk_cache = NULL;

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
  probereq_t *prs[MAX_QUEUE_SIZE];
  int qs;
  struct timespec now;

  mac_pk_cache = lruc_new(MAC_CACHE_SIZE, 1);
  ssid_pk_cache = lruc_new(SSID_CACHE_SIZE, 1);

  clock_gettime(CLOCK_MONOTONIC, &start_ts_cache);

  while (true) {
    pthread_mutex_lock(&mutex_queue);
    pthread_cond_wait(&cv, &mutex_queue);

    qs = queue->size;
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
      // check if mac is not in ignored list
      uint64_t *res = NULL;
      if (ignored != NULL) {
        char *tmp = str_replace(pr->mac, ":", "");
        uint64_t mac_number = strtoll(tmp, NULL, 16);
        free(tmp);
        res = bsearch(&mac_number, ignored, sizeof(uint64_t), ignored_count, cmp_uint64_t);
      }
      if (res == NULL) {
        insert_probereq(*pr, db, mac_pk_cache, ssid_pk_cache);
        if (option_stdout) {
          char *pr_str = probereq_to_str(*pr);
          printf("%s\n", pr_str);
          free(pr_str);
        }
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
      free_probereq(prs[j]);
    }
  }

  lruc_free(mac_pk_cache);
  lruc_free(ssid_pk_cache);

  return NULL;
}
