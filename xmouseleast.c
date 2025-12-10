#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XTest.h>

#define LENGTH(X) (sizeof X / sizeof X[0])
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(val, low, up) (MIN(MAX(low, val), up))

#define PHYSICS(...) __VA_ARGS__,    .type = ARG_PHYSICS
#define SPEED(arg)   .value = arg,   .type = ARG_SPEED
#define MOVE(X, Y)   .x = X, .y = Y, .type = ARG_MOVE
#define SCROLL(X, Y) .x = X, .y = Y, .type = ARG_SCROLL
#define BUTTON(arg)  .value = arg,   .type = ARG_BUTTON
#define QUIT(rc)     .value = rc,    .type = ARG_QUIT

typedef enum {
  ARG_PHYSICS,
  ARG_SPEED,
  ARG_MOVE,
  ARG_SCROLL,
  ARG_BUTTON,
  ARG_QUIT,
} ArgType;

typedef struct {
  KeySym keysym;
  union {
    struct { int x, y; };
    struct { float frict, accel; };
    unsigned int value;
  };
  ArgType type;
} GenericBinding;

#include "./config.h"

Bool running = True;
int status = 0;
int xiop;
Display *dpy;
Window root;
pthread_t movethread;
int grabbed_ids[64];
int n_grabbed_ids = 0;

static unsigned int speed = default_speed;
static float frict = default_frict;
static float accel = default_accel;

static struct {
  float x, y;
  int dir_x, dir_y;
  float vel_x, vel_y;
} mouse;

static struct {
  float x, y;
  int dir_x, dir_y;
} scrollinfo;

void x_init();
void release_keys();
void grab_keyboard(XIEventMask mask);
void ungrab_keyboard(); 
void genericevent(XEvent *e);
void *move_loop(void *_);
void move_relative(float x, float y);
void handle_key(KeyCode keycode, Bool is_press);
void get_pointer();
void click(unsigned int button, Bool is_press);
void click_full(unsigned int button);
void scroll(float x, float y);

static void (*handler[LASTEvent])(XEvent *) = {
  [GenericEvent] = genericevent,
};

int main() {
  x_init();
  get_pointer();

  int rc = pthread_create(&movethread, NULL, &move_loop, NULL);
  if (rc != 0) {
    fprintf(stderr, "Couldn't start mouse thread.\n");
    exit(1);
  }

  unsigned char mask_bytes[XIMaskLen(XI_LASTEVENT)];
  memset(mask_bytes, 0, sizeof(mask_bytes));
  XISetMask(mask_bytes, XI_RawKeyPress);
  XISetMask(mask_bytes, XI_RawKeyRelease);

  XIEventMask mask;
  mask.deviceid = XIAllDevices;
  mask.mask_len = sizeof(mask_bytes);
  mask.mask = mask_bytes;
  XISelectEvents(dpy, root, &mask, 1);

  release_keys();
  #ifdef GRABBED_KB
    XIGrabDevice(dpy, GRABBED_KB, root, CurrentTime, None,
                 GrabModeAsync, GrabModeAsync, False, &mask); 
  #else
    grab_keyboard(mask);
    if (n_grabbed_ids == 0) {
      fprintf(stderr, "No devices were grabbed\n");
      goto defer;
    }
  #endif // GRABBED_KB

  // event loop
  XEvent ev;
  while (running) {
    struct pollfd pfd = {
      .fd = ConnectionNumber(dpy),
      .events = POLLIN,
    };
    int pending = (XPending(dpy) > 0 || poll(&pfd, 1, -1) > 0);

    if (!running) break;
    if (!pending) continue;

    XNextEvent(dpy, &ev);
    if (handler[ev.type]) handler[ev.type](&ev); // call handler
  }

defer:
  #ifdef GRABBED_KB
    XIUngrabDevice(dpy, GRABBED_KB, CurrentTime);
  #else
    ungrab_keyboard();
  #endif // GRABBED_KB
  XSync(dpy, False);
  XCloseDisplay(dpy);
  return status;
}

void x_init() {
  dpy = XOpenDisplay(NULL);
  if (dpy == NULL) {
    fprintf(stderr, "Couldn't open X Display\n");
    exit(1);
  }

  int screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);

  int _;
  if (!XQueryExtension(dpy, "XInputExtension", &xiop, &_, &_)) {
    fprintf(stderr, "XInput not supported\n");
    exit(1);
  }
}

// release keys to avoid infinite loop at the start
void release_keys() {
  char keymap[32];
  XQueryKeymap(dpy, keymap);

  for (int i = 0; i < 256; ++i) {
    KeySym keysym = XkbKeycodeToKeysym(dpy, i, 0, 0);
    for (size_t j = 0; j < LENGTH(modifiers); ++j) {
      if (keysym == modifiers[j]) goto skip;
    }
    if (0x01 & keymap[i/8] >> (i%8))
      XTestFakeKeyEvent(dpy, i, False, CurrentTime);
    skip:
  }
  XSync(dpy, False);
}

void grab_keyboard(XIEventMask mask) {
  XIDeviceInfo *devs;
  int n;

  devs = XIQueryDevice(dpy, XIAllDevices, &n);
  for (int i = 0; i < n; ++i) {
    XIDeviceInfo d = devs[i];
    if (d.enabled && !strstr(d.name, "XTEST") &&
        (d.use == XISlaveKeyboard || d.use == XIFloatingSlave)) {

      int rc;
      if ((rc = XIGrabDevice(dpy, d.deviceid, root, CurrentTime, None,
                             GrabModeAsync, GrabModeAsync, False, &mask))) {
        fprintf(stderr, "Failed to grab device (%d) %s. Reason: %d\n", d.deviceid, d.name, rc);
        continue;
      }
      grabbed_ids[n_grabbed_ids++] = d.deviceid;
      printf("Grabbed device (%d) %s\n", d.deviceid, d.name);
    }
  }
  XIFreeDeviceInfo(devs);
}

void ungrab_keyboard() {
  for (int i = 0; i < n_grabbed_ids; ++i) {
    XIUngrabDevice(dpy, grabbed_ids[i], CurrentTime);
    printf("Ungrabbed id %d\n", grabbed_ids[i]);
  }
}

void genericevent(XEvent *e) {
  if (e->xcookie.extension != xiop) return;
  if (!XGetEventData(dpy, &e->xcookie)) return;
  switch (e->xcookie.evtype) {
    int code;
    Bool is_press;
    XIDeviceEvent *dev;

    case XI_RawKeyPress:
    case XI_RawKeyRelease:
      dev = (XIDeviceEvent *)(e->xcookie.data);
      code = dev->detail;
      is_press = (e->xcookie.evtype == XI_RawKeyPress);
      handle_key(code, is_press);
      break;
  }
  XFreeEventData(dpy, &e->xcookie);
}

// this function is executed in a seperate thread
void *move_loop(void *_) {
  const float eps = 1E-3;
  for (;;) {
    float unit = (float)speed/rate;

    mouse.vel_x += accel*mouse.dir_x;
    mouse.vel_y += accel*mouse.dir_y;
    mouse.vel_x  = CLAMP(mouse.vel_x, -unit, unit);
    mouse.vel_y  = CLAMP(mouse.vel_y, -unit, unit);

    // friction starts taking over when we're not moving anymore
    if (mouse.dir_x == 0) mouse.vel_x *= frict;
    if (mouse.dir_y == 0) mouse.vel_y *= frict;

    if (mouse.dir_x == 0 && -eps < mouse.vel_x && mouse.vel_x < eps) mouse.vel_x = 0;
    if (mouse.dir_y == 0 && -eps < mouse.vel_y && mouse.vel_y < eps) mouse.vel_y = 0;

    // move mouse?
    if (mouse.vel_x != 0 || mouse.vel_y != 0) {
      move_relative(mouse.vel_x,
                    mouse.vel_y);
    }
    // scroll?
    if (scrollinfo.dir_x != 0 || scrollinfo.dir_y != 0) {
      scroll((float)scrollinfo.dir_x/rate,
             (float)scrollinfo.dir_y/rate);
    }
    usleep(1000000/rate);
  }
  (void)_;
}

void move_relative(float x, float y) {
  mouse.x += x;
  mouse.y += y;
  XWarpPointer(dpy, None, root, 0, 0, 0, 0, (int)mouse.x, (int)mouse.y);
  XFlush(dpy);
}

void handle_key(KeyCode keycode, Bool is_press) {
  KeySym keysym = XkbKeycodeToKeysym(dpy, keycode, 0, 0);
  for (size_t i = 0; i < LENGTH(modifiers); ++i) {
    if (modifiers[i] != keysym) continue;
    XTestFakeKeyEvent(dpy, keycode, is_press, CurrentTime);
    XSync(dpy, True);
  }
  for (size_t i = 0; i < LENGTH(bindings); ++i) {
    GenericBinding b = bindings[i];
    if (b.keysym != keysym) continue;

    int sign = (is_press ? 1 : -1);
    switch (b.type) {
      case ARG_MOVE:
        mouse.dir_x += sign*b.x;
        mouse.dir_y += sign*b.y;
        break;
      case ARG_BUTTON:
        click(b.value, is_press);
        break;
      case ARG_SPEED:
        speed = (is_press ? b.value : default_speed);
        printf("speed: %d\n", speed);
        break;
      case ARG_PHYSICS:
        frict = (is_press ? b.frict : default_frict);
        printf("frict: %f\n", frict);
        accel = (is_press ? b.accel : default_accel);
        printf("accel: %f\n", accel);
        break;
      case ARG_SCROLL:
        scrollinfo.dir_x += sign*b.x;
        scrollinfo.dir_y += sign*b.y;
        printf("scroll: %d %d %d\n", scrollinfo.dir_x, scrollinfo.dir_y, is_press);
        break;
      case ARG_QUIT:
        if (!is_press) { running = False; status = (int)b.value; }
        break;
    }
  }
}

void get_pointer() {
  Window dummy;
  int x, y;
  int _;
  unsigned int $;
  XQueryPointer(dpy, root, &dummy, &dummy, &x, &y, &_, &_, &$);
  mouse.x = x;
  mouse.y = y;
}

void click(unsigned int button, Bool is_press) {
  XTestFakeButtonEvent(dpy, button, is_press, CurrentTime);
  XFlush(dpy);
  printf("click: %d %d\n", button, is_press);
}

void click_full(unsigned int button) {
  XTestFakeButtonEvent(dpy, button, True, CurrentTime);
  XTestFakeButtonEvent(dpy, button, False, CurrentTime);
  XFlush(dpy);
}

void scroll(float x, float y) {
  scrollinfo.x += x;
  scrollinfo.y += y;
  while (scrollinfo.y <= -0.51) {
    scrollinfo.y += 1;
    click_full(4);
  }
  while (scrollinfo.y >= 0.51) {
    scrollinfo.y -= 1;
    click_full(5);
  }
  while (scrollinfo.x <= -0.51) {
    scrollinfo.x += 1;
    click_full(6);
  }
  while (scrollinfo.x >= 0.51) {
    scrollinfo.x -= 1;
    click_full(7);
  }
}
