#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include <math.h>
#include <libgen.h>

#include "logger_thread.h"
#include "manuf.h"
#include "parsers.h"
#include "base64.h"

static inline int do_nothing(void *not_used, int argc, char **argv, char **col_name)
{
  return 0;
}

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
  if((ret = sqlite3_exec(*db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
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
  if((ret = sqlite3_exec(*db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "create table if not exists ssid("
    "id integer not null primary key,"
    "name text"
    ");";
  if((ret = sqlite3_exec(*db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
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
  if((ret = sqlite3_exec(*db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "create index if not exists idx_probemon_date on probemon(date);";
  if((ret = sqlite3_exec(*db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "pragma synchronous = normal;";
  if((ret = sqlite3_exec(*db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "pragma temp_store = 2;"; // to store temp table and indices in memory
  if((ret = sqlite3_exec(*db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }
  sql = "pragma journal_mode = off;"; // disable journal for rollback (we don't use this)
  if((ret = sqlite3_exec(*db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(*db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(*db);
    return ret;
  }

  return 0;
}

int search_ssid(const char *ssid, sqlite3 *db)
{
  char *sql;
  sqlite3_stmt *stmt;
  int ssid_id = 0, ret;

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
        ssid_id = sqlite3_column_int(stmt, 0);
      } else {
        fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
        break;
      }
    }
    sqlite3_finalize(stmt);
  }
  return ssid_id;
}

int insert_ssid(const char *ssid, sqlite3 *db)
{
    // insert the ssid into the db
  int ret, ssid_id = 0;
  char sql[128];

  ssid_id = search_ssid(ssid, db);
  if (!ssid_id) {
    // we need to escape quote by doubling them
    char *nssid = str_replace(ssid, "'", "''");
    snprintf(sql, 128, "insert into ssid (name) values ('%s');", nssid);
    free(nssid);
    if((ret = sqlite3_exec(db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
      fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
      sqlite3_close(db);
      return ret * -1;
    }
    ssid_id = search_ssid(ssid, db);
  }

  return ssid_id;
}

int search_vendor(const char *vendor, sqlite3 *db)
{
  char *sql;
  sqlite3_stmt *stmt;
  int vendor_id = 0, ret;

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
        vendor_id = sqlite3_column_int(stmt, 0);
      } else {
        fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
        break;
      }
    }
    sqlite3_finalize(stmt);
  }
  return vendor_id;
}

int insert_vendor(const char *vendor, sqlite3 *db)
{
    // insert the vendor into the db
  int ret, vendor_id = 0;
  char sql[128];

  vendor_id = search_vendor(vendor, db);
  if (!vendor_id) {
    // we need to escape quote by doubling them
    char *nvendor = str_replace(vendor, "'", "''");
    snprintf(sql, 128, "insert into vendor (name) values ('%s');", nvendor);
    free(nvendor);
    if((ret = sqlite3_exec(db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
      fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
      sqlite3_close(db);
      return ret * -1;
    }
    vendor_id = search_vendor(vendor, db);
  }

  return vendor_id;
}

int search_mac(const char *mac, sqlite3 *db)
{
  char *sql;
  sqlite3_stmt *stmt;
  int mac_id = 0, ret;

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
        mac_id = sqlite3_column_int(stmt, 0);
      } else {
        fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
        break;
      }
    }
    sqlite3_finalize(stmt);
  }
  return mac_id;
}

int insert_mac(const char *mac, int vendor_id, sqlite3 *db)
{
    // insert the mac into the db
  int ret, mac_id = 0;
  char sql[128];

  mac_id = search_mac(mac, db);
  if (!mac_id) {
    snprintf(sql, 128, "insert into mac (address, vendor) values ('%s', '%d');", mac, vendor_id);
    if((ret = sqlite3_exec(db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
      fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
      sqlite3_close(db);
      return ret * -1;
    }
    mac_id = search_mac(mac, db);
  }

  return mac_id;
}

int insert_probereq(probereq_t pr, sqlite3 *db)
{
  int vendor_id, ssid_id, mac_id, ret;

  // is ssid a valid utf-8 string
  char tmp[64];
  memcpy(tmp, pr.ssid, pr.ssid_len);
  tmp[pr.ssid_len] = '\0';

  if (!is_utf8(tmp)) {
    // base64 encode the ssid
    size_t length;
    char * b64tmp = base64_encode((unsigned char *)tmp, pr.ssid_len, &length);
    strcpy(tmp, "b64_");
    strncat(tmp, b64tmp, length);
    free(b64tmp);
  }

  ssid_id = insert_ssid(tmp, db);
  vendor_id = insert_vendor(pr.vendor, db);
  mac_id = insert_mac(pr.mac, vendor_id, db);

  char sql[256];
  snprintf(sql, 256, "insert into probemon (date, mac, ssid, rssi)"
    "values ('%lu', '%u', '%u', '%d');", pr.ts, mac_id, ssid_id, pr.rssi);
  if((ret = sqlite3_exec(db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
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
  if((ret = sqlite3_exec(db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
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
  if((ret = sqlite3_exec(db, sql, do_nothing, 0, NULL)) != SQLITE_OK) {
    fprintf(stderr, "Error: %s (%s:%d in %s)\n", sqlite3_errmsg(db), basename(__FILE__), __LINE__, __func__);
    sqlite3_close(db);
    return ret * -1;
  }

  return 0;
}
