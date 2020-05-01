#ifndef MANUF_H
#define MANUF_H

#include <stdint.h>

struct manuf {
    uint64_t min;
    uint64_t max;
    char *short_oui;
    char *long_oui;
    char *comment;
};
typedef struct manuf manuf_t;

void free_manuf_t(manuf_t *m);
manuf_t *parse_manuf_file(const char*path, size_t *ouidb_size);
int lookup_oui(char *mac, manuf_t *ouidb, size_t ouidb_size);

char *str_replace(const char *orig, const char *rep, const char *with);

#endif
