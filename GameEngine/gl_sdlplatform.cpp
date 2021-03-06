
// TODO rename this to just be "sdl_platform"
// TODO modify includes to allow for different platforms/renderers

#define DEBUG_BUILD 1


// NOTE : These includes are OSX specific.
// gl3.h is included instead of the alternatives because 
// it includes only the core functionality and gives compiler
// errors for depreciated functions.

#include <SDL.h>
#include <OpenGL/gl3.h>

//#include <OpenGL/gl.h>
//#include <OpenGL/glu.h>
//#include <OpenGL/glext.h>
//#include <SDL_opengl.h>

#include <common.h>
#include <vector_math.h>

#include "vmath.cpp"
#include "sdl_thread.cpp"

#define PUSH_ALLOCATOR_MULTITHREADED
#include "push_allocator.h"

#include "heap_allocator.h"

#include "pixel.h"

#include "game.h"

struct PlatformWindow {
  RenderBuffer *render_buffer;
  SDL_Window *window;
  SDL_GLContext glcontext;
};


#define NUM_THREADS 4
#include "debug_system.cpp"



// NOTE : This is currently the only shared header containing non-static functions in this project.
#include "custom_stb.h"

#include "unix_file_io.h"

#include "glrenderer.cpp"
#include "assets.cpp"
#include "sdlgl_init.cpp"


#include "entity.cpp"
#include "game.cpp"

// TODO fix resizing

// TODO actually use these or something
#define MAX_SCREEN_WIDTH 1800
#define MAX_SCREEN_HEIGHT 800

static inline ButtonType keycode_to_button_type(SDL_Keycode keycode) {
  switch (keycode) {
    case SDLK_1 :
      return BUTTON_DEBUG_DISPLAY_TOGGLE;
    case SDLK_2 :
      return BUTTON_DEBUG_CAMERA_TOGGLE;
    case SDLK_3 :
      return BUTTON_DEBUG_ANIMATION_TOGGLE;
    case SDLK_MINUS :
      return BUTTON_DEBUG_CAMERA_OUT;
    case SDLK_EQUALS :
      return BUTTON_DEBUG_CAMERA_IN;
    

    case SDLK_a :
      return BUTTON_LEFT;
    case SDLK_s :
      return BUTTON_DOWN;
    case SDLK_d :
      return BUTTON_RIGHT;
    case SDLK_w :
      return BUTTON_UP;

    case SDLK_h :
      return BUTTON_DEBUG_CAMERA_LEFT;
    case SDLK_j :
      return BUTTON_DEBUG_CAMERA_DOWN;
    case SDLK_l :
      return BUTTON_DEBUG_CAMERA_RIGHT;
    case SDLK_k :
      return BUTTON_DEBUG_CAMERA_UP;
    case SDLK_i :
      return BUTTON_DEBUG_CAMERA_TILT_UP;
    case SDLK_u :
      return BUTTON_DEBUG_CAMERA_TILT_DOWN;

    case SDLK_SPACE :
      return BUTTON_NONE;
    case SDLK_ESCAPE :
      return BUTTON_QUIT;
    case SDLK_LSHIFT :
      return BUTTON_NONE;
    case SDLK_RSHIFT :
      return BUTTON_NONE;


    case SDLK_LEFT :
      return BUTTON_LEFT;
    case SDLK_RIGHT :
      return BUTTON_RIGHT;
    case SDLK_DOWN :
      return BUTTON_DOWN;
    case SDLK_UP :
      return BUTTON_UP;
  }
  //assert(false);
  return BUTTON_NONE;
}

static inline void set_button_state(ControllerState *controller, ButtonType idx, ButtonEventType event_type) {
  if (idx == BUTTON_NONE) return;
  assert(BUTTON_MAX == count_of(controller->buttons));

  if (idx > BUTTON_MAX) return;
  if (controller->buttons[idx].first_press == NO_EVENTS) {
    controller->buttons[idx].first_press = event_type;
  }
  controller->buttons[idx].last_press = event_type;
  controller->buttons[idx].num_transitions++;
}

static inline ButtonType mouse_button_to_button_type(uint8_t button) {
  switch (button) {
    case SDL_BUTTON_LEFT :
      return BUTTON_MOUSE_LEFT;
    case SDL_BUTTON_RIGHT :
      return BUTTON_MOUSE_RIGHT;
    case SDL_BUTTON_MIDDLE :
      return BUTTON_MOUSE_MIDDLE;
    default :
      return BUTTON_NONE;
  }
  assert(false);
  return BUTTON_NONE;
}


int main(int argc, char ** argv){


  //
  // Initialization ---
  //

  // TODO move this to after gl creation and create new context in each thread so that
  //      we can asynchronously download textures to the gpu.
  //      
  //      edit : Or maybe not, since apparently that doesn't work very well? OpenGL sucks.
  WorkQueue work_queue;
  auto queue = &work_queue;
  int num_threads_total = NUM_THREADS;
  u32 thread_ids[4];
  int count = init_worker_threads(queue, num_threads_total - 1, thread_ids + 1);
  assert(count == num_threads_total - 1);
  thread_ids[0] = SDL_ThreadID();

  GameMemory game_memory = {};
  RenderBuffer render_buffer = {};

  // TODO just use PushAllocators and new_push_allocator?
  game_memory.permanent_size = 4096 * 2048;
  game_memory.permanent_store = calloc(game_memory.permanent_size, 1);
  game_memory.temporary_size = 4096 * 256;
  game_memory.temporary_store = calloc(game_memory.temporary_size, 1);

  if (!game_memory.permanent_store) return 1;
  if (!game_memory.temporary_store) return 1;

  init_debug_global_memory(thread_ids, &game_memory, &render_buffer);
  SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

  // TODO automatically scale to different resolutions
  int width = 800;
  int height = 600;
  //int width = 1024;
  //int height = 768;

  auto render_info = init_screen(&render_buffer, width, height);
  if (!render_info.window) return 1;

  bool window_open = true;
  bool size_changed = false;
  uint8_t ticks = 0;

  ControllerState old_controller_state = {};
  u32 previous_time = SDL_GetTicks();

  init_game_state(game_memory, queue, &render_buffer);

  //
  // Main Loop ---
  //

  while (window_open) {

    TIMED_BLOCK(Updating_controller_state);

    ControllerState controller_state = {};
    for (int i = 0; i < BUTTON_MAX; i++) {
      // TODO maybe a speed optimization here, need to check disassembly
      bool held;
      if (old_controller_state.buttons[i].last_press == PRESS_EVENT) held = true;
      else if (old_controller_state.buttons[i].last_press == RELEASE_EVENT) held = false;
      else held = old_controller_state.buttons[i].held;
      controller_state.buttons[i].held = held;
    }

    END_TIMED_BLOCK(Updating_controller_state);

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {

      switch (ev.type) {
        case SDL_KEYDOWN : {
          SDL_Keycode code = ev.key.keysym.sym;
          ButtonType b = keycode_to_button_type(code);
          set_button_state(&controller_state, b, PRESS_EVENT);
        } break;

        case SDL_KEYUP : {
          SDL_Keycode code = ev.key.keysym.sym;
          ButtonType b = keycode_to_button_type(code);
          set_button_state(&controller_state, b, RELEASE_EVENT);
        } break;

        case SDL_MOUSEBUTTONDOWN : {
          ButtonType b = mouse_button_to_button_type(ev.button.button);
          set_button_state(&controller_state, b, PRESS_EVENT);
        } break;

        case SDL_MOUSEBUTTONUP : {
          ButtonType b = mouse_button_to_button_type(ev.button.button);
          set_button_state(&controller_state, b, RELEASE_EVENT);
        } break;

        case SDL_MOUSEMOTION : {
          // TODO take the motion of the mouse as well
          controller_state.pointer.x = ev.motion.x;
          controller_state.pointer.y = ev.motion.y;
          controller_state.pointer_moved = true;
        } break;

        case SDL_QUIT : {
          window_open = false;
        } break;

        case SDL_WINDOWEVENT : {
          switch (ev.window.event) {

            case SDL_WINDOWEVENT_RESIZED : {
              width = ev.window.data1;
              height = ev.window.data2;
              size_changed = true;
            } break;

            case SDL_WINDOWEVENT_CLOSE : {
              window_open = false;
            } break;
          }
        } break;
      }
    }

    if (!window_open) break;
    
    if(size_changed) {
      size_changed = false;
      // TODO resize image
      /*
      window_buffer_size = width * height * pixel_bytes;
      buffer  = (uint8_t *) malloc(window_buffer_size);
      assert(buffer);
      
      x_window_buffer = XCreateImage(display, visual_info.visual, 
                                     visual_info.depth,
                                     ZPixmap, 0, (char *) buffer, width, height,
                                     pixel_bits, 0);
                                     */
    }

    GameInput game_input = {};

    game_input.delta_t = (double) SDL_GetTicks() - (double) previous_time;
    game_input.delta_t /= 1000.0f;
    game_input.delta_t = min(game_input.delta_t, 1.0f); // Caps to 1 second
    game_input.ticks = ticks;
    u32 current_time = SDL_GetTicks();
    //u32 time_passed = current_time - previous_time;
    previous_time = current_time;

    game_input.render_buffer = &render_buffer;
    game_input.work_queue = queue;
    game_input.controller = controller_state;

    window_open = update_and_render(game_memory, game_input);
    if (!window_open) break;

#if DEBUG_BUILD
    push_debug_records();
#endif

    display_buffer(render_info);
    clear(&render_buffer);

    old_controller_state = controller_state;
    ticks++;
  }

  SDL_Quit();

  return EXIT_SUCCESS;
}

// NOTE: This must happen after all TIMED_ calls and includes :

static inline u32 get_max_counter() {
  return __COUNTER__;
}

