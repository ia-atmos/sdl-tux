/* The C side of the game: the sector with its tilemaps, the player, the
 * controller, and the trace, over the kernels translated from the
 * original (coll.h, player.h). Everything here has the prefix `tg_`. */

#ifndef TG_TUX_H
#define TG_TUX_H

#include <stdio.h>
#include "player.h"
#include "camera.h"

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

/* The function of the game that receives an event that the player gives
 * to an object: f(id, event), as `TriggerBase::event` (TX_EVENT_*). */
static AVal    tg_trigger;

enum { TG_HIT_LEFT=1, TG_HIT_RIGHT=2, TG_HIT_TOP=4, TG_HIT_BOTTOM=8, TG_HIT_CRUSH=16 };

static void tg_on_solid (TxObj* self, const TxHit* hit) {
    if (self->user == &tg_tux) {
        tx_player_collision_solid(&tg_tux, hit);
    }
}

static void tg_on_tile (TxObj* self, uint32_t attrs) {
    if (self->user == &tg_tux) {
        tx_player_collision_tile(&tg_tux, attrs);
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
    int bits = (hit->left   ? TG_HIT_LEFT   : 0) | (hit->right ? TG_HIT_RIGHT : 0)
             | (hit->top    ? TG_HIT_TOP    : 0) | (hit->bottom ? TG_HIT_BOTTOM : 0)
             | (hit->crush  ? TG_HIT_CRUSH  : 0);
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
    }
    tg_kinds[tg_objs_n] = kind;
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

/* The sector, with the attributes of `n` tiles still to fill. */
static void tg_sector (int n) {
    tx_sector_init(&tg_sec);
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
}

#endif
