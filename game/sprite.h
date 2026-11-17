/* Sprites: their actions and their animation, as the original
 * (`SpriteData::parse_action`, `Sprite`, of SuperTux v0.7.0), in 32-bit
 * floats. The data of a sprite is given by the program, action by
 * action, in the order of its file (an action that mirrors another comes
 * after it). An image is an index of game/draw.h.
 *
 * Only images and mirror actions are done: no regions, flip, or clone
 * actions, which the sprites of the slice do not use (checked when they
 * are read).
 *
 * The names have the prefix `ts_`. */

#ifndef TS_SPRITE_H
#define TS_SPRITE_H

#include <string.h>

#define TS_NAME 48
#define TS_LOOPS_CONTINUED -100     /* Sprite::LOOPS_CONTINUED */

typedef struct TsFrame { int img; int w, h; int flip; } TsFrame;  /* flip: SDL_FLIP_* */

typedef struct TsAction {
    char  name[TS_NAME];
    char  family[TS_NAME + 2];
    float x_offset, y_offset;
    float hitbox_w, hitbox_h;
    float fps;
    int   loops;
    int   loop_frame;
    int   has_custom_loops;
    int   first, n;                 /* the frames */
} TsAction;

typedef struct TsData {
    TsAction* actions;  int actions_n, actions_cap;
    TsFrame*  frames;   int frames_n, frames_cap;
} TsData;

/* --------------------------------------------------------------------- */
/* SpriteData                                                             */
/* --------------------------------------------------------------------- */

static const TsAction* ts_find (const TsData* d, const char* name) {
    for (int i=0; i<d->actions_n; i++) {
        if (strcmp(d->actions[i].name, name) == 0) return &d->actions[i];
    }
    return NULL;
}

/* SpriteData::parse_action, the beginning: an action with its fields.
 * `nhit` is the number of values of the hitbox (0, 2, or 4). */
static TsAction* ts_action (TsData* d, const char* name, int nhit, const float* hit,
                            int has_fps, float fps, int has_loops, int loops,
                            int has_loop_frame, int loop_frame, const char* family) {
    if (d->actions_n == d->actions_cap) {
        d->actions_cap = d->actions_cap ? d->actions_cap*2 : 64;
        d->actions = realloc(d->actions, d->actions_cap * sizeof(TsAction));
    }
    TsAction* a = &d->actions[d->actions_n++];
    memset(a, 0, sizeof(*a));
    snprintf(a->name, TS_NAME, "%s", name);
    a->fps = 10; a->loops = -1; a->loop_frame = 1;
    a->first = d->frames_n;
    if (nhit == 4) { a->hitbox_h = hit[3]; a->hitbox_w = hit[2]; }
    if (nhit >= 2) { a->y_offset = hit[1]; a->x_offset = hit[0]; }
    if (has_fps) a->fps = fps;
    if (has_loops) { a->loops = loops; a->has_custom_loops = 1; }
    if (has_loop_frame) a->loop_frame = (loop_frame < 1) ? 1 : loop_frame;
    if (family) snprintf(a->family, sizeof(a->family), "%s", family);
    else        snprintf(a->family, sizeof(a->family), "::%s", a->name);
    return a;
}

static void ts_push_frame (TsData* d, TsFrame f) {
    if (d->frames_n == d->frames_cap) {
        d->frames_cap = d->frames_cap ? d->frames_cap*2 : 256;
        d->frames = realloc(d->frames, d->frames_cap * sizeof(TsFrame));
    }
    d->frames[d->frames_n++] = f;
}

/* (images ...): the frames, then the hitbox from their size */
static void ts_images (TsData* d, TsAction* a, const int* imgs, const int* ws, const int* hs, int n) {
    float max_w = 0, max_h = 0;
    for (int i=0; i<n; i++) {
        ts_push_frame(d, (TsFrame) { imgs[i], ws[i], hs[i], 0 });
        a->n++;
        max_w = fmaxf(max_w, (float) ws[i]);
        max_h = fmaxf(max_h, (float) hs[i]);
    }
    if (a->hitbox_w < 1) a->hitbox_w = max_w - a->x_offset;
    if (a->hitbox_h < 1) a->hitbox_h = max_h - a->y_offset;
}

/* (mirror-action name): the frames of the other, flipped horizontally */
static int ts_mirror (TsData* d, TsAction* a, const char* other) {
    const TsAction* src = ts_find(d, other);
    if (src == NULL) return 0;
    TsAction tmp = *src;            /* d->actions may move */
    a = &d->actions[a - d->actions];
    for (int i=0; i<tmp.n; i++) {
        TsFrame f = d->frames[tmp.first + i];
        f.flip ^= SDL_FLIP_HORIZONTAL;
        ts_push_frame(d, f);
        a->n++;
    }
    if (a->hitbox_w < 1 && a->hitbox_h < 1) {
        a->hitbox_w = tmp.hitbox_w; a->hitbox_h = tmp.hitbox_h;
        a->x_offset = tmp.x_offset; a->y_offset = tmp.y_offset;
    }
    if (!a->has_custom_loops && tmp.has_custom_loops) {
        a->has_custom_loops = 1;
        a->loops = tmp.loops;
    }
    if (a->fps == 0) a->fps = tmp.fps;
    char own[TS_NAME + 2];
    snprintf(own, sizeof(own), "::%s", a->name);
    if (strcmp(a->family, own) == 0) {
        snprintf(a->family, sizeof(a->family), "%s", tmp.family);
    }
    return 1;
}

/* --------------------------------------------------------------------- */
/* Sprite                                                                 */
/* --------------------------------------------------------------------- */

typedef struct TsSprite {
    const TsData* data;
    int   action;                   /* index, or -1 */
    float frame;                    /* m_frame */
    int   frameidx;                 /* m_frameidx */
    int   animation_loops;
    float last_ticks;
    int   is_paused;
    float angle;
    float color[4];
    float alpha;
} TsSprite;

/* Sprite::Sprite: the action "default", or else the first one of an
 * unordered map in the original, which is not known here: the first one
 * of the file. */
static void ts_init (TsSprite* s, const TsData* d) {
    memset(s, 0, sizeof(*s));
    s->data = d;
    const TsAction* a = ts_find(d, "default");
    s->action = a ? (int) (a - d->actions) : ((d->actions_n > 0) ? 0 : -1);
    s->animation_loops = -1;
    s->color[0] = s->color[1] = s->color[2] = s->color[3] = 1;
    s->alpha = 1;
    s->last_ticks = tx_game_time;
}

/* A sprite of one image (SpriteData of an image file): the action
 * "default", with the size of the image as its hitbox. */
static void ts_from_image (TsData* d, int img, int w, int h) {
    TsAction* a = ts_action(d, "default", 0, NULL, 0, 0, 0, 0, 0, 0, NULL);
    ts_images(d, a, &img, &w, &h, 1);
}

static const TsAction* ts_act (const TsSprite* s) {
    return (s->action >= 0) ? &s->data->actions[s->action] : NULL;
}

static int ts_frames (const TsSprite* s) {
    return ts_act(s)->n;
}

/* Sprite::animation_done */
static int ts_animation_done (const TsSprite* s) {
    return s->animation_loops == 0;
}

/* Sprite::update */
static void ts_update (TsSprite* s) {
    const TsAction* a = ts_act(s);
    if (a == NULL) return;
    float frame_inc = s->last_ticks > 0.f ? a->fps * (tx_game_time - s->last_ticks) : 0.f;
    s->last_ticks = tx_game_time;

    if (s->is_paused) return;

    s->frame += frame_inc;

    while (s->frame >= 1.0f) {
        s->frame -= 1.0f;
        s->frameidx++;
    }

    while (s->frameidx >= ts_frames(s) && !ts_animation_done(s)) {
        /* Loop animation. */
        s->frameidx -= ts_frames(s) - (a->loop_frame - 1);
        s->animation_loops--;
    }

    if (ts_animation_done(s)) {
        s->frame = 0;
        s->frameidx = ts_frames(s) - 1;
    }
}

/* Sprite::set_action(name, loops) */
static void ts_set_action (TsSprite* s, const char* name, int loops) {
    const TsAction* cur = ts_act(s);
    if (cur && strcmp(cur->name, name) == 0) return;

    const TsAction* newaction = ts_find(s->data, name);
    if (!newaction) return;
    int idx = (int) (newaction - s->data->actions);

    /* Automatically resume if a new action is set */
    s->is_paused = 0;

    /* The action's loops were set to continued; use the ones from the previous action. */
    if (loops == TS_LOOPS_CONTINUED) {
        s->action = idx;
        ts_update(s);
        return;
    }

    /* If the new action has a loops property, we prefer that over the parameter. */
    s->animation_loops = newaction->has_custom_loops ? newaction->loops : loops;

    if (!cur || strcmp(cur->family, newaction->family) != 0) {
        s->frame = 0;
        s->frameidx = 0;
    }

    s->action = idx;
    s->last_ticks = tx_game_time;
}

/* Sprite::draw, with no flip of its own, at a position of the sector,
 * with the translation of the camera */
static void ts_draw (TsSprite* s, TxVec pos, TxVec translation, float sw, float sh) {
    const TsAction* a = ts_act(s);
    if (a == NULL || a->n == 0) return;
    ts_update(s);
    const TsFrame* f = &s->data->frames[a->first + s->frameidx];
    if (f->img < 0 || f->img >= td_imgs_n) return;
    SDL_Texture* tex = sdl_tex(td_imgs[f->img]);
    if (tex == NULL) return;

    TxVec position = tx_vec_sub(pos, tx_vec(a->x_offset, a->y_offset));

    /* Canvas::draw_surface: nothing if out of the clip rectangle */
    TxRect clip = tx_rect(translation.x, translation.y, translation.x + sw, translation.y + sh);
    if (position.x > tx_rect_right(&clip) ||
        position.y > tx_rect_bottom(&clip) ||
        position.x + (float) f->w < tx_rect_left(&clip) ||
        position.y + (float) f->h < tx_rect_top(&clip))
        return;

    TxVec p = tx_vec_sub(position, translation);
    SDL_Rect src = { 0, 0, f->w, f->h };
    SDL_FRect dst = { p.x, p.y, (float) f->w, (float) f->h };
    Uint8 r = (Uint8) (s->color[0] * 255);
    Uint8 g = (Uint8) (s->color[1] * 255);
    Uint8 b = (Uint8) (s->color[2] * 255);
    Uint8 al = (Uint8) (s->color[3] * s->alpha * 255);
    SDL_SetTextureColorMod(tex, r, g, b);
    SDL_SetTextureAlphaMod(tex, al);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_RenderCopyExF(sdl_ren(), tex, &src, &dst, (double) s->angle, NULL, (SDL_RendererFlip) f->flip);
}

#endif
