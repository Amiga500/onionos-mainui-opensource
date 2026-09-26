/* SPDX-License-Identifier: GPL-3.0-only */
#include "app/render.h"
#include "platform/audio.h"
#include "platform/timing.h"
#include "ui/drawing.h"
#include "ui/panels.h"
#include "ui/search_label.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read the line-oriented format used by legacy Favorite/Recent records.
 * This preview only displays labels: it does not claim to implement the full
 * Recent filtering or Favorite-folder model, and never rewrites the input.
 */
bool mainui_load_list(MainUIPreviewList *list, const char *path)
{
    list->data = mainui_read_text(path, 8 * 1024 * 1024);
    if (!list->data) {
        return false;
    }
    char *line = list->data;
    while (*line && list->count < 10000) {
        char *next = strchr(line, '\n');
        if (next) {
            *next = 0;
        }
        cJSON *item = cJSON_ParseWithOpts(line, NULL, true);
        const cJSON *label = cJSON_GetObjectItemCaseSensitive(item, "label");
        if (cJSON_IsObject(item) && cJSON_IsString(label)) {
            list->records[list->count] = item;
            list->labels[list->count++] = label->valuestring;
        }
        else {
            cJSON_Delete(item);
        }
        if (!next) {
            break;
        }
        line = next + 1;
    }
    return true;
}

void mainui_close_list(MainUIPreviewList *list)
{
    for (int i = 0; i < list->count; i++) {
        cJSON_Delete(list->records[i]);
    }
    free(list->data);
}

static void blit(SDL_Surface *surface, SDL_Surface *screen, int x, int y)
{
    SDL_Rect dest = {(Sint16)x, (Sint16)y, 0, 0};
    if (surface) {
        SDL_BlitSurface(surface, NULL, screen, &dest);
    }
}

const char *mainui_list_label_at(void *context, int index)
{
    MainUIListLabels *source = context;
    if (source->library) {
        return mainui_library_label(source->library, index);
    }
    if (source->catalog) {
        return mainui_browser_label(source->catalog, index);
    }
    return source->preview->labels[index];
}

/* Stock list headers retain the section/system name at every folder depth. */
const char *mainui_library_heading(const MainUILibrary *library)
{
    return library->recent ? mainui_translate(18, "Recents") : mainui_translate(1, "Favorites");
}

const char *mainui_catalog_heading(const MainUICatalog *catalog)
{
    return catalog->pages[catalog->depth > 0 ? 1 : 0].title;
}

/* Pagination keeps physical rows; counters exclude folders as in the patcher. */
static void draw_list_counter(SDL_Surface *screen, MainUITheme *theme, const MainUILibrary *library,
                              MainUICatalog *catalog, const MainUIViewport *view)
{
    int current = view->selected + 1;
    int total = view->total;
    if (total <= 0) {
        mainui_draw_footer(screen, theme, 0, -1);
        return;
    }
    if (library) {
        if (mainui_library_is_folder(library, view->selected)) {
            int row = library->visible[view->selected];
            if (row != INT_MIN) {
                mainui_draw_folder_footer(screen, theme, library->folders[-row - 1].direct_games);
                return;
            }
            mainui_draw_footer(screen, theme, 0, -1);
            return;
        }
        current -= library->leading_folders;
        total = library->visible_games;
    }
    else if (catalog) {
        if (mainui_browser_folder(catalog, view->selected)) {
            mainui_draw_footer(screen, theme, 0, -1);
            return;
        }
        int folders = catalog->pages[catalog->depth].folder_count + (catalog->depth > 1);
        current -= folders;
        total -= folders;
    }
    mainui_draw_footer(screen, theme, current, total);
}

void mainui_prepare_frame(MainUIApp *ui)
{
    if (!ui->home && !ui->library && ui->catalog) {
        if (ui->catalog == ui->games) {
            ui->games_view = ui->view;
        }
        else if (ui->catalog == ui->expert) {
            ui->expert_view = ui->view;
        }
    }
    ui->list_labels = (MainUIListLabels){ui->catalog, ui->library, ui->list};
    int destination;
    if (mainui_letter_jump_step(&ui->letter_jump, mainui_list_label_at, &ui->list_labels,
                                &destination)) {
        mainui_viewport_move(&ui->view, ui->config.rows, destination - ui->view.selected, false);
        ui->selected_at = SDL_GetTicks();
    }
    /* Changes to the active selection produce one sample, independent of repaint
     * frequency and marquee timer ticks. Initial presentation is silent. */
    int section = 4;
    int selection = ui->view.selected;
    if (ui->name_input.open) {
        section = 7;
        selection = ui->name_input.selected;
    }
    else if (ui->context_open) {
        section = 5;
        selection = ui->context.selected;
    }
    else if (ui->language_open) {
        section = 6;
        selection = ui->languages.selected;
    }
    else if (ui->settings_open) {
        section = 2;
        selection = ui->settings.selected;
    }
    else if (ui->apps) {
        section = 1;
        selection = ui->apps_view.selected;
    }
    else if (ui->home) {
        section = 0;
        selection = ui->home_view.selected;
    }
    else if (ui->library) {
        section = 3;
    }
    if (ui->sound_section == section && ui->sound_selection != selection) {
        mainui_audio_change();
    }
    /* Own row identity: the index/label can survive deletion or replacement. */
    const char *key = "", *container = "", *label = "";
    if (section == 3 && selection >= 0 && selection < ui->library->visible_count) {
        label = mainui_library_label(ui->library, selection);
        int row = ui->library->visible[selection];
        container = ui->library->current >= 0 ? ui->library->folders[ui->library->current].id : "";
        key = row == INT_MIN ? ".."
              : row < 0      ? ui->library->folders[-row - 1].id
                             : ui->library->items[row].identity;
    }
    else if (section == 4 && ui->catalog) {
        container = ui->catalog->pages[ui->catalog->depth].path;
        MainUIEntry *entry =
            mainui_catalog_entry(ui->catalog, mainui_browser_index(ui->catalog, selection));
        key = entry ? entry->path : "..";
    }
    else if (section == 0 && selection >= 0 && selection < ui->menu.count) {
        key = mainui_menu_label(ui->menu.sections[selection]);
    }
    else if (section == 1) {
        MainUIEntry *entry = ui->apps ? mainui_catalog_entry(ui->apps, selection) : NULL;
        key = entry ? entry->path : "";
    }
    else if (section == 2 && selection >= 0 && selection < ui->settings.count) {
        key = mainui_stock_setting_label(ui->settings.rows[selection]);
    }
    else if (section == 5 && selection >= 0 && selection < ui->context.visible_count) {
        key = mainui_context_label(&ui->context.entries[ui->context.visible[selection]]);
    }
    else if (section == 6 && selection >= 0 && selection < ui->languages.count) {
        key = ui->languages.entries[selection].name;
    }
    else if (section == 4 && ui->list && selection >= 0 && selection < ui->list->count) {
        key = ui->list->labels[selection];
    }
    char identity[sizeof ui->selected_identity];
    snprintf(identity, sizeof identity, "%d:%d:%p:%p:%s:%s:%s", section, ui->confirmation,
             (void *)ui->catalog, (void *)ui->library, container ? container : "", key ? key : "",
             label ? label : "");
    if (strcmp(identity, ui->selected_identity) || ui->confirmation >= 0) {
        memcpy(ui->selected_identity, identity, strlen(identity) + 1);
        ui->selected_at = SDL_GetTicks();
    }
    ui->sound_section = section;
    ui->sound_selection = selection;
    ui->animate = ui->letter_jump.active;
    SDL_Event queued_input;
    ui->draw_frame =
        ui->snapshot || ui->input_script ||
        (!ui->letter_jump.active &&
         SDL_PeepEvents(&queued_input, 1, SDL_PEEKEVENT, SDL_KEYDOWNMASK | SDL_KEYUPMASK) <= 0);
}

bool mainui_draw_frame(MainUIApp *ui)
{
    if (ui->catalog_job.thread) {
        /* Retain the previous frame until the catalog result is ready. */
    }
    else if (ui->details.open) {
        mainui_details_draw(&ui->details, &ui->theme, ui->screen);
    }
    else if (ui->settings_page.open) {
        mainui_settings_page_draw(&ui->settings_page, ui->screen, &ui->theme);
    }
    else if (ui->language_open) {
        mainui_draw_languages(ui->screen, &ui->theme, &ui->languages);
    }
    else if (ui->settings_open) {
        mainui_draw_settings(ui->screen, &ui->theme, &ui->settings);
    }
    else if (ui->apps) {
        mainui_draw_apps(ui->screen, &ui->theme, ui->apps, &ui->apps_view);
    }
    else if (ui->home) {
        mainui_menu_draw_home(&ui->menu_view, ui->screen, &ui->menu, &ui->home_view);
    }
    else if (ui->catalog && !ui->catalog->depth && !ui->library) {
        mainui_menu_draw_systems(&ui->menu_view, ui->screen, ui->catalog, &ui->view);
    }
    else {
        /* Keep rendered row labels until the visible window changes. Marquee
 * frames reuse these surfaces rather than rasterizing every 33ms.
 */
        if (ui->cached_start != ui->view.start) {
            for (int i = 0; i < 20; i++) {
                if (ui->labels[i]) {
                    SDL_FreeSurface(ui->labels[i]);
                }
                ui->labels[i] = NULL;
                if (ui->highlighted[i]) {
                    SDL_FreeSurface(ui->highlighted[i]);
                }
                ui->highlighted[i] = NULL;
            }
            for (int i = 0; i < ui->config.rows && ui->view.start + i < ui->view.total; i++) {
                const char *label = ui->library
                                        ? mainui_library_label(ui->library, ui->view.start + i)
                                    : ui->catalog ? NULL
                                                  : ui->list->labels[ui->view.start + i];
                if (ui->catalog && !ui->library) {
                    label = mainui_browser_label(ui->catalog, ui->view.start + i);
                    if (!label) {
                        fprintf(stderr, "%s\n", ui->catalog->error);
                        MainUILaunchSource source = {.section = ui->catalog == ui->expert
                                                                    ? MAINUI_MENU_EXPERT
                                                                    : MAINUI_MENU_GAMES,
                                                     .catalog = ui->catalog,
                                                     .view = &ui->view,
                                                     .home = &ui->home_view};
                        ui->reload_search = false;
                        if (!mainui_catalog_job_start(&ui->catalog_job, JOB_RELOAD, &source, ui->sd,
                                                      ui->config.case_sensitive, ui->config.rows,
                                                      NULL, ++ui->catalog_generation)) {
                            ui->status = 4;
                            ui->running = false;
                        }
                        break;
                    }
                }
                ui->favorite_rows[i] = false;
                if (ui->library && !mainui_library_is_folder(ui->library, ui->view.start + i)) {
                    ui->favorite_rows[i] = mainui_library_contains(
                        ui->favorites,
                        ui->library->items[ui->library->visible[ui->view.start + i]].rom);
                }
                else if (ui->catalog && !ui->library) {
                    cJSON *record = mainui_catalog_record(
                        ui->catalog, mainui_browser_index(ui->catalog, ui->view.start + i));
                    const cJSON *rom = cJSON_GetObjectItemCaseSensitive(record, "rompath");
                    ui->favorite_rows[i] = cJSON_IsString(rom) &&
                                           mainui_library_contains(ui->favorites, rom->valuestring);
                    cJSON_Delete(record);
                }
                char *marked = NULL;
                if (mainui_favorite_is_cut(&ui->favorite_editor, ui->library, ui->view.start + i)) {
                    size_t size = strlen(label) + 3;
                    marked = malloc(size);
                    if (marked) {
                        snprintf(marked, size, "> %s", label);
                    }
                }
                ui->labels[i] = TTF_RenderUTF8_Blended(ui->theme.font, marked ? marked : label,
                                                       ui->theme.color);
                if (ui->search.results) {
                    ui->highlighted[i] = mainui_search_label(&ui->theme, label, ui->search.query);
                }
                free(marked);
            }
            ui->cached_start = ui->view.start;
        }
        if (!ui->running) {
            return false;
        }
        if (ui->catalog_job.thread) {
            return false;
        }
        SDL_FillRect(ui->screen, NULL, SDL_MapRGB(ui->screen->format, 24, 24, 24));
        blit(ui->theme.background, ui->screen, 0, 0);
        mainui_draw_list_header_image(ui->screen, &ui->theme, ui->heading);
        /* The first frame of a list waits briefly for its cover so it does not
         * pop in, but one slow image can never stall opening the list. */
        mainui_preview_request_within(&ui->preview, ui->catalog, ui->library,
                                      ui->library || !ui->catalog
                                          ? ui->view.selected
                                          : mainui_browser_index(ui->catalog, ui->view.selected),
                                      ui->snapshot            ? MAINUI_PREVIEW_WAIT_FOREVER
                                      : ui->preview_sync_once ? 80
                                                              : 0);
        ui->preview_sync_once = false;
        int list_origin = 0;
        int list_width = 640;
        int outer = ui->config.rows <= 9 ? 20 : (ui->config.rows >= 14 ? 15 : 29 - ui->config.rows);
        int gap = ui->config.rows <= 7 ? 15 : (ui->config.rows >= 17 ? 5 : 22 - ui->config.rows);
        Uint32 elapsed = ui->snapshot ? ui->snapshot_elapsed : SDL_GetTicks() - ui->selected_at;
        for (int i = 0; i < ui->config.rows && ui->view.start + i < ui->view.total; i++) {
            int y = 60 + i * ui->config.row_height;
            SDL_Rect clip = {(Sint16)list_origin, (Sint16)y, (Uint16)list_width,
                             (Uint16)ui->config.row_height};
            SDL_SetClipRect(ui->screen, &clip);
            bool selected = ui->view.start + i == ui->view.selected;
            if (selected) {
                if (ui->theme.selection) {
                    blit(ui->theme.selection, ui->screen, list_origin, y);
                }
                else {
                    SDL_FillRect(ui->screen, &clip, SDL_MapRGB(ui->screen->format, 68, 68, 68));
                }
            }
            bool folder_row =
                ui->library ? mainui_library_is_folder(ui->library, ui->view.start + i)
                            : ui->catalog && mainui_browser_folder(ui->catalog, ui->view.start + i);
            SDL_Surface *row_icon = folder_row ? ui->theme.folder : ui->theme.icon;
            /* Transparent ui->theme spacers still have a meaningful width.
         * Reserving a synthetic 71px slot indented Silky by 70px. */
            int icon_width = row_icon ? row_icon->w : 0;
            int icon_x = list_origin + (ui->theme.icon_margin >= 0 ? ui->theme.icon_margin : 5);
            int text_x = row_icon ? icon_x + icon_width + gap : list_origin + outer;
            int available = list_width - outer - text_x;
            if (ui->favorite_rows[i] &&
                (!ui->library || ui->library->recent || ui->search.results) && ui->theme.favorite &&
                !folder_row) {
                bool spacer = ui->theme.icon && ui->theme.icon->w >= 120 &&
                              ui->theme.icon->w >= 3 * ui->theme.icon->h;
                int origin = spacer || (ui->config.dynamic_favorite_position && !ui->preview.image)
                                 ? 0
                                 : 250;
                int marker_x = 640 - ui->theme.favorite->w - origin - 20;
                blit(ui->theme.favorite, ui->screen, marker_x,
                     y + (ui->config.row_height - ui->theme.favorite->h) / 2);
                if (marker_x - 6 - text_x < available) {
                    available = marker_x - 6 - text_x;
                }
            }
            if (available < 1) {
                available = 1;
            }
            /* Preview opacity belongs to the later compositor, not the text
         * clip. Only overflow activation uses the live ui->preview edge. */
            int activation_width = available;
            int pane_width = mainui_preview_edge(&ui->theme) - text_x;
            if (ui->preview.image && pane_width > 0 && pane_width < activation_width) {
                activation_width = pane_width;
            }
            if (row_icon) {
                SDL_Rect source = {0, 0, (Uint16)icon_width, (Uint16)ui->config.row_height};
                if (source.w > row_icon->w) {
                    source.w = (Uint16)row_icon->w;
                }
                if (source.h > row_icon->h) {
                    source.h = (Uint16)row_icon->h;
                }
                source.y = (Sint16)((row_icon->h - source.h) / 2);
                SDL_Rect dest = {(Sint16)icon_x,
                                 (Sint16)(y + (ui->config.row_height - source.h) / 2), 0, 0};
                SDL_BlitSurface(row_icon, &source, ui->screen, &dest);
            }
            SDL_Surface *label =
                !selected && ui->highlighted[i] ? ui->highlighted[i] : ui->labels[i];
            if (!label) {
                continue;
            }
            int label_y = y + (ui->config.row_height - label->h) / 2;
            ui->animate = ui->animate || (selected && label->w > activation_width &&
                                          ui->config.scroll_status == 2);
            if (selected && ui->config.scroll_status == 2 &&
                elapsed >= (unsigned)ui->config.scroll_delay && label->w > activation_width) {
                MainUIBlit segments[2];
                int n =
                    mainui_marquee_stream(elapsed - (unsigned)ui->config.scroll_delay,
                                          ui->config.scroll_speed, label->w, available, segments);
                for (int part = 0; part < n; part++) {
                    SDL_Rect source = {(Sint16)segments[part].source_x, 0,
                                       (Uint16)segments[part].width, (Uint16)label->h};
                    SDL_Rect dest = {(Sint16)(text_x + segments[part].destination_x),
                                     (Sint16)label_y, 0, 0};
                    SDL_BlitSurface(label, &source, ui->screen, &dest);
                }
            }
            else {
                SDL_Rect source = {0, 0, (Uint16)available, (Uint16)label->h};
                SDL_Rect dest = {(Sint16)text_x, (Sint16)label_y, 0, 0};
                SDL_BlitSurface(label, &source, ui->screen, &dest);
            }
        }
        /* Row clips must never leak into the next frame's header/background. */
        SDL_SetClipRect(ui->screen, NULL);
        mainui_preview_draw(&ui->preview, &ui->theme, ui->screen);
        if (!ui->view.total ||
            (ui->catalog && !ui->library && ui->catalog->depth > 1 && ui->view.total == 1) ||
            (ui->library && ui->library->current >= 0 && ui->library->visible_count == 1 &&
             ui->library->visible[0] == INT_MIN)) {
            mainui_draw_empty(ui->screen, &ui->theme);
        }
        draw_list_counter(ui->screen, &ui->theme, ui->library, ui->catalog, &ui->view);
    }
    if (ui->context_open) {
        mainui_draw_context(ui->screen, &ui->theme, &ui->context);
    }
    if (ui->confirmation >= 0) {
        mainui_draw_confirmation(ui->screen, &ui->theme,
                                 ui->confirmation == CONTEXT_CLEAR_RECENT ? 77
                                 : ui->confirmation == CONTEXT_DELETE_ROM ? 116
                                                                          : 86,
                                 ui->confirmation == CONTEXT_CLEAR_RECENT ? 78
                                 : ui->confirmation == CONTEXT_DELETE_ROM ? 117
                                                                          : 108);
    }
    if (ui->name_input.open) {
        mainui_name_input_draw(&ui->name_input, ui->screen, &ui->theme);
    }
    if (*ui->message_title) {
        mainui_draw_message(ui->screen, &ui->theme, ui->message_title, ui->message_body);
    }
    if (ui->catalog_job.thread && !atomic_load(&ui->catalog_job.done) &&
        (SDL_GetTicks() - ui->catalog_job.started_at >= 500 ||
         atomic_load(&ui->catalog_job.cancel))) {
        mainui_draw_catalog_loading(ui->screen, &ui->theme, atomic_load(&ui->catalog_job.cancel));
    }
    if (ui->snapshot && !ui->catalog_job.thread && !ui->device_job.thread &&
        !(ui->about_visible && ui->about_job.thread) &&
        (ui->launch_pending ||
         (!ui->letter_jump.active && (!ui->input_script || !*ui->input_script)))) {
        if (SDL_SaveBMP(ui->screen, ui->snapshot) < 0) {
            fprintf(stderr, "Snapshot: %s\n", SDL_GetError());
            ui->status = 4;
        }
        ui->running = false;
        return false;
    }
    if (ui->real_device) {
        /* Copy inverted pixels directly to the display; the logical frame
         * remains upright for retained drawing and snapshots. */
        mainui_blit_rotated(ui->screen, ui->display);
    }
    if (SDL_Flip(ui->display) == 0) {
        mainui_mark(MAINUI_MARK_FIRST_FRAME);
        mainui_count_add("frames", 1);
    }
    return true;
}
