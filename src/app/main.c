/* SPDX-License-Identifier: GPL-3.0-only
 * Shared entry point for the Onion device launcher and host development preview.
 */
#include "app/loop.h"
#include "app/render.h"
#include "app/screen_events.h"
#include "app/startup.h"
#ifdef main
#undef main
#endif
#include "platform/timing.h"
#include <stdlib.h>
#include <unistd.h>

/* The device wrapper creates this marker before exec. Removing it after the
 * first drawn frame tells the next wrapper start that this one did not fail
 * during startup. Unset (host, tests): nothing to do. */
static void clear_start_marker(void)
{
    const char *marker = getenv("MAINUI_START_MARKER");
    if (marker && *marker) {
        unlink(marker);
    }
}

static int run(int argc, char **argv, MainUIApp *ui)
{
    mainui_mark(MAINUI_MARK_ENTRY);
    int result = mainui_setup_session(ui, argc, argv);
    mainui_mark(MAINUI_MARK_SESSION);
    if (result >= 0) {
        return result;
    }
    if (ui->real_device && !ui->snapshot) {
        mainui_timing_handoff("/tmp/mainui-exit");
    }
    result = mainui_setup_video(ui);
    mainui_mark(MAINUI_MARK_VIDEO);
    if (result >= 0) {
        return result;
    }
    mainui_restore_session(ui);
    mainui_mark(MAINUI_MARK_RESTORE);
    mainui_setup_render(ui);
    mainui_mark(MAINUI_MARK_READY);
    bool first_frame_done = false;
    while (ui->running) {
        if (!mainui_poll_jobs(ui) || !mainui_reap_jobs(ui)) {
            break;
        }
        mainui_prepare_frame(ui);
        if (ui->draw_frame) {
            struct timespec draw_start = mainui_timing_start();
            bool drawn = mainui_draw_frame(ui);
            /* finish accumulates through mainui_count_add, with no clock reads
             * when logging is disabled. Include rotation and SDL_Flip. */
            mainui_timing_finish("draw-ms", draw_start);
            if (!drawn) {
                continue;
            }
            if (!first_frame_done) {
                first_frame_done = true;
                clear_start_marker();
            }
        }
        SDL_Event event;
        if (!mainui_wait_event(ui, &event)) {
            break;
        }
        mainui_dispatch_event(ui, &event);
    }
    return mainui_teardown(ui);
}

int main(int argc, char **argv)
{
    MainUIApp *ui = calloc(1, sizeof *ui);
    if (!ui) {
        return 3;
    }
    int result = run(argc, argv, ui);
    free(ui);
    /* Also report early initialization failures; completed teardown reports once. */
    mainui_mark(MAINUI_MARK_EXIT);
    mainui_timing_report();
    return result;
}
