// uncomment if you want to grab a specific keyboard id
// #define GRABBED_KB 16

// whether debug output should be shown
#define DEBUG False

// mouse acceleration (set to 1 for no acceleration)
#define default_accel 0.111 
// mouse friction (set to 0 for no friction)
#define default_frict 0.90  
// mouse speed in pixels per second
#define default_speed 800   

const unsigned int rate = 250; // mouse polling rate

// modifiers which will be passed through
const KeySym modifiers[] = {
  XK_Shift_L, XK_Shift_R, XK_Control_L, XK_Control_R,
  XK_Meta_L, XK_Meta_R, XK_Alt_L, XK_Alt_R,
  XK_Super_L, XK_Super_R, XK_Hyper_L, XK_Hyper_R,
  XK_ISO_Level3_Shift,
};

// main keybindings
GenericBinding bindings[] = {
  { XK_space,  PHYSICS(.frict = 0.92, .accel = 0.22) },
  { XK_a,      PHYSICS(.frict = 0.87, .accel = 0.05) },
  { XK_space,  SPEED(1500) },
  { XK_a,      SPEED(250) },

  { XK_r,      MOVE(-1,  0) }, // left
  { XK_t,      MOVE( 1,  0) }, // right
  { XK_f,      MOVE( 0, -1) }, // up
  { XK_s,      MOVE( 0,  1) }, // down

  { XK_period, BUTTON(Button1) }, // left
  { XK_comma,  BUTTON(Button2) }, // middle
  { XK_slash,  BUTTON(Button3) }, // right

  { XK_Escape, SCROLL(  0,  15) },
  { XK_o,      SCROLL(  0, -15) },
  { XK_plus,   SCROLL(  0,  80) },
  { XK_minus,  SCROLL(  0, -80) },
  { XK_y,      SCROLL( 15,   0) },
  { XK_u,      SCROLL(-15,   0) },

  { XK_ISO_Level3_Shift, QUIT(0) },
  { XK_q,                QUIT(0) },
};
