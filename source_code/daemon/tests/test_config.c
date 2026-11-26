/* U-08 (yapılandırma), U-09 (scope adı), dışlama listesi. */
#include "../src/apps.h"
#include "../src/config.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char path[64];

static void write_conf(const char *text)
{
    FILE *f = fopen(path, "w");
    fputs(text, f);
    fclose(f);
}

static void u08_config_parse(void)
{
    struct zp_config c;
    write_conf("# yorum\n"
               "profile = akilli-tahta\n"
               "enabled = false\n"
               "never_freeze = vlc, obs ,  \n"
               "bilinmeyen = 1\n"
               "esitsiz satir\n");
    CHECK(cfg_load(&c, path) == 0);
    CHECK(strcmp(c.profile, "akilli-tahta") == 0);
    CHECK(!c.enabled);
    CHECK(c.n_never_freeze == 2);
    CHECK(cfg_in_list(c.never_freeze, c.n_never_freeze, "obs"));
    CHECK(!cfg_in_list(c.never_freeze, c.n_never_freeze, "firefox"));
}

static void u08_invalid_profile(void)
{
    struct zp_config c;
    write_conf("profile = yok-boyle-profil\n");
    CHECK(cfg_load(&c, path) == 0);
    CHECK(strcmp(c.profile, cfg_default_profile()->name) == 0);
}

static void u08_missing_file(void)
{
    struct zp_config c;
    CHECK(cfg_load(&c, "/olmayan/dizin/zperf.conf") == 0);
    CHECK(c.enabled);
}

static void config_roundtrip(void)
{
    struct zp_config a, b;
    cfg_defaults(&a);
    snprintf(a.profile, sizeof a.profile, "ofis");
    a.enabled = false;
    snprintf(a.never_freeze[0], ZP_NAME_LEN, "vlc");
    a.n_never_freeze = 1;
    CHECK(cfg_save(&a, path) == 0);
    CHECK(cfg_load(&b, path) == 0);
    CHECK(strcmp(b.profile, "ofis") == 0);
    CHECK(!b.enabled);
    CHECK(b.n_never_freeze == 1 && strcmp(b.never_freeze[0], "vlc") == 0);
}

static void profiles_exist(void)
{
    CHECK(cfg_profile("dengeli") != NULL);
    CHECK(cfg_profile("akilli-tahta") != NULL);
    CHECK(cfg_profile("ofis") != NULL);
    CHECK(!cfg_profile("ofis")->freeze);   /* Ofis hiç dondurmaz (U-07) */
}

static void u09_unit_name(void)
{
    char out[96];
    CHECK(apps_unit_name("soffice.bin", 3120, out, sizeof out) == 0);
    CHECK(strcmp(out, "zperf-soffice-bin-3120.scope") == 0);
    CHECK(apps_unit_name("Web Content", 7, out, sizeof out) == 0);
    CHECK(strcmp(out, "zperf-web-content-7.scope") == 0);
    CHECK(apps_unit_name("../../etc", 1, out, sizeof out) == 0);
    CHECK(strchr(out, '/') == NULL && strstr(out, "..") == NULL);
    CHECK(apps_unit_name("", 5, out, sizeof out) == 0);
    CHECK(strcmp(out, "zperf-app-5.scope") == 0);
    CHECK(apps_unit_name("x", 1, out, 8) < 0);   /* sığmıyor */
}

static void system_components(void)
{
    CHECK(apps_is_system_component("xfwm4"));
    CHECK(apps_is_system_component("zperf-agent"));
    CHECK(!apps_is_system_component("mousepad"));
}

int main(void)
{
    snprintf(path, sizeof path, "/tmp/zperf-test-XXXXXX");
    int fd = mkstemp(path);
    if (fd < 0)
        return 2;
    close(fd);

    RUN(u08_config_parse);
    RUN(u08_invalid_profile);
    RUN(u08_missing_file);
    RUN(config_roundtrip);
    RUN(profiles_exist);
    RUN(u09_unit_name);
    RUN(system_components);

    unlink(path);
    return SUMMARY();
}
