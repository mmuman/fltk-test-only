//
// "$Id: haiku.H 10312 2014-09-15 09:35:05Z ossman $"
//
// Haiku header file for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2015 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     http://www.fltk.org/COPYING.php
//
// Please report all bugs and problems on the following page:
//
//     http://www.fltk.org/str.php
//

// Do not directly include this file, instead use <FL/x.H>.  It will
// include this file if __HAIKU__ is defined.  This is to encourage
// portability of even the system-specific code...

#ifndef FL_DOXYGEN
#ifndef Fl_X_H
#  error "Never use <FL/haiku.H> directly; include <FL/x.H> instead."
#endif // !Fl_X_H

typedef class FLWindow *Window;
class FLView;
//typedef BPoint XPoint;
typedef class BRegion *Fl_Region;
class BApplication;
class BMessage;
class BView;

#if !(defined(FL_LIBRARY) || defined(FL_INTERNALS)) // this part is used when compiling an application program
#  include <FL/Fl_Widget.H>


Window fl_xid_(const Fl_Window* w);
#define fl_xid(w) fl_xid_(w)

#else // this part must be compiled when building the FLTK libraries

// Standard Haiku C/C++ includes...
#  include <Application.h>
#  include <Bitmap.h>
#  include <Clipboard.h>
#  include <Cursor.h>
#  include <Polygon.h>
#  include <Region.h>
#  include <Screen.h>
#  include <View.h>
#  include <Window.h>

#  include <FL/Fl_Window.H>
#  include "../src/Fl_Font.H"

// some random X equivalents
struct XPoint { int x, y; };
struct XRectangle {int x, y, width, height;};
inline Fl_Region XRectangleRegion(int x, int y, int w, int h) {
  Fl_Region R = new BRegion(BRect(x, y, x + w - 1, y + h - 1));
  return R;
}
inline void XDestroyRegion(Fl_Region r) { delete r; }
inline void XClipBox(Fl_Region r,XRectangle* rect) {
    clipping_rect clip = r->FrameInt();
    rect->x=clip.left;
    rect->y=clip.top;
    rect->width=clip.right-clip.left+1;
    rect->height=clip.bottom-clip.top+1;
}
#if 0
#define XDestroyWindow(a,b) DestroyWindow(b)
#define XMapWindow(a,b) ShowWindow(b, SW_RESTORE)
#define XUnmapWindow(a,b) ShowWindow(b, SW_HIDE)
#endif

// this object contains all Haiku-specific stuff about a window:
// Warning: this object is highly subject to change!
class Fl_X {

public:
  // member variables - add new variables only at the end of this block
  FLWindow *xid;
  BBitmap *other_xid; // for double-buffered windows
  Fl_Window* w;
  Fl_Region region;
  Fl_X *next;
  int wait_for_expose;
  //HDC private_dc; // used for OpenGL
  BCursor *cursor;
  // BCursorID ?
  //int custom_cursor;
  //HDC saved_hdc;  // saves the handle of the DC currently loaded
  // static variables, static functions and member functions
  static Fl_X* first;
  static Fl_X* i(const Fl_Window* w) {return w->i;}
  static int fake_X_wm(const Fl_Window* w,int &X, int &Y,
		                 int &bt,int &bx,int &by);
  void make_fullscreen(int X, int Y, int W, int H);
  void setwindow(Fl_Window* wi) {w=wi; wi->i=this;}
  void flush();// {w->flush();}
//  void set_minmax(LPMINMAXINFO minmax);
  void mapraise();
  // Haiku doesn't have per window icons
  static void set_default_icons(const Fl_RGB_Image*[], int) {};
  void set_icons() {};
  int set_cursor(Fl_Cursor);
  int set_cursor(const Fl_RGB_Image*, int, int);
  static Fl_X* make(Fl_Window*);
  void destroy(void);
  void map(void);
  void unmap(void);
};
extern uint32 fl_wake_msg;
extern char fl_override_redirect; // hack into Fl_Window::make_xid()
extern int fl_background_pixel;  // hack into Fl_Window::make_xid()
//extern HPALETTE fl_palette; // non-zero only on 8-bit displays!
extern void fl_release_dc(FLWindow *w, BView *dc);
extern void fl_save_dc(FLWindow *w, BView *dc);
extern void fl_cleanup_dc_list(void);

inline Window fl_xid(const Fl_Window* w) { Fl_X *temp = Fl_X::i(w); return temp ? temp->xid : 0; }

#endif // FL_LIBRARY || FL_INTERNALS

extern void fl_open_display();

Fl_Window* fl_find(Window xid);
void fl_clip_region(Fl_Region);

// feed events into fltk:
int fl_handle(const BMessage *);


//extern class BApplication *fl_display;
extern Window fl_window;
//extern HDC fl_gc;
extern BView *fl_gc;
extern struct pattern fl_gc_pattern;
//extern MSG fl_msg;
//extern HDC fl_GetDC(Window);
//extern HDC fl_makeDC(HBITMAP);

// XXX BView * ???
typedef class BBitmap *Fl_Offscreen;

extern Fl_Offscreen fl_create_offscreen(int w, int h);
extern void fl_copy_offscreen(int x,int y,int w,int h, Fl_Offscreen gWorld, int srcx,int srcy);
extern void fl_delete_offscreen(Fl_Offscreen gWorld);
extern void fl_begin_offscreen(Fl_Offscreen gWorld);
extern void fl_end_offscreen();

// Bitmap masks
typedef BBitmap *Fl_Bitmask;

extern Fl_Bitmask fl_create_bitmask(int w, int h, const uchar *data);
extern Fl_Bitmask fl_create_alphamask(int w, int h, int d, int ld, const uchar *data);
extern void fl_delete_bitmask(Fl_Bitmask bm);

extern int fl_parse_color(const char* p, uchar& r, uchar& g, uchar& b);
#endif // FL_DOXYGEN
/** \file
 Haiku-specific symbols.
 */

/** \defgroup group_haiku Haiku-specific symbols
 Haiku-specific symbols declared in <FL/x.H> or <FL/gl.h>
 \sa \ref osissues_haiku
 @{ */

/** @brief Register a function called for each file dropped onto an application icon.
 \e cb will be called with a single Unix-style file name and path.
 If multiple files were dropped, \e cb will be called multiple times.
 */
extern void fl_open_callback(void (*cb)(const char *));

/** 
 * \brief Attaches a callback to the "About myprog" item of the system application menu.
 *
 * \param cb   a callback that will be called by "About myprog" menu item
 *		   with NULL 1st argument.
 * \param user_data   a pointer transmitted as 2nd argument to the callback.
 * \param shortcut    optional shortcut to attach to the "About myprog" menu item (e.g., FL_META+'a')
 */
//XXX: is that really needed?
extern void fl_haiku_set_about( Fl_Callback *cb, void *user_data, int shortcut = 0);

/** \brief The version number of the running Haiku (e.g., 0x00010000 for 1.0)
 */
extern int fl_haiku_version;

/** @} */

//
// End of "$Id: haiku.H 10312 2014-09-15 09:35:05Z ossman $".
//
