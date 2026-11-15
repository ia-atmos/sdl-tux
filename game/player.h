/*
 * player.h -- the logic of the class Player of SuperTux v0.7.0
 * (src/object/player.cpp/.hpp), translated from C++17 to C99, one method at a
 * time, statement by statement, keeping the 32-bit float arithmetic of the
 * original operation by operation. It builds on coll.h and follows it.
 *
 * SuperTux is (C) the SuperTux team, GPL v3 or later; this file is a
 * translation of these sources and carries the same licence:
 *   src/object/player.cpp/.hpp          (C) 2006 Matthias Braun
 *   src/supertux/timer.cpp/.hpp         (Timer)
 *   src/control/controller.cpp/.hpp, codecontroller.cpp (Controller)
 *   src/math/util.hpp, src/math/vector.hpp (math::)
 *   src/supertux/game_object_manager.cpp (get_width, get_height)
 *   src/supertux/player_status.hpp      (BonusType)
 *
 * Every C function carries the name of the C++ method and its line in
 * player.cpp ("L384" = line 384 of src/object/player.cpp of v0.7.0).
 *
 * ===========================================================================
 * HOW THE HOST DRIVES IT
 * ===========================================================================
 *  - tx_game_time is g_game_time. ScreenManager::loop_iter does, per logical
 *    step: `g_game_time += dtime;` then process_events() (which calls
 *    Controller::update() of every controller: tx_controller_update, then the
 *    new key states: tx_controller_set_control), then the game logic
 *    (Sector::update: every object's update -> tx_player_update, then
 *    CollisionSystem::update -> tx_coll_update), then the draw.
 *  - TxPlayer.col is the CollisionObject (m_col). tx_player_init leaves it out
 *    of the sector: the host calls tx_sector_add(sector, &player.col), sets
 *    player.col.user, and routes the callbacks of TxSector to
 *    tx_player_collision_solid / tx_player_collision_tile /
 *    tx_player_collision. col.physic_hint points into the TxPlayer: do not
 *    copy or move a TxPlayer after tx_player_init.
 *  - The bonus (m_player_status.bonus[m_id]) is kept in TxPlayer.bonus: it is
 *    all the translated code needs of PlayerStatus besides three calls that
 *    are hooks (respawns_at_checkpoint, take_checkpoint_coins,
 *    add_item_to_pocket). The coins stay with the host.
 *  - The grabbed object (Portable*, always also a MovingObject) and the
 *    Climbable are `TxObj*`: their bbox is read from the TxObj, everything
 *    else they do is a hook.
 *
 * ===========================================================================
 * HOOKS (TxPlayerHost) -- each is called exactly where the C++ makes the
 * call, in the same order. A null hook does nothing and returns the neutral
 * value given here.
 * ===========================================================================
 *  play_sound(user, name, pos)   SoundManager::current()->play(name, pos):
 *        L614 splash, L756 grow, L1303 skid, L1427 flip, L1455 bigjump,
 *        L1457 jump, L1571 bigjump/jump, L1681 shoot, L2545 hurt, L2570 kill,
 *        L3147 brick; L2514 invincible_start.ogg has no position (pos NULL).
 *  stop_looping_sounds(user)     L629 Sector::get().stop_looping_sounds().
 *  add_particles(user, epicenter, min_angle, max_angle, min_initial_velocity,
 *        max_initial_velocity, acceleration, number, r, g, b, size,
 *        life_time, drawing_layer)
 *        Sector::get().add<Particles>(...): L1305 (skid dust), L2411 and
 *        L2415 (buttjump dust).
 *  add_sprite_particle(user, sprite_name, action, position, velocity,
 *        acceleration, drawing_layer, notimeout)
 *        Sector::get().add<SpriteParticle>(..., ANCHOR_MIDDLE, ...): L792
 *        (invincibility sparkle), L3142 (rocks of stop_rolling).
 *  camera_shake(user, duration, x, y)   L2419 get_camera().shake(.1f, 0.f, 10.f).
 *  sprite_set_angle(user, angle)        m_sprite->set_angle: L271, L692,
 *        L744, L884, L1161, L1171, L1621, L2541, L2783, L2946, L3127.
 *  sprite_get_angle(user) -> float      L692 m_sprite->get_angle(). Null: 0.0f.
 *  sprite_animation_done(user) -> bool  L805 m_sprite->animation_done().
 *        Null: true (m_growing ends at the first update).
 *  sprite_pause_animation(user)         L813.
 *  sprite_resume_animation(user)        L818.
 *  sprite_set_action(user, name, dir, loops)   L2000-L2006
 *        MovingSprite::set_action(name, dir, 1) of set_bonus.
 *  graphics_rand_i(user, lo, hi) -> int   graphicsRandom.rand(lo, hi), an
 *        integer in [lo, hi): L558, L562, L785. Null: lo.
 *  graphics_rand_f(user, lo, hi) -> float graphicsRandom.randf(lo, hi), and
 *        randf(v) as (0.0f, v) (Random::randf(v) builds
 *        uniform_real_distribution<float>(0.0f, v)): L266, L581, L582, L594,
 *        L787, L788, L2597 (two), L2598, L3140 (two), L3141. Null: lo.
 *        (player.cpp never uses gameRandom: there is no hook for it.)
 *  break_bricks(user, which, box, stone, sliding, fast, is_big)
 *        the three loops over Sector::get().get_objects_by_type<Brick>():
 *        one call per loop; the host walks its bricks in the order of the
 *        Sector and calls brick.try_break(player, is_big) on each brick for
 *        which
 *          TX_BRICKS_SIDE (L910): box.overlaps(brick.get_bbox()) && (stone ||
 *            (sliding && brick.get_class_name() != "heavy-brick")) && fast
 *            (fast is `std::abs(m_physic.get_velocity_x()) >= 150.f`)
 *          TX_BRICKS_DOWN (L923): box.overlaps(brick.get_bbox()) && (stone ||
 *            !dynamic_cast<HeavyBrick*>(&brick))
 *          TX_BRICKS_TOP  (L939): box.overlaps(brick.get_bbox())
 *  kill_fall_badguys(user, box)   L928 loop over get_objects_by_type<BadGuy>():
 *        badguy.kill_fall() on each badguy for which
 *        box.overlaps(badguy.get_bbox()) && badguy.is_snipable() &&
 *        !badguy.is_grabbed().
 *  portable_is_hampering(user, obj) -> bool   L1264. Null: false.
 *  portable_grab(user, obj, pos, dir)         L1831 m_grabbed_object->grab.
 *  portable_ungrab(user, obj, dir)            L1756, L1760, L1764, L1769, L2997.
 *  object_set_pos(user, obj, pos)             L1753, L1830
 *        moving_object->set_pos(pos) (virtual). Null: tx_obj_set_pos(obj, pos)
 *        (MovingObject::set_pos, the base behaviour).
 *  object_add_remove_listener(user, obj)      L1873.
 *  object_del_remove_listener(user, obj)      L1771, L3002.
 *  find_portable(user, pos) -> TxObj*   L1858 loop over
 *        get_objects_by_type<MovingObject>(): the first object, in the order
 *        of the Sector, that is a Portable with is_portable() &&
 *        !is_grabbed(), whose group is not COLGROUP_DISABLED and whose bbox
 *        contains pos (tx_rect_contains). Null: NULL (nothing to grab).
 *  count_active_bullets(user) -> int    L1663 get_object_count<Bullet>(of
 *        this player). Null: 0.
 *  add_bullet(user, pos, xm, dir, bonus, is_waterlogged)   L1676.
 *  start_sequence(user, seq, data)      L380 GameSession::start_sequence.
 *  session_prevent_death(user) -> bool  L2562-L2564 `session &&
 *        session->m_prevent_death && !session->reset_checkpoint_button`.
 *        Null: false.
 *  count_alive_players(user) -> int     L627 and L2587
 *        get_object_count<Player>(is_alive). Null: 0 (the single player is
 *        dying or dead at both places).
 *  status_respawns_at_checkpoint(user) -> bool   L2591. Null: false.
 *  add_falling_coin(user, start_position, x_vel) L2596.
 *  status_take_checkpoint_coins(user)   L2600.
 *  status_add_item_to_pocket(user, bonus)   L2025
 *        m_player_status.add_item_to_pocket(get_bonus(), this).
 *  effect_fade_out(user, time)          L2603 get_effect().fade_out(3.0).
 *  pause_music(user, time)              L2604 pause_music(3.0).
 *  play_music(user, type)               L2516 MusicObject::play_music(HERRING_MUSIC).
 *  trigger_event(user, other, event)    L2473 trigger->event(*this, EVENT_ACTIVATE).
 *  badguy_is_frozen(user, other) -> bool        L2485. Null: false.
 *  badguy_get_velocity_y(user, other) -> float  L2485
 *        badguy->get_physic().get_velocity_y(), only called when is_frozen
 *        answered true. Null: 0.0f.
 *  move_object_to_sector(user, obj, other_sector)   L330 (obj = the grabbed
 *        object) and L337 (obj = &player.col) get_parent()->move_object.
 *
 * ===========================================================================
 * NOT TRANSLATED (each one is also marked at its place in the code)
 * ===========================================================================
 *  NT1  L3169 register_class (scripting), and the scripting overloads taking
 *       strings: add_bonus(string) L1953, set_bonus(string) L1959,
 *       string_to_bonus L1911, bonus_to_string L1933, trigger_sequence(string)
 *       L363/L369, get_input_pressed/held/released L2730-L2745, has_grabbed
 *       L2969, get_action L2056.
 *  NT2  Multiplayer: next_target L3008, prev_target L3049,
 *       multiplayer_prepare_spawn L3080, multiplayer_respawn L3097; in update
 *       the block L430-L456 (m_target is null with one player, so the block
 *       is never entered) and the `else next_target();` of L631-L634.
 *  NT3  The tag/label of the player number: update L389-L410 (m_tag_timer,
 *       m_tag_fade, m_tag_alpha, m_has_moved) and handle_input L1588-L1592.
 *  NT4  The swim bubbles' drawing state: update L412-L427 (movement and
 *       removal of m_active_bubbles) and, inside L532-L595, the positions and
 *       sprites of the new bubbles. The timer and every graphicsRandom call
 *       of that block are kept, in order.
 *  NT5  Item pocket: eject_item_pocket L2951, get_item_pocket L2957,
 *       set_item_pocket L2963 (the call of set_bonus L2025 is a hook).
 *  NT6  Keys: m_collected_keys, add_collected_key L3153, remove_collected_key
 *       L3159, and the loops `for (Key* key : m_collected_keys)` of
 *       move_to_sector L333, set_pos L2618 and set_pos_reset L2639.
 *  NT7  draw L2062: nothing of it is translated. What it does to the state
 *       that update reads (checked statement by statement):
 *        - m_skidding_timer.check() (L2192) and m_kick_timer.check() (L2195)
 *          set m_period = 0 once the timer has run out. update only reads
 *          m_skidding_timer.started(), which is already false exactly when
 *          check() would clear the period (`g - start >= period` is
 *          `period - (g - start) <= 0` in IEEE floats), and never reads
 *          m_kick_timer: no effect.
 *        - set_action(...) and Sprite::update() (called by Sprite::draw)
 *          drive the animation that m_sprite->animation_done() reports at
 *          L805, which ends m_growing; m_growing is read by the input code.
 *          This is the one way draw reaches update: the hook
 *          sprite_animation_done must model it (the "grow" action started by
 *          set_bonus(..., animate = true) or re-set by draw with
 *          LOOPS_CONTINUED, played once).
 *        - m_sprite->set_angle(0.0f) when dying (L2126): the angle is only
 *          read back at L692 (stone), into the sprite again.
 *        - m_was_crawling_before_slide = false (L2169), m_reset_action,
 *          m_idle_stage, m_idle_timer, m_fancy_idle_active: written by draw,
 *          read only by draw (update and the input code only write
 *          m_was_crawling_before_slide and m_reset_action).
 *       The choice of the sprite action is not translated.
 *  NT8  The constructor: the sprite, m_multiplayer_arrow, m_airarrow,
 *       m_bubbles_sprite, m_name, the SoundManager preloads L256-L265,
 *       m_grabbed_object_remove_listener (the listener itself; adding and
 *       removing it are hooks). `m_controller(&InputManager::current()->
 *       get_controller(player_id))` is the argument `controller`.
 *  NT9  set_id L284, remove_me L2504 (InputManager).
 *  NT10 The assert()s: update L387, handle_input L1714, position_grabbed_object
 *       L1809. The official build is Release (NDEBUG).
 *  NT11 The log_debug / log_warning lines: L1993, L2016, L2767, L2772, L2823.
 *  NT12 Physic::get_movement reads Sector::get().get_gravity(): here
 *       TxPlayer.sector->gravity (coll.h NT3). Sector::get().get_width() /
 *       get_height() are computed from TxSector.maps as
 *       GameObjectManager::get_width / get_height do.
 *
 * ===========================================================================
 * FLOAT RULES FOLLOWED (see also coll.h)
 * ===========================================================================
 *  - Every constant of the anonymous namespace is a macro
 *    `((float)<the literal as written>)`: `const float X = 300;` is
 *    ((float)300), `const float JUMP_EARLY_APEX_FACTOR = 3.0;` is
 *    ((float)3.0) (a double literal converted once, as the C++ does).
 *  - player.cpp has no double arithmetic: the double literals it has are all
 *    arguments converted to a float parameter (start(0.5), start(3.0),
 *    fade_out(3.0), pause_music(3.0)). std::abs / std::cos / std::sin on
 *    floats are fabsf / cosf / sinf; math::angle uses atan2f as the C++ does.
 *  - Mixed int/float operations (`vx - 5`, `ax *= 2`, `BONUS_RUN_XM * (cond ?
 *    1 : 0)`, `x + 0`) are kept as written: C converts the int to float as
 *    C++ does.
 *  - ORDER OF EVALUATION. Two C++ expressions call graphicsRandom several
 *    times inside one argument list, where C++ leaves the order unspecified:
 *    L2596-L2598 (falling coins) and L3140 (rocks). The official build is
 *    g++ 13 on x86-64, which evaluates arguments right to left (checked with
 *    a test program at -O0 and -O3: L2596 draws randf(-100, 100), then
 *    randf(-32, 18), then randf(5); L3140 draws randf(-200, -150), then
 *    randf(-100, 100)). That order is used when
 *    TX_PLAYER_ARGS_RIGHT_TO_LEFT is 1 (the default); define it to 0 for a
 *    game built with clang (left to right).
 */
#ifndef TX_PLAYER_H
#define TX_PLAYER_H

#include <stdio.h>
#include "coll.h"

#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#endif

#ifndef TX_PLAYER_ARGS_RIGHT_TO_LEFT
#define TX_PLAYER_ARGS_RIGHT_TO_LEFT 1
#endif

/* ------------------------------------------------------------------------ */
/* supertux/globals.cpp : float g_game_time = 0;                             */
/* The host advances it as ScreenManager does: `g_game_time += dtime;` once  */
/* per logical step, before the input and the game logic of that step.       */
/* ------------------------------------------------------------------------ */
static float tx_game_time;

/* ------------------------------------------------------------------------ */
/* math/util.hpp, math/vector.hpp                                            */
/* ------------------------------------------------------------------------ */

/* constexpr float TAU = 6.28318530717958647693f; */
#define TX_TAU 6.28318530717958647693f
/* constexpr float PI = 3.14159265358979323846f; */
#define TX_PI 3.14159265358979323846f
/* constexpr float PI_2 = 1.57079632679489661923f; */
#define TX_PI_2 1.57079632679489661923f
/* constexpr float PI_4 = 0.78539816339744830962f; */
#define TX_PI_4 0.78539816339744830962f

/* math::clamp<float> */
TX_FN float tx_math_clamp(float val, float min, float max)
{
  if (val < min)
  {
    return min;
  }
  else if (val > max)
  {
    return max;
  }
  else
  {
    return val;
  }
}

/* math::degrees */
TX_FN float tx_math_degrees(float rad)
{
  return rad / TX_TAU * 360.0f;
}

/* math::radians */
TX_FN float tx_math_radians(float deg)
{
  return deg / 360.0f * TX_TAU;
}

/* math::vec2_from_polar: Vector(cosf(angle), sinf(angle)) * length */
TX_FN TxVec tx_math_vec2_from_polar(float length, float angle)
{
  return tx_vec_scale(tx_vec(cosf(angle), sinf(angle)), length);
}

/* math::angle: (v.x == 0 && v.y == 0) ? 0 : atan2f(v.y, v.x) */
TX_FN float tx_math_angle(TxVec v)
{
  return (v.x == 0 && v.y == 0) ? 0 : atan2f(v.y, v.x);
}

/* math::at_angle: vec2_from_polar(glm::length(v), angle) */
TX_FN TxVec tx_math_at_angle(TxVec v, float angle)
{
  return tx_math_vec2_from_polar(tx_vec_length(v), angle);
}

/* ------------------------------------------------------------------------ */
/* supertux/timer.cpp/.hpp : Timer                                           */
/* ------------------------------------------------------------------------ */

typedef struct TxTimer {
  float period;       /* m_period */
  float cycle_start;  /* m_cycle_start */
  float cycle_pause;  /* m_cycle_pause */
  bool cyclic;        /* m_cyclic */
} TxTimer;

/* Timer::Timer */
TX_FN void tx_timer_init(TxTimer* t)
{
  t->period = 0;
  t->cycle_start = 0;
  t->cycle_pause = 0;
  t->cyclic = false;
}

/* Timer::start(float period, bool cyclic = false)
   start the timer with the given period (in seconds). If cyclic=true then
   the timer will be reset after each period. Set period to zero if you want
   to disable the timer. */
TX_FN void tx_timer_start_cyclic(TxTimer* t, float period, bool cyclic)
{
  t->period = period;
  t->cyclic = cyclic;
  t->cycle_start = tx_game_time;
  t->cycle_pause = 0;
}

/* Timer::start(period) with the default cyclic = false */
TX_FN void tx_timer_start(TxTimer* t, float period)
{
  tx_timer_start_cyclic(t, period, false);
}

/* Timer::check
   returns true if a period (or more) passed since start call or last
   successful check */
TX_FN bool tx_timer_check(TxTimer* t)
{
  if (t->period == 0)
    return false;

  if (tx_game_time - t->cycle_start >= t->period) {
    if (t->cyclic) {
      t->cycle_start = tx_game_time - fmodf(tx_game_time - t->cycle_start, t->period);
    } else {
      t->period = 0;
    }
    return true;
  }

  return false;
}

/* Timer::stop: start(0) */
TX_FN void tx_timer_stop(TxTimer* t) { tx_timer_start(t, 0); }

/* Timer::get_period: the period of the timer or 0 if it isn't started */
TX_FN float tx_timer_get_period(const TxTimer* t) { return t->period; }
/* Timer::get_timeleft */
TX_FN float tx_timer_get_timeleft(const TxTimer* t) { return t->period - (tx_game_time - t->cycle_start); }
/* Timer::get_timegone */
TX_FN float tx_timer_get_timegone(const TxTimer* t) { return tx_game_time - t->cycle_start; }
/* Timer::get_progress */
TX_FN float tx_timer_get_progress(const TxTimer* t) { return tx_timer_get_timegone(t) / tx_timer_get_period(t); }
/* Timer::started */
TX_FN bool tx_timer_started(const TxTimer* t) { return (t->period != 0 && tx_timer_get_timeleft(t) > 0); }
/* Timer::paused */
TX_FN bool tx_timer_paused(const TxTimer* t) { return t->cycle_pause != 0; }

/* Timer::pause */
TX_FN void tx_timer_pause(TxTimer* t)
{
  float left = tx_timer_get_timeleft(t);
  tx_timer_stop(t);
  t->cycle_pause = left;
}

/* Timer::resume */
TX_FN void tx_timer_resume(TxTimer* t)
{
  tx_timer_start(t, t->cycle_pause);
}

/* ------------------------------------------------------------------------ */
/* control/controller.cpp/.hpp : enum class Control, Controller              */
/* ------------------------------------------------------------------------ */
enum {
  TX_CONTROL_LEFT = 0,
  TX_CONTROL_RIGHT,
  TX_CONTROL_UP,
  TX_CONTROL_DOWN,

  TX_CONTROL_JUMP,
  TX_CONTROL_ACTION,
  TX_CONTROL_ITEM,

  TX_CONTROL_START,
  TX_CONTROL_ESCAPE,
  TX_CONTROL_MENU_SELECT,
  TX_CONTROL_MENU_SELECT_SPACE,
  TX_CONTROL_MENU_BACK,
  TX_CONTROL_REMOVE,

  TX_CONTROL_CHEAT_MENU,
  TX_CONTROL_DEBUG_MENU,
  TX_CONTROL_CONSOLE,

  TX_CONTROL_PEEK_LEFT,
  TX_CONTROL_PEEK_RIGHT,
  TX_CONTROL_PEEK_UP,
  TX_CONTROL_PEEK_DOWN,

  TX_CONTROL_COUNT
};

/* Controller. hold is m_controls; pressed and released are what
   Controller::pressed() / released() compute from m_controls and
   m_old_controls, refreshed by every function below that changes one of the
   two (so they always hold the value the C++ method would return). */
typedef struct TxController {
  bool hold[TX_CONTROL_COUNT];          /* m_controls: hold(control) */
  bool pressed[TX_CONTROL_COUNT];       /* !m_old_controls[c] && m_controls[c] */
  bool released[TX_CONTROL_COUNT];      /* m_old_controls[c] && !m_controls[c] */
  bool old_controls[TX_CONTROL_COUNT];  /* m_old_controls */
  bool touchscreen;                     /* m_touchscreen */
  bool jump_key_pressed;                /* m_jump_key_pressed */
} TxController;

TX_FN void tx__controller_refresh(TxController* c, int control)
{
  c->pressed[control] = !c->old_controls[control] && c->hold[control];
  c->released[control] = c->old_controls[control] && !c->hold[control];
}

/* Controller::reset (and Controller::Controller, which calls it) */
TX_FN void tx_controller_reset(TxController* c)
{
  int i;
  for (i = 0; i < TX_CONTROL_COUNT; ++i) {
    c->hold[i] = false;
    c->old_controls[i] = false;
    tx__controller_refresh(c, i);
  }
  c->touchscreen = false;
  c->jump_key_pressed = false;
}

/* Controller::set_control */
TX_FN void tx_controller_set_control(TxController* c, int control, bool value)
{
  if (control == TX_CONTROL_JUMP) {
    c->jump_key_pressed = value;
  }
  c->hold[control] = value;
  tx__controller_refresh(c, control);
}

/* Controller::set_jump_key_with_up */
TX_FN void tx_controller_set_jump_key_with_up(TxController* c, bool value)
{
  /* Do not release the jump key if the jump key is still pressed. */
  if (!c->jump_key_pressed) {
    c->hold[TX_CONTROL_JUMP] = value;
    tx__controller_refresh(c, TX_CONTROL_JUMP);
  }
}

/* Controller::set_touchscreen / is_touchscreen */
TX_FN void tx_controller_set_touchscreen(TxController* c, bool value) { c->touchscreen = value; }
TX_FN bool tx_controller_is_touchscreen(const TxController* c) { return c->touchscreen; }

/* Controller::hold / pressed / released */
TX_FN bool tx_controller_hold(const TxController* c, int control) { return c->hold[control]; }
TX_FN bool tx_controller_pressed(const TxController* c, int control) { return c->pressed[control]; }
TX_FN bool tx_controller_released(const TxController* c, int control) { return c->released[control]; }

/* Controller::update (InputManager::update calls it once per logical step,
   before the events of that step) */
TX_FN void tx_controller_update(TxController* c)
{
  int i;
  for (i = 0; i < TX_CONTROL_COUNT; ++i) {
    c->old_controls[i] = c->hold[i];
    tx__controller_refresh(c, i);
  }
}

/* CodeController::press */
TX_FN void tx_code_controller_press(TxController* c, int control, bool pressed_)
{
  c->hold[control] = pressed_;
  tx__controller_refresh(c, control);
}

/* CodeController::update */
TX_FN void tx_code_controller_update(TxController* c)
{
  int i;
  tx_controller_update(c);

  for (i = 0; i < TX_CONTROL_COUNT; ++i) {
    c->hold[i] = false;
    tx__controller_refresh(c, i);
  }
}

/* ------------------------------------------------------------------------ */
/* Enums                                                                     */
/* ------------------------------------------------------------------------ */

/* supertux/direction.hpp : enum class Direction */
enum {
  TX_DIR_AUTO = 0,
  TX_DIR_NONE,
  TX_DIR_LEFT,
  TX_DIR_RIGHT,
  TX_DIR_UP,
  TX_DIR_DOWN
};

/* supertux/player_status.hpp : enum BonusType */
enum {
  TX_BONUS_NONE = 0,
  TX_BONUS_GROWUP,
  TX_BONUS_FIRE,
  TX_BONUS_ICE,
  TX_BONUS_AIR,
  TX_BONUS_EARTH
};

/* Player::FallMode */
enum {
  TX_FALL_ON_GROUND = 0,
  TX_FALL_JUMPING,
  TX_FALL_TRAMPOLINE_JUMP,
  TX_FALL_FALLING
};

/* supertux/sequence.hpp : enum Sequence */
enum {
  TX_SEQ_ENDSEQUENCE = 0,
  TX_SEQ_STOPTUX,
  TX_SEQ_FIREWORKS
};

/* trigger/trigger_base.hpp : TriggerBase::EventType */
enum {
  TX_EVENT_TOUCH = 0,
  TX_EVENT_LOSETOUCH,
  TX_EVENT_ACTIVATE
};

/* object/music_object.hpp : enum MusicType */
enum {
  TX_LEVEL_MUSIC = 0,
  TX_HERRING_MUSIC,
  TX_HERRING_WARNING_MUSIC
};

/* video/layer.hpp : LAYER_OBJECTS = 50 */
#define TX_LAYER_OBJECTS 50

/* which loop over the bricks (see break_bricks at the top) */
enum {
  TX_BRICKS_SIDE = 0,
  TX_BRICKS_DOWN,
  TX_BRICKS_TOP
};

/* the dynamic type of `other` in Player::collision, as far as the method
   tests it (the four classes are disjoint) */
enum {
  TX_OTHER_OTHER = 0,   /* none of the classes below */
  TX_OTHER_BULLET,      /* dynamic_cast<Bullet*> */
  TX_OTHER_PLAYER,      /* dynamic_cast<Player*> */
  TX_OTHER_TRIGGER,     /* dynamic_cast<TriggerBase*> */
  TX_OTHER_BADGUY       /* dynamic_cast<BadGuy*> */
};

/* ------------------------------------------------------------------------ */
/* The constants of player.cpp (L55-L158), literals as written               */
/* ------------------------------------------------------------------------ */

/* const float TUX_INVINCIBLE_TIME_WARNING = 2.0f; */
#define TX_TUX_INVINCIBLE_TIME_WARNING ((float)2.0f)

/* Times: */
#define TX_TUX_SAFE_TIME ((float)1.8f)
#define TX_TUX_INVINCIBLE_TIME ((float)14.0f)
#define TX_TUX_BACKFLIP_TIME ((float)2.1f) /* minimum air time that backflip results in a loss of control */

/* const int TIME_UNTIL_IDLE = 5000; */
#define TX_TIME_UNTIL_IDLE 5000

/** acceleration in horizontal direction when walking
 * (all accelerations are in  pixel/s^2) */
#define TX_WALK_ACCELERATION_X ((float)300)
/** acceleration in horizontal direction when running */
#define TX_RUN_ACCELERATION_X ((float)400)
/** acceleration when skidding */
#define TX_SKID_XM ((float)200)
/** time of skidding in seconds */
#define TX_SKID_TIME ((float).3f)
/** maximum walk velocity (pixel/s) */
#define TX_MAX_WALK_XM ((float)230)
/** maximum run velocity (pixel/s) */
#define TX_MAX_RUN_XM ((float)320)
/** bonus run velocity addition (pixel/s) */
#define TX_BONUS_RUN_XM ((float)80)
/** maximum horizontal climb velocity */
#define TX_MAX_CLIMB_XM ((float)96)
/** maximum vertical climb velocity */
#define TX_MAX_CLIMB_YM ((float)128)
/** maximum vertical glide velocity */
#define TX_MAX_GLIDE_YM ((float)128)
/** sliding down walls velocity */
#define TX_MAX_WALLCLING_YM ((float)64)
/** instant velocity when tux starts to walk */
#define TX_WALK_SPEED ((float)100)
/** rate at which m_boost decreases */
#define TX_BOOST_DECREASE_RATE ((float)500)
/** rate at which the speed decreases if going above maximum */
#define TX_OVERSPEED_DECELERATION ((float)100)

/** multiplied by WALK_ACCELERATION to give friction */
#define TX_NORMAL_FRICTION_MULTIPLIER ((float)1.5f)
/** multiplied by WALK_ACCELERATION to give friction */
#define TX_ICE_FRICTION_MULTIPLIER ((float)0.1f)
#define TX_ICE_ACCELERATION_MULTIPLIER ((float)0.25f)

/** time of the kick (kicking mriceblock) animation */
#define TX_KICK_TIME ((float).3f)

/** if Tux cannot unduck for this long, he will get hurt */
#define TX_UNDUCK_HURT_TIME ((float)0.25f)
/** gravity is higher after the jump key is released before
    the apex of the jump is reached */
/* const float JUMP_EARLY_APEX_FACTOR = 3.0;  (a double literal) */
#define TX_JUMP_EARLY_APEX_FACTOR ((float)3.0)

#define TX_JUMP_GRACE_TIME ((float)0.25f) /**< time before hitting the ground that the jump button may be pressed (and still trigger a jump) */
#define TX_COYOTE_TIME ((float)0.1f) /**< time between the moment leaving a platform without jumping and being able to jump anyways despite being in the air */

#define TX_MAX_SLIDE_SPEED ((float)700.f) /**< Max speed for sliding */
#define TX_MAX_FALL_SLIDE_SPEED ((float)475.f) /**< Max slide speed that Tux can get from falling */
#define TX_DOWN_SLIDE_ACCEL ((float)1000.f) /** < Acceleration for sliding DOWN slopes */
#define TX_UP_SLIDE_ACCEL ((float)1100.f) /**< Acceleration for sliding UP slopes */

#define TX_MAX_SLIDE_ROTATING_TIME ((float)0.15f)
#define TX_MIN_SLIDE_ROTATING_TIME ((float)0.075f)

/* Tux's collision rectangle */
#define TX_TUX_WIDTH ((float)31.8f)
#define TX_RUNNING_TUX_WIDTH ((float)34)
#define TX_SMALL_TUX_HEIGHT ((float)30.8f)
#define TX_BIG_TUX_HEIGHT ((float)62.8f)
#define TX_DUCKED_TUX_HEIGHT ((float)31.8f)

/* Stone Tux variables */
#define TX_MAX_STONE_SPEED ((float)500.f)
#define TX_STONE_KEY_ACCELERATION ((float)200.f)
#define TX_STONE_DOWN_ACCELERATION ((float)300.f)
#define TX_STONE_UP_ACCELERATION ((float)400.f)

/* Swim variables */
#define TX_SWIM_SPEED ((float)300.f)
#define TX_SWIM_BOOST_SPEED ((float)600.f)
#define TX_SWIM_TO_BOOST_ACCEL ((float)15.f)
#define TX_TURN_MAGNITUDE ((float)0.15f)
#define TX_TURN_MAGNITUDE_BOOST ((float)0.2f)

/* Buttjump variables */

#define TX_BUTTJUMP_WAIT_TIME ((float)0.2f) /* the length of time that the buttjump action is being played */
#define TX_BUTTJUMP_SPEED ((float)800.f)

/* const int MAX_FIRE_BULLETS = 2; const int MAX_ICE_BULLETS  = 2; */
#define TX_MAX_FIRE_BULLETS 2
#define TX_MAX_ICE_BULLETS 2

/* ------------------------------------------------------------------------ */
/* The hooks (see the list at the top)                                       */
/* ------------------------------------------------------------------------ */
typedef struct TxPlayerHost {
  void* user;

  void  (*play_sound)(void* user, const char* name, const TxVec* pos);
  void  (*stop_looping_sounds)(void* user);
  void  (*add_particles)(void* user, TxVec epicenter, int min_angle, int max_angle,
                         float min_initial_velocity, float max_initial_velocity,
                         TxVec acceleration, int number, float r, float g, float b,
                         int size, float life_time, int drawing_layer);
  void  (*add_sprite_particle)(void* user, const char* sprite_name, const char* action,
                               TxVec position, TxVec velocity, TxVec acceleration,
                               int drawing_layer, bool notimeout);
  void  (*camera_shake)(void* user, float duration, float x, float y);

  void  (*sprite_set_angle)(void* user, float angle);
  float (*sprite_get_angle)(void* user);
  bool  (*sprite_animation_done)(void* user);
  void  (*sprite_pause_animation)(void* user);
  void  (*sprite_resume_animation)(void* user);
  void  (*sprite_set_action)(void* user, const char* name, int dir, int loops);

  int   (*graphics_rand_i)(void* user, int lo, int hi);
  float (*graphics_rand_f)(void* user, float lo, float hi);

  void  (*break_bricks)(void* user, int which, const TxRect* box,
                        bool stone, bool sliding, bool fast, bool is_big);
  void  (*kill_fall_badguys)(void* user, const TxRect* box);

  bool  (*portable_is_hampering)(void* user, TxObj* obj);
  void  (*portable_grab)(void* user, TxObj* obj, TxVec pos, int dir);
  void  (*portable_ungrab)(void* user, TxObj* obj, int dir);
  void  (*object_set_pos)(void* user, TxObj* obj, TxVec pos);
  void  (*object_add_remove_listener)(void* user, TxObj* obj);
  void  (*object_del_remove_listener)(void* user, TxObj* obj);
  TxObj* (*find_portable)(void* user, TxVec pos);

  int   (*count_active_bullets)(void* user);
  void  (*add_bullet)(void* user, TxVec pos, TxVec xm, int dir, int bonus, bool is_waterlogged);

  void  (*start_sequence)(void* user, int seq, const void* data);
  bool  (*session_prevent_death)(void* user);
  int   (*count_alive_players)(void* user);
  bool  (*status_respawns_at_checkpoint)(void* user);
  void  (*add_falling_coin)(void* user, TxVec start_position, float x_vel);
  void  (*status_take_checkpoint_coins)(void* user);
  void  (*status_add_item_to_pocket)(void* user, int bonus);
  void  (*effect_fade_out)(void* user, float fade_time);
  void  (*pause_music)(void* user, float fade_time);
  void  (*play_music)(void* user, int type);

  void  (*trigger_event)(void* user, TxObj* other, int event);
  bool  (*badguy_is_frozen)(void* user, TxObj* other);
  float (*badguy_get_velocity_y)(void* user, TxObj* other);

  void  (*move_object_to_sector)(void* user, TxObj* obj, void* other_sector);
} TxPlayerHost;

/* ------------------------------------------------------------------------ */
/* Player                                                                    */
/* ------------------------------------------------------------------------ */
typedef struct TxPlayer {
  TxObj col;                     /* m_col (MovingObject) */
  TxSector* sector;              /* Sector::get() */
  TxPlayerHost host;             /* everything else outside the class */

  int id;                        /* m_id */
  bool deactivated;              /* m_deactivated */

  const TxController* controller;       /* m_controller */
  TxController scripting_controller;    /* *m_scripting_controller */
  int bonus;                     /* m_player_status.bonus[m_id] */
  bool duck;
  bool crawl;
  bool dead;
  bool dying;
  bool winning;
  bool backflipping;
  int  backflip_direction;
  int peekingX;                  /* a TX_DIR_* */
  int peekingY;
  bool stone;
  bool sliding;
  bool slidejumping;
  bool swimming;
  bool swimboosting;
  bool no_water;
  bool on_left_wall;
  bool on_right_wall;
  bool in_walljump_tile;
  bool can_walljump;
  float boost;
  float speedlimit;
  bool velocity_override;
  const TxController* scripting_controller_old; /**< Saves the old controller while the scripting_controller is used */
  bool jump_early_apex;
  bool on_ice;
  bool ice_this_frame;

  int dir;                       /* m_dir, a TX_DIR_* */
  int old_dir;

  float last_ground_y;
  int fall_mode;                 /* a TX_FALL_* */

  bool on_ground_flag;
  bool jumping;
  bool can_jump;
  TxTimer jump_button_timer; /**< started when player presses the jump button; runs until Tux jumps or JUMP_GRACE_TIME runs out */
  TxTimer coyote_timer; /**< started when Tux falls off a ledge; runs until Tux jumps or COYOTE_TIME runs out */
  bool wants_buttjump;
  bool buttjump_stomp;

  bool does_buttjump;
  TxTimer invincible_timer;

  TxTimer skidding_timer;
  TxTimer post_damage_safety_timer;
  TxTimer temp_safety_timer;

  bool is_intentionally_safe;

  TxTimer kick_timer;
  TxTimer buttjump_timer;

  TxTimer dying_timer;

  TxTimer second_growup_sound_timer;
  bool growing;
  TxTimer backflip_timer;

  TxPhysic physic;

  bool visible;

  TxObj* grabbed_object;         /* m_grabbed_object (Portable*, a MovingObject) */
  bool released_object;

  float swimming_angle;
  float swimming_accel_modifier;
  bool water_jump;

  TxTimer bubble_timer; /**< timer for spawning bubble particles */

  bool should_fancy_idle;
  bool fancy_idle_active;
  bool reset_action; /**< Attempts to reset tux back to the initial idle state
                          (not initialised by the C++ constructor; false here) */

  TxVec floor_normal;

  bool ghost_mode; /**< indicates if Tux should float around and through solid objects */

  TxTimer unduck_hurt_timer; /**< if Tux wants to stand up again after ducking and cannot, this timer is started */

  TxTimer idle_timer;
  unsigned int idle_stage;

  TxObj* climbing; /**< Climbable object we are currently climbing, null if none */

  int ending_direction;

  float last_sliding_angle;
  float current_sliding_angle;
  float target_sliding_angle;
  TxTimer sliding_rotation_timer;
  bool is_slidejump_falling;
  bool was_crawling_before_slide;
} TxPlayer;

/* ------------------------------------------------------------------------ */
/* Hook callers (null hook: nothing, or the neutral value)                   */
/* ------------------------------------------------------------------------ */

/* SoundManager::current()->play(name, get_pos()): get_pos() is read at the
   call, as the C++ argument is. */
TX_FN void tx__ph_play_sound_here(TxPlayer* p, const char* name)
{
  if (p->host.play_sound) {
    TxVec pos = tx_obj_get_pos(&p->col);
    p->host.play_sound(p->host.user, name, &pos);
  }
}

TX_FN void tx__ph_sprite_set_angle(TxPlayer* p, float angle)
{
  if (p->host.sprite_set_angle) p->host.sprite_set_angle(p->host.user, angle);
}

TX_FN float tx__ph_sprite_get_angle(TxPlayer* p)
{
  if (p->host.sprite_get_angle) return p->host.sprite_get_angle(p->host.user);
  return 0.0f;
}

TX_FN int tx__ph_rand_i(TxPlayer* p, int lo, int hi)
{
  if (p->host.graphics_rand_i) return p->host.graphics_rand_i(p->host.user, lo, hi);
  return lo;
}

TX_FN float tx__ph_rand_f(TxPlayer* p, float lo, float hi)
{
  if (p->host.graphics_rand_f) return p->host.graphics_rand_f(p->host.user, lo, hi);
  return lo;
}

TX_FN void tx__ph_add_particles(TxPlayer* p, TxVec epicenter, int min_angle, int max_angle,
                                float min_initial_velocity, float max_initial_velocity,
                                TxVec acceleration, int number, float r, float g, float b,
                                int size, float life_time, int drawing_layer)
{
  if (p->host.add_particles)
    p->host.add_particles(p->host.user, epicenter, min_angle, max_angle,
                          min_initial_velocity, max_initial_velocity, acceleration,
                          number, r, g, b, size, life_time, drawing_layer);
}

TX_FN void tx__ph_break_bricks(TxPlayer* p, int which, const TxRect* box, bool fast)
{
  if (p->host.break_bricks)
    p->host.break_bricks(p->host.user, which, box, p->stone, p->sliding, fast,
                         p->bonus != TX_BONUS_NONE);
}

/* moving_object->set_pos(pos) (virtual; the base is MovingObject::set_pos) */
TX_FN void tx__ph_object_set_pos(TxPlayer* p, TxObj* obj, TxVec pos)
{
  if (p->host.object_set_pos) p->host.object_set_pos(p->host.user, obj, pos);
  else tx_obj_set_pos(obj, pos);
}

TX_FN void tx__ph_portable_ungrab(TxPlayer* p, TxObj* obj, int dir)
{
  if (p->host.portable_ungrab) p->host.portable_ungrab(p->host.user, obj, dir);
}

/* ------------------------------------------------------------------------ */
/* Sector::get().get_width() / get_height()                                  */
/* ------------------------------------------------------------------------ */

/* GameObjectManager::get_width (game_object_manager.cpp L777) */
TX_FN float tx__sector_get_width(const TxSector* s)
{
  float width = 0;
  int i;
  for (i = 0; i < s->maps_n; ++i) {
    TxRect bbox = tx_map_get_bbox(s->maps[i]);
    width = tx_maxf(width, tx_rect_right(&bbox));
  }

  return width;
}

/* GameObjectManager::get_height (game_object_manager.cpp L787) */
TX_FN float tx__sector_get_height(const TxSector* s)
{
  float height = 0;
  int i;
  for (i = 0; i < s->maps_n; ++i) {
    TxRect bbox = tx_map_get_bbox(s->maps[i]);
    height = tx_maxf(height, tx_rect_bottom(&bbox));
  }

  return height;
}

/* ------------------------------------------------------------------------ */
/* The inline methods of player.hpp                                          */
/* ------------------------------------------------------------------------ */

/* Player::set_controller (hpp L104) */
TX_FN void tx_player_set_controller(TxPlayer* p, const TxController* controller) { p->controller = controller; }
/* Player::is_winning (hpp L107) */
TX_FN bool tx_player_is_winning(const TxPlayer* p) { return p->winning; }
/* Player::set_speedlimit / get_speedlimit (hpp L110) */
TX_FN void tx_player_set_speedlimit(TxPlayer* p, float limit) { p->speedlimit = limit; }
TX_FN float tx_player_get_speedlimit(const TxPlayer* p) { return p->speedlimit; }
/* Player::is_invincible (hpp L140) */
TX_FN bool tx_player_is_invincible(const TxPlayer* p) { return tx_timer_started(&p->invincible_timer); }
/* Player::is_dying (hpp L141) */
TX_FN bool tx_player_is_dying(const TxPlayer* p) { return p->dying; }
/* Player::is_dead (hpp L294) */
TX_FN bool tx_player_is_dead(const TxPlayer* p) { return p->dead; }
/* Player::is_alive (hpp L147) */
TX_FN bool tx_player_is_alive(const TxPlayer* p) { return !tx_player_is_dying(p) && !tx_player_is_dead(p); }
/* Player::is_active (hpp L153) */
TX_FN bool tx_player_is_active(const TxPlayer* p) { return tx_player_is_alive(p) && !tx_player_is_winning(p); }
/* Player::get_bonus (hpp L210) */
TX_FN int tx_player_get_bonus(const TxPlayer* p) { return p->bonus; }
/* Player::is_big (hpp L295) */
TX_FN bool tx_player_is_big(const TxPlayer* p) { return tx_player_get_bonus(p) != TX_BONUS_NONE; }
/* Player::is_stone / is_sliding / is_swimming / is_swimboosting /
   is_water_jumping / is_skidding / is_scripting_activated /
   get_swimming_angle (hpp L296-L303) */
TX_FN bool tx_player_is_stone(const TxPlayer* p) { return p->stone; }
TX_FN bool tx_player_is_sliding(const TxPlayer* p) { return p->sliding; }
TX_FN bool tx_player_is_swimming(const TxPlayer* p) { return p->swimming; }
TX_FN bool tx_player_is_swimboosting(const TxPlayer* p) { return p->swimboosting; }
TX_FN bool tx_player_is_water_jumping(const TxPlayer* p) { return p->water_jump; }
TX_FN bool tx_player_is_skidding(const TxPlayer* p) { return tx_timer_started(&p->skidding_timer); }
TX_FN bool tx_player_is_scripting_activated(const TxPlayer* p) { return !p->deactivated; }
TX_FN float tx_player_get_swimming_angle(const TxPlayer* p) { return p->swimming_angle; }
/* Player::set_visible / get_visible (hpp L310) */
TX_FN void tx_player_set_visible(TxPlayer* p, bool visible) { p->visible = visible; }
TX_FN bool tx_player_get_visible(const TxPlayer* p) { return p->visible; }
/* Player::on_ground / set_on_ground (hpp L329) */
TX_FN bool tx_player_on_ground(const TxPlayer* p) { return p->on_ground_flag; }
TX_FN void tx_player_set_on_ground(TxPlayer* p, bool flag) { p->on_ground_flag = flag; }
/* Player::get_velocity / get_velocity_x / get_velocity_y / set_velocity
   (hpp L272-L289) */
TX_FN TxVec tx_player_get_velocity(const TxPlayer* p) { return tx_physic_get_velocity(&p->physic); }
TX_FN float tx_player_get_velocity_x(const TxPlayer* p) { return tx_physic_get_velocity_x(&p->physic); }
TX_FN float tx_player_get_velocity_y(const TxPlayer* p) { return tx_physic_get_velocity_y(&p->physic); }
TX_FN void tx_player_set_velocity(TxPlayer* p, float x, float y) { tx_physic_set_velocity(&p->physic, x, y); }
/* Player::override_velocity (hpp L292) */
TX_FN void tx_player_override_velocity(TxPlayer* p) { p->velocity_override = true; }
/* Player::get_ghost_mode (hpp L352) */
TX_FN bool tx_player_get_ghost_mode(const TxPlayer* p) { return p->ghost_mode; }
/* Player::enable_fancy_idling / disable_fancy_idling (hpp L399) */
TX_FN void tx_player_enable_fancy_idling(TxPlayer* p) { p->should_fancy_idle = true; }
TX_FN void tx_player_disable_fancy_idling(TxPlayer* p) { p->should_fancy_idle = false; }
/* Player::walk (hpp L432) */
TX_FN void tx_player_walk(TxPlayer* p, float speed) { tx_physic_set_velocity_x(&p->physic, speed); }
/* Player::set_ending_direction / get_ending_direction (hpp L467) */
TX_FN void tx_player_set_ending_direction(TxPlayer* p, int direction) { p->ending_direction = direction; }
TX_FN int tx_player_get_ending_direction(const TxPlayer* p) { return p->ending_direction; }
/* Player::set_is_intentionally_safe / get_is_intentionally_safe (L2038) */
TX_FN void tx_player_set_is_intentionally_safe(TxPlayer* p, bool safe) { p->is_intentionally_safe = safe; }
TX_FN bool tx_player_get_is_intentionally_safe(const TxPlayer* p) { return p->is_intentionally_safe; }

/* MovingObject::get_pos / get_bbox / set_group */
TX_FN TxVec tx__player_get_pos(const TxPlayer* p) { return tx_rect_p1(&p->col.bbox); }
TX_FN void tx__player_set_group(TxPlayer* p, int group) { p->col.group = group; }

/* ------------------------------------------------------------------------ */
/* Forward declarations                                                      */
/* ------------------------------------------------------------------------ */
TX_FN void tx_player_ungrab_object(TxPlayer* p, bool from_remove_listener);
TX_FN void tx_player_stop_climbing(TxPlayer* p, TxObj* climbable);
TX_FN void tx_player_stop_backflipping(TxPlayer* p);
TX_FN void tx_player_check_bounds(TxPlayer* p);
TX_FN void tx_player_do_duck(TxPlayer* p);
TX_FN void tx_player_do_standup_force(TxPlayer* p, bool force_standup);
TX_FN void tx_player_do_backflip(TxPlayer* p);
TX_FN void tx_player_do_jump(TxPlayer* p, float yspeed);
TX_FN void tx_player_stop_rolling(TxPlayer* p, bool violent);
TX_FN bool tx_player_set_bonus(TxPlayer* p, int type, bool animate, bool pocket);
TX_FN void tx_player_handle_input(TxPlayer* p);
TX_FN void tx_player_handle_input_ghost(TxPlayer* p);
TX_FN void tx_player_handle_input_climbing(TxPlayer* p);
TX_FN void tx_player_handle_input_rolling(TxPlayer* p);
TX_FN void tx_player_position_grabbed_object(TxPlayer* p, bool teleport);
TX_FN bool tx_player_try_grab(TxPlayer* p);
TX_FN void tx_player_apply_friction(TxPlayer* p);
TX_FN void tx_player_kill(TxPlayer* p, bool completely);
TX_FN void tx_player_set_ghost_mode(TxPlayer* p, bool enable);
TX_FN void tx_player_sideways_push(TxPlayer* p, float delta);

/* ------------------------------------------------------------------------ */
/* player.cpp, in the order of the file                                      */
/* ------------------------------------------------------------------------ */

/* Player::Player  (L160)
   `player_status` is the argument `bonus` (see the top); `host` may be null
   (no hooks) and is copied.
   MovingSprite({0, 0}, "images/creatures/tux/tux.sprite", 9001,
   COLGROUP_MOVING): the object is at (0, 0); the size the sprite gives it is
   replaced at L268. */
TX_FN void tx_player_init(TxPlayer* p, TxSector* sector, const TxController* controller,
                          const TxPlayerHost* host, int bonus, int player_id)
{
  memset(p, 0, sizeof(*p));
  tx_obj_init(&p->col, TX_COLGROUP_MOVING);
  p->sector = sector;
  if (host) p->host = *host;

  p->id = player_id;
  /* NOT TRANSLATED (NT2): m_target(nullptr) */
  p->deactivated = false;
  p->controller = controller;
  tx_controller_reset(&p->scripting_controller);
  p->bonus = bonus;
  p->duck = false;
  p->crawl = false;
  p->dead = false;
  p->dying = false;
  p->winning = false;
  p->backflipping = false;
  p->backflip_direction = 0;
  p->peekingX = TX_DIR_AUTO;
  p->peekingY = TX_DIR_AUTO;
  p->stone = false;
  p->sliding = false;
  p->slidejumping = false;
  p->swimming = false;
  p->swimboosting = false;
  p->no_water = true;
  p->on_left_wall = false;
  p->on_right_wall = false;
  p->in_walljump_tile = false;
  p->can_walljump = false;
  p->boost = 0.f;
  p->speedlimit = 0; /* no special limit */
  p->velocity_override = false;
  p->scripting_controller_old = NULL;
  p->jump_early_apex = false;
  p->on_ice = false;
  p->ice_this_frame = false;
  /* NOT TRANSLATED (NT8, NT3): m_multiplayer_arrow, m_tag_timer, m_tag_fade,
     m_tag_alpha(1.f), m_has_moved(false) */
  p->dir = TX_DIR_RIGHT;
  p->old_dir = p->dir;
  p->last_ground_y = 0;
  p->fall_mode = TX_FALL_ON_GROUND;
  p->on_ground_flag = false;
  p->jumping = false;
  p->can_jump = true;
  tx_timer_init(&p->jump_button_timer);
  tx_timer_init(&p->coyote_timer);
  p->wants_buttjump = false;
  p->buttjump_stomp = false;
  p->does_buttjump = false;
  tx_timer_init(&p->invincible_timer);
  tx_timer_init(&p->skidding_timer);
  tx_timer_init(&p->post_damage_safety_timer);
  tx_timer_init(&p->temp_safety_timer);
  p->is_intentionally_safe = false;
  tx_timer_init(&p->kick_timer);
  tx_timer_init(&p->buttjump_timer);
  tx_timer_init(&p->dying_timer);
  tx_timer_init(&p->second_growup_sound_timer);
  p->growing = false;
  tx_timer_init(&p->backflip_timer);
  tx_physic_init(&p->physic);
  p->visible = true;
  p->grabbed_object = NULL;
  /* NOT TRANSLATED (NT8): m_grabbed_object_remove_listener(new GrabListener(*this)) */
  p->released_object = false;
  p->swimming_angle = 0;
  p->swimming_accel_modifier = 100.f;
  p->water_jump = false;
  /* NOT TRANSLATED (NT8): m_airarrow, m_bubbles_sprite */
  tx_timer_init(&p->bubble_timer);
  p->should_fancy_idle = true;
  p->fancy_idle_active = true;
  p->reset_action = false; /* no initialiser in the C++ */
  p->floor_normal = tx_vec(0.0f, 0.0f);
  p->ghost_mode = false;
  tx_timer_init(&p->unduck_hurt_timer);
  tx_timer_init(&p->idle_timer);
  p->idle_stage = 0;
  p->climbing = NULL;
  p->ending_direction = 0;
  /* NOT TRANSLATED (NT6): m_collected_keys() */
  p->last_sliding_angle = 0.0f;
  p->current_sliding_angle = 0.0f;
  p->target_sliding_angle = 0.0f;
  tx_timer_init(&p->sliding_rotation_timer);
  p->is_slidejump_falling = false;
  p->was_crawling_before_slide = false;

  /* NOT TRANSLATED (NT8): m_name = name_; */
  tx_timer_start(&p->idle_timer, (float)(TX_TIME_UNTIL_IDLE) / 1000.0f);

  /* NOT TRANSLATED (NT8): the ten SoundManager::current()->preload(...) */
  tx_timer_start(&p->bubble_timer, 3.0f + tx__ph_rand_f(p, 0.0f, 2));

  tx_obj_set_size(&p->col, TX_TUX_WIDTH, tx_player_is_big(p) ? TX_BIG_TUX_HEIGHT : TX_SMALL_TUX_HEIGHT);
  tx_obj_set_physic_hint(&p->col, &p->physic);

  tx__ph_sprite_set_angle(p, 0.0f);
  /*m_santahatsprite->set_angle(0.0f);*/

  tx_physic_reset(&p->physic);
}

/* Player::~Player  (L277) (also releases the collision object's set) */
TX_FN void tx_player_free(TxPlayer* p)
{
  tx_player_ungrab_object(p, false);
  if (p->climbing) tx_player_stop_climbing(p, p->climbing);
  tx_obj_free(&p->col);
}

/* Player::set_id  (L284)
   NOT TRANSLATED (NT9):
     m_id = id;
     m_controller = &(InputManager::current()->get_controller(id)); */

/* Player::set_winning  (L291) */
TX_FN void tx_player_set_winning(TxPlayer* p)
{
  if (!tx_player_is_winning(p)) {
    p->winning = true;
    tx_timer_start(&p->invincible_timer, 10000.0f);
  }
}

/* Player::use_scripting_controller  (L300) */
TX_FN void tx_player_use_scripting_controller(TxPlayer* p, bool enable)
{
  if (enable && (p->controller != &p->scripting_controller))
  {
    p->scripting_controller_old = p->controller;
    tx_player_set_controller(p, &p->scripting_controller);
  }
  else if (!enable && (p->controller == &p->scripting_controller))
  {
    tx_player_set_controller(p, p->scripting_controller_old);
    p->scripting_controller_old = NULL;
  }
}

/* Player::do_scripting_controller  (L315)
   `control` is the TX_CONTROL_* that Control_from_string(control_text) gives;
   pass a negative value where it gives std::nullopt. */
TX_FN void tx_player_do_scripting_controller(TxPlayer* p, int control, bool pressed)
{
  if (control >= 0 && control < TX_CONTROL_COUNT) {
    tx_code_controller_press(&p->scripting_controller, control, pressed);
  }
}

/* Player::move_to_sector  (L323) (state only: the objects are moved by the
   hook; the host then sets TxPlayer.sector) */
TX_FN void tx_player_move_to_sector(TxPlayer* p, void* other)
{
  tx_player_stop_climbing(p, p->climbing);
  if (p->grabbed_object)
  {
    /* auto grabbed_game_object = dynamic_cast<GameObject*>(m_grabbed_object);
       if (grabbed_game_object) */
    if (p->host.move_object_to_sector)
      p->host.move_object_to_sector(p->host.user, p->grabbed_object, other);
  }

  /* NOT TRANSLATED (NT6):
       for (Key* key : m_collected_keys)
         get_parent()->move_object(key->get_uid(), other); */

  /* Move the player. */
  if (p->host.move_object_to_sector)
    p->host.move_object_to_sector(p->host.user, &p->col, other);
}

/* Player::adjust_height  (L341) (C++ default: bottom_offset = 0) */
TX_FN bool tx_player_adjust_height(TxPlayer* p, float new_height, float bottom_offset)
{
  TxRect bbox2 = p->col.bbox;
  tx_rect_move(&bbox2, tx_vec(0, tx_rect_height(&p->col.bbox) - new_height - bottom_offset));
  tx_rect_set_height(&bbox2, new_height);


  if (new_height > tx_rect_height(&p->col.bbox)) {
    /*Rectf additional_space = bbox2;*/
    /*additional_space.set_height(new_height - m_col.m_bbox.get_height());*/
    if (!tx_is_free_of_statics(p->sector, &bbox2, &p->col, true, TX_TILE_SOLID))
      return false;
  }

  /* adjust bbox accordingly */
  /* note that we use members of moving_object for this, so we can run this during CD, too */
  tx_obj_set_pos(&p->col, tx_rect_p1(&bbox2));
  tx_obj_set_size(&p->col, tx_rect_width(&bbox2), tx_rect_height(&bbox2));
  return true;
}

/* Player::trigger_sequence(Sequence seq, const SequenceData* data)  (L375)
   NOT TRANSLATED (NT1): the two overloads taking the name of the sequence
   (L363, L369: `trigger_sequence(string_to_sequence(sequence_name), data)`). */
TX_FN void tx_player_trigger_sequence(TxPlayer* p, int seq, const void* data)
{
  if (p->climbing) tx_player_stop_climbing(p, p->climbing);
  tx_player_stop_backflipping(p);

  if (p->host.start_sequence) p->host.start_sequence(p->host.user, seq, data);
}

/* Player::slide  (L966) */
TX_FN void tx_player_slide(TxPlayer* p)
{
  TxRect pre_slide_box;
  float fast_fall_speed;
  bool pre_slide;

  if (p->swimming || p->water_jump || p->stone)
  {
    p->sliding = false;
    p->was_crawling_before_slide = false;
    return;
  }
  p->sliding = true;

  if (tx_player_on_ground(p))
  {
    if (tx_physic_get_velocity_x(&p->physic) > 0.f) {
      p->dir = TX_DIR_RIGHT;
    }
    else if (tx_physic_get_velocity_x(&p->physic) < 0.f) {
      p->dir = TX_DIR_LEFT;
    }
  }

  /* pre_slide helps us detect the ground where Tux is about to slide on because sometimes on_ground() doesn't work or isn't relevant */
  pre_slide_box = p->col.bbox;
  fast_fall_speed = tx_physic_get_velocity_y(&p->physic) <= 400.f ? 0.f : tx_physic_get_velocity_y(&p->physic)*0.03f;
  tx_rect_set_bottom(&pre_slide_box, tx_rect_bottom(&p->col.bbox) + fast_fall_speed + 16.f);
  pre_slide = !tx_is_free_of_statics(p->sector, &pre_slide_box, NULL, false, TX_TILE_SOLID);

  if (fabsf(tx_physic_get_velocity_x(&p->physic)) > TX_MAX_SLIDE_SPEED) {
    tx_physic_set_acceleration_x(&p->physic, -tx_physic_get_velocity_x(&p->physic));
  }
  else
  {
    if (!pre_slide) {
      tx_physic_set_acceleration_x(&p->physic, 0.f);

    }
    else
    {
      /* handle adding acceleration from falling down */
      if (p->sliding && !tx_player_on_ground(p) && p->floor_normal.x*tx_physic_get_velocity_x(&p->physic) <= 0.f && tx_physic_get_velocity_y(&p->physic) > 0.f)
      {
        /* less max momentum when getting it on flat surfaces */
        if (fabsf(tx_physic_get_velocity_x(&p->physic)) + (fabsf(tx_physic_get_velocity_y(&p->physic))*0.2f) <= TX_MAX_FALL_SLIDE_SPEED) {
          tx_physic_set_velocity_x(&p->physic, tx_physic_get_velocity_x(&p->physic) + (fabsf(tx_physic_get_velocity_y(&p->physic))*(p->dir == TX_DIR_LEFT ? -0.125f : 0.125f)));
        }
        else {
          tx_physic_set_velocity_x(&p->physic, TX_MAX_FALL_SLIDE_SPEED * (p->dir == TX_DIR_LEFT ? -1.f : 1.f));
        }
      }

      /* handle adding acceleration from sliding down, removing it from reaching floor or an incline */
      if (p->floor_normal.y == 0.f && p->can_jump)
      {
        if (!p->slidejumping && !p->jumping) {
          tx_player_apply_friction(p);
        }
      }
      else
      {
        if (p->floor_normal.x > 0.f) {
          tx_physic_set_acceleration_x(&p->physic, (p->dir == TX_DIR_LEFT ? TX_UP_SLIDE_ACCEL : TX_DOWN_SLIDE_ACCEL)*fabsf(p->floor_normal.x));
        }
        if (p->floor_normal.x < 0.f) {
          tx_physic_set_acceleration_x(&p->physic, (p->dir == TX_DIR_RIGHT ? -TX_UP_SLIDE_ACCEL : -TX_DOWN_SLIDE_ACCEL)*fabsf(p->floor_normal.x));
        }
      }

      /* passed on from handle_collision_logic(), we'll give the player an
         initial speed boost if they slide */
      if (p->does_buttjump)
      {
        /* ...but not if we're already moving a bit */
        if (fabsf(tx_physic_get_velocity_x(&p->physic)) < 267.f)
          tx_physic_set_velocity_x(&p->physic, (p->floor_normal.x > 0 ? 1 : -1) * (TX_MAX_SLIDE_SPEED / 3.f));
        p->does_buttjump = false;
        p->buttjump_stomp = false;
      }
    }
  }
}

/* Player::swim  (L1058) */
TX_FN void tx_player_swim(TxPlayer* p, float pointx, float pointy, bool boost)
{
    bool is_ang_defined;
    float pointed_angle;
    float delta;
    float vx;
    float vy;

    if (p->swimming)
      tx_physic_set_gravity_modifier(&p->physic, .0f);

    /* Angle */
    is_ang_defined = (pointx != 0) || (pointy != 0);
    pointed_angle = tx_math_angle(tx_vec(pointx, pointy));
    delta = 0;

    if(is_ang_defined)
    {
      float epsilon;
      delta = pointed_angle - p->swimming_angle;

      if(fabsf(delta) > TX_PI)
        delta += delta > 0 ? -TX_TAU : TX_TAU;

      epsilon = (boost ? TX_TURN_MAGNITUDE : TX_TURN_MAGNITUDE_BOOST) * delta;
      p->swimming_angle += epsilon;

      if (p->swimming_angle > TX_PI)
        p->swimming_angle -= TX_TAU;

      if (p->swimming_angle <= -TX_PI)
        p->swimming_angle += TX_TAU;
    }

    vx = tx_physic_get_velocity_x(&p->physic);
    vy = tx_physic_get_velocity_y(&p->physic);

    if (p->swimming && !p->water_jump)
    {
      TxVec swimming_direction;
      float minboostspeed;

      if(is_ang_defined && fabsf(delta) < 0.01f)
        p->swimming_angle = pointed_angle;

      p->swimming_accel_modifier = is_ang_defined ? 600.f : 0.f;
      swimming_direction = tx_math_vec2_from_polar(p->swimming_accel_modifier, pointed_angle);

      tx_physic_set_acceleration_x(&p->physic, (swimming_direction.x - 1.0f * vx) * 2.f);
      tx_physic_set_acceleration_y(&p->physic, (swimming_direction.y - 1.0f * vy) * 2.f);

      /* Limit speed, if you go above this speed your acceleration is set to opposite (?) */
      if (tx_vec_length(tx_physic_get_velocity(&p->physic)) > TX_SWIM_SPEED)
      {
        tx_physic_set_acceleration(&p->physic, -vx,-vy);   /* Was too lazy to set it properly ~~zwatotem */
      }

      /* Natural friction */
      if (!is_ang_defined)
      {
        tx_physic_set_acceleration(&p->physic, -3.f*vx, -3.f*vy);
      }

      /* not boosting? let's slow this penguin down!!! */
      if (!boost && is_ang_defined && tx_vec_length(tx_physic_get_velocity(&p->physic)) > (TX_SWIM_SPEED + 10.f))
      {
        tx_physic_set_acceleration(&p->physic, -5.f*vx, -5.f*vy);
      }

      /* Snapping to prevent unwanted floating */
        if (!is_ang_defined && tx_vec_length(tx_vec(vx,vy)) < 100.f)
      {
        vx = 0;
        vy = 0;
      }

      /* Turbo, using pointsign */
      minboostspeed = 100.f;
      if (boost && tx_vec_length(tx_physic_get_velocity(&p->physic)) > minboostspeed)
      {
        if (tx_vec_length(tx_physic_get_velocity(&p->physic)) < TX_SWIM_BOOST_SPEED)
        {
          p->swimboosting = true;
          if (is_ang_defined)
          {
            vx += TX_SWIM_TO_BOOST_ACCEL * pointx;
            vy += TX_SWIM_TO_BOOST_ACCEL * pointy;
          }
        }
        else
        {
          /* cap on boosting */
          tx_physic_set_acceleration(&p->physic, -vx, -vy);
        }
        tx_physic_set_velocity(&p->physic, vx, vy);
      }
      else
      {
          if (tx_vec_length(tx_physic_get_velocity(&p->physic)) < (TX_SWIM_SPEED + 10.f))
        {
          p->swimboosting = false;
        }
      }
    }
    if (p->water_jump && !p->swimming)
    {
      p->swimming_angle = tx_math_angle(tx_vec(vx, vy));
    }

  /* snap angle dir when water jumping to avoid crazy spinning graphics... */
  if (p->water_jump && !p->swimming && fabsf(tx_physic_get_velocity_x(&p->physic)) < 10.f)
  {
    tx__ph_sprite_set_angle(p, tx_math_degrees(p->swimming_angle));
    /*m_santahatsprite->set_angle(math::degrees(m_swimming_angle));*/
  }
  else
  {
    /* otherwise angle the sprite normally */
    float angle = (fabsf(p->swimming_angle) <= TX_PI_2) ?
                    tx_math_degrees(p->swimming_angle) :
                    tx_math_degrees(TX_PI + p->swimming_angle);

    tx__ph_sprite_set_angle(p, angle);
    /*m_santahatsprite->set_angle(angle);*/

    /* Force the speed to point in the direction Tux is going unless Tux is being pushed by something else */
    if (p->swimming && !p->water_jump && boost && p->boost == 0.f && !p->velocity_override)
    {
      tx_physic_set_velocity_v(&p->physic, tx_math_at_angle(tx_physic_get_velocity(&p->physic), p->swimming_angle));
    }
  }
}

/* Player::handle_input_swimming  (L1047) */
TX_FN void tx_player_handle_input_swimming(TxPlayer* p)
{
  float pointx = (float)(tx_controller_hold(p->controller, TX_CONTROL_RIGHT)) - (float)(tx_controller_hold(p->controller, TX_CONTROL_LEFT)),
        pointy = (float)(tx_controller_hold(p->controller, TX_CONTROL_DOWN)) - (float)(tx_controller_hold(p->controller, TX_CONTROL_UP));

  bool boost = tx_controller_hold(p->controller, TX_CONTROL_JUMP);

  tx_player_swim(p, pointx, pointy, boost);
}

/* Player::apply_friction  (L1183)
   slows Tux down a little, based on where he's standing */
TX_FN void tx_player_apply_friction(TxPlayer* p)
{
  bool is_on_ground = tx_player_on_ground(p);
  float velx = tx_physic_get_velocity_x(&p->physic);
  float friction;
  if (is_on_ground && (fabsf(velx) < (p->stone ? 5.f : TX_WALK_SPEED))) {
    tx_physic_set_velocity_x(&p->physic, 0);
    tx_physic_set_acceleration_x(&p->physic, 0);
    return;
  }
  friction = TX_WALK_ACCELERATION_X;
  if (p->on_ice && is_on_ground)
    /* we need this or else sliding on ice will cause Tux to go on for a very long time */
    friction *= (TX_ICE_FRICTION_MULTIPLIER*(p->sliding ? 4.f : p->stone ? 5.f : 1.f));
  else
    friction *= (TX_NORMAL_FRICTION_MULTIPLIER*(p->sliding ? 0.8f : p->stone ? 0.4f : 1.f));
  if (velx < 0) {
    tx_physic_set_acceleration_x(&p->physic, friction);
  } else if (velx > 0) {
    tx_physic_set_acceleration_x(&p->physic, -friction);
  } /* no friction for physic.get_velocity_x() == 0 */
}

/* Player::handle_horizontal_input  (L1206) */
TX_FN void tx_player_handle_horizontal_input(TxPlayer* p)
{
  float vx = tx_physic_get_velocity_x(&p->physic);
  float vy = tx_physic_get_velocity_y(&p->physic);
  float ax = 0;
  float ay = tx_physic_get_acceleration_y(&p->physic);

  float dirsign = 0;
  if (!p->duck || tx_physic_get_velocity_y(&p->physic) != 0) {
    if (tx_controller_hold(p->controller, TX_CONTROL_LEFT) && !tx_controller_hold(p->controller, TX_CONTROL_RIGHT)) {
      p->old_dir = p->dir;
      if (!p->water_jump) p->dir = TX_DIR_LEFT;
      dirsign = -1;
    } else if (!tx_controller_hold(p->controller, TX_CONTROL_LEFT)
              && tx_controller_hold(p->controller, TX_CONTROL_RIGHT)) {
      p->old_dir = p->dir;
      if (!p->water_jump) p->dir = TX_DIR_RIGHT;
      dirsign = 1;
    }
  }

  if (p->duck && (tx_controller_hold(p->controller, TX_CONTROL_LEFT) || tx_controller_hold(p->controller, TX_CONTROL_RIGHT))) {
    p->crawl = true;
  }

  /* player on slope -> duck? start sliding. */
  if (tx_controller_hold(p->controller, TX_CONTROL_DOWN) && tx_player_on_ground(p) && p->floor_normal.y != 0)
  {
    if (tx_player_get_bonus(p) == TX_BONUS_EARTH)
    {
      p->stone = true;
      p->does_buttjump = false;
    }
    p->sliding = true;
    /* silly nonsense; tuxs "unslides" back into tall tux if he's large and his
       action 'clips' through the ground. Don't blame me, i hate this file. */
    if (tx_player_is_big(p))
      p->duck = true;
  }

  if (p->crawl && tx_player_on_ground(p) && fabsf(tx_physic_get_velocity_x(&p->physic)) < TX_WALK_SPEED)
  {
    if (tx_controller_hold(p->controller, TX_CONTROL_LEFT) && !tx_controller_hold(p->controller, TX_CONTROL_RIGHT))
    {
      vx = -TX_WALK_SPEED;
      p->dir = TX_DIR_LEFT;
    }
    else if (tx_controller_hold(p->controller, TX_CONTROL_RIGHT) && !tx_controller_hold(p->controller, TX_CONTROL_LEFT))
    {
      vx = TX_WALK_SPEED;
      p->dir = TX_DIR_RIGHT;
    }
    else {
      vx = 0.f;
    }
  }

  /* do not run if we're holding something which slows us down */
  if ( p->grabbed_object && (p->host.portable_is_hampering ? p->host.portable_is_hampering(p->host.user, p->grabbed_object) : false) ) {
    ax = dirsign * TX_WALK_ACCELERATION_X;
    /* limit speed */
    if (vx >= TX_MAX_WALK_XM && dirsign > 0) {
      ax = tx_minf(ax, -TX_OVERSPEED_DECELERATION);
    } else if (vx <= -TX_MAX_WALK_XM && dirsign < 0) {
      ax = tx_maxf(ax, TX_OVERSPEED_DECELERATION);
    }
  } else {
    if ( vx * dirsign < TX_MAX_WALK_XM ) {
      ax = dirsign * TX_WALK_ACCELERATION_X;
    } else {
      ax = dirsign * TX_RUN_ACCELERATION_X;
    }
    /* limit speed */
    if (vx >= TX_MAX_RUN_XM + TX_BONUS_RUN_XM *((tx_player_get_bonus(p) == TX_BONUS_AIR) ? 1 : 0)) {
      ax = tx_minf(ax, -TX_OVERSPEED_DECELERATION);
    } else if (vx <= -TX_MAX_RUN_XM - TX_BONUS_RUN_XM * ((tx_player_get_bonus(p) == TX_BONUS_AIR) ? 1 : 0)) {
      ax = tx_maxf(ax, TX_OVERSPEED_DECELERATION);
    }
  }

  /* we can reach WALK_SPEED without any acceleration */
  if (dirsign != 0 && fabsf(vx) < TX_WALK_SPEED) {
    vx = dirsign * TX_WALK_SPEED;
  }

  /* Check speedlimit. */
  if ( p->speedlimit > 0 &&  vx * dirsign >= p->speedlimit ) {
    vx = dirsign * p->speedlimit;
    ax = 0;
  }

  /* changing directions? */
  if ((vx < 0 && dirsign >0) || (vx>0 && dirsign<0)) {
    if (tx_player_on_ground(p)) {
      /* let's skid! */
      if (fabsf(vx)>TX_SKID_XM && !tx_timer_started(&p->skidding_timer)) {
        tx_timer_start(&p->skidding_timer, TX_SKID_TIME);
        tx__ph_play_sound_here(p, "sounds/skid.wav");
        /* dust some particles */
        tx__ph_add_particles(p,
            tx_vec(p->dir == TX_DIR_LEFT ? tx_rect_right(&p->col.bbox) : tx_rect_left(&p->col.bbox), tx_rect_bottom(&p->col.bbox)),
            p->dir == TX_DIR_LEFT ? 50 : -70, p->dir == TX_DIR_LEFT ? 70 : -50, 260.0f, 280.0f,
            tx_vec(0, 300), 3, .4f, .4f, .4f, 3, .8f, TX_LAYER_OBJECTS+1);

        ax *= 2.5f;
      } else {
        ax *= 2;
      }
    }
    else {
      /* give Tux tighter air control */
      ax *= 2.f;
    }
  }

  if (p->on_ice && tx_player_on_ground(p)) {
    ax *= TX_ICE_ACCELERATION_MULTIPLIER;
  }

  /* get_collision_object()->get_pressure() != Vector(0.0f, 0.0f) */
  if(p->col.pressure.x != 0.0f || p->col.pressure.y != 0.0f) {
    vx = 0.0f; vy = 0.0f;
    ax = 0.0f; ay = 0.0f;
  }

  tx_physic_set_velocity(&p->physic, vx, vy);
  tx_physic_set_acceleration(&p->physic, ax, ay);

  /* we get slower when not pressing any keys */
  if (dirsign == 0) {
    tx_player_apply_friction(p);
  }
}

/* Player::do_cheer  (L1340) */
TX_FN void tx_player_do_cheer(TxPlayer* p)
{
  tx_player_do_duck(p);
  tx_player_do_backflip(p);
  tx_player_do_standup_force(p, false);
}

/* Player::do_duck  (L1348) */
TX_FN void tx_player_do_duck(TxPlayer* p) {
  if (p->duck)
    return;
  if (!tx_player_is_big(p))
    return;
  if (p->sliding) {
    TxRect r = tx_rect(tx_rect_left(&p->col.bbox), tx_rect_top(&p->col.bbox) - 32.f,
      tx_rect_right(&p->col.bbox), tx_rect_bottom(&p->col.bbox));
    if (tx_is_free_of_statics(p->sector, &r, NULL, false, TX_TILE_SOLID))
      return;
  }

  if (!p->swimming && !p->water_jump && tx_physic_get_velocity_y(&p->physic) != 0)
    return;
  if (!tx_player_on_ground(p))
    return;
  if (p->does_buttjump)
    return;

  if (tx_player_adjust_height(p, TX_DUCKED_TUX_HEIGHT, 0)) {
    p->duck = true;
    p->growing = false;
    tx_timer_stop(&p->unduck_hurt_timer);
  } else {
    /* FIXME: what now? */
  }
}

/* Player::do_standup(bool force_standup)  (L1381) */
TX_FN void tx_player_do_standup_force(TxPlayer* p, bool force_standup)
{
  TxRect new_bbox;
  float new_height;

  if (!p->duck || !tx_player_is_big(p) || p->backflipping || p->stone)
  {
    p->crawl = false;
    return;
  }

  new_bbox = p->col.bbox;
  new_height = p->swimming ? TX_TUX_WIDTH : TX_BIG_TUX_HEIGHT;
  tx_rect_move(&new_bbox, tx_vec(0, tx_rect_height(&p->col.bbox) - new_height));
  tx_rect_set_height(&new_bbox, new_height);
  if (!tx_is_free_of_movingstatics(p->sector, &new_bbox, &p->col, true) && !force_standup)
  {
    p->crawl = true;
    return;
  }

  if (p->swimming ? tx_player_adjust_height(p, TX_TUX_WIDTH, 0) : tx_player_adjust_height(p, TX_BIG_TUX_HEIGHT, 0)) {
    p->duck = false;
    p->crawl = false;
    tx_timer_stop(&p->unduck_hurt_timer);
  } else if (force_standup) {
    /* if timer is not already running, start it. */
    if (tx_timer_get_period(&p->unduck_hurt_timer) == 0) {
      tx_timer_start(&p->unduck_hurt_timer, TX_UNDUCK_HURT_TIME);
    }
    else if (tx_timer_check(&p->unduck_hurt_timer)) {
      tx_player_kill(p, false);
    }
  }

}

/* Player::do_standup()  (L1374) */
TX_FN void tx_player_do_standup(TxPlayer* p)
{
  /* Scripting: Force standup for backwards compatibility. */
  tx_player_do_standup_force(p, true);
}

/* Player::do_backflip  (L1416) */
TX_FN void tx_player_do_backflip(TxPlayer* p) {
  if (!p->duck)
    return;
  if (p->crawl)
    return;
  if (!tx_player_on_ground(p))
    return;

  p->backflip_direction = (p->dir == TX_DIR_LEFT)?(+1):(-1);
  p->backflipping = true;
  tx_player_do_jump(p, (tx_player_get_bonus(p) == TX_BONUS_AIR) ? -720.0f : -580.0f);
  tx__ph_play_sound_here(p, "sounds/flip.wav");
  tx_timer_start(&p->backflip_timer, TX_TUX_BACKFLIP_TIME);
}

/* Player::do_jump  (L1432) */
TX_FN void tx_player_do_jump(TxPlayer* p, float yspeed) {
  if (!p->can_walljump && !p->in_walljump_tile && !tx_player_on_ground(p) && !tx_timer_started(&p->coyote_timer))
    return;

  /* jump only if it would make Tux go faster upwards */
  if (p->can_walljump || tx_physic_get_velocity_y(&p->physic) > yspeed) {
    tx_physic_set_velocity_y(&p->physic, yspeed * (p->sliding ? 0.75f : 1.f));
    /*bbox.move(Vector(0, -1));*/
    p->jumping = true;
    p->on_ground_flag = false;
    p->can_jump = false;

    /*if (m_sliding)
    {
      if (!adjust_height(is_big() ? BIG_TUX_HEIGHT : SMALL_TUX_HEIGHT)) {
        m_duck = true;
      }
      m_sliding = false;
      m_slidejumping = false;
    }*/

    /* play sound */
    if (tx_player_is_big(p)) {
      tx__ph_play_sound_here(p, "sounds/bigjump.wav");
    } else {
      tx__ph_play_sound_here(p, "sounds/jump.wav");
    }
  }
}

/* Player::early_jump_apex  (L1463) */
TX_FN void tx_player_early_jump_apex(TxPlayer* p)
{
  if (!p->jump_early_apex)
  {
    p->jump_early_apex = true;
    tx_physic_set_gravity_modifier(&p->physic, TX_JUMP_EARLY_APEX_FACTOR);
  }
}

/* Player::do_jump_apex  (L1473) */
TX_FN void tx_player_do_jump_apex(TxPlayer* p)
{
  if (p->jump_early_apex)
  {
    p->jump_early_apex = false;
    tx_physic_set_gravity_modifier(&p->physic, 1.0f);
  }
}

/* Player::handle_vertical_input  (L1483) */
TX_FN void tx_player_handle_vertical_input(TxPlayer* p)
{
  /* Press jump key */
  if (tx_controller_pressed(p->controller, TX_CONTROL_JUMP)) tx_timer_start(&p->jump_button_timer, TX_JUMP_GRACE_TIME);
  if (tx_controller_hold(p->controller, TX_CONTROL_JUMP) && tx_timer_started(&p->jump_button_timer) && (p->can_jump || tx_timer_started(&p->coyote_timer))) {
    tx_timer_stop(&p->jump_button_timer);
    if (p->duck && !p->sliding) {
      /* when running, only jump a little bit; else do a backflip */
      if ((tx_physic_get_velocity_x(&p->physic) != 0) ||
          (tx_controller_hold(p->controller, TX_CONTROL_LEFT)) ||
          (tx_controller_hold(p->controller, TX_CONTROL_RIGHT)))
      {
        tx_player_do_jump(p, -300);
      }
      else
      {
        tx_player_do_backflip(p);
      }
    } else {
      /* airflower allows for higher jumps-
         jump a bit higher if we are running; else do a normal jump */
      if (!p->can_walljump) {
        if (tx_player_get_bonus(p) == TX_BONUS_AIR)
          tx_player_do_jump(p, (fabsf(tx_physic_get_velocity_x(&p->physic)) > TX_MAX_WALK_XM) ? -620.0f : -580.0f);
        else
          tx_player_do_jump(p, (fabsf(tx_physic_get_velocity_x(&p->physic)) > TX_MAX_WALK_XM) ? -580.0f : -520.0f);
      }
    }
    /* Stop the coyote timer only after calling do_jump, because do_jump also checks for the timer */
    tx_timer_stop(&p->coyote_timer);
    /* airflower glide only when holding jump key */
  }
  else if (tx_controller_hold(p->controller, TX_CONTROL_JUMP) && tx_player_get_bonus(p) == TX_BONUS_AIR && tx_physic_get_velocity_y(&p->physic) > TX_MAX_GLIDE_YM) {
    /* glide stops if buttjump is initiated */
    if (!tx_controller_hold(p->controller, TX_CONTROL_DOWN))
    {
      tx_physic_set_velocity_y(&p->physic, TX_MAX_GLIDE_YM);
      tx_physic_set_acceleration_y(&p->physic, 0);
    }
  }


  /* Let go of jump key */
  else if (!tx_controller_hold(p->controller, TX_CONTROL_JUMP)) {
    if (!p->backflipping && p->jumping && tx_physic_get_velocity_y(&p->physic) < 0) {
      p->jumping = false;

      if (p->sliding)
      {
        p->is_slidejump_falling = true;
      }

      tx_player_early_jump_apex(p);
    }
  }

  if (p->jump_early_apex && (tx_physic_get_velocity_y(&p->physic) >= 0 ||
      (p->can_walljump && tx_controller_pressed(p->controller, TX_CONTROL_JUMP)))) {
    tx_player_do_jump_apex(p);
  }

  /* In case the player has pressed Down while in a certain range of air,
     enable butt jump action */
  if (tx_controller_hold(p->controller, TX_CONTROL_DOWN) && !p->duck && tx_player_is_big(p) && !tx_player_on_ground(p) && !p->sliding && !p->stone) {
    if (!p->wants_buttjump && !p->does_buttjump) {
      tx_timer_start(&p->buttjump_timer, TX_BUTTJUMP_WAIT_TIME);
    }
    p->wants_buttjump = true;
    if (tx_timer_check(&p->buttjump_timer))
    {
      tx_timer_stop(&p->buttjump_timer);
      p->does_buttjump = true;
    }
    if (p->does_buttjump) {
      tx_physic_set_velocity_y(&p->physic, TX_BUTTJUMP_SPEED);
    }
  }

  /* When Down is not held anymore, disable butt jump */
  if (!tx_controller_hold(p->controller, TX_CONTROL_DOWN)) {
    p->wants_buttjump = false;
    p->does_buttjump = false;
    p->buttjump_stomp = false;
  }

  /* The real walljumping magic */
  if (tx_controller_pressed(p->controller, TX_CONTROL_JUMP) && p->can_walljump && !p->backflipping)
  {
    tx__ph_play_sound_here(p, (tx_player_is_big(p)) ? "sounds/bigjump.wav" : "sounds/jump.wav");
    tx_physic_set_velocity_x(&p->physic, tx_player_get_bonus(p) == TX_BONUS_AIR ?
      p->on_left_wall ? 480.f : -480.f : p->on_left_wall ? 380.f : -380.f);
    tx_player_do_jump(p, -520.f);
  }

 tx_physic_set_acceleration_y(&p->physic, 0);
}

/* Player::handle_input  (L1581) */
TX_FN void tx_player_handle_input(TxPlayer* p)
{
  bool just_grabbed;
  int active_bullets;

  /* NOT TRANSLATED (NT3):
       // Display the player's ID on top of them at the beginning of the level/sector
       // and persist the number until the player moves, because players will be
       // stacked upon spawning.
       if (!m_has_moved && (m_controller->hold(Control::LEFT) || m_controller->hold(Control::RIGHT)))
       {
         m_has_moved = true;
         m_tag_timer.start(1.f);
       } */

  if (p->ghost_mode) {
    tx_player_handle_input_ghost(p);
    return;
  }
  if (p->climbing) {
    tx_player_handle_input_climbing(p);
    return;
  }
  if (p->stone) {
    tx_player_handle_input_rolling(p);
    return;
  }
  if (p->swimming) {
    tx_player_handle_input_swimming(p);
  }
  else
  {
    if (p->water_jump)
    {
      tx_player_swim(p, 0, 0, 0);
    }
  }

  if (!p->swimming)
  {
    if (!p->water_jump && !p->backflipping && !p->sliding)
    {
      tx__ph_sprite_set_angle(p, 0);
      /*m_santahatsprite->set_angle(0);*/
    }

    if (!p->jump_early_apex) {
      tx_physic_set_gravity_modifier(&p->physic, 1.0f);
    }
    else {
      tx_physic_set_gravity_modifier(&p->physic, TX_JUMP_EARLY_APEX_FACTOR);
    }
  }

  /* Peeking */
  if (!tx_controller_hold(p->controller, TX_CONTROL_PEEK_LEFT) && !tx_controller_hold(p->controller, TX_CONTROL_PEEK_RIGHT))
    p->peekingX = TX_DIR_AUTO;
  if (!tx_controller_hold(p->controller, TX_CONTROL_PEEK_UP) && !tx_controller_hold(p->controller, TX_CONTROL_PEEK_DOWN))
    p->peekingY = TX_DIR_AUTO;

  if (tx_controller_pressed(p->controller, TX_CONTROL_PEEK_LEFT))
    p->peekingX = TX_DIR_LEFT;
  else if (tx_controller_pressed(p->controller, TX_CONTROL_PEEK_RIGHT))
    p->peekingX = TX_DIR_RIGHT;

  if (tx_controller_pressed(p->controller, TX_CONTROL_PEEK_UP))
    p->peekingY = TX_DIR_UP;
  else if (tx_controller_pressed(p->controller, TX_CONTROL_PEEK_DOWN))
    p->peekingY = TX_DIR_DOWN;

  /* Handle horizontal movement: */
  if (!p->backflipping && !p->stone && !p->swimming && !p->sliding) tx_player_handle_horizontal_input(p);

  /* Jump/jumping? */
  if (tx_player_on_ground(p))
    p->can_jump = true;

  /* Handle vertical movement: */
  if (!p->stone && !p->swimming) tx_player_handle_vertical_input(p);

  /* grabbing */
  just_grabbed = tx_player_try_grab(p);

  /* Shoot! */
  active_bullets = p->host.count_active_bullets ? p->host.count_active_bullets(p->host.user) : 0;
  if (tx_controller_pressed(p->controller, TX_CONTROL_ACTION) && (tx_player_get_bonus(p) == TX_BONUS_FIRE || tx_player_get_bonus(p) == TX_BONUS_ICE) && !just_grabbed) {
    if ((tx_player_get_bonus(p) == TX_BONUS_FIRE && active_bullets < TX_MAX_FIRE_BULLETS) ||
        (tx_player_get_bonus(p) == TX_BONUS_ICE  && active_bullets < TX_MAX_ICE_BULLETS))
    {
      TxVec pos = tx_vec_add(tx__player_get_pos(p), tx_vec(tx_rect_width(&p->col.bbox) / 2.f, tx_rect_height(&p->col.bbox) / 4.f));
      int swim_dir;
      TxVec xm;
      swim_dir = ((fabsf(p->swimming_angle) <= TX_PI_2)
        || (p->water_jump && fabsf(tx_physic_get_velocity_x(&p->physic)) < 10.f)) ? TX_DIR_RIGHT : TX_DIR_LEFT;
      if (p->swimming || p->water_jump)
      {
        p->dir = swim_dir;
      }
      xm = (p->swimming || p->water_jump) ?
          (tx_vec_add(tx_vec_scale(tx_vec(cosf(p->swimming_angle), sinf(p->swimming_angle)), 600.f), tx_physic_get_velocity(&p->physic))) :
          (tx_vec(((p->dir == TX_DIR_RIGHT ? 600.f : -600.f) + tx_physic_get_velocity_x(&p->physic)), 0.f));
      if (p->host.add_bullet)
        p->host.add_bullet(p->host.user, pos, xm,
          p->dir, tx_player_get_bonus(p), !p->swimming);
      tx__ph_play_sound_here(p, "sounds/shoot.wav");
    }
  }

  /* Turn to Stone */
  if (tx_controller_hold(p->controller, TX_CONTROL_DOWN) && !p->does_buttjump && tx_timer_started(&p->coyote_timer) && !p->swimming && (fabsf(tx_physic_get_velocity_x(&p->physic)) > 150.f) && tx_player_get_bonus(p) == TX_BONUS_EARTH && !p->growing) {
    tx_physic_set_gravity_modifier(&p->physic, 1.0f); /* Undo jump_early_apex */
    tx_player_adjust_height(p, TX_TUX_WIDTH, 0);
    p->stone = true;
    p->swimming = false;
    p->sliding = false;
    p->was_crawling_before_slide = false;
    p->crawl = false;
    p->duck = false;
  }

  if (p->stone)
    tx_player_apply_friction(p);

  /* Duck or Standup! */
  if (((tx_controller_pressed(p->controller, TX_CONTROL_DOWN) && !p->growing && !p->stone) || ((p->duck || p->wants_buttjump || p->crawl) && tx_controller_hold(p->controller, TX_CONTROL_DOWN))) &&
    !p->swimming && !p->sliding && !p->stone && !p->growing) {
    tx_player_do_duck(p);
  }
  else if (!p->sliding && !p->growing) {
    tx_player_do_standup_force(p, false);
  }

  /* Drop grabbed object when releasing the Action button on keyboard or gamepad, and on the second button press when using touchscreen */
  if ((tx_controller_is_touchscreen(p->controller) ? tx_controller_pressed(p->controller, TX_CONTROL_ACTION) : !tx_controller_hold(p->controller, TX_CONTROL_ACTION)) &&
      p->grabbed_object && !just_grabbed)
  {
    TxObj* moving_object = p->grabbed_object;
    /* NOT TRANSLATED (NT10): assert(moving_object); */
    TxRect grabbed_bbox;
    TxRect dest_;

    /* move the grabbed object a bit away from tux */
    grabbed_bbox = moving_object->bbox;
    dest_ = tx_rect_zero();
    if (p->swimming || p->water_jump)
    {
      tx_rect_set_bottom(&dest_, tx_rect_bottom(&p->col.bbox) + (sinf(p->swimming_angle) * 32.f));
      tx_rect_set_top(&dest_, tx_rect_bottom(&dest_) - tx_rect_height(&grabbed_bbox));
      tx_rect_set_left(&dest_, tx_rect_left(&p->col.bbox) + (cosf(p->swimming_angle) * 32.f));
      tx_rect_set_right(&dest_, tx_rect_left(&dest_) + tx_rect_width(&grabbed_bbox));
    }
    else
    {
      TxRect player_head_clear_box = tx_rect_grown(&p->col.bbox, -2.f);
      tx_rect_set_top(&player_head_clear_box, tx_rect_top(&p->col.bbox) - 2.f);
      if ((tx_player_is_big(p) && !p->duck) || tx_is_free_of_statics(p->sector, &player_head_clear_box, moving_object, true, TX_TILE_SOLID)) {
        tx_rect_set_bottom(&dest_, tx_rect_top(&p->col.bbox) + tx_rect_height(&p->col.bbox) * 0.66666f);
      }
      else {
        tx_rect_set_bottom(&dest_, tx_rect_bottom(&p->col.bbox) + 2.f);
      }
      tx_rect_set_top(&dest_, tx_rect_bottom(&dest_) - tx_rect_height(&grabbed_bbox));

      if (p->dir == TX_DIR_LEFT)
      {
        tx_rect_set_right(&dest_, tx_rect_left(&p->col.bbox) - 1);
        tx_rect_set_left(&dest_, tx_rect_right(&dest_) - tx_rect_width(&grabbed_bbox));
      }
      else
      {
        tx_rect_set_left(&dest_, tx_rect_right(&p->col.bbox) + 1);
        tx_rect_set_right(&dest_, tx_rect_left(&dest_) + tx_rect_width(&grabbed_bbox));
      }
    }

    if (tx_is_free_of_tiles(p->sector, &dest_, true, TX_TILE_SOLID) &&
       tx_is_free_of_statics(p->sector, &dest_, moving_object, true, TX_TILE_SOLID))
    {
      tx__ph_object_set_pos(p, moving_object, tx_rect_p1(&dest_));
      if (tx_controller_hold(p->controller, TX_CONTROL_UP))
      {
        tx__ph_portable_ungrab(p, p->grabbed_object, TX_DIR_UP);
      }
      else if (tx_controller_hold(p->controller, TX_CONTROL_DOWN))
      {
        tx__ph_portable_ungrab(p, p->grabbed_object, TX_DIR_DOWN);
      }
      else if (p->swimming || p->water_jump)
      {
        tx__ph_portable_ungrab(p, p->grabbed_object,
          fabsf(p->swimming_angle) <= TX_PI_2 ? TX_DIR_RIGHT : TX_DIR_LEFT);
      }
      else
      {
        tx__ph_portable_ungrab(p, p->grabbed_object, p->dir);
      }
      if (p->host.object_del_remove_listener)
        p->host.object_del_remove_listener(p->host.user, moving_object);
      p->grabbed_object = NULL;
      p->released_object = true;
    }
  }

  if (!tx_controller_hold(p->controller, TX_CONTROL_ACTION) && p->released_object) {
    p->released_object = false;
  }

  /* stop backflipping at will */
  if ( p->backflipping && ( !tx_controller_hold(p->controller, TX_CONTROL_JUMP) && !tx_timer_started(&p->backflip_timer)) ){
    tx_player_stop_backflipping(p);
  }

  if (p->sliding)
  {
    tx_player_adjust_height(p, TX_DUCKED_TUX_HEIGHT, 0);
    tx_player_slide(p);
  }
  else if (!p->sliding && (tx_timer_started(&p->coyote_timer)) && !tx_timer_started(&p->skidding_timer) &&
    (p->floor_normal.y != 0 || (tx_controller_hold(p->controller, TX_CONTROL_LEFT) || tx_controller_hold(p->controller, TX_CONTROL_RIGHT)))
    && tx_controller_pressed(p->controller, TX_CONTROL_DOWN) && fabsf(tx_physic_get_velocity_x(&p->physic)) > 1.f &&
    tx_player_get_bonus(p)!= TX_BONUS_EARTH)
  {
    tx_player_sideways_push(p, p->dir == TX_DIR_LEFT ? -100.f : 100.f);
    tx_player_adjust_height(p, TX_DUCKED_TUX_HEIGHT, 0);
    tx_player_slide(p);
  }
}

/* Player::position_grabbed_object  (L1803) (C++ default: teleport = false) */
TX_FN void tx_player_position_grabbed_object(TxPlayer* p, bool teleport)
{
  TxObj* moving_object;
  const TxRect* object_bbox;
  TxVec pos;

  if (!p->grabbed_object)
    return;

  moving_object = p->grabbed_object;
  /* NOT TRANSLATED (NT10): assert(moving_object); */
  object_bbox = &moving_object->bbox;

  if (!p->swimming && !p->water_jump)
  {
    /* Position where we will hold the lower-inner corner */
    pos = tx_vec(tx_rect_left(&p->col.bbox) + tx_rect_width(&p->col.bbox) / 2,
                 tx_rect_top(&p->col.bbox) + tx_rect_height(&p->col.bbox) * 0.66666f);
    /* Adjust to find the grabbed object's upper-left corner */
    if (p->dir == TX_DIR_LEFT)
      pos.x -= tx_rect_width(object_bbox);
    pos.y -= tx_rect_height(object_bbox);
  }
  else
  {
    pos = tx_vec(tx_rect_left(&p->col.bbox) + (cosf(p->swimming_angle) * 32.f),
                 tx_rect_top(&p->col.bbox) + (sinf(p->swimming_angle) * 32.f));
  }

  if (teleport)
    tx__ph_object_set_pos(p, moving_object, pos);
  if (p->host.portable_grab)
    p->host.portable_grab(p->host.user, p->grabbed_object, pos, p->dir);
}

/* Player::try_grab  (L1835) */
TX_FN bool tx_player_try_grab(TxPlayer* p)
{
  if (tx_controller_hold(p->controller, TX_CONTROL_ACTION) && !p->grabbed_object && !(p->duck ^ p->crawl) && !p->released_object)
  {

    TxVec pos = tx_vec(0.0f, 0.0f);
    if (!p->swimming && !p->water_jump)
    {
      if (p->dir == TX_DIR_LEFT)
      {
        pos = tx_vec(tx_rect_left(&p->col.bbox) - 5, tx_rect_bottom(&p->col.bbox) - 16);
      }
      else
      {
        pos = tx_vec(tx_rect_right(&p->col.bbox) + 5, tx_rect_bottom(&p->col.bbox) - 16);
      }
    }
    else
    {
      pos = tx_vec(tx_rect_left(&p->col.bbox) + 16.f + (cosf(p->swimming_angle) * 48.f),
                   tx_rect_top(&p->col.bbox) + 16.f + (sinf(p->swimming_angle) * 48.f));
    }

    /* for (auto& moving_object : Sector::get().get_objects_by_type<MovingObject>())
       {
         Portable* portable = dynamic_cast<Portable*>(&moving_object);
         if (portable && portable->is_portable() && !portable->is_grabbed())
         {
           // make sure the Portable isn't currently non-solid
           if (moving_object.get_group() == COLGROUP_DISABLED) continue;

           // check if we are within reach
           if (moving_object.get_bbox().contains(pos))
           {
       : the hook answers the first object that reaches this point */
    {
      TxObj* moving_object = p->host.find_portable ? p->host.find_portable(p->host.user, pos) : NULL;
      if (moving_object)
      {
          if (p->climbing)
            tx_player_stop_climbing(p, p->climbing);
          p->grabbed_object = moving_object;

          if (p->host.object_add_remove_listener)
            p->host.object_add_remove_listener(p->host.user, moving_object);

          tx_player_position_grabbed_object(p, false);
          return true;
      }
    }
  }
  return false;
}

/* Player::handle_input_ghost  (L1885) */
TX_FN void tx_player_handle_input_ghost(TxPlayer* p)
{
  float vx = 0;
  float vy = 0;
  if (tx_controller_hold(p->controller, TX_CONTROL_LEFT)) {
    p->dir = TX_DIR_LEFT;
    vx -= TX_MAX_RUN_XM * 2;
  }
  if (tx_controller_hold(p->controller, TX_CONTROL_RIGHT)) {
    p->dir = TX_DIR_RIGHT;
    vx += TX_MAX_RUN_XM * 2;
  }
  if (tx_controller_hold(p->controller, TX_CONTROL_UP)) {
    vy -= TX_MAX_RUN_XM * 2;
  }
  if (tx_controller_hold(p->controller, TX_CONTROL_DOWN)) {
    vy += TX_MAX_RUN_XM * 2;
  }
  if (tx_controller_hold(p->controller, TX_CONTROL_ACTION)) {
    tx_player_set_ghost_mode(p, false);
  }
  tx_physic_set_velocity_v(&p->physic, tx_vec_scale(tx_vec(vx, vy), (tx_controller_hold(p->controller, TX_CONTROL_JUMP) ? 2.5f : 1.f)));
  tx_physic_set_acceleration(&p->physic, 0, 0);
}

/* Player::update_hitbox  (L2375) */
TX_FN void tx_player_update_hitbox(TxPlayer* p)
{
  /* Don't use the default MovingSprite hitbox behavior
     because this class already has logic for that.
     Refer to the adjust_height method. */
  (void)p;
}

/* MovingSprite::set_action(const std::string& name, const Direction& dir,
   int loops): m_sprite->set_action(name, dir, loops); update_hitbox(); */
TX_FN void tx__player_set_action(TxPlayer* p, const char* name, int dir, int loops)
{
  if (p->host.sprite_set_action) p->host.sprite_set_action(p->host.user, name, dir, loops);
  tx_player_update_hitbox(p);
}

/* Player::set_bonus(BonusType type, bool animate, bool pocket)  (L1982)
   (C++ defaults: animate = false, pocket = true)
   like add_bonus, but can also downgrade the bonus items carried */
TX_FN bool tx_player_set_bonus(TxPlayer* p, int type, bool animate, bool pocket)
{
  if (p->dying) {
    return false;
  }

  if ((tx_player_get_bonus(p) == TX_BONUS_NONE) && (type != TX_BONUS_NONE || p->stone)) {
    if (!p->swimming && !p->sliding)
    {
      if (!tx_player_adjust_height(p, TX_BIG_TUX_HEIGHT, 0))
      {
        /* NOT TRANSLATED (NT11): log_debug << "Can't adjust Tux height" << std::endl; */
        return false;
      }
    }
    if (animate) {
      p->growing = true;
      if (p->climbing)
        tx__player_set_action(p, "climbgrow", p->dir, 1);
      else if (p->swimming)
        tx__player_set_action(p, "swimgrow", p->dir, 1);
      else if (p->sliding)
        tx__player_set_action(p, "slidegrow", p->dir, 1);
      else
        tx__player_set_action(p, "grow", p->dir , 1);
    }
    else if (type == TX_BONUS_GROWUP) {
      /* force-change Tux's sprite immediately when growing up */
      p->reset_action = true;
    }
  }

  if (type == TX_BONUS_NONE) {
    if (!tx_player_adjust_height(p, TX_SMALL_TUX_HEIGHT, 0)) {
      /* NOT TRANSLATED (NT11): log_debug << "Can't adjust Tux height" << std::endl; */
      return false;
    }
    if (p->does_buttjump) p->does_buttjump = false;
  }

  if (type > TX_BONUS_GROWUP)
  {
    if (pocket) {
      if (p->host.status_add_item_to_pocket)
        p->host.status_add_item_to_pocket(p->host.user, tx_player_get_bonus(p));
    }

    if (!tx_timer_started(&p->second_growup_sound_timer) && type != tx_player_get_bonus(p))
    {
      tx_timer_start(&p->second_growup_sound_timer, 0.5);
    }
  }

  p->bonus = type;
  return true;
}

/* Player::add_bonus(BonusType type, bool animate)  (L1965)
   (C++ default: animate = false)
   picks up a bonus, taking care not to pick up lesser bonus items than we
   already have */
TX_FN bool tx_player_add_bonus(TxPlayer* p, int type, bool animate)
{
  /* always ignore NO_BONUS */
  if (type == TX_BONUS_NONE) {
    return true;
  }

  /* ignore GROWUP_BONUS if we're already big */
  if (type == TX_BONUS_GROWUP) {
    if (tx_player_get_bonus(p) != TX_BONUS_NONE)
      return true;
  }

  return tx_player_set_bonus(p, type, animate, true);
}

/* Player::kick  (L2050) */
TX_FN void tx_player_kick(TxPlayer* p)
{
  tx_timer_start(&p->kick_timer, TX_KICK_TIME);
}

/* Player::draw  (L2062)
   NOT TRANSLATED (NT7): see the top for what it does to the state. */

/* Player::collision_tile  (L2349) */
TX_FN void tx_player_collision_tile(TxPlayer* p, uint32_t tile_attributes)
{
  if (tile_attributes & TX_TILE_HURTS)
  {
    if (tile_attributes & TX_TILE_UNISOLID)
      tx_player_kill(p, false);
    else
    {
      TxRect hurtbox = tx_rect_grown(&p->col.bbox, -6.f);
      if (!tx_is_free_of_tiles(p->sector, &hurtbox, false, TX_TILE_HURTS))
        tx_player_kill(p, false);
    }
  }

  if (tile_attributes & TX_TILE_WALLJUMP)
  {
    p->in_walljump_tile = true;
  }

  if (tile_attributes & TX_TILE_ICE) {
    p->ice_this_frame = true;
    p->on_ice = true;
  }
}

/* Player::handle_collision_logic  (L2383)
   Method for shared collision logic */
TX_FN void tx_player_handle_collision_logic(TxPlayer* p, const TxHit* hit)
{
  if (hit->bottom) {
    if (tx_physic_get_velocity_y(&p->physic) > 0)
      tx_physic_set_velocity_y(&p->physic, 0);

    if (!p->swimming)
      p->on_ground_flag = true;
    p->floor_normal = hit->slope_normal;
    p->is_slidejump_falling = false;
    p->slidejumping = false;

    /* Butt Jump landed */
    if (p->does_buttjump)
    {
      /* don't boing if on a slope. */
      if (tx_player_on_ground(p) && p->floor_normal.y != 0)
      {
        /* m_does_buttjump will get processed/reset in slide(), as we want to
           also give the player an "oomph" if they buttjump while standing
           mostly still. At this point, the player will slide. */
      }
      else
      { /* the buttjump */
        p->does_buttjump = false;
        p->buttjump_stomp = true;
        tx_physic_set_velocity_y(&p->physic, -300);
        p->on_ground_flag = false;
        tx__ph_add_particles(p,
          tx_rect_p2(&p->col.bbox),
          50, 70, 260, 280, tx_vec(0, 300), 3,
          .4f, .4f, .4f, 3, .8f, TX_LAYER_OBJECTS+1);
        tx__ph_add_particles(p,
          tx_vec(tx_rect_left(&p->col.bbox), tx_rect_bottom(&p->col.bbox)),
          -70, -50, 260, 280, tx_vec(0, 300), 3,
          .4f, .4f, .4f, 3, .8f, TX_LAYER_OBJECTS+1);
        if (p->host.camera_shake) p->host.camera_shake(p->host.user, .1f, 0.f, 10.f);
      }
    }

  } else if (hit->top) {
    if (tx_physic_get_velocity_y(&p->physic) < 0)
      tx_physic_set_velocity_y(&p->physic, .2f);
  }

  if (p->stone && p->floor_normal.y == 0 && (((tx_physic_get_velocity_x(&p->physic) < -TX_MAX_RUN_XM) && hit->left) ||
    ((tx_physic_get_velocity_x(&p->physic) > TX_MAX_RUN_XM) && hit->right)))
  {
    tx_physic_set_acceleration_x(&p->physic, 0);
    tx_physic_set_velocity_x(&p->physic, 0);
    tx_player_stop_rolling(p, true);
  }

  if ((hit->left || hit->right) && hit->slope_normal.x == 0) {
    tx_physic_set_velocity_x(&p->physic, 0);
  }

  if (hit->crush)
    tx_player_kill(p, true);

  if ((hit->left && p->boost < 0.f) || (hit->right && p->boost > 0.f))
    p->boost = 0.f;
}

/* Player::collision_solid  (L2448) */
TX_FN void tx_player_collision_solid(TxPlayer* p, const TxHit* hit)
{
  tx_player_handle_collision_logic(p, hit);
}

/* Player::collision  (L2454)
   `other_kind` is the TX_OTHER_* of the dynamic type of the other object;
   `other` is its TxObj* (its group is read from it, and it is what the hooks
   receive). Returns a HitResponse (TX_ABORT_MOVE, TX_CONTINUE,
   TX_FORCE_MOVE). */
TX_FN int tx_player_collision(TxPlayer* p, int other_kind, void* other, const TxHit* hit)
{
  TxObj* other_obj = (TxObj*)other;

  /* auto bullet = dynamic_cast<Bullet*> (&other); */
  if (other_kind == TX_OTHER_BULLET) {
    return TX_FORCE_MOVE;
  }

  /* auto player = dynamic_cast<Player*> (&other); */
  if (other_kind == TX_OTHER_PLAYER) {
    return TX_ABORT_MOVE;
  }

  if (hit->left || hit->right) {
    tx_player_try_grab(p); /* grab objects right now, in update it will be too late */
  }
  if (other_obj->group == TX_COLGROUP_TOUCHABLE) {
    /* auto trigger = dynamic_cast<TriggerBase*> (&other); */
    if (other_kind == TX_OTHER_TRIGGER && !p->deactivated) {
      if (tx_controller_pressed(p->controller, TX_CONTROL_UP)) {
        if (p->host.trigger_event)
          p->host.trigger_event(p->host.user, other_obj, TX_EVENT_ACTIVATE);
      }
    }

    return TX_FORCE_MOVE;
  }

  /* auto badguy = dynamic_cast<BadGuy*> (&other); */
  if (other_kind == TX_OTHER_BADGUY) {
    if (p->is_intentionally_safe || tx_timer_started(&p->post_damage_safety_timer) || tx_timer_started(&p->temp_safety_timer) || tx_timer_started(&p->invincible_timer))
      return TX_FORCE_MOVE;
    if (p->stone)
      return TX_ABORT_MOVE;
    if ((p->host.badguy_is_frozen ? p->host.badguy_is_frozen(p->host.user, other_obj) : false)
        && (p->host.badguy_get_velocity_y ? p->host.badguy_get_velocity_y(p->host.user, other_obj) : 0.0f) != 0) {
      tx_player_handle_collision_logic(p, hit);
    }
  }

  return TX_CONTINUE;
}

/* Player::set_pos_reset  (L2623)
   Set Tux's position, reset state and velocity. */
TX_FN void tx_player_set_pos_reset(TxPlayer* p, TxVec vector)
{
  tx_obj_set_pos(&p->col, vector);

  /* Reset size to get correct hitbox if Tux was eg. ducked before moving */
  if (tx_player_is_big(p))
    tx_obj_set_size(&p->col, TX_TUX_WIDTH, TX_BIG_TUX_HEIGHT);
  else
    tx_obj_set_size(&p->col, TX_TUX_WIDTH, TX_SMALL_TUX_HEIGHT);
  p->duck = false;
  tx_player_stop_backflipping(p);
  p->last_ground_y = vector.y;
  if (p->climbing) tx_player_stop_climbing(p, p->climbing);

  /* Make sure objects following Tux move directly with him */
  tx_player_position_grabbed_object(p, true);
  /* NOT TRANSLATED (NT6):
       for (Key* key : m_collected_keys)
         key->update_pos(); */

  tx_physic_reset(&p->physic);
}

/* Player::on_flip  (L2494) */
TX_FN void tx_player_on_flip(TxPlayer* p, float height)
{
  TxVec pos = tx__player_get_pos(p);
  pos.y = height - pos.y - tx_rect_height(&p->col.bbox);
  tx_player_set_pos_reset(p, pos);

  tx_player_position_grabbed_object(p, true);
}

/* Player::remove_me  (L2504)
   NOT TRANSLATED (NT9):
     InputManager::current()->on_player_removed(get_id());
     MovingObject::remove_me(); */

/* Player::make_invincible  (L2511)
   Make Tux invincible for a short amount of time. */
TX_FN void tx_player_make_invincible(TxPlayer* p)
{
  /* No get_pos() here since the music affects the whole sector */
  if (p->host.play_sound) p->host.play_sound(p->host.user, "sounds/invincible_start.ogg", NULL);
  tx_timer_start(&p->invincible_timer, TX_TUX_INVINCIBLE_TIME);
  if (p->host.play_music) p->host.play_music(p->host.user, TX_HERRING_MUSIC);
}

/* Player::make_temporarily_safe  (L2520) */
TX_FN void tx_player_make_temporarily_safe(TxPlayer* p, float safe_time)
{
  tx_timer_start(&p->temp_safety_timer, safe_time);
}

/* Player::kill  (L2526)
   Hurts Tux. If completely is true, he will be killed even if he had "grow"
   or a superior bonus. */
TX_FN void tx_player_kill(TxPlayer* p, bool completely)
{
  if (p->dying || p->deactivated || tx_player_is_winning(p))
    return;

  if (!completely && (p->is_intentionally_safe || tx_timer_started(&p->post_damage_safety_timer) || tx_timer_started(&p->temp_safety_timer) || tx_timer_started(&p->invincible_timer)))
    return;

  p->growing = false;

  if (p->climbing) tx_player_stop_climbing(p, p->climbing);

  tx_physic_set_velocity_x(&p->physic, 0);
  p->boost = 0.f;

  tx__ph_sprite_set_angle(p, 0.0f);
  /*m_santahatsprite->set_angle(0.0f);*/

  if (!completely && tx_player_is_big(p)) {
    tx__ph_play_sound_here(p, "sounds/hurt.wav");
    p->reset_action = true;

    if (tx_player_get_bonus(p) > TX_BONUS_GROWUP)
    {
      tx_timer_start(&p->post_damage_safety_timer, TX_TUX_SAFE_TIME);
      tx_player_set_bonus(p, TX_BONUS_GROWUP, true, true);
    }
    else if (tx_player_get_bonus(p) == TX_BONUS_GROWUP)
    {
      tx_timer_start(&p->post_damage_safety_timer, TX_TUX_SAFE_TIME /* + GROWING_TIME */);
      p->duck = false;
      p->crawl = false;
      tx_player_stop_backflipping(p);
      tx_player_set_bonus(p, TX_BONUS_NONE, true, true);
    }
  } else {
    int alive_players;

    /* auto* session = GameSession::current();
       if (session && session->m_prevent_death &&
                      !session->reset_checkpoint_button) */
    if (p->host.session_prevent_death ? p->host.session_prevent_death(p->host.user) : false)
    {
      tx_player_set_ghost_mode(p, true);
      return;
    }

    tx__ph_play_sound_here(p, "sounds/kill.wav");

    if (p->stone) tx_player_stop_rolling(p, false);

    tx_physic_enable_gravity(&p->physic, true);
    tx_physic_set_gravity_modifier(&p->physic, 1.0f); /* Undo jump_early_apex */
    tx_timer_stop(&p->post_damage_safety_timer);
    tx_timer_stop(&p->temp_safety_timer);
    tx_timer_stop(&p->invincible_timer);
    tx_physic_set_acceleration(&p->physic, 0, 0);
    tx_physic_set_velocity(&p->physic, 0, -700);
    tx_player_set_bonus(p, TX_BONUS_NONE, true, true);
    p->dying = true;
    p->duck = p->crawl = false;
    tx_timer_start(&p->dying_timer, 3.0);
    tx__player_set_group(p, TX_COLGROUP_DISABLED);

    /* auto alive_players = Sector::get().get_object_count<Player>([](const Player& p){ return p.is_alive(); }); */
    alive_players = p->host.count_alive_players ? p->host.count_alive_players(p->host.user) : 0;

    if (!alive_players)
    {
      if (p->host.status_respawns_at_checkpoint ? p->host.status_respawns_at_checkpoint(p->host.user) : false)
      {
        int i;
        for (i = 0; i < 5; i++)
        {
          /* the numbers: starting x, starting y, velocity y */
          /* Sector::get().add<FallingCoin>(get_pos() +
               Vector(graphicsRandom.randf(5.0f), graphicsRandom.randf(-32.0f, 18.0f)),
               graphicsRandom.randf(-100.0f, 100.0f));
             the order of the three draws is the compiler's (see the top) */
          float rx, ry, rv;
#if TX_PLAYER_ARGS_RIGHT_TO_LEFT
          rv = tx__ph_rand_f(p, -100.0f, 100.0f);
          ry = tx__ph_rand_f(p, -32.0f, 18.0f);
          rx = tx__ph_rand_f(p, 0.0f, 5.0f);
#else
          rx = tx__ph_rand_f(p, 0.0f, 5.0f);
          ry = tx__ph_rand_f(p, -32.0f, 18.0f);
          rv = tx__ph_rand_f(p, -100.0f, 100.0f);
#endif
          if (p->host.add_falling_coin)
            p->host.add_falling_coin(p->host.user, tx_vec_add(tx__player_get_pos(p),
                                                              tx_vec(rx, ry)),
                                                              rv);
        }
        if (p->host.status_take_checkpoint_coins)
          p->host.status_take_checkpoint_coins(p->host.user);
      }

      if (p->host.effect_fade_out) p->host.effect_fade_out(p->host.user, 3.0);
      if (p->host.pause_music) p->host.pause_music(p->host.user, 3.0);
    }
  }

  /*Sector::get().get_camera().shake(0.1f, m_dying ? 32.f : 0.f, m_dying ? 20.f : 10.f);*/
}

/* Player::set_pos  (L2612) */
TX_FN void tx_player_set_pos(TxPlayer* p, TxVec vector)
{
  /* MovingObject::set_pos(vector): m_col.set_pos(pos); */
  tx_obj_set_pos(&p->col, vector);

  /* Make sure objects following Tux move directly with him */
  tx_player_position_grabbed_object(p, true);
  /* NOT TRANSLATED (NT6):
       for (Key* key : m_collected_keys)
         key->update_pos(); */
}

/* Player::check_bounds  (L2646) */
TX_FN void tx_player_check_bounds(TxPlayer* p)
{
  /* Keep tux in sector bounds: */
  if (tx__player_get_pos(p).x < 0) {
    /* Lock Tux to the size of the level, so that he doesn't fall off
       the left side */
    tx_obj_set_pos(&p->col, tx_vec(0, tx__player_get_pos(p).y));
  }

  if (tx_rect_right(&p->col.bbox) > tx__sector_get_width(p->sector)) {
    /* Lock Tux to the size of the level, so that he doesn't fall off
       the right side */
    tx_obj_set_pos(&p->col, tx_vec(tx__sector_get_width(p->sector) - tx_rect_width(&p->col.bbox),
                         tx_rect_top(&p->col.bbox)));
  }

  /* If Tux is swimming, don't allow him to go below the sector */
  if (p->swimming && !p->ghost_mode && tx_player_is_alive(p)
      && tx_rect_bottom(&p->col.bbox) > tx__sector_get_height(p->sector)) {
    tx_obj_set_pos(&p->col, tx_vec(tx_rect_left(&p->col.bbox),
                         tx__sector_get_height(p->sector) - tx_rect_height(&p->col.bbox)));
  }

  /* fallen out of the level? */
  if ((tx__player_get_pos(p).y > tx__sector_get_height(p->sector))
      && !p->ghost_mode
      && !tx_timer_started(&p->temp_safety_timer)) {
    tx_player_kill(p, true);
    return;
  }
}

/* Player::add_velocity(const Vector& velocity)  (L2679)
   Adds velocity to the player (be careful when using this) */
TX_FN void tx_player_add_velocity(TxPlayer* p, TxVec velocity)
{
  tx_physic_set_velocity_v(&p->physic, tx_vec_add(tx_physic_get_velocity(&p->physic), velocity));
}

/* Player::add_velocity(const Vector& velocity, const Vector& end_speed)  (L2685)
   Adds velocity to the player until given end speed is reached */
TX_FN void tx_player_add_velocity_until(TxPlayer* p, TxVec velocity, TxVec end_speed)
{
  if (end_speed.x > 0)
    tx_physic_set_velocity_x(&p->physic, tx_minf(tx_physic_get_velocity_x(&p->physic) + velocity.x, end_speed.x));
  if (end_speed.x < 0)
    tx_physic_set_velocity_x(&p->physic, tx_maxf(tx_physic_get_velocity_x(&p->physic) + velocity.x, end_speed.x));
  if (end_speed.y > 0)
    tx_physic_set_velocity_y(&p->physic, tx_minf(tx_physic_get_velocity_y(&p->physic) + velocity.y, end_speed.y));
  if (end_speed.y < 0)
    tx_physic_set_velocity_y(&p->physic, tx_maxf(tx_physic_get_velocity_y(&p->physic) + velocity.y, end_speed.y));
}

/* Player::bounce(BadGuy&)  (L2698) (the badguy is not used) */
TX_FN void tx_player_bounce(TxPlayer* p)
{
  if (!(tx_player_get_bonus(p) == TX_BONUS_AIR))
    tx_physic_set_velocity_y(&p->physic, tx_controller_hold(p->controller, TX_CONTROL_JUMP) ? -520.0f : -300.0f);
  else {
    tx_physic_set_velocity_y(&p->physic, tx_controller_hold(p->controller, TX_CONTROL_JUMP) ? -580.0f : -340.0f);
  }
}

/* scripting Functions Below */

/* Player::activate  (L2710) */
TX_FN void tx_player_activate(TxPlayer* p)
{
  if (!p->deactivated)
    return;
  p->deactivated = false;
}

/* Player::deactivate  (L2718) */
TX_FN void tx_player_deactivate(TxPlayer* p)
{
  if (p->deactivated)
    return;
  p->deactivated = true;
  tx_physic_set_velocity(&p->physic, 0, 0);
  tx_physic_set_acceleration_x(&p->physic, 0);
  tx_physic_set_acceleration_y(&p->physic, 0);
  if (p->climbing) tx_player_stop_climbing(p, p->climbing);
}

/* Player::set_dir  (L2748) */
TX_FN void tx_player_set_dir(TxPlayer* p, bool right)
{
  p->dir = right ? TX_DIR_RIGHT : TX_DIR_LEFT;
}

/* Player::set_ghost_mode  (L2754) */
TX_FN void tx_player_set_ghost_mode(TxPlayer* p, bool enable)
{
  if (p->ghost_mode == enable)
    return;

  if (p->climbing) tx_player_stop_climbing(p, p->climbing);

  tx_player_ungrab_object(p, false);

  if (enable) {
    p->ghost_mode = true;
    tx__player_set_group(p, TX_COLGROUP_DISABLED);
    tx_physic_enable_gravity(&p->physic, false);
    /* NOT TRANSLATED (NT11): log_debug << "You feel lightheaded. Use movement controls to float around, press ACTION to scare badguys." << std::endl; */
  } else {
    p->ghost_mode = false;
    tx__player_set_group(p, TX_COLGROUP_MOVING);
    tx_physic_enable_gravity(&p->physic, true);
    /* NOT TRANSLATED (NT11): log_debug << "You feel solid again." << std::endl; */
  }
}

/* Player::start_climbing  (L2777)
   Requests that the player start climbing the given Climbable */
TX_FN void tx_player_start_climbing(TxPlayer* p, TxObj* climbable)
{
  if (p->climbing || p->swimming || p->stone)
    return;

  p->climbing = climbable;
  tx__ph_sprite_set_angle(p, 0.0f);
  p->boost = 0.f;
  tx_physic_enable_gravity(&p->physic, false);
  tx_physic_set_velocity(&p->physic, 0, 0);
  tx_physic_set_acceleration(&p->physic, 0, 0);
  if (p->backflipping) {
    tx_player_stop_backflipping(p);
    tx_player_do_standup_force(p, true);
  }
}

/* Player::stop_climbing  (L2795)
   Requests that the player stop climbing the given Climbable */
TX_FN void tx_player_stop_climbing(TxPlayer* p, TxObj* climbable)
{
  (void)climbable; /* Climbable& climbable is not used */
  if (!p->climbing) return;

  p->climbing = NULL;

  tx_player_ungrab_object(p, false);

  tx_physic_enable_gravity(&p->physic, true);
  tx_physic_set_velocity(&p->physic, 0, 0);
  tx_physic_set_acceleration(&p->physic, 0, 0);

  if (tx_controller_hold(p->controller, TX_CONTROL_JUMP) && !tx_controller_hold(p->controller, TX_CONTROL_DOWN)) {
    p->on_ground_flag = true;
    p->jump_early_apex = false;
    tx_player_do_jump(p, tx_player_get_bonus(p) == TX_BONUS_AIR ? -540.0f : -480.0f);
  }
  else if (tx_controller_hold(p->controller, TX_CONTROL_UP)) {
    p->on_ground_flag = true;
    /* TODO: This won't help. Why? */
    tx_player_do_jump(p, -300);
  }
}

/* Player::handle_input_climbing  (L2820) */
TX_FN void tx_player_handle_input_climbing(TxPlayer* p)
{
  float vx;
  float vy;
  TxRect obj_bbox;

  if (!p->climbing) {
    /* NOT TRANSLATED (NT11): log_warning << "handle_input_climbing called with climbing set to 0. Input handling skipped" << std::endl; */
    return;
  }

  vx = 0;
  vy = 0;
  obj_bbox = p->climbing->bbox;
  if (tx_controller_hold(p->controller, TX_CONTROL_LEFT) && tx_rect_left(&p->col.bbox) > tx_rect_left(&obj_bbox)) {
    p->dir = TX_DIR_LEFT;
    vx -= TX_MAX_CLIMB_XM;
  }
  if (tx_controller_hold(p->controller, TX_CONTROL_RIGHT) && tx_rect_right(&p->col.bbox) < tx_rect_right(&obj_bbox)) {
    p->dir = TX_DIR_RIGHT;
    vx += TX_MAX_CLIMB_XM;
  }
  if (tx_controller_hold(p->controller, TX_CONTROL_UP) && tx_rect_top(&p->col.bbox) > tx_rect_top(&obj_bbox)) {
    vy -= TX_MAX_CLIMB_YM;
  }
  if (tx_controller_hold(p->controller, TX_CONTROL_DOWN) && tx_rect_bottom(&p->col.bbox) < tx_rect_bottom(&obj_bbox)) {
    vy += TX_MAX_CLIMB_YM;
  }
  if (tx_controller_hold(p->controller, TX_CONTROL_JUMP)) {
    if (p->can_jump) {
      tx_player_stop_climbing(p, p->climbing);
      return;
    }
  } else {
    p->can_jump = true;
  }
  tx_physic_set_velocity(&p->physic, vx, vy);
  tx_physic_set_acceleration(&p->physic, 0, 0);
}

/* Player::handle_input_rolling  (L2857) */
TX_FN void tx_player_handle_input_rolling(TxPlayer* p)
{
  /* handle exiting */
  if (p->stone)
  {
    if (!tx_controller_hold(p->controller, TX_CONTROL_DOWN)) {
      tx_player_stop_rolling(p, false);
    }
    else if (tx_player_get_bonus(p) != TX_BONUS_EARTH) {
      tx_player_stop_rolling(p, true);
    }
  }

  /* handle jumping */
  if (tx_controller_pressed(p->controller, TX_CONTROL_JUMP)) tx_timer_start(&p->jump_button_timer, TX_JUMP_GRACE_TIME);
  if (tx_controller_hold(p->controller, TX_CONTROL_JUMP) && tx_timer_started(&p->jump_button_timer) && (p->can_jump || tx_timer_started(&p->coyote_timer)))
  {
    tx_timer_stop(&p->jump_button_timer);
    tx_player_do_jump(p, -450.f);
    tx_timer_stop(&p->coyote_timer);
  }

  /* Let go of jump key */
  else if (!tx_controller_hold(p->controller, TX_CONTROL_JUMP)) {
    if (!p->backflipping && p->jumping && tx_physic_get_velocity_y(&p->physic) < 0) {
      p->jumping = false;
      tx_player_early_jump_apex(p);
    }
  }

  if (p->jump_early_apex && tx_physic_get_velocity_y(&p->physic) >= 0) {
    tx_player_do_jump_apex(p);
  }

  /* handle x-movement */

  if (fabsf(tx_physic_get_velocity_x(&p->physic)) > TX_MAX_STONE_SPEED) {
    tx_physic_set_acceleration_x(&p->physic, -tx_physic_get_velocity_x(&p->physic));
  }
  else
  {
    /* these variables are apparently used differently and must be initialized differently to avoid errors */
    float ax;
    float sx = 0.f;

    /* slope velocity */
    if (p->floor_normal.y != 0)
    {
      if (p->floor_normal.x > 0.f) {
        sx = ((p->dir == TX_DIR_LEFT ? TX_STONE_UP_ACCELERATION : TX_STONE_DOWN_ACCELERATION)*fabsf(p->floor_normal.x));
      }
      if (p->floor_normal.x < 0.f) {
        sx = ((p->dir == TX_DIR_RIGHT ? -TX_STONE_UP_ACCELERATION : -TX_STONE_DOWN_ACCELERATION)*fabsf(p->floor_normal.x));
      }
    }
    else
    {
      sx = 0.f;
    }

    /* key velocity */
    if (tx_controller_hold(p->controller, TX_CONTROL_LEFT) && !tx_controller_hold(p->controller, TX_CONTROL_RIGHT))
    {
      ax = -TX_STONE_KEY_ACCELERATION;
      p->dir = TX_DIR_LEFT;
    }
    else if (tx_controller_hold(p->controller, TX_CONTROL_RIGHT) && !tx_controller_hold(p->controller, TX_CONTROL_LEFT))
    {
      ax = TX_STONE_KEY_ACCELERATION;
      p->dir = TX_DIR_RIGHT;
    }
    else {
      ax = 0.f;
    }

    if (tx_controller_hold(p->controller, TX_CONTROL_RIGHT) || tx_controller_hold(p->controller, TX_CONTROL_LEFT) || p->floor_normal.y != 0.f) {
      tx_physic_set_acceleration_x(&p->physic, ax + sx);
    }
    else {
      tx_player_apply_friction(p);
    }
  }
}

/* Player::stop_backflipping  (L2942) */
TX_FN void tx_player_stop_backflipping(TxPlayer* p)
{
  p->backflipping = false;
  p->backflip_direction = 0;
  tx__ph_sprite_set_angle(p, 0.0f);
  /*m_santahatsprite->set_angle(0.0f);*/
}

/* Player::eject_item_pocket (L2951), get_item_pocket (L2957),
   set_item_pocket (L2963): NOT TRANSLATED (NT5).
   Player::has_grabbed (L2969): NOT TRANSLATED (NT1). */

/* Player::sideways_push  (L2983)
   Boosts Tux in a certain direction, sideways. Useful for bumpers/walljumping. */
TX_FN void tx_player_sideways_push(TxPlayer* p, float delta)
{
  p->boost = delta;
}

/* Player::ungrab_object(GameObject* gameobject = nullptr)  (L2989)
   Ungrabs the currently grabbed object, if any. `from_remove_listener` is
   `gameobject != nullptr`: only the ObjectRemoveListener passes it. */
TX_FN void tx_player_ungrab_object(TxPlayer* p, bool from_remove_listener)
{
  TxObj* go;

  if (!p->grabbed_object)
    return;

  /* If gameobject is not null, then the function was called from the
     ObjectRemoveListener. */
  if (!from_remove_listener)
    tx__ph_portable_ungrab(p, p->grabbed_object, p->dir);

  go = p->grabbed_object;

  /* if (go && m_grabbed_object_remove_listener) */
  if (go && p->host.object_del_remove_listener)
    p->host.object_del_remove_listener(p->host.user, go);

  p->grabbed_object = NULL;
}

/* Player::stop_grabbing (hpp L333) */
TX_FN void tx_player_stop_grabbing(TxPlayer* p) { tx_player_ungrab_object(p, false); }

/* Player::next_target (L3008), prev_target (L3049),
   multiplayer_prepare_spawn (L3080), multiplayer_respawn (L3097):
   NOT TRANSLATED (NT2). */

/* Player::stop_rolling  (L3125) (C++ default: violent = true) */
TX_FN void tx_player_stop_rolling(TxPlayer* p, bool violent)
{
  tx__ph_sprite_set_angle(p, 0.0f);
  if (!p->swimming && !p->water_jump && !p->sliding && !p->duck)
  {
    if (!tx_player_adjust_height(p, TX_BIG_TUX_HEIGHT, 0))
    {
      tx_player_adjust_height(p, TX_BIG_TUX_HEIGHT, 10.f);
      tx_player_do_duck(p);
    }
  }
  if (violent)
  {
    int i;
    for (i = 0; i < 5; i++)
    {
      /* Vector pspeed = Vector(graphicsRandom.randf(-100.f, 100.f)*(static_cast<float>(i)-2), graphicsRandom.randf(-200.f, -150.f));
         the order of the two draws is the compiler's (see the top) */
      TxVec pspeed;
      TxVec paccel;
      char action[32];
#if TX_PLAYER_ARGS_RIGHT_TO_LEFT
      pspeed.y = tx__ph_rand_f(p, -200.f, -150.f);
      pspeed.x = tx__ph_rand_f(p, -100.f, 100.f)*((float)(i)-2);
#else
      pspeed.x = tx__ph_rand_f(p, -100.f, 100.f)*((float)(i)-2);
      pspeed.y = tx__ph_rand_f(p, -200.f, -150.f);
#endif
      paccel = tx_vec(0, 1000.f + tx__ph_rand_f(p, -100.f, 100.f));
      /* "rock-"+std::to_string(i) */
      snprintf(action, sizeof(action), "rock-%d", i);
      if (p->host.add_sprite_particle)
        p->host.add_sprite_particle(p->host.user,
          "images/particles/rock.sprite", action,
          tx_rect_middle(&p->col.bbox),
          pspeed, paccel, TX_LAYER_OBJECTS + 6, true);
    }
    tx__ph_play_sound_here(p, "sounds/brick.wav");
  }
  p->stone = false;
}

/* Player::add_collected_key (L3153), remove_collected_key (L3159):
   NOT TRANSLATED (NT6).
   Player::register_class (L3169): NOT TRANSLATED (NT1). */

/* Player::update  (L384) */
TX_FN void tx_player_update(TxPlayer* p, float dt_sec)
{
  /* sanity asserts */
  /* NOT TRANSLATED (NT10): assert(!(m_duck && !is_big())); */

  /* NOT TRANSLATED (NT3): L389-L410
       if (is_dead() || Sector::get().get_object_count<Player>() == 1)
       {
         m_tag_timer.stop();
         m_tag_fade = nullptr;
         m_tag_alpha = 0.f;
         m_has_moved = true;
       }

       if (m_tag_timer.check())
       {
         m_tag_timer.stop();
         m_tag_fade = std::make_unique<FadeHelper>(1.f, 0.f, 1.f);
       }

       if (m_tag_fade)
       {
         m_tag_alpha = m_tag_fade->update(dt_sec);
         if (m_tag_fade->completed())
         {
           m_tag_fade = nullptr;
         }
       } */

  /* NOT TRANSLATED (NT4): L412-L427
       if (!m_active_bubbles.empty())
       {
         for (auto& bubble : m_active_bubbles)
         {
           bubble.second.y -= dt_sec * 40.0f;
           bubble.second.x += std::sin(bubble.second.y * 0.1f) * dt_sec * 5.0f;
         }

         m_active_bubbles.remove_if([&](const std::pair<SpritePtr, Vector>& bubble)
           {
             Rectf bubble_box(bubble.second.x, bubble.second.y, bubble.second.x + 16.f, bubble.second.y + 16.f);
             bool is_out_of_water = Sector::get().is_free_of_tiles(bubble_box, true, Tile::WATER);
             bool hits_solid = !Sector::get().is_free_of_tiles(bubble_box, false, Tile::SOLID);
             return is_out_of_water || hits_solid;
           });
       } */

  /* NOT TRANSLATED (NT2): L429-L456 (m_target is null with one player: the
     block is never entered)
       // Skip if in multiplayer respawn
       if (is_dead() && m_target && Sector::get().get_object_count<Player>([this](const Player& p) { return p.is_active() && &p != this; }))
       {
         auto* target = Sector::get().get_object_by_uid<Player>(*m_target);
         if (!target || !target->is_active())
         {
           next_target();
         }

         // Respawn input is handled outside handle_input() because it happens while the player is dead
         if (is_dead() && m_target)
         {
           if (m_controller->pressed(Control::ACTION))
           {
             multiplayer_respawn();
           }
           else if (m_controller->pressed(Control::LEFT))
           {
             prev_target();
           }
           else if (m_controller->pressed(Control::RIGHT))
           {
             next_target();
           }
         }

         return;
       } */

  tx_player_check_bounds(p);

  /* catch-all for other circumstances in which Tux's hitbox can't be properly adjusted */
  if (tx_player_is_big(p) &&
    !p->duck && !p->swimming && !p->water_jump && !p->backflipping && !p->sliding && !p->stone &&
    !tx_player_adjust_height(p, TX_BIG_TUX_HEIGHT, 0))
  {
    /* Force Tux's box up a little in order to not phase into floor */
    tx_player_adjust_height(p, TX_BIG_TUX_HEIGHT, 10.f);
  }

  if (p->velocity_override && tx_vec_length(tx_physic_get_velocity(&p->physic)) < TX_SWIM_BOOST_SPEED) {
    p->velocity_override = false;
  }

  /* handling of swimming */

/* #ifdef SWIMMING */
  if (!p->ghost_mode)
  {
    TxRect swim_here_box;
    bool can_swim_here;

    if (p->no_water)
    {
      if (p->swimming)
      {
        p->water_jump = true;
        if (tx_physic_get_velocity_y(&p->physic) > -350.f && tx_controller_hold(p->controller, TX_CONTROL_UP))
          tx_physic_set_velocity_y(&p->physic, -350.f);
      }
      p->swimming = false;
    }

    if ((tx_player_on_ground(p) || p->climbing || p->does_buttjump) && p->water_jump)
    {
      if (tx_player_is_big(p) && !p->stone && !tx_player_adjust_height(p, TX_BIG_TUX_HEIGHT, 0))
      {
        /* Force Tux's box up a little in order to not phase into floor */
        tx_player_adjust_height(p, TX_BIG_TUX_HEIGHT, 10.f);
        tx_player_do_duck(p);
      }
      else if (!tx_player_is_big(p) || (p->stone && !p->growing))
      {
        tx_player_adjust_height(p, TX_SMALL_TUX_HEIGHT, 0);
      }
      p->dir = (tx_physic_get_velocity_x(&p->physic) >= 0.f) ? TX_DIR_RIGHT : TX_DIR_LEFT;
      p->water_jump = false;
      p->swimboosting = false;
      /*m_santahatsprite->set_angle(0.f);*/
    }
    p->no_water = true;

    if ((p->swimming || p->water_jump) && tx_player_is_big(p))
    {
      tx_obj_set_size(&p->col, TX_TUX_WIDTH, TX_TUX_WIDTH);
      tx_player_adjust_height(p, TX_TUX_WIDTH, 0);
    }

    swim_here_box = p->col.bbox;
    tx_rect_set_bottom(&swim_here_box, tx_rect_bottom(&p->col.bbox) - 16.f);
    can_swim_here = !tx_is_free_of_tiles(p->sector, &swim_here_box, true, TX_TILE_WATER);

    if (p->swimming)
    {
      if (can_swim_here)
      {
        p->no_water = false;
      }
      else
      {
        p->swimming = false;
        p->water_jump = true;
        if (tx_physic_get_velocity_y(&p->physic) > -350.f && tx_controller_hold(p->controller, TX_CONTROL_UP))
          tx_physic_set_velocity_y(&p->physic, -350.f);
      }

      if (tx_timer_check(&p->bubble_timer))
      {
        int num_bubbles;
        int i;

        /* NOT TRANSLATED (NT4): L534-L556, the position of the beak
           (beak_local_offset, big_offset_x, offset_x, offset_y,
           rotated_beak_offset_x/y, player_center, beak_position): it has no
           effect outside m_active_bubbles. */

        num_bubbles = tx__ph_rand_i(p, 1, 3);

        for (i = 0; i < num_bubbles; ++i)
        {
          int random_action_index = tx__ph_rand_i(p, 0, 2);
          (void)random_action_index;

          /* NOT TRANSLATED (NT4): L563-L577
               std::string selected_action = BUBBLE_ACTIONS[random_action_index];
               SpritePtr bubble_sprite = m_bubbles_sprite->clone();
               bubble_sprite->set_animation_loops(1);
               bubble_sprite->set_action(selected_action);
               Vector bubble_pos(0.f, 0.f);
               if (std::abs(m_swimming_angle) > static_cast<float>(math::PI_2)) // Facing left
                 bubble_pos = beak_position + Vector(offset_x - big_offset_x * 2, offset_y);
               else // Facing right (including straight up or down)
                 bubble_pos = beak_position + Vector(offset_x - 4.0f + big_offset_x, offset_y); */

          if (num_bubbles > 1)
          {
            float burst_offset_x = tx__ph_rand_f(p, -5.0f, 5.0f);
            float burst_offset_y = tx__ph_rand_f(p, -5.0f, 5.0f);
            (void)burst_offset_x;
            (void)burst_offset_y;
            /* NOT TRANSLATED (NT4):
                 bubble_pos.x += burst_offset_x;
                 bubble_pos.y += burst_offset_y; */
          }

          /* NOT TRANSLATED (NT4):
               if (bubble_pos.y > -1)
               {
                 m_active_bubbles.emplace_back(std::make_pair(std::move(bubble_sprite), bubble_pos));
               } */
        }

        /* Restart the timer for the next wave of bubbles */
        tx_timer_start(&p->bubble_timer, 0.8f + tx__ph_rand_f(p, 0.0f, 0.6f));
      }
    }
    else
    {
      if (can_swim_here && !p->climbing)
      {
        if (p->stone)
          tx_player_stop_rolling(p, true);
        p->sliding = false;
        p->slidejumping = false;
        p->was_crawling_before_slide = false;
        p->no_water = false;
        p->water_jump = false;
        p->swimming = true;
        p->swimming_angle = tx_math_angle(tx_vec(tx_physic_get_velocity_x(&p->physic), tx_physic_get_velocity_y(&p->physic)));
        if (tx_player_is_big(p))
          tx_player_adjust_height(p, TX_TUX_WIDTH, 0);
        p->wants_buttjump = p->does_buttjump = p->backflipping = false;
        p->dir = (tx_physic_get_velocity_x(&p->physic) > 0) ? TX_DIR_LEFT : TX_DIR_RIGHT;
        tx__ph_play_sound_here(p, "sounds/splash.wav");
      }
    }
  }
/* #endif */

  /* end of swimming handling */

  if (p->dying && tx_timer_check(&p->dying_timer)) {

    tx_player_set_bonus(p, TX_BONUS_NONE, true, true);
    p->dead = true;

    /* if (!Sector::get().get_object_count<Player>([](const Player& p) { return p.is_alive(); })) */
    if (!(p->host.count_alive_players ? p->host.count_alive_players(p->host.user) : 0))
    {
      if (p->host.stop_looping_sounds) p->host.stop_looping_sounds(p->host.user);
    }
    else
    {
      /* NOT TRANSLATED (NT2): next_target(); */
    }
    return;
  }

  if (!p->dying && !p->deactivated)
    tx_player_handle_input(p);

  /*
  // handle_input() calls apply_friction() when Tux is not walking, so we'll have to do this ourselves
  if (deactivated)
  apply_friction();
  */

  /* extend/shrink tux collision rectangle so that we fall through/walk over 1
     tile holes */

  /* wallclinging and walljumping */

  {
    TxRect wallclingleft = p->col.bbox;
    TxRect wallclingright;
    tx_rect_set_left(&wallclingleft, tx_rect_left(&wallclingleft) - 8.f);
    p->on_left_wall = !tx_is_free_of_statics(p->sector, &wallclingleft, NULL, false, TX_TILE_SOLID);

    wallclingright = p->col.bbox;
    tx_rect_set_right(&wallclingright, tx_rect_right(&wallclingright) + 8.f);
    p->on_right_wall = !tx_is_free_of_statics(p->sector, &wallclingright, NULL, false, TX_TILE_SOLID);
  }

  p->can_walljump = ((p->on_right_wall || p->on_left_wall) && !tx_player_on_ground(p) && !p->swimming && p->in_walljump_tile && !p->stone);
  if (p->can_walljump && (tx_controller_hold(p->controller, TX_CONTROL_LEFT) || tx_controller_hold(p->controller, TX_CONTROL_RIGHT)) && tx_physic_get_velocity_y(&p->physic) >= 0.f && !tx_controller_pressed(p->controller, TX_CONTROL_JUMP))
  {
    tx_physic_set_velocity_y(&p->physic, TX_MAX_WALLCLING_YM);
    tx_physic_set_acceleration_y(&p->physic, 0);
    if (p->water_jump)
    {
      tx_player_adjust_height(p, tx_player_is_big(p) ? TX_BIG_TUX_HEIGHT : TX_SMALL_TUX_HEIGHT, 0);
      p->water_jump = false;
      p->swimboosting = false;
    }
    if (p->sliding)
    {
      tx_player_adjust_height(p, tx_player_is_big(p) ? TX_BIG_TUX_HEIGHT : TX_SMALL_TUX_HEIGHT, 0);
      p->sliding = false;
      p->slidejumping = false;
    }
    /*m_santahatsprite->set_angle(0.f);*/
  }

  p->in_walljump_tile = false;

  /* End of wallclinging */

  /* Roll the sprite if Tux is rolling (and not growing) */
  if (p->stone && !p->growing)
  {
    float f = 1.f;
    float angle;

    if (!isnan(p->floor_normal.x))
      f = cosf(p->floor_normal.x);

    /* m_sprite->set_angle(m_sprite->get_angle() + m_physic.get_movement(dt_sec).x * 3.141592653898f / 2.f / f);
       (this get_movement integrates the velocity a first time; L769 does it
       again. get_angle() is called first here; the C++ leaves the order of
       the two operands of `+` unspecified, and they do not depend on each
       other.) */
    angle = tx__ph_sprite_get_angle(p);
    tx__ph_sprite_set_angle(p, angle + tx_physic_get_movement(&p->physic, p->sector->gravity, dt_sec).x * 3.141592653898f / 2.f / f);
  }

  /* extend/shrink tux collision rectangle so that we fall through/walk over 1
     tile holes */
  if (fabsf(tx_physic_get_velocity_x(&p->physic)) > TX_MAX_WALK_XM) {
    tx_obj_set_width(&p->col, TX_RUNNING_TUX_WIDTH);
  }
  else {
    tx_obj_set_width(&p->col, TX_TUX_WIDTH);
  }

  /* on downward slopes, adjust vertical velocity so tux walks smoothly down */
  if (tx_player_on_ground(p) && !p->swimming && !p->dying) {
    if (p->floor_normal.y != 0) {
      if ((p->floor_normal.x * tx_physic_get_velocity_x(&p->physic)) >= 0) {
        tx_physic_set_velocity_y(&p->physic, (fabsf(tx_physic_get_velocity_x(&p->physic)) * fabsf(p->floor_normal.x)) + 100.f);
      }
    }
  }

  /* handle backflipping */
  if (p->backflipping && !p->dying) {
    /* prevent player from changing direction when backflipping */
    p->dir = (p->backflip_direction == 1) ? TX_DIR_LEFT : TX_DIR_RIGHT;
    if (tx_timer_started(&p->backflip_timer)) tx_physic_set_velocity_x(&p->physic, 100.0f * (float)(p->backflip_direction));
  }

  if (tx_player_on_ground(p)) {
    tx_timer_start(&p->coyote_timer, TX_COYOTE_TIME * ((p->sliding || p->stone) ? 2.f : 1.f));
  }

  /* set fall mode... */
  if (tx_player_on_ground(p)) {
    p->fall_mode = TX_FALL_ON_GROUND;
    p->last_ground_y = tx__player_get_pos(p).y;
  }
  else {
    if (tx__player_get_pos(p).y > p->last_ground_y)
      p->fall_mode = TX_FALL_FALLING;
    else if (p->fall_mode == TX_FALL_ON_GROUND)
      p->fall_mode = TX_FALL_JUMPING;
  }

  /* check if we landed */
  if (tx_player_on_ground(p)) {
    p->jumping = false;
    if (p->backflipping && (tx_timer_get_timegone(&p->backflip_timer) > 0.15f)) {
      p->backflipping = false;
      p->backflip_direction = 0;
      tx_physic_set_velocity_x(&p->physic, 0);
      if (!p->stone && !p->sliding) {
        tx__ph_sprite_set_angle(p, 0.0f);
        /*m_santahatsprite->set_angle(0.0f);*/
      }

      /* if controls are currently deactivated, we take care of standing up ourselves */
      if (p->deactivated)
        tx_player_do_standup_force(p, false);
    }
  }

  if (tx_timer_check(&p->second_growup_sound_timer))
  {
    tx__ph_play_sound_here(p, "sounds/grow.wav");
    tx_timer_stop(&p->second_growup_sound_timer);
  }

  if (p->boost != 0.f)
  {
    bool sign = signbit(p->boost) != 0;
    p->boost = (sign ? -1.f : +1.f) * (fabsf(p->boost) - dt_sec * TX_BOOST_DECREASE_RATE);
    if ((signbit(p->boost) != 0) != sign)
      p->boost = 0.f;
  }

  /* calculate movement for this frame */
  tx_obj_set_movement(&p->col, tx_vec_add(tx_physic_get_movement(&p->physic, p->sector->gravity, dt_sec), tx_vec(p->boost * dt_sec, 0)));

  if (p->grabbed_object != NULL && !p->dying)
  {
    tx_player_position_grabbed_object(p, false);
  }

  if (p->dying)
    tx_player_ungrab_object(p, false);

  if (!p->ice_this_frame && tx_player_on_ground(p))
    p->on_ice = false;

  /* when invincible, spawn particles */
  if (tx_timer_started(&p->invincible_timer))
  {
    if (tx__ph_rand_i(p, 0, 2) == 0)
    {
      float px = tx__ph_rand_f(p, tx_rect_left(&p->col.bbox) + 0, tx_rect_right(&p->col.bbox) - 0);
      float py = tx__ph_rand_f(p, tx_rect_top(&p->col.bbox) + 0, tx_rect_bottom(&p->col.bbox) - 0);
      TxVec ppos = tx_vec(px, py);
      TxVec pspeed = tx_vec(0, 0);
      TxVec paccel = tx_vec(0, 0);
      if (p->host.add_sprite_particle)
        p->host.add_sprite_particle(p->host.user,
          "images/particles/sparkle.sprite",
          /* draw bright sparkle when there is lots of time left,
             dark sparkle when invincibility is about to end */
          (tx_timer_get_timeleft(&p->invincible_timer) > TX_TUX_INVINCIBLE_TIME_WARNING) ?
          /* make every other a longer sparkle to make trail a bit fuzzy */
          (((size_t)(tx_game_time * 20) % 2) ? "small" : "medium")
          :
          "dark", ppos, pspeed, paccel, TX_LAYER_OBJECTS + 1 + 5, false);
    }
  }

  if (p->growing) {
    if (p->host.sprite_animation_done ? p->host.sprite_animation_done(p->host.user) : true)
      p->growing = false;
  }

  /* when climbing animate only while moving */
  if (p->climbing) {
    if ((tx_physic_get_velocity_x(&p->physic) == 0) && (tx_physic_get_velocity_y(&p->physic) == 0))
    {
      if (p->host.sprite_pause_animation) p->host.sprite_pause_animation(p->host.user);
      /*m_santahatsprite->pause_animation();*/
    }
    else if (!p->growing)
    {
      if (p->host.sprite_resume_animation) p->host.sprite_resume_animation(p->host.user);
      /*m_santahatsprite->resume_animation();*/
    }
  }

  if (p->floor_normal.y < 0.f && p->crawl && !p->stone)
  {
    p->crawl = false;
    p->sliding = true;
    p->was_crawling_before_slide = true;
  }

  /* sliding */

  if (p->sliding)
  {
    float sliding_angle = 0.0f;

    if (tx_player_on_ground(p))
    {
      float floor_angle = 0.0f;

      if (p->floor_normal.y != 0.0f)
      {
        floor_angle = tx_math_degrees(tx_math_angle(p->floor_normal)) + 90.0f;
      }

      if (p->target_sliding_angle != floor_angle)
      {
        const float current_velocity = tx_vec_length(tx_physic_get_velocity(&p->physic));
        const float max_velocity = 500.0f;
        const float rotation_time = TX_MIN_SLIDE_ROTATING_TIME + (1.0f - (tx_minf(max_velocity, current_velocity) / max_velocity)) * (TX_MAX_SLIDE_ROTATING_TIME - TX_MIN_SLIDE_ROTATING_TIME);
        p->last_sliding_angle = p->current_sliding_angle;
        p->target_sliding_angle = floor_angle;
        tx_timer_start(&p->sliding_rotation_timer, rotation_time);
      }

      if (tx_timer_started(&p->sliding_rotation_timer))
      {
        const float progress = tx_timer_get_timegone(&p->sliding_rotation_timer) / tx_timer_get_period(&p->sliding_rotation_timer);
        const float angle_difference = p->target_sliding_angle - p->last_sliding_angle;

        sliding_angle = p->current_sliding_angle = p->last_sliding_angle + (angle_difference * progress);
      }
      else
      {
        sliding_angle = p->last_sliding_angle = floor_angle;
      }
    }
    else
    {
      if (!p->jumping && !p->is_slidejump_falling)
      {
        sliding_angle = tx_math_degrees(tx_math_angle(tx_obj_get_movement(&p->col)));
        if (tx_obj_get_movement(&p->col).x < 0.0f)
        {
          sliding_angle -= 180.0f;
        }
        p->target_sliding_angle = p->current_sliding_angle = sliding_angle;
      }
      else
      {
        /* Do not rotate while slidejump animation is displayed */
        sliding_angle = 0.0f;
      }
    }
    tx__ph_sprite_set_angle(p, sliding_angle);

    /* if you stop holding down when sliding, then it stops.
       or, stop sliding if you come to a stop and are not on a slope. */
    if (!tx_controller_hold(p->controller, TX_CONTROL_DOWN))
    {
      p->sliding = false;
      p->slidejumping = false;

      tx_player_do_standup_force(p, false);
    }
    else if (p->floor_normal.y == 0.f && fabsf(tx_physic_get_velocity_x(&p->physic)) <= 1.f)
    {
      if (tx_player_is_big(p) || p->growing)
          p->crawl = true;
      p->sliding = false;
      p->slidejumping = false;
    }
  }

  if (p->sliding || p->stone)
  {
    TxRect sidebrickbox = tx_rect_grown(&p->col.bbox, -1.f);
    tx_rect_set_left(&sidebrickbox, tx_rect_left(&p->col.bbox) + (p->dir == TX_DIR_LEFT ? -12.f : 1.f));
    tx_rect_set_right(&sidebrickbox, tx_rect_right(&p->col.bbox) + (p->dir == TX_DIR_RIGHT ? 12.f : -1.f));

    /* for (auto& brick : Sector::get().get_objects_by_type<Brick>()) {
         if (sidebrickbox.overlaps(brick.get_bbox()) && (m_stone || (m_sliding && brick.get_class_name() != "heavy-brick")) &&
           std::abs(m_physic.get_velocity_x()) >= 150.f) {
           brick.try_break(this, is_big());
         }
       } */
    tx__ph_break_bricks(p, TX_BRICKS_SIDE, &sidebrickbox,
                        fabsf(tx_physic_get_velocity_x(&p->physic)) >= 150.f);
  }

  if (p->does_buttjump || (p->stone && tx_physic_get_velocity_y(&p->physic) > 30.f && !tx_timer_started(&p->coyote_timer)))
  {
    TxRect downbox = tx_rect_grown(&p->col.bbox, -1.f);
    tx_rect_set_top(&downbox, tx_rect_bottom(&p->col.bbox));
    tx_rect_set_bottom(&downbox, tx_rect_bottom(&downbox) + 16.f);
    /* for (auto& brick : Sector::get().get_objects_by_type<Brick>()) {
         // stoneform breaks through any kind of bricks
         if (downbox.overlaps(brick.get_bbox()) && (m_stone || !dynamic_cast<HeavyBrick*>(&brick)))
           brick.try_break(this, is_big());
       } */
    tx__ph_break_bricks(p, TX_BRICKS_DOWN, &downbox, false);
    /* for (auto& badguy : Sector::get().get_objects_by_type<BadGuy>()) {
         if (downbox.overlaps(badguy.get_bbox()) && badguy.is_snipable() && !badguy.is_grabbed())
           badguy.kill_fall();
       } */
    if (p->host.kill_fall_badguys) p->host.kill_fall_badguys(p->host.user, &downbox);
  }

  /* break bricks above without stopping */
  if (p->stone && tx_physic_get_velocity_y(&p->physic) < 30.f)
  {
    TxRect topbox = tx_rect_grown(&p->col.bbox, -1.f);
    tx_rect_set_top(&topbox, tx_rect_top(&p->col.bbox) - 16.f);
    /* for (auto& brick : Sector::get().get_objects_by_type<Brick>()) {
         if (topbox.overlaps(brick.get_bbox()))
           brick.try_break(this, is_big());
       } */
    tx__ph_break_bricks(p, TX_BRICKS_TOP, &topbox, false);
  }

  /* launch from slopes */

  {
    TxRect launchbox = p->col.bbox;
    tx_rect_set_bottom(&launchbox, tx_rect_bottom(&p->col.bbox) - 8.f);
    tx_rect_set_left(&launchbox, tx_rect_left(&p->col.bbox) + (p->dir == TX_DIR_LEFT ? -32.f : 33.f));
    tx_rect_set_right(&launchbox, tx_rect_right(&p->col.bbox) + (p->dir == TX_DIR_RIGHT ? 32.f : -33.f));
    if (p->sliding && tx_player_on_ground(p) && p->floor_normal.y != 0 && p->floor_normal.x * tx_physic_get_velocity_x(&p->physic) < 0.f &&
      tx_is_free_of_statics(p->sector, &launchbox, NULL, false, TX_TILE_SOLID) && !p->slidejumping)
    {
      p->slidejumping = true;
      tx_physic_set_velocity_y(&p->physic, -tx_vec_length(tx_physic_get_velocity(&p->physic)) * fabsf(p->floor_normal.x));
    }
    else if (p->sliding && tx_player_on_ground(p)) {
      p->slidejumping = false;
    }
  }

  p->ice_this_frame = false;
  p->on_ground_flag = false;
}

#endif /* TX_PLAYER_H */
