#include <stdio.h>
#include <string.h>
#include "ui_pages_internal.h"

#define LOG_CHUNK 12

static void log_nth_segment(size_t seg_index, int *out_num, int *out_first, char *out_chunk, size_t chunk_sz)
{
    if (out_num) {
        *out_num = 0;
    }
    if (out_first) {
        *out_first = 0;
    }
    if (out_chunk && chunk_sz) {
        out_chunk[0] = '\0';
    }
    size_t walk = 0;
    size_t n = ui_log_count();
    for (size_t e = 0; e < n; e++) {
        char b[64];
        ui_log_get_recent(e, b, sizeof(b));
        size_t L = strlen(b);
        int label = (int)e + 1;
        if (L == 0) {
            if (walk == seg_index) {
                if (out_num) {
                    *out_num = label;
                }
                if (out_first) {
                    *out_first = 1;
                }
                if (out_chunk && chunk_sz > 1) {
                    out_chunk[0] = '-';
                    out_chunk[1] = '\0';
                }
                return;
            }
            walk++;
            continue;
        }
        size_t off = 0;
        while (off < L) {
            if (walk == seg_index) {
                if (out_num) {
                    *out_num = label;
                }
                if (out_first) {
                    *out_first = (off == 0) ? 1 : 0;
                }
                if (out_chunk && chunk_sz) {
                    size_t take = L - off;
                    if (take > LOG_CHUNK) {
                        take = LOG_CHUNK;
                    }
                    if (take >= chunk_sz) {
                        take = chunk_sz - 1;
                    }
                    memcpy(out_chunk, b + off, take);
                    out_chunk[take] = '\0';
                }
                return;
            }
            off += LOG_CHUNK;
            walk++;
        }
    }
}

void ui_page_build_logs(const ui_state_t *state, ui_frame_t *frame)
{
    ui_pages_header_title(frame, "LOGS");
    ui_pages_set_linef(frame->rows[1], "Count:%u", (unsigned)ui_log_count());
    size_t max_scroll = 0;
    size_t nseg = 0;
    size_t n = ui_log_count();
    for (size_t e = 0; e < n; e++) {
        char b[64];
        ui_log_get_recent(e, b, sizeof(b));
        size_t L = strlen(b);
        if (L == 0) {
            nseg++;
        } else {
            nseg += (L + LOG_CHUNK - 1) / LOG_CHUNK;
        }
    }
    if (nseg > 6) {
        max_scroll = nseg - 6;
    }
    size_t scroll = state->scroll;
    if (scroll > max_scroll) {
        scroll = max_scroll;
    }
    for (int i = 0; i < 6; i++) {
        int num = 0;
        int first = 0;
        char ch[16];
        log_nth_segment(scroll + (size_t)i, &num, &first, ch, sizeof(ch));
        if (num <= 0) {
            ui_pages_set_line(frame->rows[i + 2], "");
        } else {
            char row[24];
            if (first) {
                int nw = snprintf(row, sizeof(row), "%d ", num);
                if (nw > 0 && nw < (int)sizeof(row)) {
                    size_t room = sizeof(row) - 1 - (size_t)nw;
                    size_t cl = strlen(ch);
                    if (cl > room) {
                        cl = room;
                    }
                    memcpy(row + nw, ch, cl);
                    row[nw + cl] = '\0';
                }
            } else {
                size_t cl = strlen(ch);
                if (cl > 13) {
                    cl = 13;
                }
                row[0] = ' ';
                row[1] = ' ';
                row[2] = ' ';
                memcpy(row + 3, ch, cl);
                row[3 + cl] = '\0';
            }
            ui_pages_set_line(frame->rows[i + 2], row);
        }
    }
    frame->invert_row = state->cursor == 0 ? 0 : 1;
}
