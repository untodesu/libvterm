#ifndef _VTERM_H_
#define _VTERM_H_ 1
#include <stddef.h>

#define VTERM_CHR_NUL (0x00) /* nothing          */
#define VTERM_CHR_BEL (0x07) /* audible signal   */
#define VTERM_CHR_BS  (0x08) /* backspace        */
#define VTERM_CHR_HT  (0x09) /* horizontal tab   */
#define VTERM_CHR_LF  (0x0A) /* linefeed/newline */
#define VTERM_CHR_VT  (0x0B) /* vertical tab     */
#define VTERM_CHR_FF  (0x0C) /* formfeed/newpage */
#define VTERM_CHR_CR  (0x0D) /* carriage return  */
#define VTERM_CHR_DEL (0x7F) /* delete character */
#define VTERM_CHR_ESC (0x1B) /* sequence start   */
#define VTERM_CHR_CSI (0x5B) /* control sequence */

#define VTERM_ATTR_BOLD     (1 << 0)
#define VTERM_ATTR_DIM      (1 << 1)
#define VTERM_ATTR_ITALIC   (1 << 2)
#define VTERM_ATTR_UNDERLN  (1 << 3)
#define VTERM_ATTR_SLOWBLNK (1 << 4)
#define VTERM_ATTR_FASTBLNK (1 << 5)
#define VTERM_ATTR_INVERT   (1 << 6)
#define VTERM_ATTR_HIDDEN   (1 << 7)
#define VTERM_ATTR_STRIKE   (1 << 8)
#define VTERM_ATTR_UNDERLN2 (1 << 9)
#define VTERM_ATTR_SUPERSCR (1 << 10)
#define VTERM_ATTR_UNDERSCR (1 << 11)
#define VTERM_ATTR_BRIGHT   (1 << 12)

#define VTERM_COLOR_BLK (0)
#define VTERM_COLOR_RED (1)
#define VTERM_COLOR_GRN (2)
#define VTERM_COLOR_YLW (3)
#define VTERM_COLOR_BLU (4)
#define VTERM_COLOR_MAG (5)
#define VTERM_COLOR_CYN (6)
#define VTERM_COLOR_WHT (7)
#define VTERM_COLOR_DEF (9)

#define VTERM_MODEF_COLOR  (1 << 0)
#define VTERM_MODEF_SCROLL (1 << 1)

#define VTERM_MAX_ARGS (8)
#define VTERM_MAX_CURS (8)

#define VTERM_STATE_ESCAPE  (0)
#define VTERM_STATE_BRACKET (1)
#define VTERM_STATE_ATTRIB  (2)
#define VTERM_STATE_ENDVAL  (3)

struct vterm;

struct vterm_attrib {
    unsigned int attr;
    unsigned int bg, fg;
};

struct vterm_cell {
    struct vterm_attrib attrib;
    int chr;
};

struct vterm_cursor {
    unsigned int x;
    unsigned int y;
};

struct vterm_mode {
    unsigned int flags;
    unsigned int scr_w;
    unsigned int scr_h;
};

struct vterm_callbacks {
    void *(*mem_alloc)(size_t n);
    void (*mem_free)(void *ptr);
    void (*misc_sequence)(const struct vterm *vt, int chr);
    void (*set_cursor)(const struct vterm *vt, const struct vterm_cursor *cursor);
    void (*mode_change)(const struct vterm *vt, const struct vterm_mode *new_mode);
    void (*draw_cell)(const struct vterm *vt, int chr, unsigned int x, unsigned int y, const struct vterm_attrib *attrib);
    void (*response)(const struct vterm *vt, int chr);
    void (*ascii)(const struct vterm *vt, int chr);
};

struct vterm_parser {
    int prefix_chr;
    unsigned int state, argp;
    unsigned int argv_val[VTERM_MAX_ARGS];
    unsigned int argv_map[VTERM_MAX_ARGS];
};

struct vterm {
    struct vterm_attrib current_attrib;
    struct vterm_callbacks callbacks;
    struct vterm_cell *buffer;
    struct vterm_cursor cursor;
    struct vterm_mode mode;
    struct vterm_parser parser;
    struct vterm_cursor curstack[VTERM_MAX_CURS];
    unsigned int curstack_sp;
    void *user;
};

int vterm_init(struct vterm *vt, const struct vterm_callbacks *callbacks, void *user);
void vterm_shutdown(struct vterm *vt);
int vterm_write(struct vterm *vt, const void *s, size_t n);

#endif
