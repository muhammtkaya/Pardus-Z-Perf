#include "config.h"
#include "log.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define MB (1024ULL * 1024ULL)

/* Başlangıç değerleri tahmindir; ölçümlerden sonra en fazla bir kez ayarlanır
 * ve gerekçesi raporda yazılır (docs/05 bölüm 5, docs/07 bölüm 5.4).
 * PSI tetikleyici biçimi: "<some|full> <eşik µs> <pencere µs>". */
static const struct zp_profile profiles[] = {
    { "dengeli",      "lz4",  100, "some 100000 1000000", "some 250000 1000000",
      "full 50000 1000000", 5.0, 64 * MB, 60,  true  },
    { "akilli-tahta", "lz4",  150, "some 50000 1000000",  "some 150000 1000000",
      "full 30000 1000000", 3.0, 96 * MB, 30,  true  },
    { "ofis",         "zstd", 100, "some 100000 1000000", "some 300000 1000000",
      "full 80000 1000000", 5.0, 32 * MB, 120, false },
};

const struct zp_profile *cfg_profile(const char *name)
{
    for (size_t i = 0; i < sizeof profiles / sizeof profiles[0]; i++)
        if (strcmp(profiles[i].name, name) == 0)
            return &profiles[i];
    return NULL;
}

const struct zp_profile *cfg_default_profile(void) { return &profiles[0]; }

void cfg_defaults(struct zp_config *c)
{
    memset(c, 0, sizeof *c);
    snprintf(c->profile, sizeof c->profile, "%s", profiles[0].name);
    c->enabled = true;
}

static char *trim(char *s)
{
    while (isspace((unsigned char) *s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char) e[-1]))
        *--e = '\0';
    return s;
}

/* "a, b ,c" → listeye; fazlası uyarıyla atlanır. */
static void parse_list(const char *val, char list[][ZP_NAME_LEN], size_t *n)
{
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s", val);
    *n = 0;
    for (char *save = NULL, *tok = strtok_r(tmp, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        tok = trim(tok);
        if (!*tok)
            continue;
        if (*n == ZP_MAX_NAMES) {
            LOG_WARN("liste en fazla %d öğe alır, '%s' atlandı", ZP_MAX_NAMES, tok);
            continue;
        }
        snprintf(list[(*n)++], ZP_NAME_LEN, "%s", tok);
    }
}

int cfg_load(struct zp_config *c, const char *path)
{
    cfg_defaults(c);
    FILE *f = fopen(path, "re");
    if (!f)
        return errno == ENOENT ? 0 : -errno;

    char line[1024];
    int lineno = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        char *s = trim(line);
        if (!*s || *s == '#')
            continue;
        char *eq = strchr(s, '=');
        if (!eq) {
            LOG_WARN("%s:%d: '=' yok, satır atlandı", path, lineno);
            continue;
        }
        *eq = '\0';
        char *key = trim(s), *val = trim(eq + 1);

        if (strcmp(key, "profile") == 0) {
            if (cfg_profile(val))
                snprintf(c->profile, sizeof c->profile, "%s", val);
            else
                LOG_WARN("%s:%d: bilinmeyen profil '%s', varsayılan kullanılıyor",
                         path, lineno, val);
        } else if (strcmp(key, "enabled") == 0) {
            c->enabled = strcmp(val, "false") != 0 && strcmp(val, "0") != 0;
        } else if (strcmp(key, "never_freeze") == 0) {
            parse_list(val, c->never_freeze, &c->n_never_freeze);
        } else if (strcmp(key, "exclude_extra") == 0) {
            parse_list(val, c->exclude_extra, &c->n_exclude_extra);
        } else {
            LOG_WARN("%s:%d: bilinmeyen anahtar '%s' atlandı", path, lineno, key);
        }
    }
    fclose(f);
    return 0;
}

static void write_list(FILE *f, const char *key, const char list[][ZP_NAME_LEN], size_t n)
{
    fprintf(f, "%s = ", key);
    for (size_t i = 0; i < n; i++)
        fprintf(f, "%s%s", i ? ", " : "", list[i]);
    fputc('\n', f);
}

int cfg_save(const struct zp_config *c, const char *path)
{
    /* Yarım yazılmış dosya kalmasın: önce geçici dosyaya yaz, sonra rename(). */
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "we");
    if (!f)
        return -errno;
    fprintf(f, "# Z-Performans yapılandırması. zperf-gui ve zperfctl tarafından da yazılır.\n");
    fprintf(f, "profile = %s\n", c->profile);
    fprintf(f, "enabled = %s\n", c->enabled ? "true" : "false");
    write_list(f, "never_freeze", c->never_freeze, c->n_never_freeze);
    write_list(f, "exclude_extra", c->exclude_extra, c->n_exclude_extra);
    if (fclose(f) != 0)
        return -errno;
    return rename(tmp, path) < 0 ? -errno : 0;
}

bool cfg_in_list(char list[][ZP_NAME_LEN], size_t n, const char *name)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(list[i], name) == 0)
            return true;
    return false;
}
