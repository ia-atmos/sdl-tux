/*
 * coll.h -- the collision and physics kernel of SuperTux v0.7.0, translated
 * from C++17 to C99, one function at a time, keeping the 32-bit float
 * arithmetic of the original operation by operation.
 *
 * SuperTux is (C) the SuperTux team, GPL v3 or later; this file is a
 * translation of these sources and carries the same licence:
 *   src/collision/collision_system.cpp/.hpp   (C) 2006 Matthias Braun, 2018 Ingo Ruhnke
 *   src/collision/collision.cpp/.hpp, collision_object.cpp/.hpp,
 *   collision_hit.hpp, collision_group.hpp, collision_movement_manager.cpp/.hpp
 *   src/math/aatriangle.cpp/.hpp, rectf.hpp, rect.hpp, anchor_point.cpp
 *   src/supertux/tile.cpp/.hpp, physic.cpp/.hpp, constants.hpp
 *   src/object/tilemap.cpp/.hpp, src/supertux/tile_set.cpp (TileSet::get)
 *
 * ===========================================================================
 * DEVIATION FROM THE REQUESTED DESIGN (read this first)
 * ===========================================================================
 * TxRect is NOT {x1,y1,x2,y2}. It is {x1, y1, w, h}.
 *   In v0.7.0, Rectf stores `Vector m_p1; Sizef m_size;` and computes
 *   get_right() as `m_p1.x + m_size.width` (get_bottom() likewise) on every
 *   call. Rectf(x1,y1,x2,y2) stores `x2 - x1`, move() only changes m_p1, and
 *   set_left/set_right/set_top/set_bottom update the size with a subtraction.
 *   A struct holding x2/y2 cannot reproduce these roundings (x1 + (x2 - x1)
 *   is not always x2 in float), so the right and bottom edges would differ by
 *   one ulp from the game, which is exactly what this port must not do.
 *   Use tx_rect(x1,y1,x2,y2) to build one as Rectf(float,float,float,float)
 *   does, and tx_rect_right()/tx_rect_bottom() to read the far edges.
 *
 * ===========================================================================
 * ADDED BEYOND THE REQUESTED DESIGN
 * ===========================================================================
 *  - TxGroundMovementManager (embedded in TxSector) and TxGroundMove: the
 *    CollisionGroundMovementManager. The C++ uses unordered_map keyed by
 *    pointer; here it is one flat array in registration order (see
 *    tx_gmm_apply_all_ground_movement for what that changes).
 *  - TxObjSet: the std::unordered_set<CollisionObject*> m_objects_hit_bottom
 *    of CollisionObject and of TileMap, as an insertion-ordered array without
 *    duplicates.
 *  - tx_obj_init / tx_obj_free, tx_map_init / tx_map_free: constructors and
 *    destructors (the sets above own memory).
 *  - TxObj.ground_movement_manager and TxMap.ground_movement_manager: the
 *    shared_ptr members; set by tx_sector_add (as CollisionSystem::add) and
 *    tx_sector_add_map (as Sector::before_object_add does for a TileMap).
 *  - TxSector.gravity (Sector::m_gravity, 10.0f by default): Physic reads it
 *    through the Sector singleton; tx_physic_get_movement takes it as an
 *    argument.
 *  - TxRaycast and tx_get_first_line_intersection (the C++ function behind
 *    free_line_of_sight), TX_RAY_* / TX_IGNORE_*.
 *  - tx_map_propagate_ground_movement / tx_map_clear_objects_hit_bottom: the
 *    two pieces of TileMap::update that feed the ground movement manager.
 *  - tx_obj_propagate_movement, tx_obj_set_pos, tx_obj_set_width,
 *    tx_obj_set_size and the other small CollisionObject methods.
 *  - The callbacks `moving_object_bottom` and `map_hits_bottom`: in the C++
 *    these two are NOT virtual (CollisionObject::collision_moving_object_bottom
 *    and TileMap::hits_object_bottom are plain methods that insert into
 *    m_objects_hit_bottom). The insertion is therefore always done here, and
 *    the callback, when not null, is called right after it as a notification.
 *  - tx_sector_remove also drops the pending ground movements whose target is
 *    the removed object (the C++ keeps a dangling pointer there; see the
 *    function).
 *  - TX_FLT_EVAL check and the clang FP_CONTRACT pragma just below.
 *
 * ===========================================================================
 * NOT TRANSLATED (each one is also marked at its place in the code)
 * ===========================================================================
 *  NT1  CollisionSystem::draw (debug drawing of the collision rectangles).
 *  NT2  CollisionSystem::update: `if (Editor::is_active()) return;`.
 *  NT3  Physic::get_movement: `Sector::get().get_gravity()`; the gravity is
 *       an argument (pass TxSector.gravity).
 *  NT4  The assert()s: collision.cpp rectangle_aatriangle (`default:
 *       assert(false)` in both switches), tile.cpp check_movement_unisolid
 *       (4 asserts) and check_position_unisolid (`default: assert(false);
 *       return 0;`). The code behaves as the C++ built with NDEBUG.
 *  NT5  MovingObject::collision is pure virtual: there is no base class
 *       default. A null `collision` callback returns TX_ABORT_MOVE (value 0).
 *       This is a choice of this port, not something the C++ says.
 *  NT6  TileSet::get: "tile slot is null -> tile 0". TxTiles has no null
 *       slots; only "id >= count -> tile 0" exists.
 *  NT7  TileMap::update: the fading and the path walker (which compute
 *       m_offset and m_movement). The host sets TxMap.offset and .movement,
 *       then calls tx_map_propagate_ground_movement and
 *       tx_map_clear_objects_hit_bottom.
 *  NT8  CollisionSystem::RaycastResult::hit is a
 *       std::variant<const Tile*, CollisionObject*>; TxRaycast has a kind, a
 *       tile id and an object pointer instead.
 *  NT9  std::vector iteration: the C++ iterates m_objects with iterators and
 *       would be undefined if a callback added or removed an object during a
 *       pass. Here the loops index the array and re-read the count, so that
 *       case is defined (not identical to anything: the C++ has no behaviour
 *       to copy).
 *  NT10 Rectf: from_center, the SDL_FRect/Rect conversions, to_rect,
 *       operator==, operator<<, empty, distance(Rectf) are not used by the
 *       collision code and are not here. Rect is replaced by TxIRect.
 *  NT11 Player type checks in the queries: v0.7.0 collision_system.cpp
 *       includes object/player.hpp but never uses Player, so there is
 *       nothing to translate or to drop.
 *
 * ===========================================================================
 * FLOAT RULES FOLLOWED
 * ===========================================================================
 *  - All reals are float; the C++ evaluation type of every operation is kept.
 *    The only double arithmetic is where the C++ has it:
 *    `get_velocity_y() > 0.1` (collision.cpp) and the whole slope branch of
 *    Tile::check_movement_unisolid.
 *  - std::min(a,b) is `(b < a) ? b : a` and std::max(a,b) is `(a < b) ? b : a`
 *    (tx_minf/tx_maxf/tx_mini/tx_maxi): same result as libstdc++ for equal
 *    values, signed zeros and NaN. fminf/fmaxf are not used.
 *  - glm (checked in /usr/include/glm, GLM 0.9.9.8, detail/func_geometric.inl,
 *    detail/func_exponential.inl, detail/type_vec2.inl), for the default
 *    highp float vec2 that `Vector` is (SuperTux defines no GLM_FORCE_*):
 *      dot(a,b)       = { vec2 tmp(a * b); return tmp.x + tmp.y; }
 *      length(v)      = sqrt(dot(v, v))                 (float sqrt)
 *      distance(a,b)  = length(b - a)
 *      inversesqrt(x) = static_cast<float>(1) / sqrt(x)
 *      normalize(v)   = v * inversesqrt(dot(v, v))      (a multiply, not a divide)
 *      v /= s         = { x /= s; y /= s; }             (two divides)
 *      v * s          = vec2(v.x * s, v.y * s)
 *    (The fast inversesqrt bit trick is only for lowp; the SIMD paths are
 *    only for aligned vec4.)
 *  - No fma. A compiler that contracts a*b+c into an fma changes results:
 *    gcc -std=c99 does not (ISO mode means -ffp-contract=off); gcc -std=gnu99
 *    and clang do on targets that have fma (arm64, x86 with -mfma), so build
 *    with -ffp-contract=off. clang gets the pragma below; gcc ignores that
 *    pragma (with a warning), so it is not emitted for gcc.
 */
#ifndef TX_COLL_H
#define TX_COLL_H

#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#endif

/* float expressions must be evaluated in float (no x87 excess precision). */
#if defined(FLT_EVAL_METHOD) && FLT_EVAL_METHOD != 0 && !defined(TX_ALLOW_EXCESS_PRECISION)
#error "coll.h needs FLT_EVAL_METHOD == 0 (on 32-bit x86 use -msse2 -mfpmath=sse)"
#endif

#define TX_FN static inline

/* ------------------------------------------------------------------------ */
/* supertux/constants.hpp                                                    */
/* ------------------------------------------------------------------------ */

/* static const float LOGICAL_FPS = 1000.0f / 15.0f; */
#define TX_LOGICAL_FPS (1000.0f / 15.0f)
/* static const float SHIFT_DELTA = 7.0f; */
#define TX_SHIFT_DELTA 7.0f
/* static const float EPSILON = .002f; */
#define TX_EPSILON .002f
/* collision_system.cpp: const float MAX_SPEED = 16.0f; */
#define TX_MAX_SPEED 16.0f

/* std::numeric_limits<float>::infinity() (has_infinity is true for IEEE). */
#define TX_INFINITY ((float)INFINITY)

/* ------------------------------------------------------------------------ */
/* collision/collision_group.hpp : enum CollisionGroup                       */
/* ------------------------------------------------------------------------ */
enum {
  TX_COLGROUP_DISABLED = 0,
  TX_COLGROUP_MOVING_STATIC,
  TX_COLGROUP_MOVING,
  TX_COLGROUP_MOVING_ONLY_STATIC,
  TX_COLGROUP_STATIC,
  TX_COLGROUP_TOUCHABLE
};

/* ------------------------------------------------------------------------ */
/* collision/collision_hit.hpp : enum HitResponse                            */
/* ------------------------------------------------------------------------ */
enum {
  TX_ABORT_MOVE = 0,
  TX_CONTINUE,
  TX_FORCE_MOVE
};

/* ------------------------------------------------------------------------ */
/* supertux/tile.hpp : attributes, unisolid directions                       */
/* ------------------------------------------------------------------------ */
enum {
  TX_TILE_SOLID    = 0x0001,
  TX_TILE_UNISOLID = 0x0002,
  TX_TILE_BRICK    = 0x0004,
  TX_TILE_GOAL     = 0x0008,
  TX_TILE_SLOPE    = 0x0010,
  TX_TILE_FULLBOX  = 0x0020,
  TX_TILE_COIN     = 0x0040,
  TX_TILE_FIRST_INTERESTING_FLAG = 0x0100,
  TX_TILE_ICE      = 0x0100,
  TX_TILE_WATER    = 0x0200,
  TX_TILE_HURTS    = 0x0400,
  TX_TILE_FIRE     = 0x0800,
  TX_TILE_WALLJUMP = 0x1000
};

enum {
  TX_UNI_DIR_NORTH = 0,
  TX_UNI_DIR_SOUTH = 1,
  TX_UNI_DIR_WEST  = 2,
  TX_UNI_DIR_EAST  = 3,
  TX_UNI_DIR_MASK  = 3
};

/* ------------------------------------------------------------------------ */
/* math/aatriangle.hpp : AATriangle::Direction                               */
/* ------------------------------------------------------------------------ */
enum {
  TX_AAT_SOUTHWEST = 0,
  TX_AAT_NORTHEAST,
  TX_AAT_SOUTHEAST,
  TX_AAT_NORTHWEST,
  TX_AAT_DIRECTION_MASK = 0x0003,
  TX_AAT_DEFORM_BOTTOM = 0x0010,
  TX_AAT_DEFORM_TOP = 0x0020,
  TX_AAT_DEFORM_LEFT = 0x0030,
  TX_AAT_DEFORM_RIGHT = 0x0040,
  TX_AAT_DEFORM_MASK = 0x0070
};

/* video/flip.hpp */
enum {
  TX_NO_FLIP = 0,
  TX_VERTICAL_FLIP = (1 << 1),
  TX_HORIZONTAL_FLIP = (1 << 2)
};

/* CollisionSystem::RaycastIgnore */
enum {
  TX_IGNORE_NONE,
  TX_IGNORE_TILES,
  TX_IGNORE_OBJECTS
};

/* which alternative of RaycastResult::hit is set (NT8) */
enum {
  TX_RAY_TILE,
  TX_RAY_OBJECT
};

/* ------------------------------------------------------------------------ */
/* Types                                                                     */
/* ------------------------------------------------------------------------ */

/* Vector = glm::vec2 */
typedef struct TxVec { float x, y; } TxVec;

/* Rectf: m_p1 (x1,y1) and m_size (w,h). See the deviation note at the top. */
typedef struct TxRect { float x1, y1, w, h; } TxRect;

/* Rect (integers): the result of get_tiles_overlapping */
typedef struct TxIRect { int left, top, right, bottom; } TxIRect;

/* CollisionHit */
typedef struct TxHit {
  bool left, right;
  bool top, bottom;
  bool crush;
  TxVec slope_normal;
} TxHit;

/* Physic */
typedef struct TxPhysic {
  float ax, ay;               /* horizontal and vertical acceleration */
  float vx, vy;               /* horizontal and vertical velocity */
  bool gravity_enabled_flag;
  float gravity_modifier;
} TxPhysic;

/* collision::Constraints */
typedef struct TxConstraints {
  TxHit hit;
  float position_left;
  float position_right;
  float position_top;
  float position_bottom;
} TxConstraints;

/* AATriangle */
typedef struct TxAATriangle {
  TxRect bbox;
  int dir;
} TxAATriangle;

typedef struct TxObj TxObj;
typedef struct TxMap TxMap;
typedef struct TxSector TxSector;

/* std::unordered_set<CollisionObject*> (insertion order, no duplicates) */
typedef struct TxObjSet {
  TxObj** v;
  int n, cap;
} TxObjSet;

/* one entry of CollisionGroundMovementManager::m_movements_per_target:
   exactly one of `obj` (m_moving_objects) and `map` (m_moving_tilemaps) is
   not null. */
typedef struct TxGroundMove {
  TxObj* target;
  TxObj* obj;
  TxMap* map;
  TxVec movement;
} TxGroundMove;

/* CollisionGroundMovementManager */
typedef struct TxGroundMovementManager {
  TxGroundMove* v;
  int n, cap;
} TxGroundMovementManager;

/* CollisionObject (+ is_valid() and the host fields) */
struct TxObj {
  TxRect bbox;                 /* m_bbox */
  int group;                   /* m_group, a TX_COLGROUP_* */
  const TxPhysic* physic_hint; /* m_physic_hint, may be null */
  TxVec movement;              /* m_movement */
  TxRect dest;                 /* m_dest */
  bool unisolid;               /* m_unisolid */
  TxVec pressure;              /* m_pressure */
  TxObjSet objects_hit_bottom; /* m_objects_hit_bottom */
  TxGroundMovementManager* ground_movement_manager; /* m_ground_movement_manager */
  bool valid;                  /* m_parent.is_valid() */
  int id;                      /* host */
  void* user;                  /* host */
};

/* TileMap (a solid one), only what the collisions use */
struct TxMap {
  int width, height;           /* m_width, m_height */
  uint32_t* tiles;             /* m_tiles, width*height, row by row (not owned) */
  TxVec offset;                /* m_offset */
  TxVec movement;              /* m_movement */
  int flip;                    /* m_flip, TX_*_FLIP bits */
  TxObjSet objects_hit_bottom; /* m_objects_hit_bottom */
  TxGroundMovementManager* ground_movement_manager; /* m_ground_movement_manager */
  void* user;                  /* host */
};

/* TileSet, reduced to Tile::m_attributes and Tile::m_data */
typedef struct TxTiles {
  uint32_t* attrs;
  int* data;
  uint32_t count;
} TxTiles;

/* CollisionSystem + what it reaches in Sector */
struct TxSector {
  TxObj** objects;             /* m_objects, insertion order */
  int objects_n, objects_cap;
  TxMap** maps;                /* Sector::get_solid_tilemaps(), in order */
  int maps_n, maps_cap;
  TxTiles tiles;
  TxGroundMovementManager ground_movement_manager;
  float gravity;               /* Sector::m_gravity (10.0f) */

  /* MovingObject::collides; null = `return true` */
  bool (*collides)(TxObj* self, TxObj* other, const TxHit* hit);
  /* MovingObject::collision (pure virtual); null = TX_ABORT_MOVE (NT5) */
  int  (*collision)(TxObj* self, TxObj* other, const TxHit* hit);
  /* MovingObject::collision_solid; null = nothing */
  void (*collision_solid)(TxObj* self, const TxHit* hit);
  /* MovingObject::collision_tile; null = nothing */
  void (*collision_tile)(TxObj* self, uint32_t tile_attributes);
  /* called after CollisionObject::collision_moving_object_bottom did its
     insertion (self is the object hit on its top, other the one above) */
  void (*moving_object_bottom)(TxObj* self, TxObj* other);
  /* called after TileMap::hits_object_bottom did its insertion */
  void (*map_hits_bottom)(TxMap* map, TxObj* obj);
};

/* CollisionSystem::RaycastResult (NT8) */
typedef struct TxRaycast {
  bool is_valid;
  int kind;          /* TX_RAY_TILE or TX_RAY_OBJECT; meaningful if is_valid */
  uint32_t tile_id;  /* TX_RAY_TILE: the id read from the tilemap */
  TxObj* obj;        /* TX_RAY_OBJECT */
  TxRect box;
} TxRaycast;

/* ------------------------------------------------------------------------ */
/* std::min / std::max                                                       */
/* ------------------------------------------------------------------------ */

/* std::min(a, b): return (b < a) ? b : a; */
TX_FN float tx_minf(float a, float b) { return (b < a) ? b : a; }
/* std::max(a, b): return (a < b) ? b : a; */
TX_FN float tx_maxf(float a, float b) { return (a < b) ? b : a; }
TX_FN int tx_mini(int a, int b) { return (b < a) ? b : a; }
TX_FN int tx_maxi(int a, int b) { return (a < b) ? b : a; }

/* ------------------------------------------------------------------------ */
/* glm::vec2                                                                 */
/* ------------------------------------------------------------------------ */

TX_FN TxVec tx_vec(float x, float y) { TxVec v; v.x = x; v.y = y; return v; }
/* a + b */
TX_FN TxVec tx_vec_add(TxVec a, TxVec b) { return tx_vec(a.x + b.x, a.y + b.y); }
/* a - b */
TX_FN TxVec tx_vec_sub(TxVec a, TxVec b) { return tx_vec(a.x - b.x, a.y - b.y); }
/* -a */
TX_FN TxVec tx_vec_neg(TxVec a) { return tx_vec(-a.x, -a.y); }
/* v * scalar */
TX_FN TxVec tx_vec_scale(TxVec v, float s) { return tx_vec(v.x * s, v.y * s); }
/* v / scalar */
TX_FN TxVec tx_vec_div(TxVec v, float s) { return tx_vec(v.x / s, v.y / s); }

/* glm::dot: vec2 tmp(a * b); return tmp.x + tmp.y; */
TX_FN float tx_vec_dot(TxVec a, TxVec b)
{
  TxVec tmp = tx_vec(a.x * b.x, a.y * b.y);
  return tmp.x + tmp.y;
}

/* glm::length: sqrt(dot(v, v)) */
TX_FN float tx_vec_length(TxVec v) { return sqrtf(tx_vec_dot(v, v)); }

/* glm::distance: length(p1 - p0) */
TX_FN float tx_vec_distance(TxVec p0, TxVec p1) { return tx_vec_length(tx_vec_sub(p1, p0)); }

/* glm::inversesqrt: static_cast<float>(1) / sqrt(x) */
TX_FN float tx_inversesqrt(float x) { return 1.0f / sqrtf(x); }

/* glm::normalize: v * inversesqrt(dot(v, v)) */
TX_FN TxVec tx_vec_normalize(TxVec v) { return tx_vec_scale(v, tx_inversesqrt(tx_vec_dot(v, v))); }

/* ------------------------------------------------------------------------ */
/* math/rectf.hpp : Rectf                                                    */
/* ------------------------------------------------------------------------ */

/* Rectf::Rectf() */
TX_FN TxRect tx_rect_zero(void)
{
  TxRect r; r.x1 = 0.0f; r.y1 = 0.0f; r.w = 0.0f; r.h = 0.0f; return r;
}

/* Rectf::Rectf(float x1, float y1, float x2, float y2) :
     m_p1(x1, y1), m_size(x2 - x1, y2 - y1) */
TX_FN TxRect tx_rect(float x1, float y1, float x2, float y2)
{
  TxRect r; r.x1 = x1; r.y1 = y1; r.w = x2 - x1; r.h = y2 - y1; return r;
}

/* Rectf::Rectf(const Vector& np1, const Vector& np2) :
     m_p1(np1), m_size(np2.x - np1.x, np2.y - np1.y) */
TX_FN TxRect tx_rect_pp(TxVec np1, TxVec np2)
{
  TxRect r; r.x1 = np1.x; r.y1 = np1.y; r.w = np2.x - np1.x; r.h = np2.y - np1.y; return r;
}

/* Rectf::Rectf(const Vector& p1, const Sizef& size) */
TX_FN TxRect tx_rect_ps(TxVec p1, float width, float height)
{
  TxRect r; r.x1 = p1.x; r.y1 = p1.y; r.w = width; r.h = height; return r;
}

/* Rectf::get_left */
TX_FN float tx_rect_left(const TxRect* r) { return r->x1; }
/* Rectf::get_right: m_p1.x + m_size.width */
TX_FN float tx_rect_right(const TxRect* r) { return r->x1 + r->w; }
/* Rectf::get_top */
TX_FN float tx_rect_top(const TxRect* r) { return r->y1; }
/* Rectf::get_bottom: m_p1.y + m_size.height */
TX_FN float tx_rect_bottom(const TxRect* r) { return r->y1 + r->h; }
/* Rectf::get_width */
TX_FN float tx_rect_width(const TxRect* r) { return r->w; }
/* Rectf::get_height */
TX_FN float tx_rect_height(const TxRect* r) { return r->h; }

/* Rectf::set_left: m_size.width -= v - m_p1.x; m_p1.x = v; */
TX_FN void tx_rect_set_left(TxRect* r, float v) { r->w -= v - r->x1; r->x1 = v; }
/* Rectf::set_right: m_size.width += v - get_right(); */
TX_FN void tx_rect_set_right(TxRect* r, float v) { r->w += v - tx_rect_right(r); }
/* Rectf::set_top: m_size.height -= v - m_p1.y; m_p1.y = v; */
TX_FN void tx_rect_set_top(TxRect* r, float v) { r->h -= v - r->y1; r->y1 = v; }
/* Rectf::set_bottom: m_size.height += v - get_bottom(); */
TX_FN void tx_rect_set_bottom(TxRect* r, float v) { r->h += v - tx_rect_bottom(r); }

/* Rectf::get_middle */
TX_FN TxVec tx_rect_middle(const TxRect* r)
{
  return tx_vec(r->x1 + r->w / 2.0f,
                r->y1 + r->h / 2.0f);
}

/* Rectf::set_pos */
TX_FN void tx_rect_set_pos(TxRect* r, TxVec v) { r->x1 = v.x; r->y1 = v.y; }
/* Rectf::set_width */
TX_FN void tx_rect_set_width(TxRect* r, float width) { r->w = width; }
/* Rectf::set_height */
TX_FN void tx_rect_set_height(TxRect* r, float height) { r->h = height; }
/* Rectf::set_size */
TX_FN void tx_rect_set_size(TxRect* r, float width, float height) { r->w = width; r->h = height; }

/* Rectf::move: m_p1 += v; */
TX_FN void tx_rect_move(TxRect* r, TxVec v) { r->x1 += v.x; r->y1 += v.y; }

/* Rectf::moved: Rectf(m_p1 + v, m_size) */
TX_FN TxRect tx_rect_moved(const TxRect* r, TxVec v)
{
  return tx_rect_ps(tx_vec(r->x1 + v.x, r->y1 + v.y), r->w, r->h);
}

/* Rectf::contains */
TX_FN bool tx_rect_contains(const TxRect* r, TxVec v)
{
  return v.x >= r->x1 && v.y >= r->y1 && v.x < tx_rect_right(r) && v.y < tx_rect_bottom(r);
}

/* Rectf::overlaps */
TX_FN bool tx_rect_overlaps(const TxRect* r, const TxRect* other)
{
  if (tx_rect_right(r) < tx_rect_left(other) || tx_rect_left(r) > tx_rect_right(other))
    return false;
  if (tx_rect_bottom(r) < tx_rect_top(other) || tx_rect_top(r) > tx_rect_bottom(other))
    return false;

  return true;
}

/* Rectf::p1 */
TX_FN TxVec tx_rect_p1(const TxRect* r) { return tx_vec(r->x1, r->y1); }
/* Rectf::p2: Vector(m_p1.x + m_size.width, m_p1.y + m_size.height) */
TX_FN TxVec tx_rect_p2(const TxRect* r) { return tx_vec(r->x1 + r->w, r->y1 + r->h); }

/* Rectf::set_p1 */
TX_FN void tx_rect_set_p1(TxRect* r, TxVec p)
{
  float nw = r->w + (r->x1 - p.x);
  float nh = r->h + (r->y1 - p.y);
  r->w = nw;
  r->h = nh;
  r->x1 = p.x;
  r->y1 = p.y;
}

/* Rectf::set_p2 */
TX_FN void tx_rect_set_p2(TxRect* r, TxVec p)
{
  float nw = p.x - r->x1;
  float nh = p.y - r->y1;
  r->w = nw;
  r->h = nh;
}

/* Rectf::grown(const Vector& border) */
TX_FN TxRect tx_rect_grown_v(const TxRect* r, TxVec border)
{
  /* If the size would be shrunk below 0, do not resize. */
  if (r->w + border.x * 2 < 0.f || r->h + border.y * 2 < 0.f)
    return *r;

  return tx_rect(r->x1 - border.x, r->y1 - border.y,
                 tx_rect_right(r) + border.x, tx_rect_bottom(r) + border.y);
}

/* Rectf::grown(float border) */
TX_FN TxRect tx_rect_grown(const TxRect* r, float border)
{
  return tx_rect_grown_v(r, tx_vec(border, border));
}

/* Rectf::distance(const Vector& other, AnchorPoint ap = ANCHOR_MIDDLE), with
   get_anchor_pos(rect, ANCHOR_MIDDLE) of math/anchor_point.cpp inlined:
     result.x = rect.get_left() + (rect.get_right() - rect.get_left()) / 2.0f;
     result.y = rect.get_top() + (rect.get_bottom() - rect.get_top()) / 2.0f;
   (this is not get_middle(): it goes through get_right()/get_bottom()) */
TX_FN float tx_rect_distance(const TxRect* r, TxVec other)
{
  TxVec v;
  v.x = tx_rect_left(r) + (tx_rect_right(r) - tx_rect_left(r)) / 2.0f;
  v.y = tx_rect_top(r) + (tx_rect_bottom(r) - tx_rect_top(r)) / 2.0f;
  return tx_vec_distance(v, other);
}

/* ------------------------------------------------------------------------ */
/* collision/collision_hit.hpp : CollisionHit                                */
/* ------------------------------------------------------------------------ */

/* CollisionHit::CollisionHit() */
TX_FN TxHit tx_hit(void)
{
  TxHit h;
  h.left = false;
  h.right = false;
  h.top = false;
  h.bottom = false;
  h.crush = false;
  h.slope_normal = tx_vec(0.0f, 0.0f);
  return h;
}

/* CollisionHit::has_direction */
TX_FN bool tx_hit_has_direction(const TxHit* h) { return h->left || h->right || h->top || h->bottom; }
/* CollisionHit::is_vertical */
TX_FN bool tx_hit_is_vertical(const TxHit* h) { return h->top || h->bottom; }
/* CollisionHit::is_horizontal */
TX_FN bool tx_hit_is_horizontal(const TxHit* h) { return h->left || h->right; }

/* std::swap(hit.left, hit.right); std::swap(hit.top, hit.bottom); */
TX_FN void tx_hit_swap(TxHit* h)
{
  bool t;
  t = h->left; h->left = h->right; h->right = t;
  t = h->top; h->top = h->bottom; h->bottom = t;
}

/* ------------------------------------------------------------------------ */
/* supertux/physic.cpp/.hpp : Physic                                         */
/* ------------------------------------------------------------------------ */

/* Physic::Physic */
TX_FN void tx_physic_init(TxPhysic* p)
{
  p->ax = 0; p->ay = 0;
  p->vx = 0; p->vy = 0;
  p->gravity_enabled_flag = true;
  p->gravity_modifier = 1.0f;
}

/* Physic::reset */
TX_FN void tx_physic_reset(TxPhysic* p)
{
  p->ax = p->ay = p->vx = p->vy = 0;
  p->gravity_enabled_flag = true;
}

/* Physic::set_velocity(float nvx, float nvy) */
TX_FN void tx_physic_set_velocity(TxPhysic* p, float nvx, float nvy)
{
  p->vx = nvx;
  p->vy = nvy;
}

/* Physic::set_velocity(const Vector& vector) */
TX_FN void tx_physic_set_velocity_v(TxPhysic* p, TxVec vector)
{
  p->vx = vector.x;
  p->vy = vector.y;
}

/* Physic::set_velocity_x / set_velocity_y */
TX_FN void tx_physic_set_velocity_x(TxPhysic* p, float nvx) { p->vx = nvx; }
TX_FN void tx_physic_set_velocity_y(TxPhysic* p, float nvy) { p->vy = nvy; }

/* Physic::inverse_velocity_x / inverse_velocity_y */
TX_FN void tx_physic_inverse_velocity_x(TxPhysic* p) { p->vx = -p->vx; }
TX_FN void tx_physic_inverse_velocity_y(TxPhysic* p) { p->vy = -p->vy; }

/* Physic::get_velocity / get_velocity_x / get_velocity_y */
TX_FN TxVec tx_physic_get_velocity(const TxPhysic* p) { return tx_vec(p->vx, p->vy); }
TX_FN float tx_physic_get_velocity_x(const TxPhysic* p) { return p->vx; }
TX_FN float tx_physic_get_velocity_y(const TxPhysic* p) { return p->vy; }

/* Physic::set_acceleration(float nax, float nay) */
TX_FN void tx_physic_set_acceleration(TxPhysic* p, float nax, float nay)
{
  p->ax = nax;
  p->ay = nay;
}

/* Physic::set_acceleration(const Vector& vector) */
TX_FN void tx_physic_set_acceleration_v(TxPhysic* p, TxVec vector)
{
  p->ax = vector.x;
  p->ay = vector.y;
}

/* Physic::set_acceleration_x / set_acceleration_y */
TX_FN void tx_physic_set_acceleration_x(TxPhysic* p, float nax) { p->ax = nax; }
TX_FN void tx_physic_set_acceleration_y(TxPhysic* p, float nay) { p->ay = nay; }

/* Physic::get_acceleration / get_acceleration_x / get_acceleration_y */
TX_FN TxVec tx_physic_get_acceleration(const TxPhysic* p) { return tx_vec(p->ax, p->ay); }
TX_FN float tx_physic_get_acceleration_x(const TxPhysic* p) { return p->ax; }
TX_FN float tx_physic_get_acceleration_y(const TxPhysic* p) { return p->ay; }

/* Physic::enable_gravity / gravity_enabled */
TX_FN void tx_physic_enable_gravity(TxPhysic* p, bool enable) { p->gravity_enabled_flag = enable; }
TX_FN bool tx_physic_gravity_enabled(const TxPhysic* p) { return p->gravity_enabled_flag; }

/* Physic::set_gravity_modifier / get_gravity_modifier */
TX_FN void tx_physic_set_gravity_modifier(TxPhysic* p, float modifier) { p->gravity_modifier = modifier; }
TX_FN float tx_physic_get_gravity_modifier(const TxPhysic* p) { return p->gravity_modifier; }

/* Physic::get_movement
   NOT TRANSLATED (NT3): `Sector::get().get_gravity()` is the argument
   `sector_gravity` (pass TxSector.gravity; Sector::m_gravity is 10.0f). */
TX_FN TxVec tx_physic_get_movement(TxPhysic* p, float sector_gravity, float dt_sec)
{
  float grav = p->gravity_enabled_flag ? (sector_gravity * p->gravity_modifier * 100.0f) : 0;

  /* Semi-implicit Euler integration */
  p->vx += p->ax * dt_sec;
  p->vy += (p->ay + grav) * dt_sec;
  {
    TxVec result = tx_vec(p->vx * dt_sec, p->vy * dt_sec);

    return result;
  }
}

/* ------------------------------------------------------------------------ */
/* Containers                                                                */
/* ------------------------------------------------------------------------ */

TX_FN void* tx__grow(void* p, int* cap, int need, size_t elem)
{
  int ncap;
  void* np;
  if (need <= *cap) return p;
  ncap = *cap ? *cap * 2 : 8;
  if (ncap < need) ncap = need;
  np = realloc(p, (size_t)ncap * elem);
  if (!np) abort();
  *cap = ncap;
  return np;
}

/* unordered_set::insert */
TX_FN void tx__set_insert(TxObjSet* s, TxObj* o)
{
  int i;
  for (i = 0; i < s->n; ++i)
    if (s->v[i] == o) return;
  s->v = (TxObj**)tx__grow(s->v, &s->cap, s->n + 1, sizeof(TxObj*));
  s->v[s->n++] = o;
}

/* unordered_set::erase(key) */
TX_FN void tx__set_erase(TxObjSet* s, TxObj* o)
{
  int i;
  for (i = 0; i < s->n; ++i) {
    if (s->v[i] == o) {
      memmove(&s->v[i], &s->v[i + 1], (size_t)(s->n - i - 1) * sizeof(TxObj*));
      --s->n;
      return;
    }
  }
}

/* unordered_set::clear */
TX_FN void tx__set_clear(TxObjSet* s) { s->n = 0; }

TX_FN void tx__set_free(TxObjSet* s)
{
  free(s->v);
  s->v = NULL;
  s->n = s->cap = 0;
}

/* ------------------------------------------------------------------------ */
/* collision/collision_movement_manager.cpp : CollisionGroundMovementManager */
/* ------------------------------------------------------------------------ */

/* m_movements_per_target[&target].m_moving_objects.emplace(&moving_object, movement)
   or .m_moving_tilemaps.emplace(...): emplace does nothing when the key is
   already there (the first registered movement stays). */
TX_FN void tx__gmm_emplace(TxGroundMovementManager* m, TxObj* target, TxObj* obj, TxMap* map, TxVec movement)
{
  int i;
  TxGroundMove* e;
  for (i = 0; i < m->n; ++i) {
    e = &m->v[i];
    if (e->target == target && e->obj == obj && e->map == map)
      return;
  }
  m->v = (TxGroundMove*)tx__grow(m->v, &m->cap, m->n + 1, sizeof(TxGroundMove));
  e = &m->v[m->n++];
  e->target = target;
  e->obj = obj;
  e->map = map;
  e->movement = movement;
}

/* CollisionGroundMovementManager::register_movement(CollisionObject& moving_object,
     CollisionObject& target_object, const Vector& movement) */
TX_FN void tx_gmm_register_movement(TxGroundMovementManager* m, TxObj* moving_object,
                                    TxObj* target_object, TxVec movement)
{
  tx__gmm_emplace(m, target_object, moving_object, NULL, movement);
}

/* CollisionGroundMovementManager::register_movement(TileMap& moving_tilemap,
     CollisionObject& target_object, const Vector& movement) */
TX_FN void tx_gmm_register_movement_map(TxGroundMovementManager* m, TxMap* moving_tilemap,
                                        TxObj* target_object, TxVec movement)
{
  tx__gmm_emplace(m, target_object, NULL, moving_tilemap, movement);
}

/* CollisionGroundMovementManager::apply_all_ground_movement
   The C++ walks unordered_maps keyed by pointer. The order of the targets
   does not matter (each target is only changed by its own entry). Inside one
   target the order matters only when two movers have exactly the same
   movement.y and a different movement.x: the C++ then keeps whichever its
   hash order visits first (which depends on the addresses), this keeps the
   first registered. Objects are looked at before tilemaps, as in the C++. */
TX_FN void tx_gmm_apply_all_ground_movement(TxGroundMovementManager* m)
{
  int t, i;
  for (t = 0; t < m->n; ++t)
  {
    TxObj* target = m->v[t].target;
    TxVec lowest_y_vector;
    bool first_to_do;
    bool seen = false;

    /* one pass per target (a key of m_movements_per_target) */
    for (i = 0; i < t; ++i)
      if (m->v[i].target == target) { seen = true; break; }
    if (seen) continue;

    /* Find the lowest "y" position (i.e. the highest point since
       (0,0) is the top-left corner) and the associated object. */
    lowest_y_vector = tx_vec(0.0f, 0.0f);
    first_to_do = true;

    for (i = t; i < m->n; ++i) {
      if (m->v[i].target != target || m->v[i].obj == NULL) continue;
      if (first_to_do || m->v[i].movement.y < lowest_y_vector.y) {
        lowest_y_vector = m->v[i].movement;
        first_to_do = false;
      }
    }

    for (i = t; i < m->n; ++i) {
      if (m->v[i].target != target || m->v[i].map == NULL) continue;
      if (first_to_do || m->v[i].movement.y < lowest_y_vector.y) {
        lowest_y_vector = m->v[i].movement;
        first_to_do = false;
      }
    }

    if (!first_to_do) {
      /* Move the object to the highest possible point. */
      target->movement = tx_vec_add(target->movement, lowest_y_vector);
    }
  }

  m->n = 0;
}

/* ------------------------------------------------------------------------ */
/* collision/collision_object.cpp/.hpp : CollisionObject                     */
/* ------------------------------------------------------------------------ */

/* CollisionObject::CollisionObject (valid/id/user are the host's: valid is
   set to true, id to 0, user to null) */
TX_FN void tx_obj_init(TxObj* o, int group)
{
  o->bbox = tx_rect_zero();
  o->group = group;
  o->physic_hint = NULL;
  o->movement = tx_vec(0.0f, 0.0f);
  o->dest = tx_rect_zero();
  o->unisolid = false;
  o->pressure = tx_vec(0.0f, 0.0f);
  o->objects_hit_bottom.v = NULL;
  o->objects_hit_bottom.n = 0;
  o->objects_hit_bottom.cap = 0;
  o->ground_movement_manager = NULL;
  o->valid = true;
  o->id = 0;
  o->user = NULL;
}

/* ~CollisionObject (releases m_objects_hit_bottom) */
TX_FN void tx_obj_free(TxObj* o) { tx__set_free(&o->objects_hit_bottom); }

/* CollisionObject::notify_object_removal */
TX_FN void tx_obj_notify_object_removal(TxObj* o, TxObj* other)
{
  tx__set_erase(&o->objects_hit_bottom, other);
}

/* CollisionObject::clear_bottom_collision_list */
TX_FN void tx_obj_clear_bottom_collision_list(TxObj* o) { tx__set_clear(&o->objects_hit_bottom); }

/* CollisionObject::set_movement / get_movement */
TX_FN void tx_obj_set_movement(TxObj* o, TxVec movement) { o->movement = movement; }
TX_FN TxVec tx_obj_get_movement(const TxObj* o) { return o->movement; }

/* CollisionObject::get_pos */
TX_FN TxVec tx_obj_get_pos(const TxObj* o) { return tx_rect_p1(&o->bbox); }

/* CollisionObject::get_pressure */
TX_FN TxVec tx_obj_get_pressure(const TxObj* o) { return o->pressure; }

/* CollisionObject::set_pos (and move_to, which only calls it):
     m_dest.move(pos - get_pos());
     m_bbox.set_pos(pos); */
TX_FN void tx_obj_set_pos(TxObj* o, TxVec pos)
{
  tx_rect_move(&o->dest, tx_vec_sub(pos, tx_obj_get_pos(o)));
  tx_rect_set_pos(&o->bbox, pos);
}

/* CollisionObject::set_width */
TX_FN void tx_obj_set_width(TxObj* o, float w)
{
  tx_rect_set_width(&o->dest, w);
  tx_rect_set_width(&o->bbox, w);
}

/* CollisionObject::set_size */
TX_FN void tx_obj_set_size(TxObj* o, float w, float h)
{
  tx_rect_set_size(&o->dest, w, h);
  tx_rect_set_size(&o->bbox, w, h);
}

/* CollisionObject::set_physic_hint */
TX_FN void tx_obj_set_physic_hint(TxObj* o, const TxPhysic* physic) { o->physic_hint = physic; }

/* CollisionObject::propagate_movement
   (the set is walked in insertion order; the C++ walks its hash order. The
   registrations of one call have distinct targets, so this order only shows
   through the tie described at tx_gmm_apply_all_ground_movement. Like the
   C++, this recurses without end if objects stand on each other in a cycle.) */
TX_FN void tx_obj_propagate_movement(TxObj* o, TxVec movement)
{
  int i;
  for (i = 0; i < o->objects_hit_bottom.n; ++i) {
    TxObj* other_object = o->objects_hit_bottom.v[i];
    if (other_object->group == TX_COLGROUP_STATIC) continue;
    tx_gmm_register_movement(o->ground_movement_manager, o, other_object, movement);
    tx_obj_propagate_movement(other_object, movement);
  }
}

/* CollisionObject::collides -> MovingObject::collides (default: true) */
TX_FN bool tx__collides(const TxSector* s, TxObj* self, TxObj* other, const TxHit* hit)
{
  if (s->collides) return s->collides(self, other, hit);
  return true;
}

/* CollisionObject::collision -> MovingObject::collision (pure virtual, NT5) */
TX_FN int tx__collision(const TxSector* s, TxObj* self, TxObj* other, const TxHit* hit)
{
  if (s->collision) return s->collision(self, other, hit);
  return TX_ABORT_MOVE;
}

/* CollisionObject::collision_solid -> MovingObject::collision_solid (default: {}) */
TX_FN void tx__collision_solid(const TxSector* s, TxObj* self, const TxHit* hit)
{
  if (s->collision_solid) s->collision_solid(self, hit);
}

/* CollisionObject::collision_tile -> MovingObject::collision_tile (default: {}) */
TX_FN void tx__collision_tile(const TxSector* s, TxObj* self, uint32_t tile_attributes)
{
  if (s->collision_tile) s->collision_tile(self, tile_attributes);
}

/* CollisionObject::collision_moving_object_bottom (not virtual in the C++;
   the callback is an added notification) */
TX_FN void tx__collision_moving_object_bottom(const TxSector* s, TxObj* self, TxObj* other)
{
  if (self->group == TX_COLGROUP_STATIC
    || self->group == TX_COLGROUP_MOVING_STATIC)
  {
    tx__set_insert(&self->objects_hit_bottom, other);
  }
  if (s->moving_object_bottom) s->moving_object_bottom(self, other);
}

/* ------------------------------------------------------------------------ */
/* math/aatriangle.cpp                                                       */
/* ------------------------------------------------------------------------ */

/* AATriangle::vertical_flip */
TX_FN int tx_aatriangle_vertical_flip(int dir)
{
  int direction = dir & TX_AAT_DIRECTION_MASK;
  int deform;
  direction = 3 - direction;
  deform = dir & TX_AAT_DEFORM_MASK;
  switch (deform) {
    case TX_AAT_DEFORM_BOTTOM:
      deform = TX_AAT_DEFORM_TOP;
      break;
    case TX_AAT_DEFORM_TOP:
      deform = TX_AAT_DEFORM_BOTTOM;
      break;
    default:
      /* Unchanged. */
      break;
  }
  return (direction | deform);
}

/* ------------------------------------------------------------------------ */
/* supertux/tile.cpp/.hpp : Tile, supertux/tile_set.cpp : TileSet::get       */
/* ------------------------------------------------------------------------ */

/* TileSet::get: `if (id >= m_tiles.size()) return *m_tiles[0];`
   NOT TRANSLATED (NT6): `if (tile) return *tile; else return *m_tiles[0];`
   (TxTiles has no null slots). An empty TxTiles answers 0. */
TX_FN uint32_t tx__tile_index(const TxTiles* ts, uint32_t id)
{
  if (id >= ts->count) return 0;
  return id;
}

/* Tile::get_attributes */
TX_FN uint32_t tx_tile_attributes(const TxTiles* ts, uint32_t id)
{
  if (ts->count == 0) return 0;
  return ts->attrs[tx__tile_index(ts, id)];
}

/* Tile::get_data */
TX_FN int tx_tile_data(const TxTiles* ts, uint32_t id)
{
  if (ts->count == 0) return 0;
  return ts->data[tx__tile_index(ts, id)];
}

/* Tile::is_slope */
TX_FN bool tx_tile_is_slope(uint32_t attributes) { return (attributes & TX_TILE_SLOPE) != 0; }
/* Tile::is_solid() */
TX_FN bool tx_tile_is_solid(uint32_t attributes) { return (attributes & TX_TILE_SOLID) != 0; }
/* Tile::is_unisolid */
TX_FN bool tx_tile_is_unisolid(uint32_t attributes) { return (attributes & TX_TILE_UNISOLID) != 0; }

/* tile.cpp: is_above_line */
TX_FN bool tx__is_above_line(float l_x, float l_y, float m,
                             float p_x, float p_y)
{
  float interp_y = (l_y + (m * (p_x - l_x)));
  return (interp_y >= p_y);
}

/* tile.cpp: is_below_line */
TX_FN bool tx__is_below_line(float l_x, float l_y, float m,
                             float p_x, float p_y)
{
  return !tx__is_above_line(l_x, l_y, m, p_x, p_y);
}

/* Tile::check_movement_unisolid */
TX_FN bool tx_tile_check_movement_unisolid(uint32_t attributes, int data, TxVec movement)
{
  int slope_info;
  double mv_x;
  double mv_y;
  double mv_tan;
  double slope_tan;

  /* If the tile is not a slope, this is very easy. */
  if (!tx_tile_is_slope(attributes))
  {
    int dir = data & TX_UNI_DIR_MASK;

    return ((dir == TX_UNI_DIR_NORTH) && (movement.y > -TX_EPSILON)) /* moving down */
        || ((dir == TX_UNI_DIR_SOUTH) && (movement.y < TX_EPSILON))  /* moving up */
        || ((dir == TX_UNI_DIR_WEST ) && (movement.x > -TX_EPSILON)) /* moving right */
        || ((dir == TX_UNI_DIR_EAST ) && (movement.x < TX_EPSILON)); /* moving left */
  }

  mv_x = (double)movement.x; /* note switch to double for no good reason */
  mv_y = (double)movement.y;

  slope_info = data;
  switch (slope_info & TX_AAT_DIRECTION_MASK)
  {
    case TX_AAT_SOUTHEAST:
      /* do nothing */
      break;
    case TX_AAT_SOUTHWEST:
      mv_x *= (-1.0);
      break;
    case TX_AAT_NORTHEAST:
      mv_y *= (-1.0);
      break;
    case TX_AAT_NORTHWEST:
      mv_x *= (-1.0);
      mv_y *= (-1.0);
      break;
  }

  /* Handle the easy cases first */
  /* If we're moving to the right and down, then the slope is solid. */
  if ((mv_x >= 0.0) && (mv_y >= 0.0)) /* 4th quadrant */
    return true;
  /* If we're moving to the left and up, then the slope is not solid. */
  else if ((mv_x <= 0.0) && (mv_y <= 0.0)) /* 2nd quadrant */
    return false;

  /* NOT TRANSLATED (NT4):
       assert (mv_x != 0.0);
       assert (mv_y != 0.0); */

  /* calculate tangent of movement */
  mv_tan = (-1.0) * mv_y / mv_x;

  /* determine tangent of the slope */
  slope_tan = 1.0;
  if (((slope_info & TX_AAT_DEFORM_MASK) == TX_AAT_DEFORM_BOTTOM)
      || ((slope_info & TX_AAT_DEFORM_MASK) == TX_AAT_DEFORM_TOP))
    slope_tan = 0.5; /* ~= 26.6 deg */
  else if (((slope_info & TX_AAT_DEFORM_MASK) == TX_AAT_DEFORM_LEFT)
      || ((slope_info & TX_AAT_DEFORM_MASK) == TX_AAT_DEFORM_RIGHT))
    slope_tan = 2.0; /* ~= 63.4 deg */

  /* up and right */
  if (mv_x > 0.0) /* 1st quadrant */
  {
    /* NOT TRANSLATED (NT4): assert (mv_y < 0.0); */
    return (mv_tan <= slope_tan);
  }
  /* down and left */
  else if (mv_x < 0.0) /* 3rd quadrant */
  {
    /* NOT TRANSLATED (NT4): assert (mv_y > 0.0); */
    return (mv_tan >= slope_tan);
  }

  return false;
}

/* Tile::check_position_unisolid */
TX_FN bool tx_tile_check_position_unisolid(uint32_t attributes, int data,
                                           const TxRect* obj_bbox,
                                           const TxRect* tile_bbox)
{
  int slope_info;
  float tile_x;
  float tile_y;
  float gradient;
  float delta_x;
  float delta_y;
  float obj_x = 0.0;
  float obj_y = 0.0;

  /* If this is not a slope, this is - again - easy */
  if (!tx_tile_is_slope(attributes))
  {
    int dir = data & TX_UNI_DIR_MASK;

    return ((dir == TX_UNI_DIR_NORTH) && ((tx_rect_bottom(obj_bbox) - TX_SHIFT_DELTA) <= tx_rect_top(tile_bbox)   ))
        || ((dir == TX_UNI_DIR_SOUTH) && ((tx_rect_top(obj_bbox)    + TX_SHIFT_DELTA) >= tx_rect_bottom(tile_bbox)))
        || ((dir == TX_UNI_DIR_WEST ) && ((tx_rect_right(obj_bbox)  - TX_SHIFT_DELTA) <= tx_rect_left(tile_bbox)  ))
        || ((dir == TX_UNI_DIR_EAST ) && ((tx_rect_left(obj_bbox)   + TX_SHIFT_DELTA) >= tx_rect_right(tile_bbox) ));
  }

  /* There are 20 different cases. For each case, calculate a line
     that describes the slope's surface. The line is defined by x, y,
     and m, the gradient. */
  slope_info = data;
  switch (slope_info
      & (TX_AAT_DIRECTION_MASK | TX_AAT_DEFORM_MASK))
  {
    case TX_AAT_SOUTHWEST:
    case TX_AAT_SOUTHWEST | TX_AAT_DEFORM_TOP:
    case TX_AAT_SOUTHWEST | TX_AAT_DEFORM_LEFT:
    case TX_AAT_NORTHEAST:
    case TX_AAT_NORTHEAST | TX_AAT_DEFORM_TOP:
    case TX_AAT_NORTHEAST | TX_AAT_DEFORM_LEFT:
      tile_x = tx_rect_left(tile_bbox);
      tile_y = tx_rect_top(tile_bbox);
      gradient = 1.0;
      break;

    case TX_AAT_SOUTHEAST:
    case TX_AAT_SOUTHEAST | TX_AAT_DEFORM_TOP:
    case TX_AAT_SOUTHEAST | TX_AAT_DEFORM_RIGHT:
    case TX_AAT_NORTHWEST:
    case TX_AAT_NORTHWEST | TX_AAT_DEFORM_TOP:
    case TX_AAT_NORTHWEST | TX_AAT_DEFORM_RIGHT:
      tile_x = tx_rect_right(tile_bbox);
      tile_y = tx_rect_top(tile_bbox);
      gradient = -1.0;
      break;

    case TX_AAT_SOUTHEAST | TX_AAT_DEFORM_BOTTOM:
    case TX_AAT_SOUTHEAST | TX_AAT_DEFORM_LEFT:
    case TX_AAT_NORTHWEST | TX_AAT_DEFORM_BOTTOM:
    case TX_AAT_NORTHWEST | TX_AAT_DEFORM_LEFT:
      tile_x = tx_rect_left(tile_bbox);
      tile_y = tx_rect_bottom(tile_bbox);
      gradient = -1.0;
      break;

    case TX_AAT_SOUTHWEST | TX_AAT_DEFORM_BOTTOM:
    case TX_AAT_SOUTHWEST | TX_AAT_DEFORM_RIGHT:
    case TX_AAT_NORTHEAST | TX_AAT_DEFORM_BOTTOM:
    case TX_AAT_NORTHEAST | TX_AAT_DEFORM_RIGHT:
      tile_x = tx_rect_right(tile_bbox);
      tile_y = tx_rect_bottom(tile_bbox);
      gradient = 1.0;
      break;

    default:
      /* NOT TRANSLATED (NT4): assert(false); */
      return 0;
  }

  /* delta_x, delta_y: Gradient aware version of SHIFT_DELTA. Here, we set the
     sign of the values only. Also, we determine here which corner of the
     object's bounding box is the interesting one for us. */
  delta_x = 1.0f * TX_SHIFT_DELTA;
  delta_y = 1.0f * TX_SHIFT_DELTA;
  switch (slope_info & TX_AAT_DIRECTION_MASK)
  {
    case TX_AAT_SOUTHWEST:
      delta_x *= 1.f;
      delta_y *= -1.f;
      obj_x = tx_rect_left(obj_bbox);
      obj_y = tx_rect_bottom(obj_bbox);
      break;

    case TX_AAT_SOUTHEAST:
      delta_x *= -1.f;
      delta_y *= -1.f;
      obj_x = tx_rect_right(obj_bbox);
      obj_y = tx_rect_bottom(obj_bbox);
      break;

    case TX_AAT_NORTHWEST:
      delta_x *= 1.f;
      delta_y *= 1.f;
      obj_x = tx_rect_left(obj_bbox);
      obj_y = tx_rect_top(obj_bbox);
      break;

    case TX_AAT_NORTHEAST:
      delta_x *= -1.f;
      delta_y *= 1.f;
      obj_x = tx_rect_right(obj_bbox);
      obj_y = tx_rect_top(obj_bbox);
      break;
  }

  /* Adapt the delta_x, delta_y and the gradient for the 26.6 deg and
     63.4 deg cases. */
  switch (slope_info & TX_AAT_DEFORM_MASK)
  {
    case 0:
      delta_x *= .70710678118654752440f; /* 1/sqrt(2) */
      delta_y *= .70710678118654752440f; /* 1/sqrt(2) */
      break;

    case TX_AAT_DEFORM_BOTTOM:
    case TX_AAT_DEFORM_TOP:
      delta_x *= .44721359549995793928f; /* 1/sqrt(5) */
      delta_y *= .89442719099991587856f; /* 2/sqrt(5) */
      gradient *= 0.5f;
      break;

    case TX_AAT_DEFORM_LEFT:
    case TX_AAT_DEFORM_RIGHT:
      delta_x *= .89442719099991587856f; /* 2/sqrt(5) */
      delta_y *= .44721359549995793928f; /* 1/sqrt(5) */
      gradient *= 2.f;
      break;
  }

  /* With a south slope, check if all points are above the line. If
     one point isn't, the slope is not solid. => You can pass through
     a south-slope from below but not from above. */
  if (((slope_info & TX_AAT_DIRECTION_MASK) == TX_AAT_SOUTHWEST) ||
      ((slope_info & TX_AAT_DIRECTION_MASK) == TX_AAT_SOUTHEAST))
  {
    return !tx__is_below_line(tile_x, tile_y, gradient, obj_x + delta_x, obj_y + delta_y);
  }
  /* northwest or northeast. Same as above, but inverted. You can pass
     from top to bottom but not vice versa. */
  else
  {
    return !tx__is_above_line(tile_x, tile_y, gradient, obj_x + delta_x, obj_y + delta_y);
  }
}

/* Tile::is_collisionful */
TX_FN bool tx_tile_is_collisionful(uint32_t attributes, int data, const TxRect* tile_bbox,
                                   const TxRect* position, TxVec movement)
{
  if (!(attributes & TX_TILE_UNISOLID))
    return true;

  return tx_tile_check_movement_unisolid(attributes, data, movement) &&
         tx_tile_check_position_unisolid(attributes, data, position, tile_bbox);
}

/* Tile::is_solid(const Rectf& tile_bbox, const Rectf& position, const Vector& movement) */
TX_FN bool tx_tile_is_solid_at(uint32_t attributes, int data, const TxRect* tile_bbox,
                               const TxRect* position, TxVec movement)
{
  if (!(attributes & TX_TILE_SOLID))
    return false;

  return tx_tile_is_collisionful(attributes, data, tile_bbox, position, movement);
}

/* ------------------------------------------------------------------------ */
/* object/tilemap.cpp/.hpp : TileMap                                         */
/* ------------------------------------------------------------------------ */

/* TileMap::TileMap, the collision fields only (tiles are the host's) */
TX_FN void tx_map_init(TxMap* m, int width, int height, uint32_t* tiles)
{
  m->width = width;
  m->height = height;
  m->tiles = tiles;
  m->offset = tx_vec(0, 0);
  m->movement = tx_vec(0, 0);
  m->flip = TX_NO_FLIP;
  m->objects_hit_bottom.v = NULL;
  m->objects_hit_bottom.n = 0;
  m->objects_hit_bottom.cap = 0;
  m->ground_movement_manager = NULL;
  m->user = NULL;
}

/* ~TileMap (releases m_objects_hit_bottom; not the tiles) */
TX_FN void tx_map_free(TxMap* m) { tx__set_free(&m->objects_hit_bottom); }

/* TileMap::get_movement(bool actual) */
TX_FN TxVec tx_map_get_movement(const TxMap* m, bool actual)
{
  if (actual) {
    return m->movement;
  }

  return tx_vec(m->movement.x, tx_maxf(0.0f, m->movement.y));
}

/* TileMap::get_tile_position:
     get_offset() + Vector(static_cast<float>(x), static_cast<float>(y)) * 32.0f */
TX_FN TxVec tx_map_get_tile_position(const TxMap* m, int x, int y)
{
  return tx_vec_add(m->offset, tx_vec_scale(tx_vec((float)x, (float)y), 32.0f));
}

/* TileMap::get_bbox */
TX_FN TxRect tx_map_get_bbox(const TxMap* m)
{
  return tx_rect_pp(tx_map_get_tile_position(m, 0, 0),
                    tx_map_get_tile_position(m, m->width, m->height));
}

/* TileMap::get_tile_bbox */
TX_FN TxRect tx_map_get_tile_bbox(const TxMap* m, int x, int y)
{
  return tx_rect_pp(tx_map_get_tile_position(m, x, y),
                    tx_map_get_tile_position(m, x + 1, y + 1));
}

/* TileMap::get_tiles_overlapping
   (int(float) of a value outside the int range is undefined in the C++ and
   here alike) */
TX_FN TxIRect tx_map_get_tiles_overlapping(const TxMap* m, const TxRect* rect)
{
  TxRect rect2 = *rect;
  TxIRect r;
  int t_left, t_right, t_top, t_bottom;
  tx_rect_move(&rect2, tx_vec_neg(m->offset));

  t_left   = tx_maxi(0        , (int)(floorf(tx_rect_left  (&rect2) / 32)));
  t_right  = tx_mini(m->width , (int)(ceilf (tx_rect_right (&rect2) / 32)));
  t_top    = tx_maxi(0        , (int)(floorf(tx_rect_top   (&rect2) / 32)));
  t_bottom = tx_mini(m->height, (int)(ceilf (tx_rect_bottom(&rect2) / 32)));
  r.left = t_left;
  r.top = t_top;
  r.right = t_right;
  r.bottom = t_bottom;
  return r;
}

/* TileMap::hits_object_bottom (not virtual in the C++; the callback is an
   added notification) */
TX_FN void tx__map_hits_object_bottom(const TxSector* s, TxMap* m, TxObj* object)
{
  tx__set_insert(&m->objects_hit_bottom, object);
  if (s->map_hits_bottom) s->map_hits_bottom(m, object);
}

/* TileMap::notify_object_removal */
TX_FN void tx_map_notify_object_removal(TxMap* m, TxObj* other)
{
  tx__set_erase(&m->objects_hit_bottom, other);
}

/* TileMap::get_tile_id(int x, int y) */
TX_FN uint32_t tx_map_get_tile_id(const TxMap* m, int x, int y)
{
  if (x < 0) x = 0;
  if (x >= m->width) x = m->width - 1;
  if (y < 0) y = 0;
  if (y >= m->height) y = m->height - 1;

  if (x < 0 || x >= m->width || y < 0 || y >= m->height) {
    return 0;
  }

  return m->tiles[y*m->width + x];
}

/* TileMap::is_outside_bounds */
TX_FN bool tx_map_is_outside_bounds(const TxMap* m, TxVec pos)
{
  TxVec pos_ = tx_vec_div(tx_vec_sub(pos, m->offset), 32.0f);
  float width = (float)m->width;
  float height = (float)m->height;
  return pos_.x < 0 || pos_.x >= width || pos_.y < 0 || pos_.y >= height;
}

/* TileMap::get_tile_id_at(const Vector& pos) */
TX_FN uint32_t tx_map_get_tile_id_at(const TxMap* m, TxVec pos)
{
  TxVec xy = tx_vec_div(tx_vec_sub(pos, m->offset), 32.0f);
  return tx_map_get_tile_id(m, (int)xy.x, (int)xy.y);
}

/* TileMap::update, the ground movement part only (NT7):
     if (m_ground_movement_manager != nullptr) {
       for (CollisionObject* other_object : m_objects_hit_bottom) {
         m_ground_movement_manager->register_movement(*this, *other_object, m_movement);
         other_object->propagate_movement(m_movement);
       }
     }
   The C++ runs this only when the tilemap follows a valid path, right after
   `m_movement = v - get_offset(); set_offset(v);`. */
TX_FN void tx_map_propagate_ground_movement(TxMap* m)
{
  int i;
  if (m->ground_movement_manager != NULL) {
    for (i = 0; i < m->objects_hit_bottom.n; ++i) {
      TxObj* other_object = m->objects_hit_bottom.v[i];
      tx_gmm_register_movement_map(m->ground_movement_manager, m, other_object, m->movement);
      tx_obj_propagate_movement(other_object, m->movement);
    }
  }
}

/* TileMap::update, its last line: m_objects_hit_bottom.clear(); */
TX_FN void tx_map_clear_objects_hit_bottom(TxMap* m) { tx__set_clear(&m->objects_hit_bottom); }

/* ------------------------------------------------------------------------ */
/* collision/collision.cpp/.hpp                                              */
/* ------------------------------------------------------------------------ */

/* Constraints::Constraints */
TX_FN TxConstraints tx_constraints(void)
{
  TxConstraints c;
  float infinity = TX_INFINITY;
  c.hit = tx_hit();
  c.position_left = -infinity;
  c.position_right = infinity;
  c.position_top = -infinity;
  c.position_bottom = infinity;
  return c;
}

/* Constraints::has_constraints */
TX_FN bool tx_constraints_has_constraints(const TxConstraints* c)
{
  float infinity = TX_INFINITY;
  return
    c->position_left   > -infinity ||
    c->position_right  <  infinity ||
    c->position_top    > -infinity ||
    c->position_bottom <  infinity;
}

/* Constraints::constrain_left */
TX_FN void tx_constrain_left(TxConstraints* c, float position)
{
  c->position_left = tx_maxf(c->position_left, position);
}

/* Constraints::constrain_right */
TX_FN void tx_constrain_right(TxConstraints* c, float position)
{
  c->position_right = tx_minf(c->position_right, position);
}

/* Constraints::constrain_top */
TX_FN void tx_constrain_top(TxConstraints* c, float position)
{
  c->position_top = tx_maxf(c->position_top, position);
}

/* Constraints::constrain_bottom */
TX_FN void tx_constrain_bottom(TxConstraints* c, float position)
{
  c->position_bottom = tx_minf(c->position_bottom, position);
}

/* Constraints::get_height */
TX_FN float tx_constraints_get_height(const TxConstraints* c) { return (c->position_bottom - c->position_top); }
/* Constraints::get_width */
TX_FN float tx_constraints_get_width(const TxConstraints* c) { return (c->position_right - c->position_left); }
/* Constraints::get_x_midpoint */
TX_FN float tx_constraints_get_x_midpoint(const TxConstraints* c) { return (.5f * (c->position_left + c->position_right)); }

/* Constraints::merge_constraints */
TX_FN void tx_constraints_merge(TxConstraints* c, const TxConstraints* other)
{
  tx_constrain_left(c, other->position_left);
  tx_constrain_right(c, other->position_right);
  tx_constrain_top(c, other->position_top);
  tx_constrain_bottom(c, other->position_bottom);

  c->hit.left |= other->hit.left;
  c->hit.right |= other->hit.right;
  c->hit.top |= other->hit.top;
  c->hit.bottom |= other->hit.bottom;
  c->hit.crush |= other->hit.crush;
}

/* collision.cpp: makePlane */
TX_FN void tx__make_plane(TxVec p1, TxVec p2, TxVec* n, float* c)
{
  float nval;
  *n = tx_vec(p2.y - p1.y, p1.x - p2.x);
  *c = -tx_vec_dot(p2, *n);
  nval = tx_vec_length(*n);
  n->x /= nval;
  n->y /= nval;
  *c /= nval;
}

/* collision::set_rectangle_rectangle_constraints */
TX_FN void tx_set_rectangle_rectangle_constraints(TxConstraints* constraints, const TxRect* r1, const TxRect* r2)
{
  float itop = tx_rect_bottom(r1) - tx_rect_top(r2);
  float ibottom = tx_rect_bottom(r2) - tx_rect_top(r1);
  float ileft = tx_rect_right(r1) - tx_rect_left(r2);
  float iright = tx_rect_right(r2) - tx_rect_left(r1);

  float vert_penetration = tx_minf(itop, ibottom);
  float horiz_penetration = tx_minf(ileft, iright);
  if (vert_penetration < horiz_penetration) {
    if (itop < ibottom) {
      tx_constrain_bottom(constraints, tx_rect_top(r2));
      constraints->hit.bottom = true;
    } else {
      tx_constrain_top(constraints, tx_rect_bottom(r2));
      constraints->hit.top = true;
    }
  } else {
    if (ileft < iright) {
      tx_constrain_right(constraints, tx_rect_left(r2));
      constraints->hit.right = true;
    } else {
      tx_constrain_left(constraints, tx_rect_right(r2));
      constraints->hit.left = true;
    }
  }
}

/* collision::rectangle_aatriangle(Constraints*, const Rectf& rect,
     const AATriangle& triangle, bool& hits_rectangle_bottom,
     CollisionObject* object = nullptr)
   `hits_rectangle_bottom` must not be null; `object` may be. */
TX_FN bool tx_rectangle_aatriangle_ex(TxConstraints* constraints, const TxRect* rect,
                                      const TxAATriangle* triangle,
                                      bool* hits_rectangle_bottom,
                                      const TxObj* object)
{
  TxVec normal;
  float c;
  TxVec p1;
  TxRect area;
  bool has_downwards_east_corner, has_downwards_west_corner;
  float n_p1;
  float depth;
  TxVec outvec;
  float RDELTA;

  if (!tx_rect_overlaps(rect, &triangle->bbox))
    return false;

  normal = tx_vec(0.0f, 0.0f);
  c = 0.0;
  p1 = tx_vec(0.0f, 0.0f);
  area = tx_rect_zero();
  switch (triangle->dir & TX_AAT_DEFORM_MASK) {
    case 0:
      tx_rect_set_p1(&area, tx_rect_p1(&triangle->bbox));
      tx_rect_set_p2(&area, tx_rect_p2(&triangle->bbox));
      break;
    case TX_AAT_DEFORM_BOTTOM:
      tx_rect_set_p1(&area, tx_vec(tx_rect_left(&triangle->bbox), tx_rect_top(&triangle->bbox) + tx_rect_height(&triangle->bbox)/2));
      tx_rect_set_p2(&area, tx_rect_p2(&triangle->bbox));
      break;
    case TX_AAT_DEFORM_TOP:
      tx_rect_set_p1(&area, tx_rect_p1(&triangle->bbox));
      tx_rect_set_p2(&area, tx_vec(tx_rect_right(&triangle->bbox), tx_rect_top(&triangle->bbox) + tx_rect_height(&triangle->bbox)/2));
      break;
    case TX_AAT_DEFORM_LEFT:
      tx_rect_set_p1(&area, tx_rect_p1(&triangle->bbox));
      tx_rect_set_p2(&area, tx_vec(tx_rect_left(&triangle->bbox) + tx_rect_width(&triangle->bbox)/2, tx_rect_bottom(&triangle->bbox)));
      break;
    case TX_AAT_DEFORM_RIGHT:
      tx_rect_set_p1(&area, tx_vec(tx_rect_left(&triangle->bbox) + tx_rect_width(&triangle->bbox)/2, tx_rect_top(&triangle->bbox)));
      tx_rect_set_p2(&area, tx_rect_p2(&triangle->bbox));
      break;
    default:
      /* NOT TRANSLATED (NT4): assert(false); */
      break;
  }

  has_downwards_east_corner = false;
  has_downwards_west_corner = false;
  switch (triangle->dir & TX_AAT_DIRECTION_MASK) {
    case TX_AAT_SOUTHWEST:
      p1 = tx_vec(tx_rect_left(rect), tx_rect_bottom(rect));
      tx__make_plane(tx_rect_p1(&area), tx_rect_p2(&area), &normal, &c);
      break;
    case TX_AAT_NORTHEAST:
      p1 = tx_vec(tx_rect_right(rect), tx_rect_top(rect));
      tx__make_plane(tx_rect_p2(&area), tx_rect_p1(&area), &normal, &c);
      has_downwards_west_corner = true;
      break;
    case TX_AAT_SOUTHEAST:
      p1 = tx_rect_p2(rect);
      tx__make_plane(tx_vec(tx_rect_left(&area), tx_rect_bottom(&area)),
                     tx_vec(tx_rect_right(&area), tx_rect_top(&area)), &normal, &c);
      break;
    case TX_AAT_NORTHWEST:
      p1 = tx_rect_p1(rect);
      tx__make_plane(tx_vec(tx_rect_right(&area), tx_rect_top(&area)),
                     tx_vec(tx_rect_left(&area), tx_rect_bottom(&area)), &normal, &c);
      has_downwards_east_corner = true;
      break;
    default:
      /* NOT TRANSLATED (NT4): assert(false); (unreachable: the mask is 3) */
      break;
  }

  n_p1 = -tx_vec_dot(normal, p1);
  depth = n_p1 - c;
  if (depth < 0)
    return false;

  outvec = tx_vec_scale(normal, (depth + 0.2f));

  RDELTA = 3;
  if (p1.x < tx_rect_left(&area) - RDELTA || p1.x > tx_rect_right(&area) + RDELTA
     || p1.y < tx_rect_top(&area) - RDELTA || p1.y > tx_rect_bottom(&area) + RDELTA) {
    /* somewhat of a hack? If the object is falling and hits the corner of a
       tile, they end up landing on the rectangle constraint. so check velocity
       as well as the side to make judgement. */
    /* `get_velocity_y() > 0.1`: float compared with a double literal, the
       float is promoted to double. */
    if (!object || !object->physic_hint ||
        /* left side */
        (!((double)tx_physic_get_velocity_y(object->physic_hint) > 0.1 && has_downwards_west_corner &&
           tx_rect_left(&area) >= tx_rect_left(rect)) &&
         /* right side */
         !((double)tx_physic_get_velocity_y(object->physic_hint) > 0.1 && has_downwards_east_corner &&
           tx_rect_right(&area) <= tx_rect_right(rect))))
      tx_set_rectangle_rectangle_constraints(constraints, rect, &area);
  } else {
    if (outvec.x < 0) {
      tx_constrain_right(constraints, tx_rect_right(rect) + outvec.x);
      constraints->hit.right = true;
    } else {
      tx_constrain_left(constraints, tx_rect_left(rect) + outvec.x);
      constraints->hit.left = true;
    }

    if (outvec.y < 0) {
      tx_constrain_bottom(constraints, tx_rect_bottom(rect) + outvec.y);
      constraints->hit.bottom = true;
      *hits_rectangle_bottom = true;
    } else {
      tx_constrain_top(constraints, tx_rect_top(rect) + outvec.y);
      constraints->hit.top = true;
    }
    constraints->hit.slope_normal = normal;
  }

  return true;
}

/* collision::rectangle_aatriangle(Constraints*, const Rectf&, const AATriangle&) */
TX_FN bool tx_rectangle_aatriangle(TxConstraints* constraints, const TxRect* rect,
                                   const TxAATriangle* triangle)
{
  bool dummy = false; /* uninitialised in the C++; never read */
  return tx_rectangle_aatriangle_ex(constraints, rect, triangle, &dummy, NULL);
}

/* collision::line_intersects_line */
TX_FN bool tx_line_intersects_line(TxVec line1_start, TxVec line1_end, TxVec line2_start, TxVec line2_end)
{
  /* Adapted from Striker, (C) 1999 Joris van der Hoeven, GPL */

  float a1 = line1_start.x, b1 = line1_start.y, a2 = line1_end.x, b2 = line1_end.y;
  float c1 = line2_start.x, d1 = line2_start.y, c2 = line2_end.x, d2 = line2_end.y;

  float num = (b2-b1)*(c2-c1) - (a2-a1)*(d2-d1);
  float den1 = (d2-b2)*(c1-c2) + (a2-c2)*(d1-d2);
  float den2 = (d2-b2)*(a1-a2) + (a2-c2)*(b1-b2);
  float t;

  /* Normalize to positive numerator. */
  if (num < 0) {
    num = -num;
    den1 = -den1;
    den2 = -den2;
  }

  /* Numerator is zero -> Check for parallel or coinciding lines. */
  if (num == 0) {
    if ((b1-b2)*(c1-a2) != (a1-a2)*(d1-b2)) return false;
    if (a1 == a2) {
      t = a1; a1 = b1; b1 = t;
      t = a2; a2 = b2; b2 = t;
      t = c1; c1 = d1; d1 = t;
      t = c2; c2 = d2; d2 = t;
    }
    if (a1 > a2) { t = a1; a1 = a2; a2 = t; }
    if (c1 > c2) { t = c1; c1 = c2; c2 = t; }
    return ((a1 <= c2) && (a2 >= c1));
  }

  /* Standard check. */
  return (den1>=0) && (den1<=num) && (den2>=0) && (den2<=num);
}

/* collision::intersects_line */
TX_FN bool tx_intersects_line(const TxRect* r, TxVec line_start, TxVec line_end)
{
  TxVec p1 = tx_rect_p1(r);
  TxVec p2 = tx_vec(tx_rect_right(r), tx_rect_top(r));
  TxVec p3 = tx_rect_p2(r);
  TxVec p4 = tx_vec(tx_rect_left(r), tx_rect_bottom(r));
  if (tx_line_intersects_line(p1, p2, line_start, line_end)) return true;
  if (tx_line_intersects_line(p2, p3, line_start, line_end)) return true;
  if (tx_line_intersects_line(p3, p4, line_start, line_end)) return true;
  if (tx_line_intersects_line(p4, p1, line_start, line_end)) return true;
  return false;
}

/* ------------------------------------------------------------------------ */
/* collision/collision_system.cpp : CollisionSystem                          */
/* ------------------------------------------------------------------------ */

/* CollisionSystem::CollisionSystem (callbacks null, no tiles, gravity 10.0f
   as Sector::Sector) */
TX_FN void tx_sector_init(TxSector* s)
{
  memset(s, 0, sizeof(*s));
  s->objects = NULL;
  s->maps = NULL;
  s->tiles.attrs = NULL;
  s->tiles.data = NULL;
  s->tiles.count = 0;
  s->ground_movement_manager.v = NULL;
  s->gravity = 10.0f;
  s->collides = NULL;
  s->collision = NULL;
  s->collision_solid = NULL;
  s->collision_tile = NULL;
  s->moving_object_bottom = NULL;
  s->map_hits_bottom = NULL;
}

/* ~CollisionSystem: releases the arrays of the sector, not the objects, the
   tilemaps or the tileset arrays (the host owns them). */
TX_FN void tx_sector_free(TxSector* s)
{
  free(s->objects);
  free(s->maps);
  free(s->ground_movement_manager.v);
  s->objects = NULL;
  s->objects_n = s->objects_cap = 0;
  s->maps = NULL;
  s->maps_n = s->maps_cap = 0;
  s->ground_movement_manager.v = NULL;
  s->ground_movement_manager.n = s->ground_movement_manager.cap = 0;
}

/* CollisionSystem::add */
TX_FN void tx_sector_add(TxSector* s, TxObj* object)
{
  object->ground_movement_manager = &s->ground_movement_manager;
  s->objects = (TxObj**)tx__grow(s->objects, &s->objects_cap, s->objects_n + 1, sizeof(TxObj*));
  s->objects[s->objects_n++] = object;
}

/* CollisionSystem::remove
   (m_objects.erase(std::find(...)) is undefined in the C++ when the object is
   not in the list; here nothing is erased in that case.) */
TX_FN void tx_sector_remove(TxSector* s, TxObj* object)
{
  int i, j;
  for (i = 0; i < s->objects_n; ++i) {
    if (s->objects[i] == object) {
      memmove(&s->objects[i], &s->objects[i + 1], (size_t)(s->objects_n - i - 1) * sizeof(TxObj*));
      --s->objects_n;
      break;
    }
  }

  /* FIXME: This is a patch. A better way of fixing this is coming. */
  for (i = 0; i < s->objects_n; ++i) {
    tx_obj_notify_object_removal(s->objects[i], object);
  }
  for (i = 0; i < s->maps_n; ++i) {
    tx_map_notify_object_removal(s->maps[i], object);
  }

  /* ADDED: the C++ leaves the removed object as a key of
     m_movements_per_target until the next apply_all_ground_movement, which
     then writes to it. Drop those entries so that the host may free it. */
  for (i = 0, j = 0; i < s->ground_movement_manager.n; ++i) {
    if (s->ground_movement_manager.v[i].target != object)
      s->ground_movement_manager.v[j++] = s->ground_movement_manager.v[i];
  }
  s->ground_movement_manager.n = j;
}

/* GameObjectManager::m_solid_tilemaps.push_back(tm), and
   Sector::before_object_add: tilemap->set_ground_movement_manager(...) */
TX_FN void tx_sector_add_map(TxSector* s, TxMap* map)
{
  map->ground_movement_manager = &s->ground_movement_manager;
  s->maps = (TxMap**)tx__grow(s->maps, &s->maps_cap, s->maps_n + 1, sizeof(TxMap*));
  s->maps[s->maps_n++] = map;
}

/* GameObjectManager::m_solid_tilemaps.erase(it) */
TX_FN void tx_sector_remove_map(TxSector* s, TxMap* map)
{
  int i;
  for (i = 0; i < s->maps_n; ++i) {
    if (s->maps[i] == map) {
      memmove(&s->maps[i], &s->maps[i + 1], (size_t)(s->maps_n - i - 1) * sizeof(TxMap*));
      --s->maps_n;
      return;
    }
  }
}

/* NOT TRANSLATED (NT1): CollisionSystem::draw(DrawingContext& context) */

/* collision_system.cpp, anonymous namespace: check_collisions
   The rectangles are pointers because the C++ takes references to m_dest of
   the two objects, and reads them again after the collides() callbacks. */
TX_FN TxConstraints tx__check_collisions(const TxSector* s, TxVec obj_movement,
                                         const TxRect* moving_obj_rect, const TxRect* other_obj_rect,
                                         TxObj* moving_object, TxObj* other_object)
{
  TxConstraints constraints = tx_constraints();
  TxHit dummy;
  float itop, ibottom, ileft, iright;
  bool shiftout;

  /* Slightly growing the static object's rectangle to detect a
     collision not only when they overlap, but also when they're
     adjacent or at least extremely close. */
  const TxRect grown_other_obj_rect = tx_rect_grown(other_obj_rect, TX_EPSILON);

  if (!tx_rect_overlaps(moving_obj_rect, &grown_other_obj_rect))
    return constraints;

  dummy = tx_hit();

  if (other_object != NULL && moving_object != NULL && !tx__collides(s, other_object, moving_object, &dummy))
    return constraints;
  if (moving_object != NULL && other_object != NULL && !tx__collides(s, moving_object, other_object, &dummy))
    return constraints;

  /* Calculate intersection. */
  itop = tx_rect_bottom(moving_obj_rect) - tx_rect_top(&grown_other_obj_rect);
  ibottom = tx_rect_bottom(&grown_other_obj_rect) - tx_rect_top(moving_obj_rect);
  ileft = tx_rect_right(moving_obj_rect) - tx_rect_left(&grown_other_obj_rect);
  iright = tx_rect_right(&grown_other_obj_rect) - tx_rect_left(moving_obj_rect);

  shiftout = false;

  if (!other_object || !other_object->unisolid)
  {
    if (fabsf(obj_movement.y) > fabsf(obj_movement.x)) {
      if (ileft < TX_SHIFT_DELTA) {
        tx_constrain_right(&constraints, tx_rect_left(&grown_other_obj_rect));
        shiftout = true;
      }
      else if (iright < TX_SHIFT_DELTA) {
        tx_constrain_left(&constraints, tx_rect_right(&grown_other_obj_rect));
        shiftout = true;
      }
    }
    else {
      /* Shiftout bottom/top. */
      if (itop < TX_SHIFT_DELTA) {
        tx_constrain_bottom(&constraints, tx_rect_top(&grown_other_obj_rect));
        shiftout = true;
      }
      else if (ibottom < TX_SHIFT_DELTA) {
        tx_constrain_top(&constraints, tx_rect_bottom(&grown_other_obj_rect));
        shiftout = true;
      }
    }
  }

  if (!shiftout)
  {
    if (other_object && other_object->unisolid)
    {
      /* Constrain only on fall on top of the unisolid object. */
      if (tx_rect_bottom(moving_obj_rect) - obj_movement.y <= tx_rect_top(&grown_other_obj_rect) - (other_object->movement.y - 5.f))
      {
        tx_constrain_bottom(&constraints, tx_rect_top(other_obj_rect));
        constraints.hit.bottom = true;
      }
    }
    else
    {
      const float vert_penetration = tx_minf(itop, ibottom);
      const float horiz_penetration = tx_minf(ileft, iright);

      if (vert_penetration < horiz_penetration)
      {
        if (itop < ibottom)
        {
          tx_constrain_bottom(&constraints, tx_rect_top(&grown_other_obj_rect));
          constraints.hit.bottom = true;
        }
        else
        {
          tx_constrain_top(&constraints, tx_rect_bottom(&grown_other_obj_rect));
          constraints.hit.top = true;
        }
      }
      else
      {
        if (ileft < iright)
        {
          tx_constrain_right(&constraints, tx_rect_left(&grown_other_obj_rect));
          constraints.hit.right = true;
        }
        else
        {
          tx_constrain_left(&constraints, tx_rect_right(&grown_other_obj_rect));
          constraints.hit.left = true;
        }
      }
    }
  }

  if (other_object && moving_object)
  {
    TxHit hit = constraints.hit;
    int response;
    tx__collision(s, moving_object, other_object, &hit);
    tx_hit_swap(&hit);
    response = tx__collision(s, other_object, moving_object, &hit);
    if (response == TX_ABORT_MOVE)
      return tx_constraints();
  }

  return constraints;
}

/* CollisionSystem::collision_tilemap
   `dest` is a pointer for the same reason as in tx__check_collisions (it is
   the m_dest of `object`). */
TX_FN void tx__collision_tilemap(const TxSector* s, TxConstraints* constraints,
                                 TxVec movement, const TxRect* dest,
                                 TxObj* object)
{
  /* calculate rectangle where the object will move */
  const float x1 = tx_rect_left(dest);
  const float x2 = tx_rect_right(dest);
  const float y1 = tx_rect_top(dest);
  const float y2 = tx_rect_bottom(dest);
  int mi, x, y;

  for (mi = 0; mi < s->maps_n; ++mi)
  {
    TxMap* solids = s->maps[mi];

    /* Test with all tiles in this rectangle. */
    const TxRect test_rect = tx_rect(x1, y1, x2, y2);
    const TxIRect test_tiles = tx_map_get_tiles_overlapping(solids, &test_rect);

    bool hits_bottom = false;

    for (x = test_tiles.left; x < test_tiles.right; ++x)
    {
      for (y = test_tiles.top; y < test_tiles.bottom; ++y)
      {
        const uint32_t tile_id = tx_map_get_tile_id(solids, x, y);
        const uint32_t tile_attrs = tx_tile_attributes(&s->tiles, tile_id);
        const int tile_data = tx_tile_data(&s->tiles, tile_id);

        /* Skip non-solid tiles. */
        if (tx_tile_is_solid(tile_attrs))
        {
          TxRect tile_bbox = tx_map_get_tile_bbox(solids, x, y);
          bool is_relatively_solid = true;

          /* If the tile is a unisolid tile, the "is_solid()" function above
           * didn't do a thorough check. Calculate the position and (relative)
           * movement of the object and determine whether or not the tile is
           * solid with regard to those parameters. */
          if (tx_tile_is_unisolid(tile_attrs))
          {
            TxVec relative_movement = tx_vec_sub(movement,
              tx_map_get_movement(solids, /* actual = */ true));

            if (!tx_tile_is_solid_at(tile_attrs, tile_data, &tile_bbox, &object->bbox, relative_movement))
              is_relatively_solid = false;
          }

          if (is_relatively_solid)
          {
            if (tx_tile_is_slope(tile_attrs)) { /* Slope tile. */
              TxAATriangle triangle;
              int slope_data = tile_data;
              bool triangle_hits_bottom;
              if (solids->flip & TX_VERTICAL_FLIP)
                slope_data = tx_aatriangle_vertical_flip(slope_data);
              triangle.bbox = tile_bbox;
              triangle.dir = slope_data;

              triangle_hits_bottom = false;
              tx_rectangle_aatriangle_ex(constraints, dest, &triangle, &triangle_hits_bottom, object);
              hits_bottom |= triangle_hits_bottom;
            }
            else { /* Normal rectangular tile. */
              TxConstraints new_constraints = tx__check_collisions(s, movement, dest, &tile_bbox, NULL, NULL);
              hits_bottom |= new_constraints.hit.bottom;
              tx_constraints_merge(constraints, &new_constraints);
            }
          }
        }
      }
    }

    if (hits_bottom)
      tx__map_hits_object_bottom(s, solids, object);
  }
}

/* CollisionSystem::collision_tile_attributes */
TX_FN uint32_t tx__collision_tile_attributes(const TxSector* s, const TxRect* dest, TxVec mov)
{
  const float x1 = tx_rect_left(dest);
  const float y1 = tx_rect_top(dest);
  const float x2 = tx_rect_right(dest);
  const float y2 = tx_rect_bottom(dest);
  int mi, x;

  uint32_t result = 0;
  for (mi = 0; mi < s->maps_n; ++mi)
  {
    const TxMap* solids = s->maps[mi];

    /* Test with all tiles in this rectangle. */
    const TxRect test_rect = tx_rect(x1, y1, x2, y2);
    const TxIRect test_tiles = tx_map_get_tiles_overlapping(solids, &test_rect);

    /* For ice (only), add a little fudge to recognize tiles Tux is standing on. */
    const TxRect test_rect_ice = tx_rect(x1, y1, x2, y2 + TX_SHIFT_DELTA);
    const TxIRect test_tiles_ice = tx_map_get_tiles_overlapping(solids, &test_rect_ice);

    for (x = test_tiles.left; x < test_tiles.right; ++x) {
      int y;
      for (y = test_tiles.top; y < test_tiles.bottom; ++y) {
        const uint32_t tile_id = tx_map_get_tile_id(solids, x, y);
        const uint32_t tile_attrs = tx_tile_attributes(&s->tiles, tile_id);
        const int tile_data = tx_tile_data(&s->tiles, tile_id);
        const TxRect tile_bbox = tx_map_get_tile_bbox(solids, x, y);

        if (tx_tile_is_collisionful(tile_attrs, tile_data, &tile_bbox, dest, mov)) {
          result |= tile_attrs;
        }
      }
      for (; y < test_tiles_ice.bottom; ++y) {
        const uint32_t tile_id = tx_map_get_tile_id(solids, x, y);
        const uint32_t tile_attrs = tx_tile_attributes(&s->tiles, tile_id);
        const int tile_data = tx_tile_data(&s->tiles, tile_id);
        const TxRect tile_bbox = tx_map_get_tile_bbox(solids, x, y);
        if (tx_tile_is_collisionful(tile_attrs, tile_data, &tile_bbox, dest, mov)) {
          result |= (tile_attrs & TX_TILE_ICE);
        }
      }
    }
  }

  return result;
}

/* CollisionSystem::get_hit_normal
   Fills the CollisionHit and Normal vector between two intersecting rectangles. */
TX_FN void tx__get_hit_normal(const TxObj* object1, const TxObj* object2,
                              TxHit* hit, TxVec* normal)
{
  const TxRect* r1 = &object1->dest;
  const TxRect* r2 = &object2->dest;

  const float itop = tx_rect_bottom(r1) - tx_rect_top(r2);
  const float ibottom = tx_rect_bottom(r2) - tx_rect_top(r1);
  const float ileft = tx_rect_right(r1) - tx_rect_left(r2);
  const float iright = tx_rect_right(r2) - tx_rect_left(r1);

  const float vert_penetration = tx_minf(itop, ibottom);
  const float horiz_penetration = tx_minf(ileft, iright);

  /* Apply movement only on top collision with an unisolid object. */
  if (object1->unisolid &&
    tx_rect_bottom(r2) - object2->movement.y > tx_rect_top(r1))
    return;
  if (object2->unisolid &&
    tx_rect_bottom(r1) - object1->movement.y > tx_rect_top(r2))
    return;

  if (vert_penetration < horiz_penetration) {
    if (itop < ibottom) {
      hit->bottom = true;
      normal->y = vert_penetration;
    }
    else {
      hit->top = true;
      normal->y = -vert_penetration;
    }
  }
  else {
    if (ileft < iright) {
      hit->right = true;
      normal->x = horiz_penetration;
    }
    else {
      hit->left = true;
      normal->x = -horiz_penetration;
    }
  }
}

/* CollisionSystem::collision_object */
TX_FN void tx__collision_object(const TxSector* s, TxObj* object1, TxObj* object2)
{
  const TxRect* r1;
  const TxRect* r2;
  TxHit hit;

  /* If both objects are moving statics, that means
     their collision callbacks have already been called.
     We don't need to call them again. */
  if (object1->group == TX_COLGROUP_MOVING_STATIC &&
    object2->group == TX_COLGROUP_MOVING_STATIC)
    return;

  r1 = &object1->dest;
  r2 = &object2->dest;

  hit = tx_hit();
  if (tx_rect_overlaps(r1, r2)) {
    TxVec normal = tx_vec(0.0f, 0.0f);
    int response1, response2;
    tx__get_hit_normal(object1, object2, &hit, &normal);

    if (!tx__collides(s, object1, object2, &hit))
      return;
    tx_hit_swap(&hit);
    if (!tx__collides(s, object2, object1, &hit))
      return;
    tx_hit_swap(&hit);

    response1 = tx__collision(s, object1, object2, &hit);
    tx_hit_swap(&hit);
    response2 = tx__collision(s, object2, object1, &hit);
    if (response1 == TX_CONTINUE && response2 == TX_CONTINUE) {
      normal = tx_vec_scale(normal, (0.5f + TX_EPSILON));
      tx_rect_move(&object1->dest, tx_vec_neg(normal));
      tx_rect_move(&object2->dest, normal);
    }
    else if (response1 == TX_CONTINUE && response2 == TX_FORCE_MOVE) {
      normal = tx_vec_scale(normal, (1 + TX_EPSILON));
      tx_rect_move(&object1->dest, tx_vec_neg(normal));
    }
    else if (response1 == TX_FORCE_MOVE && response2 == TX_CONTINUE) {
      normal = tx_vec_scale(normal, (1 + TX_EPSILON));
      tx_rect_move(&object2->dest, normal);
    }
  }
}

/* CollisionSystem::collision_static */
TX_FN void tx__collision_static(TxSector* s, TxConstraints* constraints,
                                TxVec movement, const TxRect* dest,
                                TxObj* object)
{
  int i;
  tx__collision_tilemap(s, constraints, movement, dest, object);

  /* Collision with other (static) objects. */
  for (i = 0; i < s->objects_n; ++i)
  {
    TxObj* static_object = s->objects[i];
    if ((
      static_object->group == TX_COLGROUP_STATIC ||
      static_object->group == TX_COLGROUP_MOVING_STATIC
      ) &&
      static_object->valid &&
      static_object != object)
    {

      TxConstraints new_constraints = tx__check_collisions(s,
        movement, dest, &static_object->dest, object, static_object);

      if (new_constraints.hit.bottom)
        tx__collision_moving_object_bottom(s, static_object, object);
      else if (new_constraints.hit.top)
        tx__collision_moving_object_bottom(s, object, static_object);

      tx_constraints_merge(constraints, &new_constraints);
    }
  }
}

/* CollisionSystem::collision_static_constrains */
TX_FN void tx__collision_static_constrains(TxSector* s, TxObj* object)
{
  const float infinity = TX_INFINITY;

  TxConstraints constraints = tx_constraints();
  const TxVec movement = object->movement;
  TxVec pressure = tx_vec(0, 0);
  TxRect* dest = &object->dest;
  int i;

  for (i = 0; i < 2; ++i) {
    tx__collision_static(s, &constraints, tx_vec(0, movement.y), dest, object);
    if (!tx_constraints_has_constraints(&constraints))
      break;

    /* Apply calculated horizontal constraints. */
    if (constraints.position_bottom < infinity) {
      float height = tx_constraints_get_height(&constraints);
      if (height < tx_rect_height(&object->bbox)) {
        /* We're crushed, but ignore this for now, we'll get this again
           later if we're really crushed or things will solve itself when
           looking at the vertical constraints. */
        pressure.y += tx_rect_height(&object->bbox) - height;
        object->pressure.y = pressure.y;
      }
      else {
        tx_rect_set_bottom(dest, constraints.position_bottom - TX_EPSILON);
        tx_rect_set_top(dest, tx_rect_bottom(dest) - tx_rect_height(&object->bbox));
      }
    }
    else if (constraints.position_top > -infinity) {
      tx_rect_set_top(dest, constraints.position_top + TX_EPSILON);
      tx_rect_set_bottom(dest, tx_rect_top(dest) + tx_rect_height(&object->bbox));
    }
  }

  if (tx_constraints_has_constraints(&constraints)) {
    if (constraints.hit.top || constraints.hit.bottom) {
      constraints.hit.left = false;
      constraints.hit.right = false;
      tx__collision_solid(s, object, &constraints.hit);
    }
  }

  constraints = tx_constraints();
  for (i = 0; i < 2; ++i) {
    float width;
    tx__collision_static(s, &constraints, movement, dest, object);
    if (!tx_constraints_has_constraints(&constraints))
      break;

    /* Apply calculated vertical constraints. */
    width = tx_constraints_get_width(&constraints);

    if (width < infinity) {
      if (width + TX_SHIFT_DELTA < tx_rect_width(&object->bbox)) {
        /* We're crushed, but ignore this for now, we'll get this again
           later if we're really crushed or things will solve itself when
           looking at the horizontal constraints. */
        pressure.x += tx_rect_width(&object->bbox) - width;
        object->pressure.x = pressure.x;
      }
      else {
        float xmid = tx_constraints_get_x_midpoint(&constraints);
        tx_rect_set_left(dest, xmid - tx_rect_width(&object->bbox) / 2);
        tx_rect_set_right(dest, xmid + tx_rect_width(&object->bbox) / 2);
      }
    }
    else if (constraints.position_right < infinity) {
      tx_rect_set_right(dest, constraints.position_right - TX_EPSILON);
      tx_rect_set_left(dest, tx_rect_right(dest) - tx_rect_width(&object->bbox));
    }
    else if (constraints.position_left > -infinity) {
      tx_rect_set_left(dest, constraints.position_left + TX_EPSILON);
      tx_rect_set_right(dest, tx_rect_left(dest) + tx_rect_width(&object->bbox));
    }
  }

  if (tx_constraints_has_constraints(&constraints)) {
    if (constraints.hit.left || constraints.hit.right
      || constraints.hit.top || constraints.hit.bottom
      || constraints.hit.crush)
      tx__collision_solid(s, object, &constraints.hit);
  }

  /* An extra pass to make sure we're not crushed vertically. */
  if (pressure.y > 0) {
    constraints = tx_constraints();
    tx__collision_static(s, &constraints, movement, dest, object);
    if (constraints.position_bottom < infinity) {
      const float height = tx_constraints_get_height(&constraints);

      if (height + TX_SHIFT_DELTA < tx_rect_height(&object->bbox)) {
        TxHit h = tx_hit();
        h.top = true;
        h.bottom = true;
        h.crush = pressure.y > 16;
        tx__collision_solid(s, object, &h);
      }
    }
  }

  /* An extra pass to make sure we're not crushed horizontally. */
  if (pressure.x > 0) {
    constraints = tx_constraints();
    tx__collision_static(s, &constraints, movement, dest, object);
    if (constraints.position_right < infinity) {
      float width = tx_constraints_get_width(&constraints);
      if (width + TX_SHIFT_DELTA < tx_rect_width(&object->bbox)) {
        TxHit h = tx_hit();
        h.top = true;
        h.bottom = true;
        h.left = true;
        h.right = true;
        h.crush = pressure.x > 16;
        tx__collision_solid(s, object, &h);
      }
    }
  }
}

/* CollisionSystem::update */
TX_FN void tx_coll_update(TxSector* s)
{
  int i, i2;

  /* NOT TRANSLATED (NT2):
       if (Editor::is_active()) {
         return;
         // Objects in editor shouldn't collide.
       } */

  tx_gmm_apply_all_ground_movement(&s->ground_movement_manager);

  /* Calculate destination positions of the objects. */
  for (i = 0; i < s->objects_n; ++i)
  {
    TxObj* object = s->objects[i];
    const TxVec* mov = &object->movement;

    /* Make sure movement is never faster than MAX_SPEED. */
    if (tx_vec_length(*mov) > TX_MAX_SPEED) {
      object->movement = tx_vec_scale(tx_vec_normalize(*mov), TX_MAX_SPEED);
    }

    object->dest = object->bbox;
    object->pressure = tx_vec(0, 0);
    tx_rect_move(&object->dest, object->movement);
    tx_obj_clear_bottom_collision_list(object);
  }

  /* Part 1: COLGROUP_MOVING vs COLGROUP_STATIC and tilemap. */
  for (i = 0; i < s->objects_n; ++i) {
    TxObj* object = s->objects[i];
    if ((object->group != TX_COLGROUP_MOVING
      && object->group != TX_COLGROUP_MOVING_STATIC
      && object->group != TX_COLGROUP_MOVING_ONLY_STATIC)
      || !object->valid)
      continue;

    tx__collision_static_constrains(s, object);
  }

  /* Part 2: COLGROUP_MOVING vs tile attributes. */
  for (i = 0; i < s->objects_n; ++i) {
    TxObj* object = s->objects[i];
    uint32_t tile_attributes;
    if ((object->group != TX_COLGROUP_MOVING
      && object->group != TX_COLGROUP_MOVING_STATIC
      && object->group != TX_COLGROUP_MOVING_ONLY_STATIC)
      || !object->valid)
      continue;

    tile_attributes = tx__collision_tile_attributes(s, &object->dest, object->movement);
    if (tile_attributes >= TX_TILE_FIRST_INTERESTING_FLAG) {
      tx__collision_tile(s, object, tile_attributes);
    }
  }

  /* Part 2.5: COLGROUP_MOVING vs COLGROUP_TOUCHABLE. */
  for (i = 0; i < s->objects_n; ++i)
  {
    TxObj* object = s->objects[i];
    if ((object->group != TX_COLGROUP_MOVING
      && object->group != TX_COLGROUP_MOVING_STATIC)
      || !object->valid)
      continue;

    for (i2 = 0; i2 < s->objects_n; ++i2) {
      TxObj* object_2 = s->objects[i2];
      if (object_2->group != TX_COLGROUP_TOUCHABLE
        || !object_2->valid)
        continue;

      if (tx_rect_overlaps(&object->dest, &object_2->dest)) {
        TxVec normal = tx_vec(0.0f, 0.0f);
        TxHit hit = tx_hit();
        tx__get_hit_normal(object, object_2, &hit, &normal);
        if (!tx__collides(s, object, object_2, &hit))
          continue;
        if (!tx__collides(s, object_2, object, &hit))
          continue;

        tx__collision(s, object, object_2, &hit);
        tx__collision(s, object_2, object, &hit);
      }
    }
  }

  /* Part 3: COLGROUP_MOVING vs COLGROUP_MOVING. */
  for (i = 0; i < s->objects_n; ++i)
  {
    TxObj* object = s->objects[i];

    if (!object->valid ||
      (object->group != TX_COLGROUP_MOVING &&
        object->group != TX_COLGROUP_MOVING_STATIC))
      continue;

    for (i2 = i + 1; i2 < s->objects_n; ++i2) {
      TxObj* object_2 = s->objects[i2];
      if ((object_2->group != TX_COLGROUP_MOVING
        && object_2->group != TX_COLGROUP_MOVING_STATIC)
        || !object_2->valid)
        continue;

      tx__collision_object(s, object, object_2);
    }
  }

  /* Apply object movement. */
  for (i = 0; i < s->objects_n; ++i) {
    TxObj* object = s->objects[i];
    object->bbox = object->dest;
    object->movement = tx_vec(0, 0);
  }
}

/* CollisionSystem::is_free_of_tiles
   (C++ defaults: ignoreUnisolid = false, tiletype = Tile::SOLID) */
TX_FN bool tx_is_free_of_tiles(const TxSector* s, const TxRect* rect, bool ignoreUnisolid, uint32_t tiletype)
{
  int mi, x, y;
  for (mi = 0; mi < s->maps_n; ++mi) {
    const TxMap* solids = s->maps[mi];
    /* Test with all tiles in this rectangle. */
    const TxIRect test_tiles = tx_map_get_tiles_overlapping(solids, rect);

    for (x = test_tiles.left; x < test_tiles.right; ++x) {
      for (y = test_tiles.top; y < test_tiles.bottom; ++y) {
        const uint32_t tile_id = tx_map_get_tile_id(solids, x, y);
        const uint32_t tile_attrs = tx_tile_attributes(&s->tiles, tile_id);

        if (!(tile_attrs & tiletype))
          continue;
        if (tx_tile_is_unisolid(tile_attrs) && ignoreUnisolid)
          continue;
        if (tx_tile_is_slope(tile_attrs)) {
          TxAATriangle triangle;
          const TxRect tbbox = tx_map_get_tile_bbox(solids, x, y);
          TxConstraints constraints;
          triangle.bbox = tbbox;
          triangle.dir = tx_tile_data(&s->tiles, tile_id);
          constraints = tx_constraints();
          if (!tx_rectangle_aatriangle(&constraints, rect, &triangle))
            continue;
        }
        /* We have a solid tile that overlaps the given rectangle. */
        return false;
      }
    }
  }

  return true;
}

/* CollisionSystem::is_free_of_statics (C++ default: tiletype = Tile::SOLID) */
TX_FN bool tx_is_free_of_statics(const TxSector* s, const TxRect* rect, const TxObj* ignore_object,
                                 bool ignoreUnisolid, uint32_t tiletype)
{
  int i;
  if (!tx_is_free_of_tiles(s, rect, ignoreUnisolid, tiletype)) return false;

  for (i = 0; i < s->objects_n; ++i) {
    const TxObj* object = s->objects[i];
    if (object == ignore_object) continue;
    if (!object->valid) continue;
    if (object->group == TX_COLGROUP_STATIC) {
      if (tx_rect_overlaps(rect, &object->bbox)) return false;
    }
  }

  return true;
}

/* CollisionSystem::is_free_of_movingstatics (C++ default: ignore_unisolid = false) */
TX_FN bool tx_is_free_of_movingstatics(const TxSector* s, const TxRect* rect, const TxObj* ignore_object,
                                       bool ignore_unisolid)
{
  int i;
  if (!tx_is_free_of_tiles(s, rect, ignore_unisolid, TX_TILE_SOLID)) return false;

  for (i = 0; i < s->objects_n; ++i) {
    const TxObj* object = s->objects[i];
    if (object == ignore_object) continue;
    if (!object->valid) continue;
    if (object->unisolid && ignore_unisolid) continue;
    if ((object->group == TX_COLGROUP_MOVING)
      || (object->group == TX_COLGROUP_MOVING_STATIC)
      || (object->group == TX_COLGROUP_STATIC)) {
      if (tx_rect_overlaps(rect, &object->bbox)) return false;
    }
  }

  return true;
}

/* CollisionSystem::is_free_of_specifically_movingstatics */
TX_FN bool tx_is_free_of_specifically_movingstatics(const TxSector* s, const TxRect* rect, const TxObj* ignore_object)
{
  int i;
  for (i = 0; i < s->objects_n; ++i) {
    const TxObj* object = s->objects[i];
    if (object == ignore_object) continue;
    if (!object->valid) continue;
    if ((object->group == TX_COLGROUP_MOVING_STATIC)
      && (tx_rect_overlaps(rect, &object->bbox)))
      return false;
  }

  return true;
}

/* CollisionSystem::RaycastResult() */
TX_FN TxRaycast tx__raycast(void)
{
  TxRaycast r;
  r.is_valid = false;
  r.kind = TX_RAY_TILE;
  r.tile_id = 0;
  r.obj = NULL;
  r.box = tx_rect_zero();
  return r;
}

/* CollisionSystem::get_first_line_intersection
   (C++ defaults: ignore = IGNORE_NONE, ignore_object = nullptr) */
TX_FN TxRaycast tx_get_first_line_intersection(const TxSector* s, TxVec line_start, TxVec line_end,
                                               int ignore, const TxObj* ignore_object)
{
  TxRaycast tileresult = tx__raycast();
  TxRaycast objresult;
  int i, mi;

  if (ignore != TX_IGNORE_TILES)
  {
    /* Check if no tile is in the way. */

    const float lsx = line_start.x;
    const float lex = line_end.x;
    const float lsy = line_start.y;
    const float ley = line_end.y;

    const bool left = lsx > lex;
    const bool up   = lsy > ley;
    float test_x, test_y;

    for (test_x = lsx; left ? test_x >= lex : test_x <= lex; test_x += left ? -16.f : 16.f) {
      for (test_y = lsy; up ? test_y >= ley : test_y <= ley; test_y += up   ? -16.f : 16.f) {
        for (mi = 0; mi < s->maps_n; ++mi) {
          const TxMap* solids = s->maps[mi];
          const TxVec test_vector = tx_vec(test_x, test_y);
          uint32_t tile_id;
          if (tx_map_is_outside_bounds(solids, test_vector))
          {
            continue;
          }

          tile_id = tx_map_get_tile_id_at(solids, test_vector);

          /* FIXME: check collision with slope tiles */
          if ((tx_tile_attributes(&s->tiles, tile_id) & TX_TILE_SOLID))
          {
            tileresult.is_valid = true;
            tileresult.kind = TX_RAY_TILE;
            tileresult.tile_id = tile_id;
            /* (the C++ does not subtract the tilemap offset here) */
            tileresult.box = tx_map_get_tile_bbox(solids, (int)(test_vector.x / 32.f),
                                                  (int)(test_vector.y / 32.f));
            goto finish_tiles;
          }
        }
      }
    }
  }

finish_tiles:
  if (ignore == TX_IGNORE_OBJECTS)
    return tileresult;

  objresult = tx__raycast();

  /* Check if no object is in the way. */
  for (i = 0; i < s->objects_n; ++i) {
    TxObj* object = s->objects[i];
    if (object == ignore_object) continue;
    if (!object->valid) continue;
    if ((object->group == TX_COLGROUP_MOVING)
      || (object->group == TX_COLGROUP_MOVING_STATIC)
      || (object->group == TX_COLGROUP_STATIC))
    {
      if (tx_intersects_line(&object->bbox, line_start, line_end))
      {
        objresult.is_valid = true;
        objresult.kind = TX_RAY_OBJECT;
        objresult.obj = object;
        objresult.box = object->bbox;
        break;
      }
    }
  }

  if (ignore == TX_IGNORE_TILES)
    return objresult;

  if (tileresult.is_valid && objresult.is_valid)
  {
    float tiledist = tx_vec_distance(tx_rect_middle(&tileresult.box), line_start);
    float objdist = tx_vec_distance(tx_rect_middle(&objresult.box), line_start);
    return tiledist < objdist ? tileresult : objresult;
  }
  else if (tileresult.is_valid)
    return tileresult;
  else if (objresult.is_valid)
    return objresult;
  else
  {
    return tx__raycast();
  }
}

/* CollisionSystem::free_line_of_sight */
TX_FN bool tx_free_line_of_sight(const TxSector* s, TxVec line_start, TxVec line_end,
                                 bool ignore_objects, const TxObj* ignore_object)
{
  int ignore = (ignore_objects ? TX_IGNORE_OBJECTS : TX_IGNORE_NONE);
  return !tx_get_first_line_intersection(s, line_start, line_end, ignore, ignore_object).is_valid;
}

/* CollisionSystem::get_nearby_objects
   The C++ returns a std::vector. Here: writes at most `out_cap` objects to
   `out` (in m_objects order) and returns how many objects match in total, so
   a result greater than out_cap means the buffer was too small. `out` may be
   null when out_cap is 0. */
TX_FN int tx_get_nearby_objects(const TxSector* s, TxVec center, float max_distance,
                                TxObj** out, int out_cap)
{
  int i, n = 0;

  for (i = 0; i < s->objects_n; ++i) {
    TxObj* object = s->objects[i];
    float distance = tx_rect_distance(&object->bbox, center);
    if (distance <= max_distance) {
      if (n < out_cap) out[n] = object;
      ++n;
    }
  }

  return n;
}

#endif /* TX_COLL_H */
