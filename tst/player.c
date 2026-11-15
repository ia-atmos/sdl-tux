/* Standalone test of game/player.h: a small Tux on a flat floor, driven by a
   scripted controller.
   cc -std=c99 -Wall -Wextra -O2 -fsanitize=address,undefined -Igame tst/player.c -lm

   One line per tick: "tick x y vx vy g=<on ground> j=<jumping> k=<duck>",
   read where the official trace reads them (after the collisions of the
   step; Player::update clears m_on_ground_flag at its end and the collisions
   set it again). */
#include <stdio.h>
#include "player.h"

#define W 400
#define H 20

static TxPlayer g_tux;
static int g_sounds;

static void on_collision_solid(TxObj* self, const TxHit* hit)
{
  if (self->user) tx_player_collision_solid((TxPlayer*)self->user, hit);
}

static void on_collision_tile(TxObj* self, uint32_t tile_attributes)
{
  if (self->user) tx_player_collision_tile((TxPlayer*)self->user, tile_attributes);
}

static int on_collision(TxObj* self, TxObj* other, const TxHit* hit)
{
  if (self->user) return tx_player_collision((TxPlayer*)self->user, TX_OTHER_OTHER, other, hit);
  return TX_FORCE_MOVE;
}

static void on_play_sound(void* user, const char* name, const TxVec* pos)
{
  (void)user; (void)name; (void)pos;
  g_sounds++;
}

int main(void)
{
  /* tile ids: 0 empty, 1 solid */
  static uint32_t tiles[W * H];
  uint32_t attrs[2] = { 0, TX_TILE_SOLID };
  int data[2] = { 0, 0 };
  TxSector sector;
  TxMap map;
  TxController controller;
  TxPlayerHost host;
  int x, tick;
  /* ScreenManager: seconds_per_step = 1.0f / LOGICAL_FPS */
  const float dt = 1.0f / (1000.0f / 15.0f);

  memset(tiles, 0, sizeof(tiles));
  for (x = 0; x < W; ++x) tiles[(H - 1) * W + x] = 1; /* the floor */

  tx_sector_init(&sector);
  sector.tiles.attrs = attrs;
  sector.tiles.data = data;
  sector.tiles.count = 2;
  sector.collision = on_collision;
  sector.collision_solid = on_collision_solid;
  sector.collision_tile = on_collision_tile;

  tx_map_init(&map, W, H, tiles);
  tx_sector_add_map(&sector, &map);

  memset(&host, 0, sizeof(host));
  host.play_sound = on_play_sound;

  tx_controller_reset(&controller);

  tx_game_time = 0;
  tx_player_init(&g_tux, &sector, &controller, &host, TX_BONUS_NONE, 0);
  g_tux.col.user = &g_tux;
  g_tux.col.id = 1;
  tx_sector_add(&sector, &g_tux.col);
  /* a little above the floor: he lands during the first ticks */
  tx_player_set_pos(&g_tux, tx_vec(2000.0f, (H - 1) * 32.0f - 40.0f));

  /* stand 20 ticks, hold right 150, release 40, hold left 60 with a jump
     pressed at tick 230 and held 15 ticks */
  for (tick = 1; tick <= 270; ++tick) {
    /* g_game_time += dtime; */
    tx_game_time += dt;

    /* process_events(): InputManager::update(), then the keys */
    tx_controller_update(&controller);
    if (tick == 21) tx_controller_set_control(&controller, TX_CONTROL_RIGHT, true);
    if (tick == 171) tx_controller_set_control(&controller, TX_CONTROL_RIGHT, false);
    if (tick == 211) tx_controller_set_control(&controller, TX_CONTROL_LEFT, true);
    if (tick == 230) tx_controller_set_control(&controller, TX_CONTROL_JUMP, true);
    if (tick == 245) tx_controller_set_control(&controller, TX_CONTROL_JUMP, false);

    /* Sector::update: the objects, then the collisions */
    tx_player_update(&g_tux, dt);
    tx_map_clear_objects_hit_bottom(&map);
    tx_coll_update(&sector);

    printf("%d %.9g %.9g %.9g %.9g g=%d j=%d k=%d\n", tick,
           (double)g_tux.col.bbox.x1, (double)g_tux.col.bbox.y1,
           (double)g_tux.physic.vx, (double)g_tux.physic.vy,
           (int)g_tux.on_ground_flag, (int)g_tux.jumping, (int)g_tux.duck);
  }

  fprintf(stderr, "sounds played: %d\n", g_sounds);

  tx_sector_remove(&sector, &g_tux.col);
  tx_player_free(&g_tux);
  tx_map_free(&map);
  tx_sector_free(&sector);
  return 0;
}
