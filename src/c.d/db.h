#ifndef DB_H
#define DB_H

#include <sqlite3.h>

// to avoid SD-card wear, we avoid writing to disk every seconds, setting a delay between each transactions
#define DB_CACHE_TIME 60    // time in second between transaction

int init_probemon_db(const char *db_file, sqlite3 **db);
int search_ssid(const char *ssid, sqlite3 *db);
int insert_ssid(const char *ssid, sqlite3 *db);
int search_vendor(const char *vendor, sqlite3 *db);
int insert_vendor(const char *vendor, sqlite3 *db);
int search_mac(const char *mac, sqlite3 *db);
int insert_mac(const char *mac, int vendor_id, sqlite3 *db);
int insert_probereq(probereq_t pr, sqlite3 *db);
int begin_txn(sqlite3 *db);
int commit_txn(sqlite3 *db);

#endif
