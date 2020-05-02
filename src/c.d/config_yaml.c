#include <stdio.h>
#include <stdlib.h>
#include <yaml.h>

#include "manuf.h"

// very basic yaml parser
// it only scans for a given key (keyname) and return entries for that key
char **parse_config_yaml(const char *path, const char *keyname, int *count)
{
  yaml_parser_t parser;
  yaml_token_t  token;

  FILE *fh = fopen(path, "r");
  if (fh == NULL) {
    fprintf(stderr, "Error: failed to open file %s\n", path);
    return NULL;
  }
  if (!yaml_parser_initialize(&parser)) {
    fprintf(stderr, "Error: failed to initialize yaml parser\n");
    return NULL;
  }
  yaml_parser_set_input_file(&parser, fh);

  int key = 0, start = 0, entry = 0;
  char **entries = NULL;
  do {
    yaml_parser_scan(&parser, &token);
    char *tk = (char *)token.data.scalar.value;
    switch(token.type) {
      case YAML_KEY_TOKEN:
        key = 1;
        break;
      case YAML_BLOCK_ENTRY_TOKEN:
        if (start) {
          entry = 1;
        }
        break;
      case YAML_BLOCK_END_TOKEN:
        if (start) {
          start = 0;
        }
        break;
      case YAML_SCALAR_TOKEN:
        if (key) {
          key = 0;
          if (strcmp(tk, keyname) == 0) {
            start = 1;
          }
        }
        if (entry) {
          entries = realloc(entries, sizeof(char *)*(*count+1));
          entries[*count] = strdup(tk);
          *count += 1;
          entry = 0;
        }
        break;
      default:
        break;
    }
    if (token.type != YAML_STREAM_END_TOKEN) {
      yaml_token_delete(&token);
    }
  } while(token.type != YAML_STREAM_END_TOKEN);
  yaml_token_delete(&token);

  yaml_parser_delete(&parser);
  fclose(fh);

  return entries;
}

int cmp_uint64_t(const void *u, const void*v)
{
  return *(uint64_t *)u-*(uint64_t *)v;
}

uint64_t *parse_ignored_entries(char **entries, int count)
{

  if (entries == NULL || count == 0) {
    return NULL;
  }

  uint64_t *result = malloc(count*sizeof(uint64_t));

  for (int i=0; i<count; i++) {
    char *tmp = str_replace(entries[i], ":", "");
    result[i] = strtol(tmp, NULL, 16);
    free(tmp);
  }

  qsort(result, count, sizeof(uint64_t), cmp_uint64_t);

  return result;
}
