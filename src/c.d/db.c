#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include <libgen.h>

#include "logger_thread.h"
#include "manuf.h"
#include "parsers.h"
#include "base64.h"
#include "lruc.h"

int init_probemon_db(const char *db_file, sqlite3 **db)
{
  int ret;
  if ((ret = sqlite3_open(db_file, db)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }

  char *sql;
  sql = "create table if not exists vendor("
    "id integer not null primary key,"
    "name text"
    ");";
  if ((ret = sqlite3_exec(*db, sql, NULL, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "create table if not exists mac("
    "id integer not null primary key,"
    "address text,"
    "vendor integer,"
    "foreign key(vendor) references vendor(id)"
    ");";
  if ((ret = sqlite3_exec(*db, sql, NULL, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "create table if not exists ssid("
    "id integer not null primary key,"
    "name text"
    ");";
  if ((ret = sqlite3_exec(*db, sql, NULL, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "create table if not exists probemon("
    "date float,"
    "mac integer,"
    "ssid integer,"
    "rssi integer,"
    "foreign key(mac) references mac(id),"
    "foreign key(ssid) references ssid(id)"
    ");";
  if ((ret = sqlite3_exec(*db, sql, NULL, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "create index if not exists idx_probemon_date on probemon(date);";
  if ((ret = sqlite3_exec(*db, sql, NULL, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "pragma synchronous = normal;";
  if ((ret = sqlite3_exec(*db, sql, NULL, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "pragma temp_store = 2;"; // to store temp table and indices in memory
  if ((ret = sqlite3_exec(*db, sql, NULL, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "pragma journal_mode = off;"; // disable journal for rollback (we don't use this)
  if ((ret = sqlite3_exec(*db, sql, NULL, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "pragma foreign_keys = on;"; // turn that on to enforce foreign keys
  if ((ret = sqlite3_exec(*db, sql, NULL, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }

  return 0;
}

int64_t search_ssid(const char *ssid, sqlite3 *db)
{
  char *sql;
  sqlite3_stmt *stmt;
  int64_t ssid_id = 0, ret;

  // look for an existing ssid in the db
  sql = "select id from ssid where name=?;";
  if ((ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
    return ret * -1;
  } else {
    if ((ret = sqlite3_bind_text(stmt, 1, ssid, -1, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
      return ret * -1;
    }

    while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
      if (ret == SQLITE_ERROR) {
        fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
        break;
      } else if (ret == SQLITE_ROW) {
        ssid_id = sqlite3_column_int64(stmt, 0);
      } else {
        fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
        break;
      }
    }
    sqlite3_finalize(stmt);
  }
  return ssid_id;
}

int64_t insert_ssid(const char *ssid, sqlite3 *db)
{
    // insert the ssid into the db
  int64_t ret, ssid_id = 0;
  char sql[128];

  ssid_id = search_ssid(ssid, db);
  if (!ssid_id) {
    // we need to escape quote by doubling them
    char *nssid = str_replace(ssid, "'", "''");
    snprintf(sql, 128, "insert into ssid (name) values ('%s');", nssid);
    free(nssid);
    if ((ret = sqlite3_exec(db, sql, NULL, 0, NULL)) != SQLITE_OK) {
      fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
      sqlite3_close(db);
      return ret * -1;
    }
    ssid_id = search_ssid(ssid, db);
  }

  return ssid_id;
}

int64_t search_vendor(const char *vendor, sqlite3 *db)
{
  char *sql;
  sqlite3_stmt *stmt;
  int64_t vendor_id = 0, ret;

  // look for an existing vendor in the db
  sql = "select id from vendor where name=?;";
  if ((ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
    return ret * -1;
  } else {
    if ((ret = sqlite3_bind_text(stmt, 1, vendor, -1, NULL)) != SQLITE_OK) {
      fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
      return ret * -1;
    }

    while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
      if (ret == SQLITE_ERROR) {
        fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
        break;
      } else if (ret == SQLITE_ROW) {
        vendor_id = sqlite3_column_int64(stmt, 0);
      } else {
        fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
        break;
      }
    }
    sqlite3_finalize(stmt);
  }
  return vendor_id;
}

int64_t insert_vendor(const char *vendor, sqlite3 *db)
{
    // insert the vendor into the db
  int64_t ret, vendor_id = 0;
  char sql[128];

  vendor_id = search_vendor(vendor, db);
  if (!vendor_id) {
    // we need to escape quote by doubling them
    char *nvendor = str_replace(vendor, "'", "''");
    snprintf(sql, 128, "insert into vendor (name) values ('%s');", nvendor);
    free(nvendor);
    if ((ret = sqlite3_exec(db, sql, NULL, 0, NULL)) != SQLITE_OK) {
      fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
      sqlite3_close(db);
      return ret * -1;
    }
    vendor_id = search_vendor(vendor, db);
  }

  return vendor_id;
}

int64_t search_mac(const char *mac, sqlite3 *db)
{
  char *sql;
  sqlite3_stmt *stmt;
  int64_t mac_id = 0, ret;

  // look for an existing mac in the db
  sql = "select id from mac where address=?;";
  if ((ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
    return ret * -1;
  } else {
    if ((ret = sqlite3_bind_text(stmt, 1, mac, -1, NULL)) != SQLITE_OK) {
      fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
      return ret * -1;
    }

    while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
      if (ret == SQLITE_ERROR) {
        fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
        break;
      } else if (ret == SQLITE_ROW) {
        mac_id = sqlite3_column_int64(stmt, 0);
      } else {
        fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
        break;
      }
    }
    sqlite3_finalize(stmt);
  }
  return mac_id;
}

int64_t insert_mac(const char *mac, int64_t vendor_id, sqlite3 *db)
{
    // insert the mac into the db
  int64_t ret, mac_id = 0;
  char sql[128];

  mac_id = search_mac(mac, db);
  if (!mac_id) {
    snprintf(sql, 128, "insert into mac (address, vendor) values ('%s', '%lu');", mac, vendor_id);
    if ((ret = sqlite3_exec(db, sql, NULL, 0, NULL)) != SQLITE_OK) {
      fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
      sqlite3_close(db);
      return ret * -1;
    }
    mac_id = search_mac(mac, db);
  }

  return mac_id;
}

int insert_probereq(probereq_t pr, sqlite3 *db, lruc *mac_pk_cache, lruc *ssid_pk_cache)
{
  int64_t vendor_id, ssid_id, mac_id;
  int ret;

  // is ssid a valid utf-8 string
  char tmp[64];
  memcpy(tmp, pr.ssid, pr.ssid_len);
  tmp[pr.ssid_len] = '\0';

  if (!is_utf8(tmp)) {
    // base64 encode the ssid
    size_t length;
    char *b64tmp = base64_encode((unsigned char *)tmp, pr.ssid_len, &length);
    snprintf(tmp, length+4+1, "b64_%s", b64tmp);
    free(b64tmp);
  }

  void *value = NULL;
  // look up ssid (tmp) in ssid_pk_cache
  lruc_get(ssid_pk_cache, tmp, strlen(tmp)+1, &value);
  if (value == NULL) {
    ssid_id = insert_ssid(tmp, db);
    // add the ssid_id to the cache
    int64_t *new_value = malloc(sizeof(int64_t));
    *new_value = ssid_id;
    lruc_set(ssid_pk_cache, strdup(tmp), strlen(tmp)+1, new_value, sizeof(int64_t));
  } else {
    ssid_id = *(int64_t *)value;
  }

  // look up mac in mac_pk_cache
  value = NULL;
  lruc_get(mac_pk_cache, pr.mac, 18, &value);
  if (value == NULL) {
    vendor_id = insert_vendor(pr.vendor, db);
    mac_id = insert_mac(pr.mac, vendor_id, db);
    // add the mac_id to the cache
    int64_t *new_value = malloc(sizeof(int64_t));
    *new_value = mac_id;
    lruc_set(mac_pk_cache, strdup(pr.mac), 18, new_value, sizeof(int64_t));
  } else {
    mac_id = *(int64_t *)value;
  }

  // convert timeval to double
  double ts;
  char tstmp[32];
  sprintf(tstmp, "%lu.%06lu", pr.tv.tv_sec, pr.tv.tv_usec);
  ts = strtod(tstmp, NULL);

  char sql[256];
  snprintf(sql, 256, "insert into probemon (date, mac, ssid, rssi)"
    "values ('%f', '%lu', '%lu', '%d');", ts, mac_id, ssid_id, pr.rssi);
  if ((ret = sqlite3_exec(db, sql, NULL, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(db);
    return ret * -1;
  }

  return 0;
}

int begin_txn(sqlite3 *db)
{
  int ret;
  char sql[32];

  snprintf(sql, 32, "begin transaction;");
  if ((ret = sqlite3_exec(db, sql, NULL, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(db);
    return ret * -1;
  }

  return 0;
}

int commit_txn(sqlite3 *db)
{
  int ret;
  char sql[32];

  snprintf(sql, 32, "commit transaction;");
  if ((ret = sqlite3_exec(db, sql, NULL, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(db);
    return ret * -1;
  }

  return 0;
}
