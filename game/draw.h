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

/* A background: its images (indices, -1 for none), and its movement.
 * (`Background`, with no fill, no flip, and no change of color.) */
typedef struct TdBackground {
    int   img_top, img, img_bottom;
    int   alignment;                /* TD_NO_ALIGNMENT, ... */
    TxVec parallax_speed;           /* m_parallax_speed */
    TxVec scroll_speed;             /* m_scroll_speed */
    TxVec scroll_offset;            /* m_scroll_offset */
    float color[4];
} TdBackground;
enum { TD_NO_ALIGNMENT=0, TD_LEFT_ALIGNMENT, TD_RIGHT_ALIGNMENT, TD_TOP_ALIGNMENT, TD_BOTTOM_ALIGNMENT };

/* A drawable: what, which, its layer, and the order of its object. */
enum { TD_MAP=0, TD_GRADIENT=1, TD_BACKGROUND=2 };
typedef struct TdItem { int kind, index, layer, order; } TdItem;

static TdMap*      td_maps;      static int td_maps_n, td_maps_cap;
static TdGradient* td_grads;     static int td_grads_n, td_grads_cap;
static TdBackground* td_bgs;     static int td_bgs_n, td_bgs_cap;
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
    free(td_bgs); td_bgs = NULL; td_bgs_n = td_bgs_cap = 0;
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

static void td_background (int img_top, int img, int img_bottom, int alignment, int layer,
                           float speed_x, float speed_y, float scroll_x, float scroll_y,
                           float offset_x, float offset_y, const float* color) {
    TD_PUSH(td_bgs, td_bgs_n, td_bgs_cap);
    TdBackground* b = &td_bgs[td_bgs_n];
    b->img_top = img_top; b->img = img; b->img_bottom = img_bottom;
    b->alignment = alignment;
    b->parallax_speed = tx_vec(speed_x, speed_y);
    b->scroll_speed = tx_vec(scroll_x, scroll_y);
    b->scroll_offset = tx_vec(offset_x, offset_y);
    memcpy(b->color, color, sizeof(b->color));
    td_item(TD_BACKGROUND, td_bgs_n, layer);
    td_bgs_n++;
}

/* Background::update, for every background, at each step */
static void td_update (float dt_sec) {
    for (int i=0; i<td_bgs_n; i++) {
        TdBackground* b = &td_bgs[i];
        b->scroll_offset = tx_vec_add(b->scroll_offset, tx_vec_scale(b->scroll_speed, dt_sec));
    }
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

/* Canvas::draw_surface, at a position of the sector, with the translation
 * of the camera: nothing if out of the clip rectangle. */
static void td_surface (int img, TxVec position, TxVec translation, const TxRect* cliprect, const float* color) {
    if (img < 0 || img >= td_imgs_n) return;
    SDL_Texture* tex = sdl_tex(td_imgs[img]);
    if (tex == NULL) return;
    int w, h;
    SDL_QueryTexture(tex, NULL, NULL, &w, &h);
    if (position.x > tx_rect_right(cliprect) ||
        position.y > tx_rect_bottom(cliprect) ||
        position.x + (float) w < tx_rect_left(cliprect) ||
        position.y + (float) h < tx_rect_top(cliprect))
        return;
    TxVec p = tx_vec_sub(position, translation);
    td_texture(tex, 0, 0, w, h, p.x, p.y, (float) w, (float) h, color, 1.0f);
}

static TxVec td_img_size (int img) {
    int w = 0, h = 0;
    if (img >= 0 && img < td_imgs_n) {
        SDL_Texture* tex = sdl_tex(td_imgs[img]);
        if (tex) SDL_QueryTexture(tex, NULL, NULL, &w, &h);
    }
    return tx_vec((float) w, (float) h);
}

/* Background::draw_image */
static void td_draw_image (const TdBackground* b, TxVec pos_, TxVec translation, float sw, float sh) {
    const TxVec level = tx_vec(tx__sector_get_width(&tg_sec), tx__sector_get_height(&tg_sec));
    const TxVec screen = tx_vec(sw, sh);
    const TxVec parallax_image_size = tx_vec((1.0f - b->parallax_speed.x) * screen.x + level.x * b->parallax_speed.x,
                                             (1.0f - b->parallax_speed.y) * screen.y + level.y * b->parallax_speed.y);
    const TxRect cliprect = tx_rect(translation.x, translation.y, translation.x + sw, translation.y + sh);

    const TxVec size = td_img_size(b->img);
    const float img_w = size.x;
    const float img_h = size.y;
    if (img_w == 0 || img_h == 0) return;

    const float img_w_2 = img_w / 2.0f;
    const float img_h_2 = img_h / 2.0f;

    const int start_x = (int) floorf((tx_rect_left(&cliprect) - (pos_.x - img_w /2.0f)) / img_w);
    const int end_x   = (int) ceilf((tx_rect_right(&cliprect) - (pos_.x + img_w /2.0f)) / img_w) + 1;
    const int start_y = (int) floorf((tx_rect_top(&cliprect) - (pos_.y - img_h/2.0f)) / img_h);
    const int end_y   = (int) ceilf((tx_rect_bottom(&cliprect) - (pos_.y + img_h/2.0f)) / img_h) + 1;

    switch (b->alignment) {
        case TD_LEFT_ALIGNMENT:
            for (int y = start_y; y < end_y; ++y) {
                TxVec p = tx_vec(pos_.x - parallax_image_size.x / 2.0f,
                                 pos_.y + (float) y * img_h - img_h_2);
                td_surface(b->img, p, translation, &cliprect, b->color);
            }
            break;
        case TD_RIGHT_ALIGNMENT:
            for (int y = start_y; y < end_y; ++y) {
                TxVec p = tx_vec(pos_.x + parallax_image_size.x / 2.0f - img_w,
                                 pos_.y + (float) y * img_h - img_h_2);
                td_surface(b->img, p, translation, &cliprect, b->color);
            }
            break;
        case TD_TOP_ALIGNMENT:
            for (int x = start_x; x < end_x; ++x) {
                TxVec p = tx_vec(pos_.x + (float) x * img_w - img_w_2,
                                 pos_.y - parallax_image_size.y / 2.0f);
                td_surface(b->img, p, translation, &cliprect, b->color);
            }
            break;
        case TD_BOTTOM_ALIGNMENT:
            for (int x = start_x; x < end_x; ++x) {
                TxVec p = tx_vec(pos_.x + (float) x * img_w - img_w_2,
                                 pos_.y - img_h + parallax_image_size.y / 2.0f);
                td_surface(b->img, p, translation, &cliprect, b->color);
            }
            break;
        default:
            for (int y = start_y; y < end_y; ++y)
                for (int x = start_x; x < end_x; ++x) {
                    TxVec p = tx_vec(pos_.x + (float) x * img_w - img_w_2,
                                     pos_.y + (float) y * img_h - img_h_2);
                    if (b->img_top >= 0 && (y < 0)) {
                        td_surface(b->img_top, p, translation, &cliprect, b->color);
                    } else if (b->img_bottom >= 0 && (y > 0)) {
                        td_surface(b->img_bottom, p, translation, &cliprect, b->color);
                    } else {
                        td_surface(b->img, p, translation, &cliprect, b->color);
                    }
                }
            break;
    }
}

/* Background::draw, with a scale of 1 */
static void td_draw_background (const TdBackground* b, TxVec translation, float sw, float sh) {
    const TxVec level_size = tx_vec(tx__sector_get_width(&tg_sec), tx__sector_get_height(&tg_sec));
    const TxVec screen = tx_vec(sw, sh);
    const TxVec translation_range = tx_vec_sub(level_size, screen);
    const TxVec center_offset = tx_vec(translation.x - translation_range.x / 2.0f,
                                       translation.y - translation_range.y / 2.0f);
    const TxVec pos = tx_vec(level_size.x / 2, level_size.y / 2);
    td_draw_image(b, tx_vec_add(tx_vec_add(pos, b->scroll_offset),
                                tx_vec(center_offset.x * (1.0f - b->parallax_speed.x),
                                       center_offset.y * (1.0f - b->parallax_speed.y))),
                  translation, sw, sh);
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
            case TD_BACKGROUND: td_draw_background(&td_bgs[it->index], translation, (float) w, (float) h); break;
        }
    }
}

#endif
