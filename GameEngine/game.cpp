#ifndef _GAME_CPP_
#define _GAME_CPP_

// NOTE : All these todos are kind of old...

// TODO test circle collision somehow

// TODO Convert shape code to do the following :
// 1. Combine shapes using a minkowski sum
// 2. Use indirection so that shapes can have arbitrary meshes
// 3. Base each shape around a center point that can be easily updated

// TODO consider using geometric search to handle final position result
// TODO figure out why particles stick when they are going fast enough (bounce factor 1.5)
// TODO implement SOLID_EXTERIOR for entities? (Maybe not, not sure this makes sense anymore)
// TODO build a geometric tree for collision considerations
// TODO implement sim-regions
// TODO make entity processing somewhat multithreaded
// TODO add different rooms/camera movement


// TODO make some accessor functions for the controller. This stuff is a mess.
static bool apply_input(GameState *g, ControllerState controller) {
  TIMED_FUNCTION();

  auto buttons = controller.buttons;
  auto player_color = V4{0.2, 0.2, 0.2, 1};

  int PLAYER_FLASH_INTENSITY = 4;

  g->player_direction = {};

  auto b = buttons + BUTTON_LEFT;
  if (b->held || b->first_press == PRESS_EVENT) {
    g->player_direction.x -= 1;
    player_color.r += 0x10 * PLAYER_FLASH_INTENSITY;
    player_color.g += 0x10 * PLAYER_FLASH_INTENSITY;
  }

  b = buttons + BUTTON_RIGHT;
  if (b->held || b->first_press == PRESS_EVENT) {
    g->player_direction.x += 1;
    player_color.b += 0x20 * PLAYER_FLASH_INTENSITY;
  }
  
  b = buttons + BUTTON_UP;
  if (b->held || b->first_press == PRESS_EVENT) {
    g->player_direction.y += 1;
    player_color.r += 0x20 * PLAYER_FLASH_INTENSITY;
  }

  b = buttons + BUTTON_DOWN;
  if (b->held || b->first_press == PRESS_EVENT) {
    g->player_direction.y -= 1;
    player_color.g += 0x10 * PLAYER_FLASH_INTENSITY;
    player_color.b += 0x10 * PLAYER_FLASH_INTENSITY;
  }

  g->player_direction = normalize(g->player_direction);

  b = buttons + BUTTON_QUIT;
  if (b->first_press == 1) {
    return false;
  }

  // FIXME Projectiles are not shooting directly at the pointer, probably
  // because of screenspace/worldspace conversions
  if (controller.pointer_moved) {
    V2 pointer_p = controller.pointer;
    pointer_p.y = g->camera.screen_height - pointer_p.y;
    pointer_p += vec2(0.5);

    pointer_p.x /= g->camera.screen_width;
    pointer_p.y /= g->camera.screen_height;

    g->pointer_screen_p = pointer_p;
  }

  b = buttons + BUTTON_MOUSE_LEFT;
  if (b->held || b->first_press == PRESS_EVENT) {
    g->cursor_color = V4{1,1,0,1};

    if (b->first_press == PRESS_EVENT) {
      g->shooting++;
    }
  } else {
    g->cursor_color = V4{1,1,1,1};
  }

  b = buttons + BUTTON_MOUSE_RIGHT;
  if (b->held || b->first_press == PRESS_EVENT) {
    g->cursor_color -= V4{0,1,0,0};
    g->shooting++;
  } 

  b = buttons + BUTTON_DEBUG_DISPLAY_TOGGLE;
  if (b->first_press == PRESS_EVENT) {
    debug_global_memory.display_records = !debug_global_memory.display_records;
  } 

  b = buttons + BUTTON_DEBUG_CAMERA_TOGGLE;
  if (b->first_press == PRESS_EVENT) {
    debug_global_memory.camera_mode = !debug_global_memory.camera_mode;
  } 

  b = buttons + BUTTON_DEBUG_ANIMATION_TOGGLE;
  if (b->first_press == PRESS_EVENT) {
    debug_global_memory.animation_mode = !debug_global_memory.animation_mode;
  } 


  if (debug_global_memory.camera_mode) {
    float camera_speed = 0.2;
    float zoom_speed = 0.1;
    b = buttons + BUTTON_DEBUG_CAMERA_LEFT;
    if (b->held || b->first_press == PRESS_EVENT) {
      g->camera.p.x -= camera_speed;
      g->camera.target.x -= camera_speed;
    }

    b = buttons + BUTTON_DEBUG_CAMERA_RIGHT;
    if (b->held || b->first_press == PRESS_EVENT) {
      g->camera.p.x += camera_speed;
      g->camera.target.x += camera_speed;
    }
    
    b = buttons + BUTTON_DEBUG_CAMERA_UP;
    if (b->held || b->first_press == PRESS_EVENT) {
      g->camera.p.y += camera_speed;
      g->camera.target.y += camera_speed;
    }

    b = buttons + BUTTON_DEBUG_CAMERA_DOWN;
    if (b->held || b->first_press == PRESS_EVENT) {
      g->camera.p.y -= camera_speed;
      g->camera.target.y -= camera_speed;
    }

    b = buttons + BUTTON_DEBUG_CAMERA_IN;
    if (b->held || b->first_press == PRESS_EVENT) {
      g->camera.p.z -= zoom_speed;
    }

    b = buttons + BUTTON_DEBUG_CAMERA_OUT;
    if (b->held || b->first_press == PRESS_EVENT) {
      g->camera.p.z += zoom_speed;
    }

    float tilt_speed = 0.1;
    b = buttons + BUTTON_DEBUG_CAMERA_TILT_UP;
    if (b->first_press == PRESS_EVENT) {
      g->camera.target.yz += vec2(tilt_speed);
    }

    b = buttons + BUTTON_DEBUG_CAMERA_TILT_DOWN;
    if (b->first_press == PRESS_EVENT) {
      g->camera.target.yz -= vec2(tilt_speed);
    }

    look_at_target(&g->camera);
  }

  g->player_color = player_color;

  //
  // Calculate pointer_world_p :
  //

  // TODO Make this more accurate by better accounting for perspective
  auto pointer_off = g->pointer_screen_p;
  pointer_off -= vec2(0.5);
  pointer_off *= vec2(g->camera.screen_width,g->camera.screen_height) * METERS_PER_PIXEL;

  // TODO FIXME camera.forward is backwards, fix that
  g->pointer_world_p = g->camera.p - g->camera.forward * 10 + g->camera.right * pointer_off.x + g->camera.up * pointer_off.y;

  return true;
}


static void update_physics(GameState *g) {
  PRIORITY_TIMED_FUNCTION(1);

  // TODO this is obviously a pretty janky way to do shooting, figure out something better?
  if (g->shooting) {
    auto player_p = center(g->entities->collision_box);

    auto cursor_dir = normalize((g->pointer_world_p - player_p).xy);
    add_particle(g, player_p.xy, 25 * cursor_dir, 1, 0.2, 1, V4{0.3, 0.3, 0.2, 1});
    g->shooting--;
  }
  
  for (int i = 0; i < g->num_entities; i++) {
    auto e = g->entities + i;
    if (e->flags & ENTITY_MOVING) {
      process_entity(g, e, g->entities, g->num_entities, PHYSICS_FRAME_TIME);

      if(e->flags & ENTITY_TEMPORARY && e->lifetime <= 0) {
        free_entity(g, e);
        i--;
      }
    }
  }
}

static inline void init_game_state(GameMemory memory, WorkQueue *queue, RenderBuffer *render_buffer) {
  GameState *g = (GameState *) memory.permanent_store;

  g->temp_allocator.max_size = memory.temporary_size;
  g->temp_allocator.memory = (uint8_t *) memory.temporary_store;

  g->perm_allocator.max_size = memory.permanent_size;
  g->perm_allocator.memory = (uint8_t *) memory.permanent_store;
  // NOTE the GameState lives at the top of the permanent_store :
  ALLOC_STRUCT(&g->perm_allocator, GameState);

  g->max_entities = MAX_ENTITY_COUNT;
  g->entities = ALLOC_ARRAY(&g->perm_allocator, Entity, MAX_ENTITY_COUNT);
  
  g->width  = render_buffer->screen_width / float(PIXELS_PER_METER);
  g->height = render_buffer->screen_height / float(PIXELS_PER_METER);

  g->camera.screen_width = render_buffer->screen_width;
  g->camera.screen_height = render_buffer->screen_height;
  g->camera.p = V3{g->width / 2, g->height / 2, 10};
  g->camera.target = V3{g->width / 2, g->height / 2, 0.2};
  g->camera.aspect_ratio = g->height / g->width;
  // focal length = 1 / tan(FOV / 2) = 2 (distance to target) / (width of target)
  g->camera.focal_length = 2 * g->camera.p.z / g->width;
  g->camera.near_dist = 1;
  g->camera.far_dist = 15;
  look_at_target(&g->camera);

  render_buffer->camera = &g->camera;

  //
  // INIT TEXTURES ---
  //

  init_assets(g, queue, render_buffer);
  

  //
  // INIT PLAYER ---
  //

  Entity *player = add_player(g, vec3(5,5,0), ANIM_IDLE, DOWN);
  UNUSED(player);

  //
  // INIT WALLS
  //

#if 0
  auto center_square_pos = vec3(g->width - player_size, g->height - player_size, 0) / 2.0f;
  auto center_corner_pos = center_square_pos + vec3(player_size, player_size, 0);
  auto rect = aligned_rect(center_square_pos.xy, center_corner_pos.xy);
  add_wall(g, rect, vec4(0,0,0.9, 0.5));
  auto rect2 = aligned_rect((center_square_pos + vec3(0.1,0,0)).xy, 
                           (center_corner_pos + vec3(0.2,0.3,0)).xy);
  add_wall(g, rect2, vec4(0.5,0.1,0.1, 0.5));
#endif

  add_room(g, aligned_rect(vec2(1,1), vec2(15,11)));
}

static void render_frame_rate_text(GameState *g, float dt) {
  static float old_dt = 16;
  float new_dt = dt;
  dt = (old_dt + new_dt) / 2;
  old_dt = dt;

  V2 text_p = vec2(1,15) / 16;
  char *frame_rate_text = ALLOC_ARRAY(&g->temp_allocator, char, 8);
  sprintf(frame_rate_text, "%.1f ms", (dt * 1000.0f));
  render_shadowed_text_screen_space(g->render_buffer, text_p, frame_rate_text, vec4(1), vec4(0,0,0.2,1), 2, FONT_COURIER_NEW_BOLD_SIZE_30);
}

static void update_entity_animations(GameState *g) {
  float dt = PHYSICS_FRAME_TIME;
  assert(g->num_entities);
  for (u32 i = 0; i < g->num_entities; i++) {
    auto e = g->entities + i;
    float vel_multiplier = length(e->vel.xy) / LINK_RUN_SPEED;
    assert(vel_multiplier >= 0);
    //e->animation_dt += dt * vel_multiplier;
    e->animation_dt += dt;
    if (e->animation_dt > e->animation_duration) e->animation_dt -= e->animation_duration;
  }
}

static void do_animation_mode(GameState *g, GameMemory memory, GameInput game_input) {
  GameState animation_entity_state = *g;
  auto ag = &animation_entity_state;
  int const num_animations = 12;
  static Entity entities[num_animations];
  static int num_entities = 0;

  ag->entities = entities;
  ag->num_entities = num_entities;
  ag->max_entities = num_animations;

  if (!num_entities) {
    V3 line_start = vec3(2,9,0);
    V3 cursor = line_start;
    V3 x_offset = vec3(2,0,0);
    V3 y_offset = vec3(0,-2,0);
    add_player(ag, cursor, ANIM_IDLE, DOWN); cursor += x_offset;
    add_player(ag, cursor, ANIM_IDLE, RIGHT); cursor += x_offset;
    add_player(ag, cursor, ANIM_IDLE, UP); cursor += x_offset;
    line_start += y_offset; cursor = line_start;
    add_player(ag, cursor, ANIM_MOVE, DOWN); cursor += x_offset;
    add_player(ag, cursor, ANIM_MOVE, RIGHT); cursor += x_offset;
    add_player(ag, cursor, ANIM_MOVE, UP); cursor += x_offset;

    num_entities = ag->num_entities;
  }

  float delta_t = game_input.delta_t; // in seconds
  if (delta_t == 0) delta_t = 1.0f / 60.0f;

  ag->time_remaining += delta_t;
  while (ag->time_remaining > (PHYSICS_FRAME_TIME / 2.0)) {
    update_entity_animations(ag);
    ag->time_remaining -= PHYSICS_FRAME_TIME;
    ag->game_ticks++;
  }

  auto render_buffer = game_input.render_buffer;
  push_entities(render_buffer, ag);

  render_frame_rate_text(g, game_input.delta_t);
}

// TODO make the main thread only handle opengl stuff and the other threads handle input, physics, filling the RenderBuffer, etc
static bool update_and_render(GameMemory memory, GameInput game_input) {
  TIMED_FUNCTION();

  assert(memory.permanent_size >= sizeof(GameState) + sizeof(Entity) * MAX_ENTITY_COUNT);
  auto queue = game_input.work_queue;
  auto render_buffer = game_input.render_buffer;
  auto controller = game_input.controller;

  GameState *g = (GameState *) memory.permanent_store;
  g->render_buffer = render_buffer;

  clear(&g->temp_allocator);

  if (!apply_input(g, controller)) return false;

  auto assets = &g->assets;
  auto background_texture = get_bitmap(assets, BITMAP_BACKGROUND);
  assert(background_texture);

  if (debug_global_memory.animation_mode) {
    do_animation_mode(g, memory, game_input);
    return true;
  }

  draw_gradient_background(g, queue);

  float delta_t = game_input.delta_t; // in seconds
  if (delta_t == 0) delta_t = 1.0f / 60.0f;
  //printf("delta_t : %f\n", delta_t);

  g->time_remaining += delta_t;
  float time_remaining_init = g->time_remaining;

  //
  // Physics Loop ---
  //

  while (g->time_remaining > (PHYSICS_FRAME_TIME / 2.0)) {
    update_physics(g);
    g->time_remaining -= PHYSICS_FRAME_TIME;
    g->game_ticks++;
    assert(g->time_remaining < time_remaining_init);
  }

  //draw_gradient_background(*background_texture, g->game_ticks);

  auto background_rect = rectangle(aligned_rect(0, 0, g->width, g->height));
#define DRAW_GRADIENT_BACKGROUND 1
#if DRAW_GRADIENT_BACKGROUND
  push_rectangle(render_buffer, background_rect, vec4(1), BITMAP_BACKGROUND, 0, false);
#endif


  auto center_pos = vec2(g->width, g->height) / 2.0f;

  //auto player_color = g->player_color;
  //float square_size = 60.0f * METERS_PER_PIXEL;

  //auto center_square_pos = V2{g->width - square_size, g->height - square_size} / 2.0f;
  push_entities(render_buffer, g);

  // Collision normal :
  auto collision_indicator = rectangle(aligned_rect(g->collision_normal + center_pos, 2.0f * METERS_PER_PIXEL, 2.0f * METERS_PER_PIXEL), 1.1);
  auto white_texture = get_bitmap(&g->assets, BITMAP_WHITE); // TODO TODO I should really change it so that this is done in the renderer
  assert(white_texture);
  push_rectangle(render_buffer, collision_indicator, vec4(0,1,0,1), BITMAP_WHITE);

  // Cursor :
  //auto circ = Circle{g->pointer_position, cursor_size / 2.0f};
  //draw_circle(pixel_buffer, circ, cursor_color);

  render_circle_screen_space(render_buffer, g->pointer_screen_p, 10, vec4(1));



  //push_rectangle(render_buffer, cursor_rect, g->cursor_color, circle_texture->texture_id, circle_normal_map->texture_id);

  render_frame_rate_text(g, game_input.delta_t);
  //push_hud(render_buffer, rectangle(aligned_rect(text_p, 0.1, 0.1)), V4{0,0,1,1}, white_texture->texture_id);

  // TODO move this out to platform layer, texture downloads should be handled asynchronously
  complete_all_work(queue);
  update_texture(assets, BITMAP_BACKGROUND);

  //push_debug_records();

  return true;
}

#endif // _GAME_CPP_
