/* Standalone test of game/coll.h.
   cc -std=c99 -Wall -Wextra -O2 -fsanitize=address,undefined -Igame tst/coll.c -lm */
#include <stdio.h>
#include "coll.h"

#define W 20
#define H 10

typedef struct Box {
  TxObj obj;
  TxPhysic physic;
  int solid_hits;
  uint32_t tile_attrs;
} Box;

static int g_map_bottom_hits;
static int g_obj_bottom_hits;

static int on_collision(TxObj* self, TxObj* other, const TxHit* hit)
{
  (void)other; (void)hit;
  /* the static block pushes, the box is pushed */
  return self->group == TX_COLGROUP_STATIC ? TX_FORCE_MOVE : TX_CONTINUE;
}

static void on_collision_solid(TxObj* self, const TxHit* hit)
{
  Box* b = (Box*)self->user;
  if (!b) return;
  b->solid_hits++;
  if (hit->bottom || hit->top) tx_physic_set_velocity_y(&b->physic, 0);
  if (hit->left || hit->right) tx_physic_inverse_velocity_x(&b->physic);
}

static void on_collision_tile(TxObj* self, uint32_t tile_attributes)
{
  Box* b = (Box*)self->user;
  if (b) b->tile_attrs |= tile_attributes;
}

static void on_moving_object_bottom(TxObj* self, TxObj* other)
{
  (void)self; (void)other;
  g_obj_bottom_hits++;
}

static void on_map_hits_bottom(TxMap* map, TxObj* obj)
{
  (void)map; (void)obj;
  g_map_bottom_hits++;
}

int main(void)
{
  /* tile ids: 0 empty, 1 solid, 2 slope (south-east), 3 unisolid (north), 4 ice */
  uint32_t attrs[5] = { 0, TX_TILE_SOLID, TX_TILE_SOLID | TX_TILE_SLOPE,
                        TX_TILE_SOLID | TX_TILE_UNISOLID, TX_TILE_SOLID | TX_TILE_ICE };
  int data[5] = { 0, 0, TX_AAT_SOUTHEAST, TX_UNI_DIR_NORTH, 0 };
  uint32_t tiles[W * H];
  TxSector sector;
  TxMap map;
  Box box;
  TxObj block;
  TxObj* near[4];
  TxRect probe;
  int x, y, tick, n;
  const float dt_sec = 1.0f / TX_LOGICAL_FPS;

  memset(tiles, 0, sizeof(tiles));
  for (x = 0; x < W; ++x) tiles[(H - 1) * W + x] = 1;       /* floor */
  for (x = 7; x < 9; ++x) tiles[(H - 1) * W + x] = 4;       /* a bit of ice */
  for (y = 0; y < H; ++y) tiles[y * W + (W - 4)] = 1;       /* wall */
  tiles[(H - 2) * W + 9] = 2;                               /* slope going up to the right */
  tiles[(H - 2) * W + 10] = 1;                              /* the step behind the slope */
  tiles[4 * W + 4] = 3;                                     /* unisolid on the way down */

  tx_sector_init(&sector);
  sector.tiles.attrs = attrs;
  sector.tiles.data = data;
  sector.tiles.count = 5;
  sector.collision = on_collision;
  sector.collision_solid = on_collision_solid;
  sector.collision_tile = on_collision_tile;
  sector.moving_object_bottom = on_moving_object_bottom;
  sector.map_hits_bottom = on_map_hits_bottom;

  tx_map_init(&map, W, H, tiles);
  tx_sector_add_map(&sector, &map);

  memset(&box, 0, sizeof(box));
  tx_obj_init(&box.obj, TX_COLGROUP_MOVING);
  tx_physic_init(&box.physic);
  box.obj.id = 1;
  box.obj.user = &box;
  tx_obj_set_size(&box.obj, 31.8f, 31.8f);
  tx_obj_set_pos(&box.obj, tx_vec(100.3f, 20.7f));
  tx_obj_set_physic_hint(&box.obj, &box.physic);
  tx_physic_set_velocity_x(&box.physic, 130.0f);
  tx_sector_add(&sector, &box.obj);

  /* a static block on the floor, on the way back from the wall */
  tx_obj_init(&block, TX_COLGROUP_STATIC);
  block.id = 2;
  tx_obj_set_size(&block, 32.0f, 32.0f);
  tx_obj_set_pos(&block, tx_vec(6 * 32.0f, (H - 2) * 32.0f));
  tx_sector_add(&sector, &block);

  for (tick = 0; tick < 240; ++tick) {
    /* MovingObject::update of a simple object: m_col.set_movement(physic.get_movement(dt)) */
    tx_obj_set_movement(&box.obj, tx_physic_get_movement(&box.physic, sector.gravity, dt_sec));
    tx_map_clear_objects_hit_bottom(&map);
    tx_coll_update(&sector);
    if (tick % 6 == 0 || tick == 239)
      printf("%3d pos %.9g %.9g  vel %.9g %.9g  hits %d\n", tick,
             (double)box.obj.bbox.x1, (double)box.obj.bbox.y1,
             (double)box.physic.vx, (double)box.physic.vy, box.solid_hits);
  }

  printf("tile attrs seen 0x%x, map bottom hits %d, object bottom hits %d\n",
         (unsigned)box.tile_attrs, g_map_bottom_hits, g_obj_bottom_hits);

  probe = tx_rect(40.0f, 40.0f, 60.0f, 60.0f);
  printf("free of tiles (air) %d\n", tx_is_free_of_tiles(&sector, &probe, false, TX_TILE_SOLID));
  probe = tx_rect(9 * 32.0f + 20.0f, (H - 2) * 32.0f + 20.0f, 9 * 32.0f + 30.0f, (H - 2) * 32.0f + 30.0f);
  printf("free of tiles (in slope) %d\n", tx_is_free_of_tiles(&sector, &probe, false, TX_TILE_SOLID));
  probe = tx_rect(9 * 32.0f + 1.0f, (H - 2) * 32.0f + 1.0f, 9 * 32.0f + 6.0f, (H - 2) * 32.0f + 6.0f);
  printf("free of tiles (above slope) %d\n", tx_is_free_of_tiles(&sector, &probe, false, TX_TILE_SOLID));
  probe = tx_rect(4 * 32.0f + 4.0f, 4 * 32.0f + 4.0f, 4 * 32.0f + 8.0f, 4 * 32.0f + 8.0f);
  printf("free of tiles (unisolid) %d, ignoring unisolid %d\n",
         tx_is_free_of_tiles(&sector, &probe, false, TX_TILE_SOLID),
         tx_is_free_of_tiles(&sector, &probe, true, TX_TILE_SOLID));
  printf("free of statics (block) %d, movingstatics %d, specifically %d\n",
         tx_is_free_of_statics(&sector, &block.bbox, NULL, false, TX_TILE_SOLID),
         tx_is_free_of_movingstatics(&sector, &box.obj.bbox, &box.obj, false),
         tx_is_free_of_specifically_movingstatics(&sector, &box.obj.bbox, NULL));
  printf("line of sight: open %d, through wall %d, through block %d (ignoring objects %d)\n",
         tx_free_line_of_sight(&sector, tx_vec(40.0f, 40.0f), tx_vec(200.0f, 60.0f), false, NULL),
         tx_free_line_of_sight(&sector, tx_vec(40.0f, 40.0f), tx_vec(600.0f, 40.0f), false, NULL),
         tx_free_line_of_sight(&sector, tx_vec(170.0f, 270.0f), tx_vec(260.0f, 270.0f), false, NULL),
         tx_free_line_of_sight(&sector, tx_vec(170.0f, 270.0f), tx_vec(260.0f, 270.0f), true, NULL));
  n = tx_get_nearby_objects(&sector, tx_rect_middle(&block.bbox), 1000.0f, near, 4);
  printf("nearby %d\n", n);

  tx_sector_remove(&sector, &block);
  tx_sector_remove(&sector, &box.obj);
  tx_sector_remove_map(&sector, &map);
  tx_obj_free(&block);
  tx_obj_free(&box.obj);
  tx_map_free(&map);
  tx_sector_free(&sector);
  return 0;
}
