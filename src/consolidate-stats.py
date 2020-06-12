#!/usr/bin/python3

# This script gathers stats about each mac for each day by building a stats table.
# This allows to get faster response about day by day stats for each mac.
# The table stats is used by the stats.py script and by the mapot.py server
# This script is expected to be run every day by a cron job with the --update switch,
# once initialized with --init

import argparse
from datetime import timedelta, date, datetime
import time
import sqlite3
import sys

DESCRIPTION = 'consolidate stats from the probemon db'

def median(lst):
    n = len(lst)
    if n < 1:
        return None
    if n % 2 == 1:
        return sorted(lst)[n//2]
    else:
        return sum(sorted(lst)[n//2-1:n//2+1])//2

def init_stats(conn, c):
    sql = '''create table if not exists stats(
        mac_id integer,
        date text,
        first_seen text,
        last_seen text,
        count integer,
        min integer,
        max integer,
        avg integer,
        med integer,
        ssids text,
        unique(mac_id, date) on conflict replace,
        foreign key(mac_id) references mac(id)
    );'''
    try:
        c.execute(sql);
        sql = '''create index if not exists indx_stats on stats(mac_id);'''
        c.execute(sql);
        conn.commit()
    except sqlite3.OperationalError as o:
        print(f'Error: {o}', file=sys.stderr)
        sys.exit(1)

def gather_day_stats_for_mac(mac_id, day, c):
    start = time.mktime(time.strptime(f'{day}T00:00:00', '%Y-%m-%dT%H:%M:%S'))
    end = time.mktime(time.strptime(f'{day}T23:59:59', '%Y-%m-%dT%H:%M:%S'))
    sql = '''select date,ssid.name,rssi from probemon
    inner join ssid on ssid.id=probemon.ssid
    where mac=? and date>? and date<?;'''
    c.execute(sql, (mac_id, start, end))

    rssi = []
    ssids = set()
    ts = []
    for row in c.fetchall():
        rssi.append(row[2])
        ts.append(row[0])
        if row[1] != '':
            ssids.add(row[1])
    if len(ts) == 0:
        return None
    else:
        ts.sort()
        return ts[0], ts[-1], len(rssi), min(rssi), max(rssi), sum(rssi)//len(rssi), median(rssi), ssids

def gather_stats_day(day, conn, c):
    start = time.mktime(time.strptime(f'{day.year}-{day.month}-{day.day}T00:00:00', '%Y-%m-%dT%H:%M:%S'))
    end = time.mktime(time.strptime(f'{day.year}-{day.month}-{day.day}T23:59:59', '%Y-%m-%dT%H:%M:%S'))
    sql = '''select mac from probemon where date>? and date<? group by mac;'''
    c.execute(sql, (start, end))
    for m, in c.fetchall():
        sday = f'{day.year:04d}-{day.month:02d}-{day.day:02d}'
        first_seen, last_seen, count, rmin, rmax, ravg, rmed, ssids = gather_day_stats_for_mac(m, sday, c)
        sql = '''insert into stats (mac_id, date, first_seen, last_seen, count, min, max, avg, med, ssids)
            values (?,?,?,?,?,?,?,?,?,?)'''
        c.execute(sql, (m, sday, time.strftime('%H:%M:%S', time.localtime(first_seen)),
            time.strftime('%H:%M:%S', time.localtime(last_seen)), count, rmin, rmax, ravg, rmed, ','.join(ssids)))
    conn.commit()

def main():
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--init', action='store_true', default=False, help="build stats from db")
    group.add_argument('--clear', action='store_true', default=False, help="delete stats table from db")
    group.add_argument('-d', '--day', help='update stats for that day')
    parser.add_argument('--db', default='probemon.db', help="database file name to use")
    group.add_argument('-u', '--update', action='store_true', default=False, help="update stats from db for yesterday")
    args = parser.parse_args()

    conn = sqlite3.connect(args.db)
    c = conn.cursor()
    init_stats(conn, c)

    try:
        if args.clear:
            sql = 'drop table stats;'
            c.execute(sql)
            conn.commit()
            print(f'stats table has been deleted from {args.db}')
        elif args.day:
            try:
                day = datetime.strptime(args.day, '%Y-%m-%d')
            except  ValueError:
                try:
                    day = datetime.strptime(args.day, '%Y%m%d')
                except ValueError:
                    print(f"Error: can't parse day (expected format: YYYY-mm-dd)", file=sys.stderr)
                    sys.exit(1)
            day = date(day.year, day.month, day.day)
            print(f'Collecting data for all macs for {args.day}')
            gather_stats_day(day, conn, c)
        elif args.update:
            print('Collecting data for all macs for yesterday')
            yesterday = date.today()-timedelta(days=1)
            gather_stats_day(yesterday, conn, c)
        elif args.init:
            print('Initializing stats db by collecting data for all macs, day by day.')
            sql = 'select date from probemon order by date asc limit 1;'
            c.execute(sql)
            first_day = date.fromtimestamp(c.fetchone()[0])
            sql = 'select date from probemon order by date desc limit 1;'
            c.execute(sql)
            last_day = date.fromtimestamp(c.fetchone()[0])
            cday = first_day
            while cday != last_day:
                print('\b'*10+f'{cday.year:04d}-{cday.month:02d}-{cday.day:02d}', end='', flush=True)
                gather_stats_day(cday, conn, c)
                cday += timedelta(days=1)
            print('\b'*10+'Finished')
    except sqlite3.OperationalError as e:
        if args.init:
            print('\b'*10, flush=True)
        print(f'Error: {e}', file=sys.stderr)
        conn.close()
        sys.exit(1)

    conn.close()

if __name__ == '__main__':
    main()
