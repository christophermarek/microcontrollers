#include <stdio.h>
#include <string.h>
#include "ui_pages_internal.h"

static size_t log_first_maxw(int label)
{
    char t[12];
    int n = snprintf(t, sizeof(t), "%d ", label);
    if (n < 0) {
        n = 0;
    }
    size_t w = 16 - (size_t)n;
    if (w < 6) {
        w = 6;
    }
    return w;
}

static void log_wrap_step(const char *b, size_t L, size_t *poff, size_t maxw, char *out, size_t out_cap)
{
    size_t off = *poff;
    while (off < L && b[off] == ' ') {
        off++;
    }
    if (off >= L) {
        *poff = L;
        if (out && out_cap) {
            out[0] = '\0';
        }
        return;
    }
    size_t rem = L - off;
    size_t take;
    size_t next;
    if (rem <= maxw) {
        take = rem;
        next = L;
    } else {
        size_t sp = off + maxw;
        while (sp > off && b[sp - 1] != ' ') {
            sp--;
        }
        if (sp == off) {
            take = maxw;
            next = off + maxw;
        } else {
            take = sp - off;
            while (take > 0 && b[off + take - 1] == ' ') {
                take--;
            }
            if (take == 0) {
                take = maxw;
                next = off + maxw;
            } else {
                next = off + take;
                while (next < L && b[next] == ' ') {
                    next++;
                }
            }
        }
    }
    if (out && out_cap) {
        if (take > out_cap - 1) {
            take = out_cap - 1;
        }
        memcpy(out, b + off, take);
        out[take] = '\0';
    }
    *poff = next;
}

size_t ui_logs_wrap_segment_count(void)
{
    size_t n = ui_log_count();
    size_t walk = 0;
    for (size_t e = 0; e < n; e++) {
        char b[64];
        ui_log_get_recent(e, b, sizeof(b));
        size_t L = strlen(b);
        int label = (int)e + 1;
        if (L == 0) {
            walk++;
            continue;
        }
        size_t off = 0;
        int first = 1;
        while (off < L) {
            size_t mw = first ? log_first_maxw(label) : 13;
            char tmp[20];
            size_t before = off;
            log_wrap_step(b, L, &off, mw, tmp, sizeof(tmp));
            if (off == before && tmp[0] == '\0') {
                break;
            }
            walk++;
            first = 0;
        }
    }
    return walk;
}

void ui_logs_wrap_segment_at(size_t seg_index, int *out_num, int *out_first, char *out_chunk, size_t chunk_sz)
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
            }
            walk++;
            continue;
        }
        size_t off = 0;
        int first = 1;
        while (off < L) {
            size_t mw = first ? log_first_maxw(label) : 13;
            char tmp[20];
            size_t seg_off = off;
            log_wrap_step(b, L, &off, mw, tmp, sizeof(tmp));
            if (off == seg_off && tmp[0] == '\0') {
                break;
            }
            if (walk == seg_index) {
                if (out_num) {
                    *out_num = label;
                }
                if (out_first) {
                    *out_first = seg_off == 0 ? 1 : 0;
                }
                if (out_chunk && chunk_sz) {
                    size_t tl = strlen(tmp);
                    if (tl >= chunk_sz) {
                        tl = chunk_sz - 1;
                    }
                    memcpy(out_chunk, tmp, tl);
                    out_chunk[tl] = '\0';
                }
                return;
            }
            walk++;
            first = 0;
        }
    }
}

void ui_page_build_logs(const ui_state_t *state, ui_frame_t *frame)
{
    ui_pages_header_title_time(frame, "LOGS");
    ui_pages_set_linef(frame->rows[1], "Count:%u", (unsigned)ui_log_count());
    size_t nseg = ui_logs_wrap_segment_count();
    size_t max_scroll = nseg > 6 ? nseg - 6 : 0;
    size_t scroll = state->scroll;
    if (scroll > max_scroll) {
        scroll = max_scroll;
    }
    for (int i = 0; i < 6; i++) {
        int num = 0;
        int first = 0;
        char ch[20];
        ui_logs_wrap_segment_at(scroll + (size_t)i, &num, &first, ch, sizeof(ch));
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
