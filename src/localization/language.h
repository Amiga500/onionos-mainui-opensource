/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_LANGUAGE_H
#define MAINUI_LANGUAGE_H
#include <stdbool.h>
#define MAINUI_LANG_LIMIT 256

typedef struct {
    char *path, *name, *filename;
} MainUILanguageEntry;

typedef struct {
    MainUILanguageEntry entries[MAINUI_LANG_LIMIT];
    int count, selected, start;
} MainUILanguages;

/* No initialization or startup file reads. Discovery is explicit and on demand. */
bool mainui_languages_open(MainUILanguages *list, const char *sd, const char *fallback);
void mainui_languages_close(MainUILanguages *list);
bool mainui_language_select(const MainUILanguages *list, const char *sd);
const char *mainui_translate(int id, const char *fallback);
void mainui_language_close(void);
#endif
