/* The C side of the game: the sector with its tilemaps, the player, the
 * controller, and the trace, over the kernels translated from the
 * original (coll.h, player.h). Everything here has the prefix `tg_`. */

#ifndef TG_TUX_H
#define TG_TUX_H

#include <stdio.h>
#include "player.h"

static TxSector     tg_sec;
static TxPlayer     tg_tux;
static TxController tg_ctl;
static TxPlayerHost tg_host;
static long         tg_tick;

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
        return tx_player_collision(&tg_tux, TX_OTHER_OTHER, other, hit);
    }
    return TX_FORCE_MOVE;
}

/* The sector, with the attributes of `n` tiles still to fill. */
static void tg_sector (int n) {
    tx_sector_init(&tg_sec);
    tg_sec.tiles.count = n;
    tg_sec.tiles.attrs = calloc(n, sizeof(uint32_t));
    tg_sec.tiles.data  = calloc(n, sizeof(int));
    tg_sec.collision       = tg_on_collision;
    tg_sec.collision_solid = tg_on_solid;
    tg_sec.collision_tile  = tg_on_tile;
}

/* The end of a sector: its tilemaps, its objects, the player. */
static void tg_sector_free (void) {
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

/* The player at a spawn position, as `Sector::activate` places it. */
static void tg_player (float x, float y) {
    memset(&tg_host, 0, sizeof(tg_host));
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
}

/* A logical step, in the order of the original: the time, the input
 * (`key`, `val` arrive before it, with tg_key), the objects, the
 * collisions. */
static void tg_step_begin (void) {
    tx_game_time += 1.0f / (1000.0f / 15.0f);
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

static void tg_step_end (void) {
    float dt = 1.0f / (1000.0f / 15.0f);
    tx_player_update(&tg_tux, dt);
    for (int i=0; i<tg_sec.maps_n; i++) {
        tx_map_clear_objects_hit_bottom(tg_sec.maps[i]);
    }
    tx_coll_update(&tg_sec);
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
}

#endif
