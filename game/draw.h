/* The drawing of a sector, through the door in C of the SDL environment:
 * the tilemaps and the gradients, as the original draws them with its
 * SDL renderer (`TileMap::draw`, `Gradient::draw`, `Canvas`, and
 * `SDLPainter`, of SuperTux v0.7.0), in 32-bit floats.
 *
 * What is drawn is a list of drawables, in the order of the objects of
 * the sector, sorted by layer with a stable sort at each frame, as the
 * canvas of the original sorts its requests.
 *
 * The names have the prefix `td_`. */

#ifndef TD_DRAW_H
#define TD_DRAW_H

#include <math.h>

/* A frame of a tile: a region of an image (by index). */
typedef struct TdFrame { int img; int sx, sy, w, h; } TdFrame;

/* A tile: its frames, and the frames per second of its animation. */
typedef struct TdTile { int first, n; float fps; } TdTile;

static TdFrame* td_frames;
static int      td_frames_n, td_frames_cap;
static TdTile*  td_tiles;           /* by id */
static int      td_tiles_n;
static uint8_t* td_used;            /* by id: the tile is in a tilemap of the sector */

/* The images, by index: the values of the program, which keeps them
 * alive in a pinned table. */
static AVal*    td_imgs;
static int      td_imgs_n;

/* A tilemap to draw. */
typedef struct TdMap {
    uint32_t* tiles;                /* shared with the collisions, if solid */
    int   own;                      /* the tiles are of the drawing, to free */
    int   width, height;
    float speed_x, speed_y;
    float alpha;
    float tint[4];
    TxVec offset;
} TdMap;

/* A gradient: its colors, its direction. */
typedef struct TdGradient {
    float top[4], bottom[4];
    int   direction;                /* TD_VERTICAL, TD_HORIZONTAL */
} TdGradient;
enum { TD_HORIZONTAL=0, TD_VERTICAL=1, TD_HORIZONTAL_SECTOR=2, TD_VERTICAL_SECTOR=3 };

/* A drawable: what, which, its layer, and the order of its object. */
enum { TD_MAP=0, TD_GRADIENT=1 };
typedef struct TdItem { int kind, index, layer, order; } TdItem;

static TdMap*      td_maps;      static int td_maps_n, td_maps_cap;
static TdGradient* td_grads;     static int td_grads_n, td_grads_cap;
static TdItem*     td_items;     static int td_items_n, td_items_cap;
static TdItem*     td_sorted;

#define TD_PUSH(arr, n, cap) do { \
    if ((n) == (cap)) { (cap) = (cap) ? (cap)*2 : 16; (arr) = realloc((arr), (cap)*sizeof(*(arr))); } \
} while (0)

/* ------------------------------------------------------------------------ */
/* The tiles and their images                                                */
/* ------------------------------------------------------------------------ */

static void td_reset (int ntiles) {
    for (int i=0; i<td_maps_n; i++) {
        if (td_maps[i].own) free(td_maps[i].tiles);
    }
    free(td_tiles); free(td_used); free(td_frames);
    td_tiles = calloc(ntiles, sizeof(TdTile));
    td_used  = calloc(ntiles, 1);
    td_tiles_n = ntiles;
    td_frames = NULL; td_frames_n = td_frames_cap = 0;
    free(td_imgs); td_imgs = NULL; td_imgs_n = 0;
    free(td_maps); td_maps = NULL; td_maps_n = td_maps_cap = 0;
    free(td_grads); td_grads = NULL; td_grads_n = td_grads_cap = 0;
    free(td_items); td_items = NULL; td_items_n = td_items_cap = 0;
    free(td_sorted); td_sorted = NULL;
}

static int td_is_used (int id) {
    return (id >= 0 && id < td_tiles_n) ? td_used[id] : 0;
}

static void td_tile (int id, float fps) {
    td_tiles[id].first = td_frames_n;
    td_tiles[id].n = 0;
    td_tiles[id].fps = fps;
}

static void td_frame (int id, int img, int sx, int sy, int w, int h) {
    TD_PUSH(td_frames, td_frames_n, td_frames_cap);
    td_frames[td_frames_n++] = (TdFrame) { img, sx, sy, w, h };
    td_tiles[id].n++;
}

static void td_img (int index, AVal img) {
    if (index >= td_imgs_n) {
        td_imgs = realloc(td_imgs, (index+1) * sizeof(AVal));
        for (int i=td_imgs_n; i<=index; i++) td_imgs[i] = a_nil();
        td_imgs_n = index + 1;
    }
    td_imgs[index] = img;
}

/* ------------------------------------------------------------------------ */
/* The drawables                                                             */
/* ------------------------------------------------------------------------ */

static void td_item (int kind, int index, int layer) {
    TD_PUSH(td_items, td_items_n, td_items_cap);
    td_items[td_items_n] = (TdItem) { kind, index, layer, td_items_n };
    td_items_n++;
}

static void td_map (uint32_t* tiles, int own, int w, int h, int layer, float speed_x, float speed_y,
                    float alpha, float r, float g, float b, float a) {
    TD_PUSH(td_maps, td_maps_n, td_maps_cap);
    TdMap* m = &td_maps[td_maps_n];
    m->tiles = tiles; m->own = own; m->width = w; m->height = h;
    m->speed_x = speed_x; m->speed_y = speed_y; m->alpha = alpha;
    m->tint[0] = r; m->tint[1] = g; m->tint[2] = b; m->tint[3] = a;
    m->offset = tx_vec(0, 0);
    for (int i=0; i<w*h; i++) {
        if (tiles[i] < (uint32_t) td_tiles_n) td_used[tiles[i]] = 1;
    }
    td_item(TD_MAP, td_maps_n, layer);
    td_maps_n++;
}

static void td_gradient (float* top, float* bottom, int direction, int layer) {
    TD_PUSH(td_grads, td_grads_n, td_grads_cap);
    TdGradient* g = &td_grads[td_grads_n];
    memcpy(g->top, top, sizeof(g->top));
    memcpy(g->bottom, bottom, sizeof(g->bottom));
    g->direction = direction;
    td_item(TD_GRADIENT, td_grads_n, layer);
    td_grads_n++;
}

/* ------------------------------------------------------------------------ */
/* Drawing                                                                   */
/* ------------------------------------------------------------------------ */

/* SDLPainter::draw_texture, for one region, with no flip nor angle */
static void td_texture (SDL_Texture* tex, int sx, int sy, int sw, int sh,
                        float dx, float dy, float dw, float dh,
                        const float* color, float alpha) {
    SDL_Rect src = { sx, sy, sw, sh };
    SDL_FRect dst = { dx, dy, dw, dh };
    Uint8 r = (Uint8) (color[0] * 255);
    Uint8 g = (Uint8) (color[1] * 255);
    Uint8 b = (Uint8) (color[2] * 255);
    Uint8 a = (Uint8) (color[3] * alpha * 255);
    SDL_SetTextureColorMod(tex, r, g, b);
    SDL_SetTextureAlphaMod(tex, a);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_RenderCopyExF(sdl_ren(), tex, &src, &dst, 0.0, NULL, SDL_FLIP_NONE);
}

/* TileMap::draw, with the translation of the camera */
static void td_draw_map (const TdMap* m, TxVec translation, float screen_w, float screen_h) {
    if (m->alpha == 0.0f) return;
    const float trans_x = translation.x * m->speed_x;
    const float trans_y = translation.y * m->speed_y;

    /* DrawingContext::get_cliprect, TileMap::get_tiles_overlapping */
    TxRect rect = tx_rect(trans_x, trans_y, trans_x + screen_w, trans_y + screen_h);
    tx_rect_move(&rect, tx_vec_neg(m->offset));
    int t_left   = tx_maxi(0        , (int) floorf(tx_rect_left(&rect)   / 32));
    int t_right  = tx_mini(m->width , (int) ceilf (tx_rect_right(&rect)  / 32));
    int t_top    = tx_maxi(0        , (int) floorf(tx_rect_top(&rect)    / 32));
    int t_bottom = tx_mini(m->height, (int) ceilf (tx_rect_bottom(&rect) / 32));

    /* TileMap::get_tile_position */
    TxVec start = tx_vec_add(m->offset, tx_vec((float) t_left * 32, (float) t_top * 32));
    TxVec pos;
    int tx, ty;
    for (pos.x = start.x, tx = t_left; tx < t_right; pos.x += 32, ++tx) {
        for (pos.y = start.y, ty = t_top; ty < t_bottom; pos.y += 32, ++ty) {
            uint32_t id = m->tiles[ty*m->width + tx];
            if (id == 0 || id >= (uint32_t) td_tiles_n) continue;
            const TdTile* tile = &td_tiles[id];
            if (tile->n == 0) continue;

            /* Tile::get_current_surface */
            int f = 0;
            if (tile->n > 1) {
                f = (int) ((size_t) (tx_game_time * tile->fps) % (size_t) tile->n);
            }
            const TdFrame* fr = &td_frames[tile->first + f];
            if (fr->img >= td_imgs_n) continue;
            SDL_Texture* tex = sdl_tex(td_imgs[fr->img]);
            if (tex == NULL) continue;

            /* Canvas::draw_surface_batch: the position minus the translation */
            td_texture(tex, fr->sx, fr->sy, fr->w, fr->h,
                       pos.x - trans_x, pos.y - trans_y, (float) fr->w, (float) fr->h,
                       m->tint, m->alpha);
        }
    }
}

/* SDLPainter::draw_gradient, over the screen */
static void td_draw_gradient (const TdGradient* gr, float screen_w, float screen_h) {
    const float* top = gr->top;
    const float* bottom = gr->bottom;
    int direction = gr->direction;
    TxRect region = tx_rect(0, 0, screen_w, screen_h);

    int n = (int) (fmaxf(fmaxf(fabsf(top[0] - bottom[0]),
                               fabsf(top[1] - bottom[1])),
                         fmaxf(fabsf(top[2] - bottom[2]),
                               fabsf(top[3] - bottom[3]))) * 255);
    n = (n > 1) ? n : 1;
    int next_step = (direction == TD_VERTICAL || direction == TD_VERTICAL_SECTOR) ?
                    (int) tx_rect_top(&region) : (int) tx_rect_left(&region);
    for (int i = 0; i < n; ++i) {
        SDL_Rect rect;
        if (direction == TD_VERTICAL || direction == TD_VERTICAL_SECTOR) {
            rect.x = (int) tx_rect_left(&region);
            rect.y = next_step;
            rect.w = (int) (tx_rect_right(&region) - tx_rect_left(&region));
            rect.h = (int) ceilf((tx_rect_bottom(&region) - tx_rect_top(&region)) / (float) n);
            /* Account for the build-up of rounding errors due to floating point precision. */
            if (next_step > (int) (tx_rect_top(&region) + (tx_rect_bottom(&region) - tx_rect_top(&region)) * (float) i / (float) n))
                --rect.h;
            next_step += rect.h;
        } else {
            rect.x = next_step;
            rect.y = (int) tx_rect_top(&region);
            rect.w = (int) ceilf((tx_rect_right(&region) - tx_rect_left(&region)) / (float) n);
            rect.h = (int) (tx_rect_bottom(&region) - tx_rect_top(&region));
            if (next_step > (int) (tx_rect_left(&region) + (tx_rect_right(&region) - tx_rect_left(&region)) * (float) i / (float) n))
                --rect.w;
            next_step += rect.w;
        }
        float p = (float) i / (float) (n == 1 ? n : n - 1);
        Uint8 r = (Uint8) (((1.0f - p) * top[0] + p * bottom[0]) * 255);
        Uint8 g = (Uint8) (((1.0f - p) * top[1] + p * bottom[1]) * 255);
        Uint8 b = (Uint8) (((1.0f - p) * top[2] + p * bottom[2]) * 255);
        Uint8 a = (Uint8) (((1.0f - p) * top[3] + p * bottom[3]) * 255);
        SDL_SetRenderDrawBlendMode(sdl_ren(), SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(sdl_ren(), r, g, b, a);
        SDL_RenderFillRect(sdl_ren(), &rect);
    }
}

/* std::stable_sort of the requests, by layer */
static int td_cmp (const void* a, const void* b) {
    const TdItem* x = a;
    const TdItem* y = b;
    if (x->layer != y->layer) return (x->layer < y->layer) ? -1 : 1;
    return (x->order < y->order) ? -1 : (x->order > y->order);
}

/* A frame of the sector, with the translation of the camera. */
static void td_draw (TxVec translation) {
    int w, h;
    sdl_size(&w, &h);
    if (td_sorted == NULL) {
        td_sorted = malloc((td_items_n ? td_items_n : 1) * sizeof(TdItem));
        memcpy(td_sorted, td_items, td_items_n * sizeof(TdItem));
        qsort(td_sorted, td_items_n, sizeof(TdItem), td_cmp);
    }
    for (int i=0; i<td_items_n; i++) {
        const TdItem* it = &td_sorted[i];
        switch (it->kind) {
            case TD_MAP:      td_draw_map(&td_maps[it->index], translation, (float) w, (float) h); break;
            case TD_GRADIENT: td_draw_gradient(&td_grads[it->index], (float) w, (float) h); break;
        }
    }
}

#endif
