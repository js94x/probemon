# A C implementation of probemon

This is an impementation of *probemon* in **C**. It does exactly the same thing than *probemon.py* but this is written in C using *libpcap*, *libsqlite3* libraries.
The main implementation remains the *python* version but that could change in the future.

## Running
You run *probemon* with:

    $ sudo probemon -i wlan0mon -c 1

where *wlan0mon* is a wifi interface already put in monitor mode.

The default output format is a **sqlite3** database, named *probemon.db*. You can choose another name by using the `-d` switch.

You can query as usual the database with the python tool `stats.py` or create time plot with `plot.py`.

The complete usage:

    Usage: probemon -i IFACE -c CHANNEL [-d FILENAME] [-s]
      -c CHANNEL      channel to listen on
      -i IFACE        interface to use
      -d FILENAME     explicitly set the output filename
      -s              also log probe requests to stdout

TODO: one missing feature is currently the configuration file that is ignored. Meaning, you can't define IGNORED devices, and all mac addresses are stored in the database. But the python tools should respect the config file when reading the db, though.

## Dependencies
*probemon* depends on the following libraries:

  - libpcap
  - libpthread
  - libsqlite3
  - and on the `iw` executable

### Examples
On Ubuntu 18.04 or Raspbian, one needs to run the following command to install libraries and headers:

    sudo apt install pkg-config libpcap0.8 libpcap0.8-dev libsqlite3-dev libsqlite3-0 meson ninja-build

On archlinux-arm, this is:

    sudo pacman -S libpcap sqlite3 meson ninja

## Building
To build the executable, you need *meson* and *ninja*.

    $ cd probemon/src/c.d
    $ meson build
    $ ninja -C build

    $ sudo ./build/probemon ....
