
// TODO rename this to just be "sdl_platform"
// TODO modify includes to allow for different platforms/renderers

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

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <stdalign.h>
#include <cstring>
#include <cmath>

#include "debug_system.cpp"

#include "vmath.cpp"
#include "utilities.cpp"

// Fonts are in /Library/Fonts/ on osx
#define STB_TRUETYPE_IMPLEMENTATION 
#include "stb_truetype.h"

#include "game.h"
#include "custom_stb_image.h"


// Needed for file io :
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

// TODO move this out somewhere maybe? Or at least pre-define it so that it doesn't need to be inlined up here.
static uint8_t *read_entire_file(const char *filename, PushAllocator *allocator, int alignment = 8) {
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    assert(!"File not found.");
    return NULL;
  }

  struct stat stat_buf;
  int error = fstat(fd, &stat_buf);
  if (error < 0) {
    assert(!"File stat not found.");
    return NULL;
  }

  auto size = stat_buf.st_size;
  auto buffer = alloc_size(allocator, size, alignment);
  if (!buffer) {
    assert(!"Failed to allocate file.");
    return NULL;
  }

  size_t bytes_read = 0;
  while (bytes_read < size) {
    bytes_read += read(fd, buffer, size - bytes_read);
    if (bytes_read <= 0) {
      assert(!"Error on read.");
      // NOTE : The memory is lost here, so only use temporary allocators 
      // to store file data from this function
      return NULL;
    }
  }

  return (uint8_t *) buffer;
}

#include "glrenderer.cpp"
#include "assets.cpp"
#include "sdlgl_init.cpp"


#include "entity.cpp"
#include "game.cpp"

// TODO fix resizing

// TODO actually use these
#define MAX_SCREEN_WIDTH 1800
#define MAX_SCREEN_HEIGHT 800

static inline ButtonType keycode_to_button_type(SDL_Keycode keycode) {
  switch (keycode) {
    case SDLK_a :
      return BUTTON_LEFT;
    case SDLK_s :
      return BUTTON_DOWN;
    case SDLK_d :
      return BUTTON_RIGHT;
    case SDLK_w :
      return BUTTON_UP;

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
  // Initialization :
  //

  init_debug_global_memory();

  // TODO move this to after gl creation and create new context in each thread so that
  //      we can asynchronously download textures to the gpu
  WorkQueue work_queue;
  auto queue = &work_queue;
  int num_threads_total = 4;
  int count = init_worker_threads(queue, num_threads_total - 1);
  assert(count == num_threads_total - 1);

  // TODO automatically scale to different resolutions
  int width = 800;
  int height = 600;
  //int width = 1024;
  //int height = 768;

  auto render_info = init_screen(width, height);
  if (!render_info.window) return 1;

  bool window_open = true;
  bool size_changed = false;
  uint8_t ticks = 0;

  GameMemory game_memory = {};
  game_memory.permanent_size = 4096 * 2048;
  game_memory.permanent_store = calloc(game_memory.permanent_size, 1);
  game_memory.temporary_size = 4096 * 256;
  game_memory.temporary_store = calloc(game_memory.temporary_size, 1);
  ControllerState old_controller_state = {};
  uint32_t previous_time = SDL_GetTicks();

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
          controller_state.pointer_x = ev.motion.x;
          controller_state.pointer_y = ev.motion.y;
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

    controller_state.delta_t = (double) SDL_GetTicks() - (double) previous_time;
    controller_state.delta_t /= 1000.0f;
    controller_state.ticks = ticks;
    uint32_t current_time = SDL_GetTicks();
    //uint32_t time_passed = current_time - previous_time;
    previous_time = current_time;

    window_open = update_and_render(game_memory, 
                                    &render_info.render_buffer, 
                                    queue,
                                    controller_state);
    if (!window_open) break;
    game_memory.initialized = true;

    // TODO add debug mode toggle
    push_debug_records();

    display_buffer(render_info);
    free(&render_info.render_buffer);

    old_controller_state = controller_state;
    ticks++;
  }

  SDL_Quit();

  return EXIT_SUCCESS;
}

// NOTE: This must happen after all TIMED_ calls and includes :

static inline uint32_t get_max_counter() {
  return __COUNTER__;
}

