#ifndef CONFIG_YAML_H
#define CONFIG_YAML_H

char **parse_config_yaml(const char *path, const char *keyname, int *count);
uint64_t *parse_ignored_entries(char **entries, int count);
int cmp_uint64_t(const void *u, const void*v);

#endif
