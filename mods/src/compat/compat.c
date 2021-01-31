#define _GNU_SOURCE

#include <unistd.h>

#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_INCLUDE_ES1
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <X11/extensions/Xfixes.h>

#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>
#include <GLES/gl.h>
#include <X11/Xlib.h>

#include <libreborn/libreborn.h>

#include "../feature/feature.h"
#include "../input/input.h"
#include "../screenshot/screenshot.h"
#include "../init/init.h"

#include "compat.h"

static GLFWwindow *glfw_window;

static int is_server = 0;

// Handle GLFW Error
static void glfw_error(__attribute__((unused)) int error, const char *description) {
    ERR("GLFW Error: %s", description);
}

// Convert GLFW Key To SDL Key
static SDLKey glfw_key_to_sdl_key(int key) {
    switch (key) {
        // Movement
        case GLFW_KEY_W:
            return SDLK_w;
        case GLFW_KEY_A:
            return SDLK_a;
        case GLFW_KEY_S:
            return SDLK_s;
        case GLFW_KEY_D:
            return SDLK_d;
        case GLFW_KEY_SPACE:
            return SDLK_SPACE;
        case GLFW_KEY_LEFT_SHIFT:
            return SDLK_LSHIFT;
        case GLFW_KEY_RIGHT_SHIFT:
            return SDLK_RSHIFT;
        // Inventory
        case GLFW_KEY_E:
            return SDLK_e;
        // Hotbar
        case GLFW_KEY_1:
            return SDLK_1;
        case GLFW_KEY_2:
            return SDLK_2;
        case GLFW_KEY_3:
            return SDLK_3;
        case GLFW_KEY_4:
            return SDLK_4;
        case GLFW_KEY_5:
            return SDLK_5;
        case GLFW_KEY_6:
            return SDLK_6;
        case GLFW_KEY_7:
            return SDLK_7;
        case GLFW_KEY_8:
            return SDLK_8;
        // UI Control
        case GLFW_KEY_ESCAPE:
            return SDLK_ESCAPE;
        case GLFW_KEY_UP:
            return SDLK_UP;
        case GLFW_KEY_DOWN:
            return SDLK_DOWN;
        case GLFW_KEY_LEFT:
            return SDLK_LEFT;
        case GLFW_KEY_RIGHT:
            return SDLK_RIGHT;
        case GLFW_KEY_TAB:
            return SDLK_TAB;
        case GLFW_KEY_ENTER:
            return SDLK_RETURN;
        case GLFW_KEY_BACKSPACE:
            return SDLK_BACKSPACE;
        // Fullscreen
        case GLFW_KEY_F11:
            return SDLK_F11;
        // Screenshot
        case GLFW_KEY_F2:
            return SDLK_F2;
        // Hide GUI
        case GLFW_KEY_F1:
            return SDLK_F1;
        // Third Person
        case GLFW_KEY_F5:
            return SDLK_F5;
        // Unknown
        default:
            return SDLK_UNKNOWN;
    }
}

// Pass Key Presses To SDL
static void glfw_key(__attribute__((unused)) GLFWwindow *window, int key, int scancode, int action, __attribute__((unused)) int mods) {
    SDL_Event event;
    int up = action == GLFW_RELEASE;
    event.type = up ? SDL_KEYUP : SDL_KEYDOWN;
    event.key.state = up ? SDL_RELEASED : SDL_PRESSED;
    event.key.keysym.scancode = scancode;
    event.key.keysym.mod = KMOD_NONE;
    event.key.keysym.sym = glfw_key_to_sdl_key(key);
    SDL_PushEvent(&event);
    if (key == GLFW_KEY_BACKSPACE && !up) {
        input_key_press((char) '\b');
    }
}

// Pass Text To Minecraft
static void glfw_char(__attribute__((unused)) GLFWwindow *window, unsigned int codepoint) {
    input_key_press((char) codepoint);
}

static double last_mouse_x = 0;
static double last_mouse_y = 0;

// Pass Mouse Movement To SDL
static void glfw_motion(__attribute__((unused)) GLFWwindow *window, double xpos, double ypos) {
    SDL_Event event;
    event.type = SDL_MOUSEMOTION;
    event.motion.x = xpos;
    event.motion.y = ypos;
    event.motion.xrel = (xpos - last_mouse_x);
    event.motion.yrel = (ypos - last_mouse_y);
    last_mouse_x = xpos;
    last_mouse_y = ypos;
    SDL_PushEvent(&event);
}

// Create And Push SDL Mouse Click Event
static void click(int button, int up) {
    SDL_Event event;
    event.type = up ? SDL_MOUSEBUTTONUP : SDL_MOUSEBUTTONDOWN;
    event.button.x = last_mouse_x;
    event.button.y = last_mouse_y;
    event.button.state = up ? SDL_RELEASED : SDL_PRESSED;
    event.button.button = button;
    SDL_PushEvent(&event);
}

// Pass Mouse Click To SDL
static void glfw_click(__attribute__((unused)) GLFWwindow *window, int button, int action, __attribute__((unused)) int mods) {
    int up = action == GLFW_RELEASE;
    int sdl_button = button == GLFW_MOUSE_BUTTON_RIGHT ? SDL_BUTTON_RIGHT : (button == GLFW_MOUSE_BUTTON_LEFT ? SDL_BUTTON_LEFT : SDL_BUTTON_MIDDLE);
    click(sdl_button, up);
}

// Pass Mouse Scroll To SDL
static void glfw_scroll(__attribute__((unused)) GLFWwindow *window, __attribute__((unused)) double xoffset, double yoffset) {
    if (yoffset != 0) {
        int sdl_button = yoffset > 0 ? SDL_BUTTON_WHEELUP : SDL_BUTTON_WHEELDOWN;
        click(sdl_button, 0);
        click(sdl_button, 1);
    }
}

// Default Window Size
#define DEFAULT_WIDTH 840
#define DEFAULT_HEIGHT 480

// Init GLFW
HOOK(SDL_WM_SetCaption, void, (const char *title, __attribute__((unused)) const char *icon)) {
    // Don't Enable GLFW In Server Mode
    if (!is_server) {
        glfwSetErrorCallback(glfw_error);

        if (!glfwInit()) {
            ERR("%s", "Unable To Initialize GLFW");
        }

        // Create OpenGL ES 1.1 Context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        glfw_window = glfwCreateWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, title, NULL, NULL);
        if (!glfw_window) {
            ERR("%s", "Unable To Create GLFW Window");
        }

        // Don't Process Events In Server Mode
        glfwSetKeyCallback(glfw_window, glfw_key);
        glfwSetCharCallback(glfw_window, glfw_char);
        glfwSetCursorPosCallback(glfw_window, glfw_motion);
        glfwSetMouseButtonCallback(glfw_window, glfw_click);
        glfwSetScrollCallback(glfw_window, glfw_scroll);

        glfwMakeContextCurrent(glfw_window);
    }
}

void compat_eglSwapBuffers() {
    if (!is_server) {
        // Don't Swap Buffers In A Context-Less Window
        glfwSwapBuffers(glfw_window);
    }
}

static int is_fullscreen = 0;

// Old Size And Position To Use When Exiting Fullscreen
static int old_width = -1;
static int old_height = -1;
static int old_x = -1;
static int old_y = -1;

// Toggle Fullscreen
static void toggle_fullscreen() {
    if (is_fullscreen) {
        glfwSetWindowMonitor(glfw_window, NULL, old_x, old_y, old_width, old_height, GLFW_DONT_CARE);

        old_width = -1;
        old_height = -1;
        old_x = -1;
        old_y = -1;
    } else {
        glfwGetWindowSize(glfw_window, &old_width, &old_height);
        glfwGetWindowPos(glfw_window, &old_x, &old_y);
        Screen *screen = DefaultScreenOfDisplay(glfwGetX11Display());

        glfwSetWindowMonitor(glfw_window, glfwGetPrimaryMonitor(), 0, 0, WidthOfScreen(screen), HeightOfScreen(screen), GLFW_DONT_CARE);
    }
    is_fullscreen = !is_fullscreen;
}

// Intercept SDL Events
HOOK(SDL_PollEvent, int, (SDL_Event *event)) {
    // Process GLFW Events
    glfwPollEvents();

    // Close Window (Ignore In Server Mode)
    if (!is_server && glfwWindowShouldClose(glfw_window)) {
        SDL_Event event;
        event.type = SDL_QUIT;
        SDL_PushEvent(&event);
        glfwSetWindowShouldClose(glfw_window, GLFW_FALSE);
    }

    // Poll Events
    ensure_SDL_PollEvent();
    int ret = (*real_SDL_PollEvent)(event);

    // Handle Events
    if (ret == 1 && event != NULL) {
        int handled = 0;

        if (event->type == SDL_KEYDOWN) {
            if (event->key.keysym.sym == SDLK_F11) {
                toggle_fullscreen();
                handled = 1;
            } else if (event->key.keysym.sym == SDLK_F2) {
                take_screenshot();
                handled = 1;
            } else if (event->key.keysym.sym == SDLK_F1) {
                input_hide_gui();
                handled = 1;
            } else if (event->key.keysym.sym == SDLK_F5) {
                input_third_person();
                handled = 1;
            }
        } else if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
            if (event->button.button == SDL_BUTTON_RIGHT) {
                input_set_is_right_click(event->button.state != SDL_RELEASED);
            } else if (event->button.button == SDL_BUTTON_LEFT) {
                input_set_is_left_click(event->button.state != SDL_RELEASED);
            }
        }

        if (handled) {
            // Event Was Handled
            return SDL_PollEvent(event);
        }
    }

    return ret;
}

// Terminate GLFW
HOOK(SDL_Quit, void, ()) {
    ensure_SDL_Quit();
    (*real_SDL_Quit)();

    // GLFW Is Disabled In Server Mode
    if (!is_server) {
        glfwDestroyWindow(glfw_window);
        glfwTerminate();
    }
}

static SDL_GrabMode fake_grab_mode = SDL_GRAB_OFF;

// Fix SDL Cursor Visibility/Grabbing
HOOK(SDL_WM_GrabInput, SDL_GrabMode, (SDL_GrabMode mode)) {
    if (is_server) {
        // Don't Grab Input In Server Mode
        if (mode != SDL_GRAB_QUERY) {
            fake_grab_mode = mode;
        }
        return fake_grab_mode;
    } else {
        if (mode != SDL_GRAB_QUERY && mode != SDL_WM_GrabInput(SDL_GRAB_QUERY)) {
            glfwSetInputMode(glfw_window, GLFW_CURSOR, mode == SDL_GRAB_OFF ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            glfwSetInputMode(glfw_window, GLFW_RAW_MOUSE_MOTION, mode == SDL_GRAB_OFF ? GLFW_FALSE : GLFW_TRUE);

            // GLFW Cursor Hiding is Broken
            Display *x11_display = glfwGetX11Display();
            Window x11_window = glfwGetX11Window(glfw_window);
            if (mode == SDL_GRAB_OFF) {
                XFixesShowCursor(x11_display, x11_window);
            } else {
                XFixesHideCursor(x11_display, x11_window);
            }
            XFlush(x11_display);
        }
        return mode == SDL_GRAB_QUERY ? (glfwGetInputMode(glfw_window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL ? SDL_GRAB_OFF : SDL_GRAB_ON) : mode;
    }
}

// Stub SDL Cursor Visibility
HOOK(SDL_ShowCursor, int, (int toggle)) {
    if (is_server) {
        return toggle == SDL_QUERY ? (fake_grab_mode == SDL_GRAB_OFF ? SDL_ENABLE : SDL_DISABLE) : toggle;
    } else {
        return toggle == SDL_QUERY ? (glfwGetInputMode(glfw_window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL ? SDL_ENABLE : SDL_DISABLE) : toggle;
    }
}

// SDL Stub
HOOK(SDL_SetVideoMode, SDL_Surface *, (__attribute__((unused)) int width, __attribute__((unused)) int height, __attribute__((unused)) int bpp, __attribute__((unused)) uint32_t flags)) {
    // Return Value Is Only Used For A NULL-Check
    return (SDL_Surface *) 1;
}

HOOK(XTranslateCoordinates, int, (Display *display, Window src_w, Window dest_w, int src_x, int src_y, int *dest_x_return, int *dest_y_return, Window *child_return)) {
    if (!is_server) {
        // Use X11
        ensure_XTranslateCoordinates();
        return (*real_XTranslateCoordinates)(display, src_w, dest_w, src_x, src_y, dest_x_return, dest_y_return, child_return);
    } else {
        // No X11
        *dest_x_return = src_x;
        *dest_y_return = src_y;
        return 1;
    }
}

HOOK(XGetWindowAttributes, int, (Display *display, Window w, XWindowAttributes *window_attributes_return)) {
    if (!is_server) {
        // Use X11
        ensure_XGetWindowAttributes();
        return (*real_XGetWindowAttributes)(display, w, window_attributes_return);
    } else {
        // No X11
        XWindowAttributes attributes;
        attributes.x = 0;
        attributes.y = 0;
        attributes.width = DEFAULT_WIDTH;
        attributes.height = DEFAULT_HEIGHT;
        *window_attributes_return = attributes;
        return 1;
    }
}

static void x11_nop() {
    // NOP
}
HOOK(SDL_GetWMInfo, int, (SDL_SysWMinfo *info)) {
    // Return Fake Lock Functions In Server Mode Since SDL X11 Is Disabled
    SDL_SysWMinfo ret;
    ret.info.x11.lock_func = x11_nop;
    ret.info.x11.unlock_func = x11_nop;
    ret.info.x11.display = glfwGetX11Display();
    ret.info.x11.window = glfwGetX11Window(glfw_window);
    ret.info.x11.wmwindow = ret.info.x11.window;
    *info = ret;
    return 1;
}

#include <stdlib.h>

// Use VirGL
void init_compat() {
    int mode = feature_get_mode();
    if (mode != 1) {
        // Force Software Rendering When Not In Native Mode
        setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);
    }
    if (mode == 0) {
        // Use VirGL When In VirGL Mode
        setenv("GALLIUM_DRIVER", "virpipe", 1);
    }
    is_server = mode == 2;
    // Video is Handled By GLFW Not SDL
    setenv("SDL_VIDEODRIVER", "dummy", 1);
}