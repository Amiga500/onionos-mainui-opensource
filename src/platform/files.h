/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_FILES_H
#define MAINUI_FILES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t size, modified, identity;
    bool exists;
} MainUIFileStamp;

MainUIFileStamp mainui_file_stamp(const char *path);
bool mainui_file_stamp_equal(MainUIFileStamp a, MainUIFileStamp b);
bool mainui_sync_parent(const char *path);
/* removed remains true when unlink succeeds but the directory sync fails. */
int mainui_remove_file_status(const char *path, bool *removed);
int mainui_remove_file(const char *path); /* remove semantics, UTF-8 paths */
bool mainui_temporary_path(char out[4096], const char *target, const char *tag);
#ifdef MAINUI_TEST_FAULTS
void mainui_test_fault(const char *point);
#endif
typedef struct MainUIFileLock MainUIFileLock;
/* Nonblocking cooperative lock, keyed by the target path in /tmp/mainui-locks.
 * Callers must use a consistent normalized absolute target path.
 * The lock inode remains; process termination releases the OS lock.
 * Never infer ownership from file existence alone. */
MainUIFileLock *mainui_file_lock(const char *target);
void mainui_file_unlock(MainUIFileLock *lock);
/* Read bounded text into an owned NUL-terminated buffer; free with free().
 * Return NULL on I/O, size or allocation failure. No file writes.
 */
char *mainui_read_text(const char *path, size_t max_bytes);
/* Publish through an exclusive .writing.pid.serial sibling. A post-publication
 * directory-flush failure reports false even though a complete new file is visible. */
bool mainui_write_text_atomic(const char *path, const char *text);
/* Publish complete bytes without replacing an existing destination. The
 * exclusive sibling staging reservation and destination are never truncated. */
bool mainui_write_bytes_new(const char *path, const void *data, size_t size);
/* published remains true if publication succeeded but directory sync failed.
 * Use it, rather than the return value, to track cleanup ownership. */
bool mainui_write_bytes_new_status(const char *path, const void *data, size_t size,
                                   bool *published);
bool mainui_move_file_new(const char *source, const char *destination);
/* FAT-safe publication/move: caller must hold an exclusive cooperative lock.
 * The absence check followed by rename is NOT race-free without that lock.
 * A sync failure may report false after the rename has completed. */
bool mainui_write_bytes_new_locked(const char *path, const void *data, size_t size);
bool mainui_move_file_new_locked(const char *source, const char *destination);
/* Lexical containment for normalized paths; does not resolve symlinks. */
bool mainui_path_within(const char *path, const char *root);
bool mainui_regular_file_within(const char *path, const char *root);
bool mainui_directory_within(const char *path, const char *root);
#endif
