/* The drawing of the player: the choice of the action of its sprite, and
 * the sprite drawn (`Player::draw` of SuperTux v0.7.0). With one player:
 * no tag, no arrow of multiplayer; with no air arrow, no bubbles, and no
 * santa hat yet.
 *
 * The hooks of the sprite of the player (player.h) go to this sprite:
 * the actions that `Player::update` sets, and `animation_done`, which
 * ends the growth of Tux.
 *
 * The names have the prefix `tp_`. */

#ifndef TP_TUXDRAW_H
#define TP_TUXDRAW_H

static TsData   tp_data;            /* images/creatures/tux/tux.sprite */
static TsSprite tp_sprite;
static int      tp_ready;
static int      tp_layer_dying;     /* Sector::get_foremost_opaque_layer() + 1 */

static const char* TP_IDLE_STAGES[] = { "stand", "scratch", "idle" };
#define TP_IDLE_N 3

/* MovingSprite::set_action(name, loops) of the player: no hitbox */
static void tp_set_action (const char* name, int loops) {
    ts_set_action(&tp_sprite, name, loops);
}

/* --------------------------------------------------------------------- */
/* The hooks of player.h                                                  */
/* --------------------------------------------------------------------- */

static void tp_h_set_angle (void* u, float a) { (void) u; tp_sprite.angle = a; }
static float tp_h_get_angle (void* u) { (void) u; return tp_sprite.angle; }
static bool tp_h_animation_done (void* u) { (void) u; return ts_animation_done(&tp_sprite); }
static void tp_h_pause (void* u) { (void) u; tp_sprite.is_paused = 1; }
static void tp_h_resume (void* u) {
    (void) u;
    /* Sprite::resume_animation */
    if (tp_sprite.is_paused) {
        tp_sprite.is_paused = 0;
        tp_sprite.last_ticks = tx_game_time;
    }
}
static void tp_h_set_action (void* u, const char* name, int dir, int loops) {
    (void) u;
    char buf[TS_NAME + 8];
    if (dir == TX_DIR_NONE) {
        snprintf(buf, sizeof(buf), "%s", name);
    } else {
        const char* d = (dir == TX_DIR_LEFT) ? "left" : (dir == TX_DIR_RIGHT) ? "right" :
                        (dir == TX_DIR_UP) ? "up" : (dir == TX_DIR_DOWN) ? "down" : "auto";
        snprintf(buf, sizeof(buf), "%s-%s", name, d);
    }
    tp_set_action(buf, loops);
}

/* The hooks, in the host of the player, when a life starts. */
static void tp_hooks (TxPlayerHost* h) {
    if (!tp_ready) return;
    ts_init(&tp_sprite, &tp_data);
    h->sprite_set_angle = tp_h_set_angle;
    h->sprite_get_angle = tp_h_get_angle;
    h->sprite_animation_done = tp_h_animation_done;
    h->sprite_pause_animation = tp_h_pause;
    h->sprite_resume_animation = tp_h_resume;
    h->sprite_set_action = tp_h_set_action;
}

/* --------------------------------------------------------------------- */
/* Player::draw                                                           */
/* --------------------------------------------------------------------- */

static int tp_has_idle (const char* action) {
    for (int i=0; i<TP_IDLE_N; i++) {
        char k[24];
        snprintf(k, sizeof(k), "-%s-", TP_IDLE_STAGES[i]);
        if (strstr(action, k) != NULL) return 1;
    }
    return 0;
}

/* The action of the sprite, before the player is drawn. The layer of the
 * player: LAYER_OBJECTS + 1, or the foremost one when dying. */
static int tp_choose (TxPlayer* p) {
    const char* sa_prefix;
    char sa_postfix[8];
    char buf[TS_NAME + 8];
#define TP_ACT(...) (snprintf(buf, sizeof(buf), __VA_ARGS__), buf)

    switch (p->bonus) {
        case TX_BONUS_GROWUP: sa_prefix = "big";   break;
        case TX_BONUS_FIRE:   sa_prefix = "fire";  break;
        case TX_BONUS_ICE:    sa_prefix = "ice";   break;
        case TX_BONUS_AIR:    sa_prefix = "air";   break;
        case TX_BONUS_EARTH:  sa_prefix = "earth"; break;
        default:              sa_prefix = "small"; break;
    }
    if (!p->swimming && !p->water_jump) {
        snprintf(sa_postfix, sizeof(sa_postfix), "%s", (p->dir == TX_DIR_RIGHT) ? "-right" : "-left");
    } else {
        snprintf(sa_postfix, sizeof(sa_postfix), "%s",
            ((fabsf(p->swimming_angle) <= (float) (M_PI / 2))
             || (p->water_jump && fabsf(p->physic.vx) < 10.f)) ? "-right" : "-left");
    }
    const char* cur = ts_act(&tp_sprite) ? ts_act(&tp_sprite)->name : "";

    /* Set Tux sprite action */
    if (p->dying) {
        tp_sprite.angle = 0.0f;
        tp_set_action("gameover", -1);
    } else if (p->growing) {
        const char* action = "grow";
        if (p->swimming || p->water_jump) action = "swimgrow";
        else if (p->sliding) action = "slidegrow";
        else if (p->climbing) action = "climbgrow";
        tp_set_action(TP_ACT("%s%s", action, sa_postfix), TS_LOOPS_CONTINUED);
    } else if (p->stone) {
        tp_set_action("earth-stone", -1);
    } else if (p->climbing) {
        tp_set_action(TP_ACT("%s-climb%s", sa_prefix, sa_postfix), -1);
        if (p->physic.vx == 0 && p->physic.vy == 0) tp_sprite.is_paused = 1;
    } else if (p->backflipping) {
        tp_set_action(TP_ACT("%s-backflip%s", sa_prefix, sa_postfix), -1);
    } else if (p->sliding) {
        if (p->jumping || p->is_slidejump_falling) {
            tp_set_action(TP_ACT("%s-slidejump%s", sa_prefix, sa_postfix), -1);
        } else {
            const int was_growing_before = (strncmp(cur, "slidegrow", 9) == 0);
            tp_set_action(TP_ACT("%s-slide%s", sa_prefix, sa_postfix), -1);
            if (p->was_crawling_before_slide || was_growing_before) {
                /* Sprite::set_frame(get_frames()): skip the "duck" animation */
                tp_sprite.frameidx = ts_frames(&tp_sprite);
                p->was_crawling_before_slide = false;
            }
        }
    } else if (p->duck && tx_player_is_big(p) && !p->swimming && !p->crawl && !p->stone) {
        tp_set_action(TP_ACT("%s-duck%s", sa_prefix, sa_postfix), -1);
    } else if (p->crawl) {
        if (p->on_ground_flag) {
            tp_set_action(TP_ACT("%s-crawl%s", sa_prefix, sa_postfix), -1);
            if (p->physic.vx != 0.f) tp_h_resume(NULL);
            else tp_sprite.is_paused = 1;
        } else {
            tp_set_action(TP_ACT("%s-slidejump%s", sa_prefix, sa_postfix), -1);
        }
    } else if (tx_timer_started(&p->skidding_timer) && !tx_timer_check(&p->skidding_timer) && !p->swimming) {
        tp_set_action(TP_ACT("%s-skid%s", sa_prefix, sa_postfix), -1);
    } else if (tx_timer_started(&p->kick_timer) && !tx_timer_check(&p->kick_timer) && !p->swimming && !p->water_jump) {
        tp_set_action(TP_ACT("%s-kick%s", sa_prefix, sa_postfix), -1);
    } else if ((p->wants_buttjump || p->does_buttjump) && tx_player_is_big(p) && !p->water_jump) {
        if (p->buttjump_stomp) tp_set_action(TP_ACT("%s-stomp%s", sa_prefix, sa_postfix), 1);
        else tp_set_action(TP_ACT("%s-buttjump%s", sa_prefix, sa_postfix), 1);
    } else if ((tx_controller_hold(p->controller, TX_CONTROL_LEFT) || tx_controller_hold(p->controller, TX_CONTROL_RIGHT)) && p->can_walljump) {
        tp_set_action(TP_ACT("%s-walljump%s", sa_prefix, p->on_left_wall ? "-left" : "-right"), 1);
    } else if (!p->on_ground_flag || p->fall_mode != TX_FALL_ON_GROUND) {
        if (p->physic.vx != 0 || p->fall_mode != TX_FALL_ON_GROUND) {
            if (p->swimming || p->water_jump) {
                if (tx_vec_length(tx_vec(p->physic.vx, p->physic.vy)) < 50.f)
                    tp_set_action(TP_ACT("%s-float%s", sa_prefix, sa_postfix), -1);
                else if (p->water_jump)
                    tp_set_action(TP_ACT("%s-swimjump%s", sa_prefix, sa_postfix), -1);
                else if (p->swimboosting)
                    tp_set_action(TP_ACT("%s-boost%s", sa_prefix, sa_postfix), -1);
                else
                    tp_set_action(TP_ACT("%s-swim%s", sa_prefix, sa_postfix), -1);
            } else {
                if (p->physic.vy > 0)
                    tp_set_action(TP_ACT("%s-fall%s", sa_prefix, sa_postfix), -1);
                else if (p->physic.vy <= 0)
                    tp_set_action(TP_ACT("%s-jump%s", sa_prefix, sa_postfix), -1);
            }
        }
    } else {
        if (fabsf(p->physic.vx) < 1.0f) {
            const int is_not_idle = !tp_has_idle(cur);
            if (is_not_idle || (p->should_fancy_idle && !p->fancy_idle_active) || p->reset_action) {
                p->idle_stage = 0;
                tx_timer_start(&p->idle_timer, (float) TX_TIME_UNTIL_IDLE / 1000.0f);
                tp_set_action(TP_ACT("%s-%s%s", sa_prefix, TP_IDLE_STAGES[p->idle_stage], sa_postfix), TS_LOOPS_CONTINUED);
                if (!p->should_fancy_idle) {
                    tp_sprite.animation_loops = -1;
                    p->fancy_idle_active = false;
                } else {
                    p->fancy_idle_active = true;
                }
                if (p->reset_action) p->reset_action = false;
            } else if (p->should_fancy_idle) {
                if (tx_timer_check(&p->idle_timer) || ts_animation_done(&tp_sprite)) {
                    p->idle_stage++;
                    if (p->idle_stage >= TP_IDLE_N) {
                        p->idle_stage = TP_IDLE_N - 1;
                        tp_set_action(TP_ACT("%s-%s%s", sa_prefix, TP_IDLE_STAGES[p->idle_stage], sa_postfix), -1);
                        tp_sprite.animation_loops = -1;
                    } else {
                        tp_set_action(TP_ACT("%s-%s%s", sa_prefix, TP_IDLE_STAGES[p->idle_stage], sa_postfix), 1);
                    }
                }
            } else {
                char want[TS_NAME + 8];
                snprintf(want, sizeof(want), "%s-%s%s", sa_prefix, TP_IDLE_STAGES[0], sa_postfix);
                if (p->idle_stage != 0 || strcmp(cur, want) != 0) {
                    p->idle_stage = 0;
                    tp_set_action(want, -1);
                    tp_sprite.animation_loops = -1;
                }
                p->fancy_idle_active = false;
            }
        } else {
            if (fabsf(p->physic.vx) >= TX_MAX_RUN_XM - 3)
                tp_set_action(TP_ACT("%s-run%s", sa_prefix, sa_postfix), -1);
            else
                tp_set_action(TP_ACT("%s-walk%s", sa_prefix, sa_postfix), -1);
            p->fancy_idle_active = false;
        }
    }
#undef TP_ACT
    return p->dying ? tp_layer_dying : 50 + 1;     /* LAYER_OBJECTS + 1 */
}

/* The sprite drawn, after its action was chosen */
static void tp_draw (TxPlayer* p, TxVec translation, float sw, float sh) {
    if (!p->visible || (tx_timer_started(&p->post_damage_safety_timer) && (size_t) (tx_game_time * 40) % 2)) {
        /* don't draw Tux */
    } else {
        ts_draw(&tp_sprite, tx_obj_get_pos(&p->col), translation, sw, sh);
    }
    /* TODO: Replace recoloring with proper costumes */
    float c[3] = { 1.f, 1.f, 1.f };
    switch (p->bonus) {
        case TX_BONUS_FIRE:  c[0] = 1.f;  c[1] = 0.7f; c[2] = 0.5f; break;
        case TX_BONUS_ICE:   c[0] = 0.7f; c[1] = 1.f;  c[2] = 1.f;  break;
        case TX_BONUS_AIR:   c[0] = 0.7f; c[1] = 1.f;  c[2] = 0.5f; break;
        case TX_BONUS_EARTH: c[0] = 1.f;  c[1] = 0.9f; c[2] = 0.6f; break;
    }
    if (p->stone) { c[0] = c[1] = c[2] = 1.f; }
    tp_sprite.color[0] = c[0]; tp_sprite.color[1] = c[1]; tp_sprite.color[2] = c[2]; tp_sprite.color[3] = 1.f;
}

#endif
