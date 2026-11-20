/* The C side of the game: the sector with its tilemaps, the player, the
 * controller, and the trace, over the kernels translated from the
 * original (coll.h, player.h). Everything here has the prefix `tg_`. */

#ifndef TG_TUX_H
#define TG_TUX_H

#include <stdio.h>
#include "player.h"
#include "camera.h"
#include "random.h"

static TxSector     tg_sec;
static TxPlayer     tg_tux;
static TxController tg_ctl;
static TxPlayerHost tg_host;
static long         tg_tick;
static TcCamera     tg_cam;

/* More hooks of the player, from the view, if any (the sprite). */
static void (*tg_extra_hooks) (TxPlayerHost* h);
static int          tg_trace_camera;    /* TUX_TRACE_CAMERA: the lines `C` */
static uint8_t*     tg_objtile;     /* by tile id: the tile becomes an object */

/* The objects of the sector that are tasks in Atmos, by id (from 1; the
 * player is 0). As in the original, an object that is created enters
 * the collisions at the end of the step, and one that is removed is not
 * valid at once and leaves at the end of the step. */
static TxObj** tg_objs;
static int     tg_objs_n;
static int     tg_objs_cap;

/* The function of the game that receives a collision of an object:
 * f(self, other, hit) -> response, with `hit` as the bits TG_HIT_*. */
static AVal    tg_dispatch;
static AStatus tg_cb_st;

/* `ScreenManager::m_speed`: 0.5 in the end sequence. The length of a
 * step is `seconds_per_step * m_speed * speed_multiplier`. */
static float tg_speed = 1.0f;
static float tg_dt;

/* The end sequence that walks (`EndSequenceWalk`): its controller, its
 * timer, and the last position of the player. */
static TxController tg_end_ctl;
static TxTimer      tg_end_timer;
static float        tg_end_last_x;
static int          tg_end_stopped;     /* stop_tux: the player stops */

/* The function of the game that starts a sequence: f(seq) (TX_SEQ_*). */
static AVal tg_sequence;

/* The kind of each object, for the player (TX_OTHER_*), by id. */
static int*    tg_kinds;

/* The badguys (game/badguy.atm), by the id of their object, for the
 * trace: their uid (0: not a badguy), class, state and direction; and
 * m_detected_slope of might_fall. */
static uint32_t* tg_bg_uid;
static char    (*tg_bg_cls)[24];
static int*      tg_bg_state;
static int*      tg_bg_dir;             /* TX_DIR_* */
static int*      tg_bg_slope;           /* m_detected_slope of might_fall */
static int       tg_bg_cap;

/* The physic of each object, by id, for the objects that move by it. */
static TxPhysic* tg_physs;

/* The function of the game that receives a collision of an object with
 * the solid tiles and the static objects: f(self, hit), with `hit` as the
 * bits TG_HIT_*, and TG_HIT_FLAT when the slope normal has no x. */
static AVal    tg_solid;

/* The function of the game that receives an event that the player gives
 * to an object: f(id, event), as `TriggerBase::event` (TX_EVENT_*). */
static AVal    tg_trigger;

enum { TG_HIT_LEFT=1, TG_HIT_RIGHT=2, TG_HIT_TOP=4, TG_HIT_BOTTOM=8, TG_HIT_CRUSH=16, TG_HIT_FLAT=32 };

static int tg_hit_bits (const TxHit* hit) {
    return (hit->left   ? TG_HIT_LEFT   : 0) | (hit->right  ? TG_HIT_RIGHT  : 0)
         | (hit->top    ? TG_HIT_TOP    : 0) | (hit->bottom ? TG_HIT_BOTTOM : 0)
         | (hit->crush  ? TG_HIT_CRUSH  : 0) | ((hit->slope_normal.x == 0.0f) ? TG_HIT_FLAT : 0);
}

static void tg_on_solid (TxObj* self, const TxHit* hit) {
    if (self->user == &tg_tux) {
        tx_player_collision_solid(&tg_tux, hit);
        return;
    }
    if (tg_cb_st != A_OK || tg_solid.type == A_NIL) {
        return;
    }
    int base = a_top;
    a_push(a_int(self->id));
    a_push(a_int(tg_hit_bits(hit)));
    a_push(a_num(hit->slope_normal.x));
    a_push(a_num(hit->slope_normal.y));
    AStatus st = a_call(tg_solid, base);
    if (st == A_OK) {
        a_settop(base);
    } else {
        tg_cb_st = st;
    }
}

static AVal tg_tile;     /* f(self, attributes): a tile touched by an object */

static void tg_on_tile (TxObj* self, uint32_t attrs) {
    if (self->user == &tg_tux) {
        tx_player_collision_tile(&tg_tux, attrs);
        return;
    }
    if (tg_cb_st != A_OK || tg_tile.type == A_NIL) {
        return;
    }
    int base = a_top;
    a_push(a_int(self->id));
    a_push(a_int((int64_t) attrs));
    AStatus st = a_call(tg_tile, base);
    if (st == A_OK) {
        a_settop(base);
    } else {
        tg_cb_st = st;
    }
}

static int tg_on_collision (TxObj* self, TxObj* other, const TxHit* hit) {
    if (self->user == &tg_tux) {
        int kind = (other->id > 0) ? tg_kinds[other->id-1] : TX_OTHER_OTHER;
        return tx_player_collision(&tg_tux, kind, other, hit);
    }
    if (tg_cb_st != A_OK) {
        return TX_ABORT_MOVE;
    }
    int bits = tg_hit_bits(hit);
    int base = a_top;
    a_push(a_int(self->id));
    a_push(a_int(other->id));
    a_push(a_int(bits));
    AStatus st = a_call(tg_dispatch, base);
    int ret = TX_ABORT_MOVE;
    if (st == A_OK) {
        if (a_top > base) {
            ret = (int) a_stk[base].u.i;
        }
        a_settop(base);
    } else {
        tg_cb_st = st;
    }
    return ret;
}

static void tg_on_trigger (void* user, TxObj* other, int event) {
    (void) user;
    if (tg_cb_st != A_OK) {
        return;
    }
    int base = a_top;
    a_push(a_int(other->id));
    a_push(a_int(event));
    AStatus st = a_call(tg_trigger, base);
    if (st == A_OK) {
        a_settop(base);
    } else {
        tg_cb_st = st;
    }
}

/* `graphicsRandom`: seeded once, at the start of the game, with
 * TUX_GSEED, or else with the time, as the original. */
static TrRandom tg_grand;
static int      tg_grand_ready;

static void tg_grand_init (void) {
    if (!tg_grand_ready) {
        const char* v = getenv("TUX_GSEED");
        tr_seed(&tg_grand, v ? atoi(v) : 0);
        tg_grand_ready = 1;
    }
}

static int tg_on_rand_i (void* user, int lo, int hi) {
    (void) user;
    return tr_rand(&tg_grand, lo, hi);
}

static float tg_on_rand_f (void* user, float lo, float hi) {
    (void) user;
    return tr_randf(&tg_grand, lo, hi);
}

/* `Particles::Particles` (the second one): the random numbers that it
 * draws, per particle. The particles are not drawn yet. */
static void tg_on_particles (void* user, TxVec epicenter, int min_angle, int max_angle,
                             float min_initial_velocity, float max_initial_velocity,
                             TxVec acceleration, int number, float r, float g, float b,
                             int size, float life_time, int drawing_layer) {
    (void) user; (void) epicenter; (void) acceleration; (void) r; (void) g; (void) b;
    (void) size; (void) life_time; (void) drawing_layer;
    for (int p=0; p<number; p++) {
        if (min_initial_velocity != max_initial_velocity) {
            tr_randf(&tg_grand, min_initial_velocity, max_initial_velocity);
        }
        if (min_angle != max_angle) {
            tr_randf(&tg_grand, (float) min_angle, (float) max_angle);
        }
    }
}

static void tg_on_shake (void* user, float duration, float x, float y) {
    (void) user;
    tc_shake(&tg_cam, duration, x, y);
}

static void tg_on_sequence (void* user, int seq, const void* data) {
    (void) user; (void) data;
    if (tg_cb_st != A_OK) {
        return;
    }
    int base = a_top;
    a_push(a_int(seq));
    AStatus st = a_call(tg_sequence, base);
    if (st == A_OK) {
        a_settop(base);
    } else {
        tg_cb_st = st;
    }
}

/* `GameSession::start_sequence`, for the player who calls it. */
static void tg_end_prepare (void) {
    tx_player_set_ending_direction(&tg_tux, (tg_tux.physic.vx < 0) ? -1 : 1);
    tx_player_set_controller(&tg_tux, &tg_end_ctl);
    tx_player_set_speedlimit(&tg_tux, 230); /* MAX_WALK_XM */
}

/* `EndSequence::start`, `EndSequenceWalk::starting`, and what
 * `GameSession::start_sequence` does after them. */
static void tg_end_start (void) {
    tg_speed = 0.5f;
    tg_end_last_x = -1;
    tx_timer_start(&tg_end_timer, 7.3f * tg_speed);
    tx_player_set_winning(&tg_tux);
    tx_player_set_controller(&tg_tux, &tg_end_ctl);
    tx_player_set_speedlimit(&tg_tux, 230); /* MAX_WALK_XM */
}

/* `EndSequenceWalk::running`: true when the sequence is done. */
static int tg_end_running (void) {
    tx_code_controller_update(&tg_end_ctl);
    int dir = tx_player_get_ending_direction(&tg_tux);
    if (dir && !tg_end_stopped) {
        tx_code_controller_press(&tg_end_ctl, dir > 0 ? TX_CONTROL_RIGHT : TX_CONTROL_LEFT, true);
        if ((int) tg_end_last_x == (int) tg_tux.col.bbox.x1) {
            tx_code_controller_press(&tg_end_ctl, TX_CONTROL_JUMP, true);
        }
        tg_end_last_x = tg_tux.col.bbox.x1;
    }
    return tx_timer_check(&tg_end_timer);
}

static int tg_obj_new (float x, float y, float w, float h, int group, int kind) {
    if (tg_objs_n == tg_objs_cap) {
        tg_objs_cap = tg_objs_cap ? tg_objs_cap*2 : 256;
        tg_objs = realloc(tg_objs, tg_objs_cap * sizeof(TxObj*));
        tg_kinds = realloc(tg_kinds, tg_objs_cap * sizeof(int));
        tg_physs = realloc(tg_physs, tg_objs_cap * sizeof(TxPhysic));
    }
    tg_kinds[tg_objs_n] = kind;
    tx_physic_init(&tg_physs[tg_objs_n]);
    if (tg_objs_n + 1 < tg_bg_cap) tg_bg_uid[tg_objs_n + 1] = 0;
    TxObj* o = malloc(sizeof(TxObj));
    tx_obj_init(o, group);
    tx_obj_set_size(o, w, h);
    tx_obj_set_pos(o, tx_vec(x, y));
    o->id = tg_objs_n + 1;
    o->user = NULL;                 /* not in the sector yet */
    tg_objs[tg_objs_n++] = o;
    return o->id;
}

static void tg_obj_del (int id) {
    tg_objs[id-1]->valid = false;
}

/* `flush_game_objects`: the removed leave, the created enter. */
static void tg_flush (void) {
    for (int i=0; i<tg_objs_n; i++) {
        TxObj* o = tg_objs[i];
        if (o == NULL) {
            /* gone */
        } else if (!o->valid) {
            if (o->user != NULL) {
                tx_sector_remove(&tg_sec, o);
            }
            tx_obj_free(o);
            free(o);
            tg_objs[i] = NULL;
        } else if (o->user == NULL) {
            o->user = o;
            tx_sector_add(&tg_sec, o);
        }
    }
}

/* `bbox.overlaps(player.bbox.grown(border))` */
static int tg_touches_player (int id, float border) {
    TxRect r = tx_rect_grown(&tg_tux.col.bbox, border);
    return tx_rect_overlaps(&tg_objs[id-1]->bbox, &r);
}

/* The movement of an object in this step (`set_movement`), and its
 * position. */
static void tg_obj_move (int id, float x, float y) {
    tx_obj_set_movement(tg_objs[id-1], tx_vec(x, y));
}
static void tg_obj_size (int id, float w, float h) {
    tx_obj_set_size(tg_objs[id-1], w, h);
}
/* The movement of an object by its physic in this step. */
static void tg_obj_physic_move (int id) {
    tx_obj_set_movement(tg_objs[id-1], tx_physic_get_movement(&tg_physs[id-1], tg_sec.gravity, tg_dt));
}
#define TG_PHYS(id) (tg_physs[(id)-1])

static float tg_obj_y (int id) {
    return tg_objs[id-1]->bbox.y1;
}

/* What a block sees in its collision with the player (`Block::collision`,
 * and the start of `BonusBlock::collision` and `Brick::collision`), as
 * bits: 1, the player does a buttjump; 4, he swimboosts from above; 2, he
 * hits the block from below (`hit`). The bonus block drops (`try_drop`)
 * on 1 or 4, the brick breaks on 1, and both do so before the hit. */
static int tg_block_player (int id, int bits) {
    const TxObj* b = tg_objs[id-1];
    const TxPlayer* p = &tg_tux;
    const TxRect* pb = &p->col.bbox;
    int r = 0;
    if (!(bits & (TG_HIT_LEFT|TG_HIT_RIGHT|TG_HIT_TOP|TG_HIT_BOTTOM))) {
        return 0;               /* !hit_.has_direction() */
    }
    if (p->does_buttjump) {
        r |= 1;
    }
    if (tx_player_is_swimboosting(p) && tx_rect_bottom(pb) < tx_rect_top(&b->bbox) + TX_SHIFT_DELTA) {
        r |= 4;
    }
    bool in_line_with_player = ((p->physic.vx > 0.f &&
        tx_rect_right(pb) < tx_rect_left(&b->bbox)) ||
        (p->physic.vx < 0.f &&
        tx_rect_left(pb) > tx_rect_right(&b->bbox)));
    if (tx_player_is_swimboosting(p) ||
        ((tx_player_is_sliding(p) || tx_player_is_stone(p)) && in_line_with_player)) {
        r |= 2;
    } else if (!tx_player_is_water_jumping(p) && !tx_player_is_swimming(p)) {
        bool x_coordinates_intersect =
            tx_rect_right(pb) >= tx_rect_left(&b->bbox) &&
            tx_rect_left(pb) <= tx_rect_right(&b->bbox);
        if (tx_rect_top(pb) > tx_rect_bottom(&b->bbox) - TX_SHIFT_DELTA && x_coordinates_intersect) {
            r |= 2;
        }
    }
    return r;
}

/* `Block::start_bounce(player)`: the angle of the sprite. */
static float tg_block_angle (int id) {
    const TxObj* b = tg_objs[id-1];
    float center_of_hitter = tx_rect_middle(&tg_tux.col.bbox).x;
    float offset = (tx_rect_middle(&b->bbox).x - center_of_hitter)*2 / tx_rect_width(&b->bbox);
    if (offset > 2 || offset < -2)
        offset = 0;
    return 10 * offset;         /* BUMP_ROTATION_ANGLE */
}

/* The looks of the objects: what the view draws of each one, a sprite
 * at a position. The objects that are tasks in Atmos keep their looks
 * here, by index, in the order in which they are created; the view, if
 * any, binds each new look to the data of its sprite, and draws the
 * looks that are alive (game/draw.h). With no view, they are only kept. */
typedef struct TgLook {
    char  sprite[96];
    char  action[32];
    int   changed;                  /* the action changed */
    int   loops;                    /* the loops of that action (-1: forever) */
    float x, y, angle, alpha;
    int   layer;
    int   alive;
    int   hidden;                   /* not drawn (an inactive badguy) */
    int   vflip;                    /* drawn flipped upside down */
    int   obj;                      /* at the box of this object, if > 0 */
    float ty;                       /* added to the translation of the camera */
    float born;                     /* the game time of its creation */
} TgLook;
static TgLook* tg_looks;
static int     tg_looks_n, tg_looks_cap;

static int tg_look_new (const char* sprite, float x, float y, int layer) {
    if (tg_looks_n == tg_looks_cap) {
        tg_looks_cap = tg_looks_cap ? tg_looks_cap*2 : 256;
        tg_looks = realloc(tg_looks, tg_looks_cap * sizeof(TgLook));
    }
    TgLook* l = &tg_looks[tg_looks_n];
    memset(l, 0, sizeof(*l));
    snprintf(l->sprite, sizeof(l->sprite), "%s", sprite);
    l->x = x; l->y = y; l->layer = layer;
    l->alpha = 1;
    l->alive = 1;
    l->born = tx_game_time;
    return tg_looks_n++;
}
static void tg_look_action_loops (int i, const char* action, int loops) {
    snprintf(tg_looks[i].action, sizeof(tg_looks[i].action), "%s", action);
    tg_looks[i].changed = 1;
    tg_looks[i].loops = loops;
}
static void tg_look_action (int i, const char* action) {
    tg_look_action_loops(i, action, -1);
}
static void tg_look_del (int i) {
    tg_looks[i].alive = 0;
}

/* `Sector::get().get_camera().get_rect().contains(pos)` */
static int tg_cam_contains (float x, float y) {
    TxVec c = tx_vec(tg_cam.translation.x + tg_cam.screen_size.x / 2.0f,
                     tg_cam.translation.y + tg_cam.screen_size.y / 2.0f);
    TxRect r = tx_rect(c.x - tg_cam.screen_size.x / 2.0f, c.y - tg_cam.screen_size.y / 2.0f,
                       c.x + tg_cam.screen_size.x / 2.0f, c.y + tg_cam.screen_size.y / 2.0f);
    return x >= r.x1 && y >= r.y1 && x < tx_rect_right(&r) && y < tx_rect_bottom(&r);
}

/* ------------------------------------------------------------------------ */
/* The badguys (game/badguy.atm): what they ask of the sector              */
/* ------------------------------------------------------------------------ */

/* The trace of the badguys: their uid, class, state and direction, by
 * the id of their object (uid 0: not a badguy). */
static FILE*     tg_trace_out;          /* the trace, if any */
static int       tg_trace_objs;         /* TUX_TRACE_OBJS: the lines `B` */
static int       tg_foremost;           /* Sector::get_foremost_opaque_layer, from the view */

static void tg_bg_new (int id, uint32_t uid, const char* cls, int dir) {
    if (id >= tg_bg_cap) {
        int n = tg_bg_cap ? tg_bg_cap : 256;
        while (n <= id) n *= 2;
        tg_bg_uid   = realloc(tg_bg_uid, n * sizeof(uint32_t));
        tg_bg_cls   = realloc(tg_bg_cls, n * sizeof(*tg_bg_cls));
        tg_bg_state = realloc(tg_bg_state, n * sizeof(int));
        tg_bg_dir   = realloc(tg_bg_dir, n * sizeof(int));
        tg_bg_slope = realloc(tg_bg_slope, n * sizeof(int));
        for (int i=tg_bg_cap; i<n; i++) tg_bg_uid[i] = 0;
        tg_bg_cap = n;
    }
    tg_bg_uid[id] = uid;
    snprintf(tg_bg_cls[id], 24, "%s", cls);
    tg_bg_state[id] = 0;
    tg_bg_dir[id] = dir;
    tg_bg_slope[id] = 0;
}

static int tg_is_bg (int id) {
    return id > 0 && id < tg_bg_cap && tg_bg_uid[id] != 0;
}

/* `TuxTestTrace::badguy_state`: the line `E`, during the step */
static void tg_bg_state_trace (int id, int from, int to) {
    tg_bg_state[id] = to;
    if (tg_trace_out) {
        TxVec pos = tx_obj_get_pos(tg_objs[id-1]);
        fprintf(tg_trace_out, "%ld E %u %s %d>%d %.9g %.9g\n",
            tg_tick, tg_bg_uid[id], tg_bg_cls[id], from, to, pos.x, pos.y);
    }
}

/* The lines `B`, after the step: the badguys in the sector, in its order. */
static void tg_bg_trace (FILE* f) {
    for (int i=0; i<tg_objs_n; i++) {
        TxObj* o = tg_objs[i];
        int id = i + 1;
        if (o == NULL || o->user == NULL || !tg_is_bg(id)) continue;
        TxVec pos = tx_obj_get_pos(o);
        const char* d = (tg_bg_dir[id] == TX_DIR_LEFT) ? "L" : (tg_bg_dir[id] == TX_DIR_RIGHT) ? "R" :
                        (tg_bg_dir[id] == TX_DIR_UP)   ? "U" : (tg_bg_dir[id] == TX_DIR_DOWN)  ? "D" : "-";
        fprintf(f, "%ld B %u %s %.9g %.9g %.9g %.9g s=%d d=%s\n",
            tg_tick, tg_bg_uid[id], tg_bg_cls[id], pos.x, pos.y,
            tg_physs[i].vx, tg_physs[i].vy, tg_bg_state[id], d);
    }
}

static void tg_obj_group (int id, int group) {
    tg_objs[id-1]->group = group;
}

/* `Sector::get_nearest_player` gives the player only while he is alive */
static int tg_player_alive (void) {
    return !tg_tux.dead && !tg_tux.dying;
}

/* BadGuy::is_offscreen */
static int tg_bg_offscreen (int id) {
    const float X_OFFSCREEN_DISTANCE = 1280, Y_OFFSCREEN_DISTANCE = 800;
    const TxRect* bb = &tg_objs[id-1]->bbox;
    if (!tg_player_alive()) {
        return 0;               /* no nearest player */
    }
    TxVec center = tx_vec(tg_cam.translation.x + tg_cam.screen_size.x / 2.0f,
                          tg_cam.translation.y + tg_cam.screen_size.y / 2.0f);
    TxVec cam_dist = tx_vec_sub(center, tx_rect_middle(bb));
    TxVec player_dist = tx_vec_sub(tx_rect_middle(&tg_tux.col.bbox), tx_rect_middle(bb));
    if (((fabsf(player_dist.x) <= X_OFFSCREEN_DISTANCE) && (fabsf(player_dist.y) <= Y_OFFSCREEN_DISTANCE))
        ||((fabsf(cam_dist.x) <= X_OFFSCREEN_DISTANCE) && (fabsf(cam_dist.y) <= Y_OFFSCREEN_DISTANCE))) {
        return 0;
    }
    return 1;
}

/* The part of BadGuy::update on the sector: out of it (1: the badguy is
 * removed). */
static int tg_bg_out (int id) {
    const TxRect* bb = &tg_objs[id-1]->bbox;
    TxVec pos = tx_obj_get_pos(tg_objs[id-1]);
    return pos.x > tx__sector_get_width(&tg_sec) || pos.x < -tx_rect_width(bb) ||
           pos.y > tx__sector_get_height(&tg_sec);
}

/* The part of BadGuy::update on the water (not frozen): the new
 * m_in_water, and the gravity modifier. */
static int tg_bg_water (int id, int in_water) {
    TxObj* o = tg_objs[id-1];
    TxPhysic* ph = &tg_physs[id-1];
    const TxRect* bb = &o->bbox;
    TxRect g1 = tx_rect_grown(bb, 1.f);
    if (tx_is_free_of_tiles(&tg_sec, &g1, true, TX_TILE_WATER) && in_water) {
        in_water = 0;
    }
    TxRect watertopbox = *bb;
    tx_rect_set_bottom(&watertopbox, tx_rect_bottom(bb) - tx_rect_height(bb) / 3.f);
    tx_rect_set_top(&watertopbox, tx_rect_top(bb) + tx_rect_height(bb) / 3.f);
    int middle_has_water = !tx_is_free_of_tiles(&tg_sec, &watertopbox, true, TX_TILE_WATER);
    if (ph->gravity_enabled_flag) {
        ph->gravity_modifier = middle_has_water ? 0.3f : 1.f;
    }
    return in_water;
}

/* `!Sector::get().is_free_of_tiles(bbox.grown(-4.f), false, Tile::WATER)` */
static int tg_bg_in_water (int id) {
    TxRect g = tx_rect_grown(&tg_objs[id-1]->bbox, -4.f);
    return !tx_is_free_of_tiles(&tg_sec, &g, false, TX_TILE_WATER);
}

/* `!is_free_of_tiles(bbox.grown(-6.f), true, Tile::HURTS)` */
static int tg_bg_hurt (int id) {
    TxRect g = tx_rect_grown(&tg_objs[id-1]->bbox, -6.f);
    return !tx_is_free_of_tiles(&tg_sec, &g, true, TX_TILE_HURTS);
}

/* The movement of BadGuy::active_update: by the physic, slowed in water */
static void tg_bg_move (int id, int water) {
    TxVec m = tx_physic_get_movement(&tg_physs[id-1], tg_sec.gravity, tg_dt);
    if (water) {
        m = tx_vec(m.x * 0.7f, m.y * 0.3f);
    }
    tx_obj_set_movement(tg_objs[id-1], m);
}

/* BadGuy::might_fall(height), with m_detected_slope kept by id */
static int tg_bg_might_fall (int id, int height) {
    TxObj* o = tg_objs[id-1];
    const TxRect* bb = &o->bbox;
    int left = (tg_bg_dir[id] == TX_DIR_LEFT);
    float oy = tx_rect_bottom(bb) + 1.f;
    float fh = (float) height;

    if (tg_bg_slope[id] == 0) {
        TxVec eye = tx_vec(0, oy - 2.f);
        eye.x = (left ? tx_rect_left(bb) : tx_rect_right(bb));
        TxVec end = tx_vec(eye.x, eye.y + fh + 2.f);
        TxRaycast result = tx_get_first_line_intersection(&tg_sec, eye, end, 0, o);
        if (!result.is_valid) {
            return 1;
        }
        if (result.kind == TX_RAY_TILE && result.tile_id < tg_sec.tiles.count &&
            tx_tile_is_slope(tg_sec.tiles.attrs[result.tile_id])) {
            int dir = tg_sec.tiles.data[result.tile_id];
            int d = dir & TX_AAT_DIRECTION_MASK;
            int south = (d == TX_AAT_SOUTHWEST || d == TX_AAT_SOUTHEAST);
            int east  = (d == TX_AAT_NORTHEAST || d == TX_AAT_SOUTHEAST);
            if (south && (left ? east : !east)) {
                tg_bg_slope[id] = dir;
            }
        }
    }

    if (tg_bg_slope[id] != 0) {
        float dirmult = (left ? 1.f : -1.f);
        float rearx = (left ? tx_rect_right(bb) : tx_rect_left(bb));
        float startoff = (tx_rect_width(bb) / 5.f) * dirmult;
        TxVec eye = tx_vec(rearx - startoff, oy);
        float endoff = startoff - (2.f * dirmult);
        TxVec end = tx_vec(eye.x + endoff, eye.y + 80.f);
        TxRaycast result = tx_get_first_line_intersection(&tg_sec, eye, end, 0, o);
        if (!result.is_valid) {
            tg_bg_slope[id] = 0;
            return 1;
        }
        if (tx_rect_top(&result.box) - oy > fh + 1.f) {
            tg_bg_slope[id] = 0;
            return 1;
        }
        if (result.kind == TX_RAY_TILE && result.tile_id < tg_sec.tiles.count &&
            tx_tile_is_slope(tg_sec.tiles.attrs[result.tile_id])) {
            return 0;
        }
        tg_bg_slope[id] = 0;
    }
    return 0;
}

/* The sector, with the attributes of `n` tiles still to fill. */
static void tg_sector (int n) {
    tx_sector_init(&tg_sec);
    tg_grand_init();
    tg_looks_n = 0;
    tg_sec.tiles.count = n;
    tg_sec.tiles.attrs = calloc(n, sizeof(uint32_t));
    tg_sec.tiles.data  = calloc(n, sizeof(int));
    tg_objtile = calloc(n, 1);
    tg_sec.collision       = tg_on_collision;
    tg_sec.collision_solid = tg_on_solid;
    tg_sec.collision_tile  = tg_on_tile;
}

/* The end of a sector: its tilemaps, its objects, the player. */
static void tg_sector_free (void) {
    for (int i=0; i<tg_objs_n; i++) {
        if (tg_objs[i] != NULL) {
            tg_objs[i]->valid = false;
        }
    }
    tg_flush();
    tg_objs_n = 0;
    tx_sector_remove(&tg_sec, &tg_tux.col);
    tx_player_free(&tg_tux);
    for (int i=0; i<tg_sec.maps_n; i++) {
        TxMap* m = tg_sec.maps[i];
        free(m->tiles);
        tx_map_free(m);
        free(m);
    }
    free(tg_sec.tiles.attrs);
    free(tg_sec.tiles.data);
    free(tg_objtile);
    tx_sector_free(&tg_sec);
}

/* The tiles of a tilemap, from the vector of the level, which has runs:
 * `-5, 0` is five times the tile 0. */
static uint32_t* tg_tiles (AVal t, int n) {
    uint32_t* v = calloc(n, sizeof(uint32_t));
    AVal l = a_nil();
    a_len(&l, t);
    int len = (int) l.u.i;
    int i = 0;
    for (int k=0; k<len && i<n; k++) {
        int64_t x = a_tab_get(A_TAB_OF(t), a_int(k)).u.i;
        if (x < 0) {
            int64_t id = a_tab_get(A_TAB_OF(t), a_int(++k)).u.i;
            for (int64_t j=0; j<-x && i<n; j++) {
                v[i++] = (uint32_t) id;
            }
        } else {
            v[i++] = (uint32_t) x;
        }
    }
    return v;
}

static void tg_map (AVal tiles, int w, int h) {
    TxMap* m = malloc(sizeof(TxMap));
    tx_map_init(m, w, h, tg_tiles(tiles, w*h));
    tx_sector_add_map(&tg_sec, m);
}

/* `Sector::convert_tiles2gameobject`: the next tile of a solid tilemap
 * that is an object (`tg_objtile[id]` is set), column by column. The
 * tile is removed, and its id and position are returned; 0 at the end. */
static int tg_conv_map, tg_conv_x, tg_conv_y;
static float tg_conv_px, tg_conv_py;

static void tg_convert_start (void) {
    tg_conv_map = tg_conv_x = tg_conv_y = 0;
}

static int tg_convert_next (void) {
    for (; tg_conv_map < tg_sec.maps_n; tg_conv_map++, tg_conv_x=0) {
        TxMap* m = tg_sec.maps[tg_conv_map];
        for (; tg_conv_x < m->width; tg_conv_x++, tg_conv_y=0) {
            for (; tg_conv_y < m->height; tg_conv_y++) {
                uint32_t id = m->tiles[tg_conv_y*m->width + tg_conv_x];
                if (id < tg_sec.tiles.count && tg_objtile[id]) {
                    TxVec pos = tx_map_get_tile_position(m, tg_conv_x, tg_conv_y);
                    tg_conv_px = pos.x;
                    tg_conv_py = pos.y;
                    m->tiles[tg_conv_y*m->width + tg_conv_x] = 0;
                    tg_conv_y++;
                    return (int) id;
                }
            }
        }
    }
    return 0;
}

/* The player at a spawn position, as `Sector::activate` places it. */
static void tg_player (float x, float y) {
    memset(&tg_host, 0, sizeof(tg_host));
    tg_host.trigger_event = tg_on_trigger;
    tg_host.start_sequence = tg_on_sequence;
    tg_host.camera_shake = tg_on_shake;
    tg_host.graphics_rand_i = tg_on_rand_i;
    tg_host.graphics_rand_f = tg_on_rand_f;
    tg_host.add_particles = tg_on_particles;
    if (tg_extra_hooks) tg_extra_hooks(&tg_host);
    tx_controller_reset(&tg_end_ctl);
    tx_timer_init(&tg_end_timer);
    tg_end_stopped = 0;
    tx_player_init(&tg_tux, &tg_sec, &tg_ctl, &tg_host, TX_BONUS_NONE, 0);
    tg_tux.col.user = &tg_tux;
    tx_sector_add(&tg_sec, &tg_tux.col);

    /* spawn smalltux below spawnpoint */
    if (tg_tux.bonus == TX_BONUS_NONE) {
        tx_player_set_pos(&tg_tux, tx_vec(x, y + 32));
    } else {
        tx_player_set_pos(&tg_tux, tx_vec(x, y));
    }
    /* spawning tux in the ground would kill him */
    if (!tx_is_free_of_tiles(&tg_sec, &tg_tux.col.bbox, false, TX_TILE_SOLID)) {
        TxVec npos = tx_obj_get_pos(&tg_tux.col);
        npos.y -= 32;
        tx_player_set_pos(&tg_tux, npos);
    }

    /* FIXME: This is a really dirty workaround for this strange camera jump */
    tc_init(&tg_cam, 1280.0f, 800.0f);
    tx_player_set_pos(&tg_tux, tx_vec_add(tx_obj_get_pos(&tg_tux.col), tx_vec(-32, 0)));
    tc_reset(&tg_cam, &tg_sec, tx_obj_get_pos(&tg_tux.col));
    tc_update(&tg_cam, &tg_sec, &tg_tux, 1);
    tx_player_set_pos(&tg_tux, tx_vec_add(tx_obj_get_pos(&tg_tux.col), tx_vec(32, 0)));
    tc_update(&tg_cam, &tg_sec, &tg_tux, 1);
}

/* A logical step, in the order of the original: the time, the input
 * (`key`, `val` arrive before it, with tg_key), the objects, the
 * collisions. */
static void tg_step_begin (void) {
    tg_dt = (1.0f / (1000.0f / 15.0f)) * tg_speed * 1.f;
    tx_game_time += tg_dt;
    tg_tick++;
    tx_controller_update(&tg_ctl);
}

/* `InputManager::reset`, when a level restarts. */
static void tg_keys_reset (void) {
    tx_controller_reset(&tg_ctl);
}

static void tg_key (int control, int value) {
    tx_controller_set_control(&tg_ctl, control, value);
}

/* The objects move: the player first (it has the priority of the object
 * manager), then the other objects, which are tasks in Atmos. */
static void tg_step_player (void) {
    tx_player_update(&tg_tux, tg_dt);
    tc_update(&tg_cam, &tg_sec, &tg_tux, tg_dt);
}

/* The collisions, and the objects that come and go. */
static void tg_step_end (void) {
    for (int i=0; i<tg_sec.maps_n; i++) {
        tx_map_clear_objects_hit_bottom(tg_sec.maps[i]);
    }
    tx_coll_update(&tg_sec);
    tg_flush();
}

/* The line `P` of the trace (tools/bench/README.md). */
static void tg_trace (FILE* f, int coins) {
    TxPlayer* p = &tg_tux;
    const char* d = (p->dir == TX_DIR_LEFT) ? "L" : (p->dir == TX_DIR_RIGHT) ? "R" :
                    (p->dir == TX_DIR_UP)   ? "U" : (p->dir == TX_DIR_DOWN)  ? "D" : "-";
    fprintf(f, "%ld P %.9g %.9g %.9g %.9g b=%d d=%s g=%d j=%d k=%d s=%d w=%d y=%d x=%d e=%d c=%d\n",
        tg_tick, p->col.bbox.x1, p->col.bbox.y1, p->physic.vx, p->physic.vy,
        p->bonus, d, p->on_ground_flag, p->jumping, p->duck,
        tx_timer_started(&p->skidding_timer), p->swimming,
        p->dying, p->dead, p->winning, coins);
    if (tg_trace_camera) {
        fprintf(f, "%ld C %.9g %.9g\n", tg_tick, tg_cam.translation.x, tg_cam.translation.y);
    }
    if (tg_trace_objs) {
        tg_bg_trace(f);
    }
}

#endif
