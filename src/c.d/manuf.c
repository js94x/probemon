#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *str_strip(char *s)
{
    size_t pos = strlen(s) - 1;
    if (s[pos] == '\n') {
        s[pos] = '\0';
    }
    return s;
}

// from https://stackoverflow.com/a/779960/283067
char *str_replace(char *orig, char *rep, char *with) {
    char *result;
    char *ins;
    char *tmp;
    int len_rep;
    int len_with;
    int len_front;
    int count;

    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL;
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep;
    }
    strcpy(tmp, orig);
    return result;
}

struct manuf {
    int64_t min;
    int64_t max;
    char *short_oui;
    char *long_oui;
    char *comment;
};
typedef struct manuf manuf_t;

void free_manuf_t(manuf_t *m)
{
  free(m->short_oui);
  free(m->long_oui);
  free(m->comment);
  free(m);
}

int cmp_manuf_t(const void *u, const void *v)
{
  return ((manuf_t *)u)->min-((manuf_t *)v)->min;
}

int parse_manuf_file(const char*path)
{
  FILE *manuf;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  int ouidb_size = 38*1024;
  int count = 0;
  manuf_t *ouidb = malloc(ouidb_size * sizeof(manuf_t));

  manuf = fopen(path, "r");
  if (manuf == NULL) {
    return -1;
  }
  while ((read = getline(&line, &len, manuf)) != -1) {
    if (line[0] == '#' || strlen(line) == 1) {
      continue;
    }
    char *token, *mr=NULL, *so=NULL, *lo=NULL, *str = line;
    int indx = 0;
    manuf_t *tmp = malloc(sizeof(manuf_t));
    tmp->short_oui = NULL;
    tmp->long_oui = NULL;
    tmp->comment = NULL;
    while ((token = strsep(&str, "\t"))) {
      if (indx == 0) {
        mr = str_replace(str_replace(str_strip(token), "-", ":"), ":", "");
        if (strlen(mr) == 6) {
          char *min = malloc(13 * sizeof(char));
          min = strdup(mr);
          strcat(min, "000000");
          tmp->min = strtol(min, NULL, 16);
          char *max = malloc(13 * sizeof(char));
          max = strdup(mr);
          strcat(max, "ffffff");
          tmp->max = strtol(max, NULL, 16);
          free(min);
          free(max);
        } else if (strlen(mr) == 12) {
          tmp->min = strtol(mr, NULL, 16);
          tmp->max = strtol(mr, NULL, 16);
        } else if (strlen(mr) == 15) {
          mr[12] = '\0';
          tmp->min = strtol(mr, NULL, 16);
          int mask = (int)strtol(mr+13, NULL, 10);
          int t=0;
          for (int i=0; i<(48-mask)/4; i++) {
            t += 0xf << i;
          }
          tmp->max = tmp->min+t;
        } else {
          printf("%s\n", mr);
        }

      } else if (indx == 1) {
        tmp->short_oui = strdup(str_strip(token));
      } else if (indx == 2) {
        tmp->long_oui = strdup(str_strip(token));
      } else if (indx == 3) {
        tmp->comment = strdup(str_strip(token));
      }
      indx++;
    }
    ouidb[count] = *tmp;
    count++;
    if (count == ouidb_size) {
      ouidb_size += 1024;
      ouidb = realloc(ouidb, ouidb_size * sizeof(manuf_t));
    }
  }
  fclose(manuf);

  qsort(ouidb, ouidb_size, sizeof(manuf_t), cmp_manuf_t);

  printf("%d, %d\n", count, count*sizeof(manuf_t));

  if (line) {
    free(line);
  }

  for (int i=0; i<count; i++  ) {
    free(&ouidb[i]);
  }
  free(ouidb);

  return 0;
}

int main(void)
{
  parse_manuf_file("../manuf");
}
