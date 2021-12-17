#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <vterm.h>

static struct vterm_attrib default_attrib = {
    0, VTERM_COLOR_BLK, VTERM_COLOR_WHT
};

static void vterm_clear(struct vterm *vt, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1)
{
    unsigned int i, beg, end;
    struct vterm_cell *cell;
    beg = x0 + (y0 * vt->mode.scr_w);
    end = x1 + (y1 * vt->mode.scr_h);
    for(i = beg; i < end; i++) {
        cell = vt->buffer + i;
        cell->attrib = default_attrib;
        cell->chr = VTERM_CHR_NUL;
        if(vt->callbacks.draw_cell)
            vt->callbacks.draw_cell(vt, cell->chr, i % vt->mode.scr_w, i / vt->mode.scr_w, &cell->attrib);
    }
}

static void vterm_scroll(struct vterm *vt, unsigned int nl)
{
    unsigned int i, line, end;
    struct vterm_cell *cell;

    if(nl > vt->mode.scr_h)
        nl = vt->mode.scr_h;
    line = vt->mode.scr_h - nl;
    end = vt->mode.scr_w * line;

    for(i = 0; i < end; i++) {
        cell = vt->buffer + i;
        memcpy(cell, cell + vt->mode.scr_w, sizeof(struct vterm_cell));
        if(vt->callbacks.draw_cell)
            vt->callbacks.draw_cell(vt, cell->chr, i % vt->mode.scr_w, i / vt->mode.scr_w, &cell->attrib);
    }

    for(i = 0; i < vt->mode.scr_w; i++) {
        cell = vt->buffer + end + i;
        cell->attrib = default_attrib;
        cell->chr = VTERM_CHR_NUL;
        if(vt->callbacks.draw_cell)
            vt->callbacks.draw_cell(vt, cell->chr, i % vt->mode.scr_w, i / vt->mode.scr_w, &cell->attrib);
    }

    vt->cursor.y -= (vt->cursor.y >= nl) ? nl : vt->cursor.y;
    if(vt->callbacks.set_cursor)
        vt->callbacks.set_cursor(vt, &vt->cursor);
}

static void vterm_newline(struct vterm *vt, int cr)
{
    if(cr)
        vt->cursor.x = 0;
    vt->cursor.y++;

    if(vt->cursor.y == vt->mode.scr_h) {
        if(vt->mode.scroll) {
            vterm_scroll(vt, 1);
            return;
        }

        vterm_clear(vt, 0, 0, vt->mode.scr_w, vt->mode.scr_h - 1);
        vt->cursor.x = vt->cursor.y = 0;
        goto set_cursor;
    }

set_cursor:
    if(vt->callbacks.set_cursor)
        vt->callbacks.set_cursor(vt, &vt->cursor);
}

static void vterm_print(struct vterm *vt, int chr)
{
    unsigned int i, tab;
    struct vterm_cell *cell;
    switch(chr) {
        case VTERM_CHR_BS:
            if(vt->cursor.x >= 1) {
                vt->cursor.x--;
                if(vt->callbacks.set_cursor)
                    vt->callbacks.set_cursor(vt, &vt->cursor);
            }
            break;
        case VTERM_CHR_HT:
            tab = 4 - (vt->cursor.x % 4);
            for(i = 0; i < tab; i++)
                vterm_print(vt, ' ');
            break;
        case VTERM_CHR_LF:
            vterm_newline(vt, 1);
            break;
        case VTERM_CHR_VT:
            vterm_newline(vt, 0);
            break;
        case VTERM_CHR_FF:
            vterm_clear(vt, 0, 0, vt->mode.scr_w, vt->mode.scr_h - 1);
            vt->cursor.x = vt->cursor.y = 0;
            if(vt->callbacks.set_cursor)
                vt->callbacks.set_cursor(vt, &vt->cursor);
            break;
        case VTERM_CHR_CR:
            vt->cursor.x = 0;
            if(vt->callbacks.set_cursor)
                vt->callbacks.set_cursor(vt, &vt->cursor);
            break;
        case VTERM_CHR_BEL:
        case VTERM_CHR_DEL:
            if(vt->callbacks.ascii)
                vt->callbacks.ascii(vt, chr);
            break;
        default:
            if(vt->cursor.x >= vt->mode.scr_w)
                vterm_newline(vt, 1);
            cell = vt->buffer + vt->cursor.x + (vt->cursor.y * vt->mode.scr_w);
            cell->attrib = vt->current_attrib;
            cell->chr = chr;
            if(vt->callbacks.set_cursor)
                vt->callbacks.set_cursor(vt, &vt->cursor);
            if(vt->callbacks.draw_cell)
                vt->callbacks.draw_cell(vt, cell->chr, vt->cursor.x++, vt->cursor.y, &cell->attrib);
            break;
    }
}

/* Cursor X.
 * Moves the cursor. */
static void vterm_csi_cux(struct vterm *vt, int dc)
{
    unsigned int attrib = vt->parser.argv_val[0];
    if(!vt->parser.argv_map[0] || !attrib)
        attrib = 1;
    int dir = (dc == 'B' || dc == 'C') ? 1 : -1;
    int *pp = (dc == 'A' || dc == 'B') ? &vt->cursor.y : &vt->cursor.x;
    int max = (dc == 'A' || dc == 'B') ? vt->mode.scr_h : vt->mode.scr_w;
    int val = *pp + dir * attrib;
    if(val < 0)
        val = 0;
    if(val > max)
        val = max;
    *pp = val;
    if(vt->callbacks.set_cursor)
        vt->callbacks.set_cursor(vt, &vt->cursor);
}

/* Cursor position.
 * Moves the cursor. */
static void vterm_csi_cup(struct vterm *vt)
{
    unsigned int x = 1, y = 1;
    if(vt->parser.argv_map[0] && vt->parser.argv_val[0])
        x = vt->parser.argv_val[0];
    if(vt->parser.argv_map[1] && vt->parser.argv_val[1])
        y = vt->parser.argv_val[1];
    if(x > vt->mode.scr_w)
        x = vt->mode.scr_w;
    if(y >= vt->mode.scr_h)
        y = vt->mode.scr_h;
    vt->cursor.x = x - 1;
    vt->cursor.y = y - 1;
    if(vt->callbacks.set_cursor)
        vt->callbacks.set_cursor(vt, &vt->cursor);
}

/* Erase in Display.
 * Clears a part of the screen. */
static void vterm_csi_ed(struct vterm *vt)
{
    unsigned int attrib = vt->parser.argv_val[0];
    if(!vt->parser.argv_map[0])
        attrib = 0;
    switch(attrib) {
        case 0:
            vterm_clear(vt, vt->cursor.x, vt->cursor.y, vt->mode.scr_w, vt->mode.scr_h - 1);
            break;
        case 1:
            vterm_clear(vt, 0, 0, vt->cursor.x, vt->cursor.y);
            break;
        case 2:
            vterm_clear(vt, 0, 0, vt->mode.scr_w, vt->mode.scr_h - 1);
            break;
    }
}

/* Erase in Line.
 * Erases a part of the line. */
static void vterm_csi_el(struct vterm *vt)
{
    unsigned int arg = vt->parser.argv_val[0];
    if(!vt->parser.argv_map[0])
        arg = 0;
    switch(arg) {
        case 0:
            vterm_clear(vt, vt->cursor.x, vt->cursor.y, vt->mode.scr_w, vt->cursor.y);
            break;
        case 1:
            vterm_clear(vt, 0, vt->cursor.y, vt->cursor.x, vt->cursor.y);
            break;
        case 2:
            vterm_clear(vt, 0, vt->cursor.y, vt->mode.scr_w, vt->cursor.y);
            break;
    }
}

/* Select Graphic Rendition.
 * Sets colors and style of the characters. */
static void vterm_csi_sgr(struct vterm *vt)
{
    int reset_color;
    unsigned int i, arg, color;
    for(i = 0; i < vt->parser.argp; i++) {
        if(!vt->parser.argv_map[i] || !vt->parser.argv_val[i]) {
            vt->current_attrib = default_attrib;
            continue;
        }

        arg = vt->parser.argv_val[i];

        switch(arg) {
            case 1:
                vt->current_attrib.attr |= VTERM_ATTR_BOLD;
                break;
            case 2:
                vt->current_attrib.attr &= ~VTERM_ATTR_BOLD;
                vt->current_attrib.attr |= VTERM_ATTR_DIM;
                break;
            case 3:
                vt->current_attrib.attr |= VTERM_ATTR_ITALIC;
                break;
            case 4:
                vt->current_attrib.attr |= VTERM_ATTR_UNDERLN;
                break;
            case 5:
                vt->current_attrib.attr &= ~VTERM_ATTR_FASTBLNK;
                vt->current_attrib.attr |= VTERM_ATTR_SLOWBLNK;
                break;
            case 6:
                vt->current_attrib.attr &= ~VTERM_ATTR_SLOWBLNK;
                vt->current_attrib.attr |= VTERM_ATTR_FASTBLNK;
                break;
            case 7:
                vt->current_attrib.attr |= VTERM_ATTR_INVERT;
                break;
            case 8:
                vt->current_attrib.attr |= VTERM_ATTR_HIDDEN;
                break;
            case 9:
                vt->current_attrib.attr |= VTERM_ATTR_STRIKE;
                break;
            case 21:
                vt->current_attrib.attr |= VTERM_ATTR_UNDERLN;
                vt->current_attrib.attr |= VTERM_ATTR_UNDERLN2;
                break;
            case 22:
                vt->current_attrib.attr &= ~VTERM_ATTR_BOLD;
                vt->current_attrib.attr &= ~VTERM_ATTR_DIM;
                break;
            case 23:
                vt->current_attrib.attr &= ~VTERM_ATTR_ITALIC;
                break;
            case 24:
                vt->current_attrib.attr &= ~VTERM_ATTR_UNDERLN;
                vt->current_attrib.attr &= ~VTERM_ATTR_UNDERLN2;
                break;
            case 25:
                vt->current_attrib.attr &= ~VTERM_ATTR_SLOWBLNK;
                vt->current_attrib.attr &= ~VTERM_ATTR_FASTBLNK;
                break;
            case 27:
                vt->current_attrib.attr &= ~VTERM_ATTR_INVERT;
                break;
            case 28:
                vt->current_attrib.attr &= ~VTERM_ATTR_HIDDEN;
                break;
            case 29:
                vt->current_attrib.attr &= ~VTERM_ATTR_STRIKE;
                break;
        }

        if(arg >= 90 && arg <= 107)
            vt->current_attrib.attr |= VTERM_ATTR_BRIGHT;

        color = (arg % 10);
        reset_color = (color == 9);
        color %= 8;
        switch(arg / 10) {
            case 3:
            case 9:
                vt->current_attrib.fg = reset_color ? default_attrib.fg : color;
                break;
            case 4:
            case 10:
                vt->current_attrib.bg = reset_color ? default_attrib.bg : color;
                break;
        }
    }
}

static void vterm_setmode(struct vterm *vt)
{
    vt->callbacks.mem_free(vt->buffer);
    vt->buffer = vt->callbacks.mem_alloc(vt->mode.scr_w * vt->mode.scr_h);
    vt->cursor.x = vt->cursor.y = 0;
    if(vt->callbacks.set_cursor)
        vt->callbacks.set_cursor(vt, &vt->cursor);
    vterm_clear(vt, 0, 0, vt->mode.scr_w, vt->mode.scr_h - 1);
}

static void vterm_csi_mode(struct vterm *vt)
{
    unsigned int mode = 0;
    if(vt->parser.prefix_chr == '=') {
        if(vt->parser.argv_map[0])
            mode = vt->parser.argv_val[0];
        switch(mode) {
            case 0:
                vt->mode.scr_w = 40;
                vt->mode.scr_h = 25;
                vt->mode.color = 0;
                break;
            case 1:
                vt->mode.scr_w = 40;
                vt->mode.scr_h = 25;
                vt->mode.color = 1;
                break;
            case 2:
                vt->mode.scr_w = 80;
                vt->mode.scr_h = 25;
                vt->mode.color = 0;
                break;
            case 3:
                vt->mode.scr_w = 80;
                vt->mode.scr_h = 25;
                vt->mode.color = 1;
                break;
        }

        vterm_setmode(vt);
    }
}

static void vterm_putchar(struct vterm *vt, int c)
{
    unsigned int arg;

    if(vt->parser.state == VTERM_STATE_ESCAPE) {
        if(c != VTERM_CHR_ESC) {
            vterm_print(vt, c);
            goto done;
        }

        vt->parser.state = VTERM_STATE_BRACKET;
        vt->parser.argp = 0;
        vt->parser.argv_val[vt->parser.argp] = 0;
        vt->parser.argv_map[vt->parser.argp] = 0;
        goto done;
    }

    if(vt->parser.state == VTERM_STATE_BRACKET) {
        if(c != '[') {
            vt->parser.prefix_chr = 0;
            vt->parser.state = VTERM_STATE_ESCAPE;
            vterm_print(vt, c);
            goto done;
        }

        vt->parser.state = VTERM_STATE_ATTRIB;
        goto done;
    }

    if(vt->parser.state == VTERM_STATE_ATTRIB) {
        if(c == '<' || c == '=' || c == '>' || c == '?' || c == '=') {
            vt->parser.prefix_chr = c;
            goto done;
        }

        if(isdigit(c)) {
            vt->parser.argv_val[vt->parser.argp] *= 10;
            vt->parser.argv_val[vt->parser.argp] += (c - '0');
            vt->parser.argv_map[vt->parser.argp] = 1;
        }
        else {
            if(vt->parser.argp < VTERM_MAX_ARGS)
                vt->parser.argp++;
            vt->parser.argv_val[vt->parser.argp] = 0;
            vt->parser.argv_map[vt->parser.argp] = 0;
            vt->parser.state = VTERM_STATE_ENDVAL;
        }

        goto done;
    }

done:
    if(vt->parser.state == VTERM_STATE_ENDVAL) {
        if(c == ';') {
            vt->parser.state = VTERM_STATE_ATTRIB;
            return;
        }

        switch(c) {
            case 'A': case 'B': case 'C': case 'D':
                vterm_csi_cux(vt, c);
                break;
            case 'G':
                arg = vt->parser.argv_val[0];
                if(!vt->parser.argv_map[0] || !arg)
                    arg = 1;
                if(arg >= vt->mode.scr_w)
                    arg = vt->mode.scr_h - 1;
                vt->cursor.x = arg;
                if(vt->callbacks.set_cursor)
                    vt->callbacks.set_cursor(vt, &vt->cursor);
                break;
            case 'H':
                vterm_csi_cup(vt);
                break;
            case 'J':
                vterm_csi_ed(vt);
                break;
            case 'K':
                vterm_csi_el(vt);
                break;
            case 'T':
                arg = vt->parser.argv_val[0];
                if(!vt->parser.argv_map[0] || !arg)
                    arg = 1;
                vterm_scroll(vt, arg);
                break;
            case 'm':
                vterm_csi_sgr(vt);
                break;
            case 'h':
                vterm_csi_mode(vt);
                break;
            default:
                /* Allows different implementations to parse
                 * standard and private control sequences like
                 * VT220's cursor ops in a VGA textmode driver. */
                if(vt->callbacks.misc_sequence)
                    vt->callbacks.misc_sequence(vt, c);
                break;
        }

        vt->parser.prefix_chr = 0;
        vt->parser.state = VTERM_STATE_ESCAPE;
        return;
    }
}

int vterm_init(struct vterm *vt, const struct vterm_callbacks *callbacks, void *user)
{
    memset(vt, 0, sizeof(struct vterm));

    memcpy(&vt->callbacks, callbacks, sizeof(struct vterm_callbacks));
    if(!vt->callbacks.mem_alloc || !vt->callbacks.mem_free)
        return 0;

    vt->user = user;

    /* Default mode: text 80x25 color */
    vt->mode.scr_w = 80;
    vt->mode.scr_h = 25;
    vt->mode.color = 1;
    vt->mode.scroll = 1;

    vt->parser.argp = 0;
    vt->parser.prefix_chr = VTERM_CHR_NUL;
    vt->parser.state = VTERM_STATE_ESCAPE;

    vt->current_attrib = default_attrib;

    memset(vt->parser.argv_map, 0, sizeof(vt->parser.argv_map));
    memset(vt->parser.argv_val, 0, sizeof(vt->parser.argv_val));

    vterm_setmode(vt);

    return 1;
}

void vterm_shutdown(struct vterm *vt)
{
    vt->callbacks.mem_free(vt->buffer);
    memset(vt, 0, sizeof(struct vterm));
}

int vterm_write(struct vterm *vt, const void *s, size_t n)
{
    const char *sp = s;
    while(n--)
        vterm_putchar(vt, *sp++);
    return 1;
}
