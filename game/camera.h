/* The camera of a sector, in the mode "normal" and with a scale of 1:
 * a translation of `Camera` (src/object/camera.cpp of SuperTux v0.7.0),
 * keeping its 32-bit float arithmetic operation by operation, as
 * coll.h and player.h. The other modes (manual, autoscroll, scrollto),
 * the scale, the shake, and the earthquake are not translated yet.
 *
 * The names have the prefix `tc_`. */

#ifndef TC_CAMERA_H
#define TC_CAMERA_H

#include "player.h"

/* Very small value, used in Camera checks. */
static const float TC_CAMERA_EPSILON = .00001f;

/* GameConfig::camera_peek_multiplier */
static const float TC_PEEK_MULTIPLIER = 0.03f;

typedef struct TcCamera {
    TxVec screen_size;          /* m_screen_size (SCREEN_WIDTH, SCREEN_HEIGHT) */
    TxVec translation;          /* m_translation */
    TxVec lookahead_pos;        /* m_lookahead_pos */
    TxVec peek_pos;             /* m_peek_pos */
    TxVec cached_translation;   /* m_cached_translation */
    float earthquake_last_offset;
} TcCamera;

/* `static Vector last_player_pos` of update_scroll_normal: initialized at
 * the first call of the program, and kept from then on. */
static TxVec tc_last_player_pos;
static int   tc_last_player_pos_init = 0;

/* math::clamp */
static inline float tc_clamp (float val, float min, float max) {
    if (val < min) {
        return min;
    } else if (val > max) {
        return max;
    }
    return val;
}

/* Camera::Camera */
static void tc_init (TcCamera* c, float screen_w, float screen_h) {
    memset(c, 0, sizeof(*c));
    c->screen_size = tx_vec(screen_w, screen_h);
}

/* Camera::keep_in_bounds(Vector&), not in the mode "manual" */
static void tc_keep_in_bounds (TcCamera* c, const TxSector* s, TxVec* t) {
    float width = tx__sector_get_width(s);
    float height = tx__sector_get_height(s);

    /* Remove any earthquake offset from the translation. */
    t->y -= c->earthquake_last_offset;

    /* Don't scroll before the start or after the sector's end. */
    t->x = tc_clamp(t->x, 0.0f, width - c->screen_size.x);
    t->y = tc_clamp(t->y, 0.0f, height - c->screen_size.y);

    /* Add any earthquake offset we may have removed earlier. */
    t->y += c->earthquake_last_offset;

    if (height < c->screen_size.y)
        t->y = height / 2.0f - c->screen_size.y / 2.0f;
    if (width < c->screen_size.x)
        t->x = width / 2.0f - c->screen_size.x / 2.0f;
}

/* Camera::reset */
static void tc_reset (TcCamera* c, const TxSector* s, TxVec tuxpos) {
    c->translation.x = tuxpos.x - c->screen_size.x / 2.0f;
    c->translation.y = tuxpos.y - c->screen_size.y / 2.0f;
    tc_keep_in_bounds(c, s, &c->translation);
    c->cached_translation = c->translation;
}

/* Camera::update_scroll_normal */
static void tc_update_scroll_normal (TcCamera* c, const TxSector* s, const TxPlayer* player, float dt_sec) {
    const TxVec player_pos = tx_vec(tx_rect_left(&player->col.bbox),
                                    tx_rect_bottom(&player->col.bbox));
    if (!tc_last_player_pos_init) {
        tc_last_player_pos = player_pos;
        tc_last_player_pos_init = 1;
    }
    const TxVec player_delta = tx_vec_sub(player_pos, tc_last_player_pos);
    tc_last_player_pos = player_pos;

    /* Check that we don't have division by zero later. */
    if (dt_sec < TC_CAMERA_EPSILON)
        return;

    /****** Vertical Scrolling part. ******/
    if (!player->dying) {
        c->cached_translation.y = tc_clamp(c->cached_translation.y,
                                           player_pos.y - c->screen_size.y * 0.67f,
                                           player_pos.y - c->screen_size.y * 0.33f);
    }

    c->translation.y = c->cached_translation.y;

    if (!player->dying) {
        const float top_edge = c->screen_size.y * 0.3f;
        const float bottom_edge = c->screen_size.y * 0.7f;
        const float translation_compensation_y = player_pos.y - c->translation.y;
        float peek_to_y = 0;

        if (player->peekingY == TX_DIR_UP)
            peek_to_y = bottom_edge - translation_compensation_y;
        else if (player->peekingY == TX_DIR_DOWN)
            peek_to_y = top_edge - translation_compensation_y;

        if (c->translation.y + c->screen_size.y < tx__sector_get_height(s))
            c->peek_pos.y += (peek_to_y - c->peek_pos.y) * TC_PEEK_MULTIPLIER;

        c->translation.y -= c->peek_pos.y;

        c->translation.y = tc_clamp(c->translation.y,
                                    player_pos.y - c->screen_size.y * 0.7f,
                                    player_pos.y - c->screen_size.y * 0.3f);
        c->cached_translation.y = tc_clamp(c->cached_translation.y,
                                           player_pos.y - c->screen_size.y * 0.7f,
                                           player_pos.y - c->screen_size.y * 0.3f);
    }

    /****** Horizontal scrolling part *******/
    if (!player->dying) {
        const float left_end = c->screen_size.x * 0.4f;
        const float right_end = c->screen_size.x * 0.6f;

        if (player_delta.x < -TC_CAMERA_EPSILON || player_delta.x > TC_CAMERA_EPSILON) /* Walking left or right. */
            c->lookahead_pos.x -= player_delta.x * 0.8f;

        c->lookahead_pos.x = tc_clamp(c->lookahead_pos.x, left_end, right_end);

        /* Adjust for level ends. */
        if (player_pos.x < left_end)
            c->lookahead_pos.x = left_end;
        if (player_pos.x > tx__sector_get_width(s) - left_end)
            c->lookahead_pos.x = right_end;

        c->cached_translation.x = player_pos.x - c->lookahead_pos.x;
    }

    c->translation.x = c->cached_translation.x;

    if (!player->dying) {
        const float left_edge = c->screen_size.x * 0.1666f;
        const float right_edge = c->screen_size.x * 0.8334f;
        const float translation_compensation_x = player_pos.x - c->translation.x;
        float peek_to_x = 0;

        if (player->peekingX == TX_DIR_LEFT)
            peek_to_x = right_edge - translation_compensation_x;
        else if (player->peekingX == TX_DIR_RIGHT)
            peek_to_x = left_edge - translation_compensation_x;

        c->peek_pos.x += (peek_to_x - c->peek_pos.x) * TC_PEEK_MULTIPLIER;

        c->translation.x -= c->peek_pos.x;

        c->translation.x = tc_clamp(c->translation.x,
                                    player_pos.x - c->screen_size.x * 0.8334f,
                                    player_pos.x - c->screen_size.x * 0.1666f);
        c->cached_translation.x = tc_clamp(c->cached_translation.x,
                                           player_pos.x - c->screen_size.x * 0.8334f,
                                           player_pos.x - c->screen_size.x * 0.1666f);
    }

    tc_keep_in_bounds(c, s, &c->translation);
    tc_keep_in_bounds(c, s, &c->cached_translation);
}

/* Camera::update, in the mode "normal", with one player and a scale of 1
 * (update_scale adds `screen_size * (1 - 1) / 2`, which changes nothing). */
static void tc_update (TcCamera* c, const TxSector* s, const TxPlayer* player, float dt_sec) {
    tc_update_scroll_normal(c, s, player, dt_sec);
    c->translation = tx_vec_add(c->translation,
        tx_vec_div(tx_vec_scale(c->screen_size, 1.f - 1.f), 2.f));
}

#endif
