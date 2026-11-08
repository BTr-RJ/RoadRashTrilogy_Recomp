/* Test the real pinned emitter with empty/populated lists; no ROM required. */
#include "game_config.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "layout_codegen line %d: %s\n", __LINE__, #x); return 1; \
} } while (0)

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    for (int level_count = 0; level_count <= 2; level_count += 2) {
        for (int ws_count = 0; ws_count <= 2; ws_count += 2) {
            GameConfig cfg = { 0 };
            cfg.ram_layout.present = true;
            cfg.ram_layout.initial_ssp = 0xFFFFF6;
            cfg.ram_layout.level_mode_count = level_count;
            cfg.ram_layout.level_modes[0] = 0x08;
            cfg.ram_layout.level_modes[1] = 0x0C;
            cfg.ram_layout.ws_eligible_mode_count = ws_count;
            cfg.ram_layout.ws_eligible_modes[0] = 0x10;
            cfg.ram_layout.ws_eligible_modes[1] = 0x14;
            CHECK(game_config_emit_layout(&cfg, argv[1]));
            FILE *file = fopen(argv[1], "r");
            CHECK(file != NULL);
            char text[4096];
            size_t length = fread(text, 1, sizeof(text) - 1, file);
            CHECK(!ferror(file));
            CHECK(feof(file));
            CHECK(fclose(file) == 0);
            text[length] = 0;
            CHECK(strstr(text, "= { }") == NULL);
            CHECK(strstr(text, "= {}") == NULL);
            CHECK(strstr(text, level_count
                ? ".level_modes        = { 0x08u, 0x0Cu }"
                : ".level_modes        = { 0 }") != NULL);
            CHECK(strstr(text, level_count
                ? ".level_mode_count   = 2,"
                : ".level_mode_count   = 0,") != NULL);
            CHECK(strstr(text, ws_count
                ? ".ws_eligible_modes  = { 0x10u, 0x14u }"
                : ".ws_eligible_modes  = { 0 }") != NULL);
            CHECK(strstr(text, ws_count
                ? ".ws_eligible_mode_count = 2,"
                : ".ws_eligible_mode_count = 0,") != NULL);
            CHECK(strstr(text, ".initial_ssp        = 0x00FFFFF6u,") != NULL);
            CHECK(remove(argv[1]) == 0);
        }
    }
    puts("Layout emission: all four empty/populated combinations passed.");
    return 0;
}
