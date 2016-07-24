//
// "$Id: Fl_haiku.cxx 10928 2015-11-24 14:26:52Z manolo $"
//
// Haiku-specific code for the Fast Light Tool Kit (FLTK).
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

// This file contains Haiku-specific code for fltk which is always linked
// in.  Search other files for "Haiku" or filenames ending in _haiku.cxx
// for other system-specific code.

// This file must be #include'd in Fl.cxx and not compiled separately.

#ifndef FL_DOXYGEN
#include <FL/Fl.H>
#include <FL/fl_utf8.h>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <FL/Enumerations.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Paged_Device.H>
#include "flstring.h"
#include "Fl_Font.H"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <wchar.h>

#include <Application.h>
#include <AppFileInfo.h>
#include <Cursor.h>
#include <Path.h>
#include <Roster.h>
#include <String.h>

// Internal functions
//static void fl_clipboard_notify_target(HWND wnd);
//static void fl_clipboard_notify_untarget(HWND wnd);
static int bmessage_input(FLWindow *win, FLView *view, BMessage *msg);

// Internal variables
//static HWND clipboard_wnd = 0;
//static HWND next_clipboard_wnd = 0;

Fl_Window *Fl_Window::current_;
// the current context
BView *fl_gc = NULL;
// the current window
FLWindow *fl_window = NULL;

static Fl_Window* resize_from_system;


static int msg_pipe[2];

static bool initial_clipboard = true;

static Fl_Window *track_mouse_win=0;	// current TrackMouseEvent() window

// USE_CAPTURE_MOUSE_WIN - this must be defined for TrackMouseEvent to work
// correctly with subwindows - otherwise a single mouse click and release
// (without a move) would generate phantom leave events.
// This defines, if the current mouse window (maybe a subwindow) or the
// main window should get mouse events after pushing (and holding) a mouse
// button, i.e. when dragging the mouse. This is done by calling SetCapture
// (see below).

//#ifdef USE_TRACK_MOUSE
//#define USE_CAPTURE_MOUSE_WIN
//#endif // USE_TRACK_MOUSE

//TODO: check runtime version instead (use libroot image_info.api_version)
int fl_haiku_version = B_HAIKU_VERSION;		// the version number of the running Haiku (e.g., 0x10000 for 1.0)

static void (*open_cb)(const char*) = NULL;

////////////////////////////////////////////////////////////////
// interface to poll/select call:

#  if USE_POLL

#    include <poll.h>
static pollfd *pollfds = 0;

#  else
#    if HAVE_SYS_SELECT_H
#      include <sys/select.h>
#    endif /* HAVE_SYS_SELECT_H */

static fd_set fdsets[3];
static int maxfd;
#    define POLLIN 1
#    define POLLOUT 4
#    define POLLERR 8

#  endif /* USE_POLL */

static int nfds = 0;
static int fd_array_size = 0;
struct FD {
#  if !USE_POLL
  int fd;
  short events;
#  endif
  void (*cb)(int, void*);
  void* arg;
};

static FD *fd = 0;

void Fl::add_fd(int n, int events, void (*cb)(int, void*), void *v) {
  remove_fd(n,events);
  int i = nfds++;
  if (i >= fd_array_size) {
    FD *temp;
    fd_array_size = 2*fd_array_size+1;

    if (!fd) temp = (FD*)malloc(fd_array_size*sizeof(FD));
    else temp = (FD*)realloc(fd, fd_array_size*sizeof(FD));

    if (!temp) return;
    fd = temp;

#  if USE_POLL
    pollfd *tpoll;

    if (!pollfds) tpoll = (pollfd*)malloc(fd_array_size*sizeof(pollfd));
    else tpoll = (pollfd*)realloc(pollfds, fd_array_size*sizeof(pollfd));

    if (!tpoll) return;
    pollfds = tpoll;
#  endif
  }
  fd[i].cb = cb;
  fd[i].arg = v;
#  if USE_POLL
  pollfds[i].fd = n;
  pollfds[i].events = events;
#  else
  fd[i].fd = n;
  fd[i].events = events;
  if (events & POLLIN) FD_SET(n, &fdsets[0]);
  if (events & POLLOUT) FD_SET(n, &fdsets[1]);
  if (events & POLLERR) FD_SET(n, &fdsets[2]);
  if (n > maxfd) maxfd = n;
#  endif
}

void Fl::add_fd(int n, void (*cb)(int, void*), void* v) {
  Fl::add_fd(n, POLLIN, cb, v);
}

void Fl::remove_fd(int n, int events) {
  int i,j;
# if !USE_POLL
  maxfd = -1; // recalculate maxfd on the fly
# endif
  for (i=j=0; i<nfds; i++) {
#  if USE_POLL
    if (pollfds[i].fd == n) {
      int e = pollfds[i].events & ~events;
      if (!e) continue; // if no events left, delete this fd
      pollfds[j].events = e;
    }
#  else
    if (fd[i].fd == n) {
      int e = fd[i].events & ~events;
      if (!e) continue; // if no events left, delete this fd
      fd[i].events = e;
    }
    if (fd[i].fd > maxfd) maxfd = fd[i].fd;
#  endif
    // move it down in the array if necessary:
    if (j<i) {
      fd[j] = fd[i];
#  if USE_POLL
      pollfds[j] = pollfds[i];
#  endif
    }
    j++;
  }
  nfds = j;
#  if !USE_POLL
  if (events & POLLIN) FD_CLR(n, &fdsets[0]);
  if (events & POLLOUT) FD_CLR(n, &fdsets[1]);
  if (events & POLLERR) FD_CLR(n, &fdsets[2]);
#  endif
}

void Fl::remove_fd(int n) {
  remove_fd(n, -1);
}

extern int fl_send_system_handlers(void *e);

// these pointers are set by the Fl::lock() function:
static void nothing() {}
void (*fl_lock_function)() = nothing;
void (*fl_unlock_function)() = nothing;

// This is never called with time_to_wait < 0.0:
// It should return negative on error, 0 if nothing happens before
// timeout, and >0 if any callbacks were done.
int fl_wait(double time_to_wait) {
#  if !USE_POLL
  fd_set fdt[3];
  fdt[0] = fdsets[0];
  fdt[1] = fdsets[1];
  fdt[2] = fdsets[2];
#  endif
  int n;

  fl_unlock_function();

  // let the window thread handle messages while we wait
  if (fl_gc)
    fl_gc->UnlockLooper();

  if (time_to_wait < 2147483.648) {
#  if USE_POLL
    n = ::poll(pollfds, nfds, int(time_to_wait*1000 + .5));
#  else
    timeval t;
    t.tv_sec = int(time_to_wait);
    t.tv_usec = int(1000000 * (time_to_wait-t.tv_sec));
    n = ::select(maxfd+1,&fdt[0],&fdt[1],&fdt[2],&t);
#  endif
  } else {
#  if USE_POLL
    n = ::poll(pollfds, nfds, -1);
#  else
    n = ::select(maxfd+1,&fdt[0],&fdt[1],&fdt[2],0);
#  endif
  }

  if (fl_gc)
    fl_gc->LockLooper();

  fl_lock_function();

  if (n > 0) {
    for (int i=0; i<nfds; i++) {
#  if USE_POLL
      if (pollfds[i].revents) fd[i].cb(pollfds[i].fd, fd[i].arg);
#  else
      int f = fd[i].fd;
      short revents = 0;
      if (FD_ISSET(f,&fdt[0])) revents |= POLLIN;
      if (FD_ISSET(f,&fdt[1])) revents |= POLLOUT;
      if (FD_ISSET(f,&fdt[2])) revents |= POLLERR;
      if (fd[i].events & revents) fd[i].cb(f, fd[i].arg);
#  endif
    }
  }
  return n;
}

// fl_ready() is just like fl_wait(0.0) except no callbacks are done:
int fl_ready() {
  if (!nfds) return 0; // nothing to select or poll
#  if USE_POLL
  return ::poll(pollfds, nfds, 0);
#  else
  timeval t;
  t.tv_sec = 0;
  t.tv_usec = 0;
  fd_set fdt[3];
  fdt[0] = fdsets[0];
  fdt[1] = fdsets[1];
  fdt[2] = fdsets[2];
  return ::select(maxfd+1,&fdt[0],&fdt[1],&fdt[2],&t);
#  endif
}

////////////////////////////////////////////////////////////////

void fl_reset_spot()
{
//TODO
//XXX: what is it actually supposed to do?
}

void fl_set_spot(int font, int size, int X, int Y, int W, int H, Fl_Window *win)
{
  if (!win) return;
  Fl_Window* tw = win;
  while (tw->parent()) tw = tw->window(); // find top level window

  if (!tw->shown())
    return;
//TODO
//XXX: what is it actually supposed to do?
}

void fl_set_status(int x, int y, int w, int h)
{
//TODO
//XXX: what is it actually supposed to do?
}

// the currently handled message
//XXX: useful?
//BMessage *fl_msg;


////////////////////////////////////////////////////////////////
// subclasses

class FLView: public BView
{
public:
  FLView(BRect frame, const char *name);
  virtual ~FLView();

virtual void	MouseDown(BPoint where);
virtual void	MouseUp(BPoint where);
virtual void	MouseMoved(BPoint where, uint32 code, const BMessage *a_message);
virtual void	KeyDown(const char *bytes, int32 numBytes);
virtual void	KeyUp(const char *bytes, int32 numBytes);
virtual void	Draw(BRect updateRect);
virtual void	WindowActivated(bool state);
virtual void	FrameResized(float new_width, float new_height);
virtual void	MessageReceived(BMessage *message);

  bool	IsClosing() const { return fIsClosing; };
  void	SetIsClosing(bool isClosing);
  BRegion	GetUpdateRegion();

private:
  bool	fIsClosing; /* the window is closing, don't send events anymore */
  BRegion	fUpdateRegion;
};

class FLWindow: public BWindow
{
public:
  FLWindow(FLView *flview, BRect frame, const char *title,
           window_look look, window_feel feel, uint32 flags);
  virtual ~FLWindow();

virtual void Show();
virtual void Hide();
virtual void Minimize(bool minimize);
virtual bool QuitRequested();
virtual void DispatchMessage(BMessage *message, BHandler *handler);
virtual	void FrameMoved(BPoint newPosition);

  FLView	*View() const { return fFLView; };

private:
  FLView	*fFLView;
};


////////////////////////////////////////////////////////////////
// BView subclass

FLView::FLView(BRect frame, const char *name)
	:BView(frame, name, B_FOLLOW_ALL_SIDES, B_WILL_DRAW|B_FRAME_EVENTS|B_FULL_UPDATE_ON_RESIZE)
{
	fIsClosing = false;
	fUpdateRegion.MakeEmpty();
	SetViewColor(0,255,0);//DEBUG
	SetLowColor(B_TRANSPARENT_COLOR);
}

FLView::~FLView()
{
}

void FLView::MouseDown(BPoint where)
{
	BMessage *message = Window()->DetachCurrentMessage();
	bmessage_input(NULL, this, message);
}

void FLView::MouseUp(BPoint where)
{
	BMessage *message = Window()->DetachCurrentMessage();
	bmessage_input(NULL, this, message);
}

void FLView::MouseMoved(BPoint where, uint32 code, const BMessage *a_message)
{
	BMessage *message = Window()->DetachCurrentMessage();
	bmessage_input(NULL, this, message);
}

void FLView::KeyDown(const char *bytes, int32 numBytes)
{
	BMessage *message = Window()->DetachCurrentMessage();
  	//message->PrintToStream();
	bmessage_input(NULL, this, message);
}

void FLView::KeyUp(const char *bytes, int32 numBytes)
{
	uint32 mods;
	BMessage *message = Window()->DetachCurrentMessage();
  	//message->PrintToStream();
  	
  	// if ALT is down, and we aren't in an FLWindow (replicant ?)
	if ((message->FindInt32("modifiers", (int32 *)&mods) == B_OK) && 
			(mods & B_COMMAND_KEY) && 
			!(dynamic_cast<FLWindow *>(Window()))) {
		BMessage *downm = new BMessage(*message);
		downm->what = B_KEY_DOWN;
		// first fake the down message as BWindow swallows them..
		bmessage_input(NULL, this, downm);
	}
	bmessage_input(NULL, this, message);
}

void FLView::Draw(BRect updateRect)
{
	BMessage *message;
	//if (fUpdateRegion.CountRects() < 1) {
		message = new BMessage(_UPDATE_);
		message->AddRect("update_rect", updateRect);
		bmessage_input(NULL, this, message);
	//}
	fUpdateRegion.Include(updateRect);
}

void FLView::WindowActivated(bool state)
{
	BMessage *message = Window()->DetachCurrentMessage();
	bmessage_input(NULL, this, message);
	BView::WindowActivated(state);
}

void FLView::FrameResized(float new_width, float new_height)
{
	BMessage *message = Window()->DetachCurrentMessage();
	bmessage_input(NULL, this, message);
	BView::FrameResized(new_width, new_height);
}

void FLView::MessageReceived(BMessage *message)
{
	switch (message->what) {
	  //case B_REPLY:
	  case B_SIMPLE_DATA:
	  case B_REFS_RECEIVED:
	  case B_MIME_DATA:
	  case B_PASTE:
	  case B_MOUSE_WHEEL_CHANGED:
	    Window()->DetachCurrentMessage();
		bmessage_input(NULL, this, message);
		break;
	  case B_UNMAPPED_KEY_DOWN:
	  case B_UNMAPPED_KEY_UP:
	  	//message->PrintToStream();
	  	break;
	  default:
		BView::MessageReceived(message);
	}
}

void FLView::SetIsClosing(bool isClosing)
{
	fIsClosing = isClosing;
}

BRegion FLView::GetUpdateRegion()
{
	BRegion reg(fUpdateRegion);
	fUpdateRegion.MakeEmpty();
	return reg;
}

////////////////////////////////////////////////////////////////
// BWindow subclass

FLWindow::FLWindow(FLView *flview, BRect frame, const char *title,
                   window_look look, window_feel feel, uint32 flags)
	:BWindow(frame, title, look, feel, flags)
{
	fFLView = flview;
}

FLWindow::~FLWindow()
{
}

void FLWindow::Show()
{

	BMessage *message = new BMessage('Xmap');
	bmessage_input(this, NULL, message);
	BWindow::Show();
}

void FLWindow::Hide()
{
	BMessage *message = new BMessage('Xunm');
	bmessage_input(this, NULL, message);
	BWindow::Hide();
}

void FLWindow::Minimize(bool minimize)
{
	BMessage *message = new BMessage(minimize?'Xunm':'Xmap');
	bmessage_input(this, NULL, message);
	BWindow::Minimize(minimize);
}

bool FLWindow::QuitRequested()
{
	BMessage *message = new BMessage(B_QUIT_REQUESTED);
	bmessage_input(this, NULL, message);
	return false;
}

void FLWindow::DispatchMessage(BMessage *message, BHandler *handler)
{
	uint32 mods;
	switch (message->what) {
	case B_KEY_DOWN:
		if ((message->FindInt32("modifiers", (int32 *)&mods) == B_OK) && 
				(mods & B_COMMAND_KEY)) {
			/* BWindow swallows KEY_DOWN when ALT is down...
			 * so this hack is needed.
			 */
			View()->KeyDown(NULL, 0);
			return;
		}
		break;
	case B_UNMAPPED_KEY_DOWN:
	case B_UNMAPPED_KEY_UP:
	case B_KEY_UP:
		//message->PrintToStream();
		break;
	}
	BWindow::DispatchMessage(message, handler);
}

void FLWindow::FrameMoved(BPoint newPosition)
{
	BMessage *message = DetachCurrentMessage();
	bmessage_input(this, NULL, message);
}

////////////////////////////////////////////////////////////////
// BApplication subclass

class FLApp: public BApplication
{
public:
  FLApp(const char* signature);
  virtual ~FLApp();
  void ReadyToRun();
virtual void	RefsReceived(BMessage *message);
virtual void	MessageReceived(BMessage *message);
virtual void	DispatchMessage(BMessage *message, BHandler *handler);
virtual bool	QuitRequested();

};
FLApp::FLApp(const char* signature)
	:BApplication(signature)
{
}

FLApp::~FLApp()
{
}

void
FLApp::ReadyToRun()
{
}

void
FLApp::RefsReceived(BMessage *message)
{
	DetachCurrentMessage();
	bmessage_input(NULL, NULL, message);
}

void
FLApp::MessageReceived(BMessage *message)
{
	switch (message->what) {
	case B_CLIPBOARD_CHANGED:
		BApplication::MessageReceived(message);
		DetachCurrentMessage();
		bmessage_input(NULL, NULL, message);
		break;
#if 0
	case '_UIC': /* B_UI_SETTINGS_CHANGED (but it's not declared in R5) */
		BApplication::MessageReceived(message);
		DetachCurrentMessage();
		bmessage_input(NULL, NULL, message);
		break;
#endif
	default:
		BApplication::MessageReceived(message);
		break;
	}
}

void
FLApp::DispatchMessage(BMessage *message, BHandler *handler)
{
	switch (message->what) {
	case '_UIC': /* B_UI_SETTINGS_CHANGED (but it's not declared in R5) */
		message->PrintToStream();
		BMessage *msg = new BMessage('_UIC');
		bmessage_input(NULL, NULL, msg);
		break;
	}
	BApplication::DispatchMessage(message, handler);
}

bool
FLApp::QuitRequested()
{
	BMessage *msg = new BMessage(B_QUIT_REQUESTED);
	bmessage_input(NULL, NULL, msg);
	return false; /* let FLTK delete be_app */
}



////////////////////////////////////////////////////////////////
// BApplication life cycle
// some black thread magic taken from my XEmacs port

static thread_id gBAppThreadID;
static thread_id gMainThreadID;
static int bapp_ref_count = 0;

static int32 bapp_quit_thread(void *arg)
{
	be_app->Lock();
	be_app->Quit();
	return 0;
}

static void int_sighandler(int sig)
{
	resume_thread(spawn_thread(bapp_quit_thread, "BApplication->Quit()", B_NORMAL_PRIORITY, NULL));
}

static int32 bapp_thread(void *arg)
{
//	signal(SIGINT, int_sighandler); /* on ^C exit the bapp_thread too */
	be_app->Lock();
	be_app->Run();
	return 0;
}

static void launch_bapplication(void)
{
  bapp_ref_count++;
  if (be_app)
    return; /* done already! */

  // default application signature
  const char *signature = "application/x-vnd.FLTK-app";
  // dig resources for correct signature
  image_info info;
  int32 cookie = 0;
  if (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
    fprintf(stderr, "image:%s\n", info.name);
    BFile app(info.name, O_RDONLY);
    if (app.InitCheck() == B_OK) {
      BAppFileInfo app_info(&app);
      if (app_info.InitCheck() == B_OK) {
        char sig[B_MIME_TYPE_LENGTH];
        if (app_info.GetSignature(sig) == B_OK)
          signature = strndup(sig, B_MIME_TYPE_LENGTH);
      }
    }
  }

  new FLApp(signature);
  gBAppThreadID = spawn_thread(bapp_thread, "BApplication(FLTK)", B_NORMAL_PRIORITY, (void *)find_thread(NULL));
  if (gBAppThreadID < B_OK)
    return; /* #### handle errors */
  if (resume_thread(gBAppThreadID) < B_OK)
    return;
  // This way we ensure we don't create BWindows before be_app is created
  // (creating it in the thread doesn't ensure this)
  be_app->Unlock();
}

static void uninit_bapplication(void)
{
	status_t err;
	if (--bapp_ref_count)
		return;
	be_app->Lock();
	be_app->Quit();
	//XXX:HACK
	be_app->Unlock();
	//be_app_messenger.SendMessage(B_QUIT_REQUESTED);
	wait_for_thread(gBAppThreadID, &err);
	//FIXME:leak or crash
	//delete be_app;
	be_app = NULL;
}


static void msg_fd_callback(int fd,void *data) {
  BMessage *msg;

  if (read(fd, &msg, sizeof(BMessage *)) < (int)sizeof(BMessage *))
    return;

  if (msg == NULL)
    return;

  fprintf(stderr, "%s(%d): got message:", __FUNCTION__, fd);
  msg->PrintToStream();

  if (!fl_send_system_handlers(msg))
    fl_handle(msg);

  delete msg;
}


/*
 * Install an open documents event handler...
 */
void fl_open_callback(void (*cb)(const char *)) {
  fl_open_display();
  open_cb = cb;
}


void fl_open_display() {
  if (be_app) return;

  //XXX: Haiku supports remote display,
  // in theory we could use the display string and setenv() APP_SERVER_NAME

  launch_bapplication();

  if (pipe(msg_pipe) < 0)
    Fl::fatal("Can't create message pipe: %s",strerror(errno));
  
  Fl::add_fd(msg_pipe[0], POLLIN, msg_fd_callback);
}

/*
 * get rid of allocated resources
 */
void fl_close_display() {
  fl_cleanup_dc_list();
  uninit_bapplication();
  fl_free_fonts();
}

#if 0
class Fl_Win32_At_Exit {
public:
  Fl_Win32_At_Exit() { }
  ~Fl_Win32_At_Exit() {
    fl_free_fonts();        // do some WIN32 cleanup
    fl_cleanup_pens();
    OleUninitialize();
    fl_brush_action(1);
    fl_cleanup_dc_list();
    // This is actually too late in the cleanup process to remove the
    // clipboard notifications, but we have no earlier hook so we try
    // to work around it anyway.
    if (clipboard_wnd != NULL)
      fl_clipboard_notify_untarget(clipboard_wnd);
  }
};
static Fl_Win32_At_Exit win32_at_exit;
#endif

static void update_modifiers(uint32 mods)
{
  ulong state = Fl::e_state & 0xff000000; // keep the mouse button state
  if (mods & B_SHIFT_KEY) state |= FL_SHIFT;
  if (mods & B_CAPS_LOCK) state |= FL_CAPS_LOCK;
  // XXX: use FL_COMMAND / FL_CONTROL instead ? or check for swapped mods?
  if (mods & B_CONTROL_KEY) state |= FL_CTRL;
  // XXX: check only B_LEFT_COMMAND_KEY? (not AltGr)
  if (mods & B_COMMAND_KEY) state |= FL_ALT;
  if (mods & B_OPTION_KEY) state |= FL_META;
  if (mods & B_NUM_LOCK) state |= FL_NUM_LOCK;
  if (mods & B_SCROLL_LOCK) state |= FL_SCROLL_LOCK;
  Fl::e_state = state;
}

// convert a Haiku key B_x to an Fltk (X) Keysym:
// See also the inverse converter in Fl_get_key_haiku.cxx
static const struct {unsigned short bk, fltk;} bktab[] = {
  {B_BACKSPACE,	FL_BackSpace},
  {B_TAB,		FL_Tab},
  {B_INSERT,	FL_Insert},
  //{VK_CLEAR,	FL_KP+'5',	0xff0b/*XK_Clear*/},
  {B_RETURN,	FL_Enter},
  //{VK_SHIFT,	FL_Shift_L,	FL_Shift_R},
  //{VK_CONTROL,	FL_Control_L,	FL_Control_R},
  //{VK_MENU,	FL_Alt_L,	FL_Alt_R},
  //{VK_CAPITAL,	FL_Caps_Lock},
  //{B_SUBSTITUTE, 	},
  {B_PAGE_UP,	FL_Page_Up},
  {B_PAGE_DOWN,	FL_Page_Down},
  {B_ESCAPE,	FL_Escape},
  {B_SPACE,	' '},
  {B_END,	FL_End},
  {B_HOME,	FL_Home},
  {B_LEFT_ARROW,	FL_Left},
  {B_UP_ARROW,	FL_Up},
  {B_RIGHT_ARROW,	FL_Right},
  {B_DOWN_ARROW,	FL_Down},
  {B_DELETE,	FL_Delete},
  {B_KATAKANA_HIRAGANA,	FL_Kana},
  //{B_HANKAKU_ZENKAKU,	},
  //{B_HANGUL,	},
  //{B_HANGUL_HANJA,	},
};
// "function" keys
static const struct {unsigned short fk, fltk;} fktab[] = {
  {B_F1_KEY,	FL_F + 1},
  {B_F2_KEY,	FL_F + 2},
  {B_F3_KEY,	FL_F + 3},
  {B_F4_KEY,	FL_F + 4},
  {B_F5_KEY,	FL_F + 5},
  {B_F6_KEY,	FL_F + 6},
  {B_F7_KEY,	FL_F + 7},
  {B_F8_KEY,	FL_F + 8},
  {B_F9_KEY,	FL_F + 9},
  {B_F10_KEY,	FL_F + 10},
  {B_F11_KEY,	FL_F + 11},
  {B_F12_KEY,	FL_F + 12},
  {B_PAUSE_KEY,	FL_Pause},
  {B_PRINT_KEY,	FL_Print},
  {B_SCROLL_KEY,	FL_Scroll_Lock},
};

static int hk2fltk(int32 rk, int32 k) {
  unsigned int i;
  if (rk == B_FUNCTION_KEY) {
    for (i = 0; i < sizeof(fktab)/sizeof(*fktab); i++) {
      if (fktab[i].fk == k)
        return fktab[i].fltk;
    }
  }
  for (i = 0; i < sizeof(bktab)/sizeof(*bktab); i++) {
    if (bktab[i].bk == rk)
      return bktab[i].fltk;
  }
  return 0;
}


// send a BMessage object through the msg_pipe
static int bmessage_input(FLWindow *win, FLView *view, BMessage *msg)
{
  if (win && !view)
    view = win->View();
  if (view && !win)
    win = (FLWindow *)view->Window();
  if (view)
    msg->AddPointer("FLView", view);
  if (win)
    msg->AddPointer("FLWindow", win);

  /* push the message in the pipe */
  if (write(msg_pipe[1], &msg, sizeof(BMessage *)) < 0)
    return errno;
  return B_OK;
}

// handled an event received through msg_pipe
int fl_handle(const BMessage *msg)
{
  FLWindow *win = NULL;
  FLView *view = NULL;

  msg->FindPointer("FLWindow", (void **)&win);
  msg->FindPointer("FLView", (void **)&view);

fprintf(stderr, "fl_handle(%p '%4.4s') %p %p\n", msg, &msg->what, win, view);

  // global events
  switch (msg->what) {
  case B_QUIT_REQUESTED:
    if (win) // not from the BApplication
      break;
    // Let applications treat B_QUIT_REQUESTED identical to SIGTERM on *nix
    raise(SIGTERM);
    break;

  case B_SCREEN_CHANGED:
    Fl::call_screen_init();
    Fl::handle(FL_SCREEN_CONFIGURATION_CHANGED, NULL);
    break;

  case B_REFS_RECEIVED:
    if (open_cb) {
      entry_ref ref;
      for (int i = 0; msg->FindRef("refs", i, &ref) == B_OK; i++) {
        BPath path(&ref);
        if (path.InitCheck() == B_OK)
          open_cb(path.Path());
      }
    }
    break;

  case '_UIC': // B_UI_SETTINGS_CHANGED
  //TODO: add the Haiku constants once looncraz' branch is merged
    Fl::get_system_colors();
    Fl::reload_scheme();
    break;

  case B_COPY:
    break;

  case B_PASTE:
    break;

  default:
    break;
  }

  int event = 0;
  Fl_Window* window = fl_find(win);
fprintf(stderr, "fl_handle: window %p\n", window);

  // events attached to a window
  if (window) switch (msg->what) {

  case B_QUIT_REQUESTED:
    event = FL_CLOSE;
    break;

  case 'Xunm':
    if (!window->parent())
      event = FL_HIDE;
    break;

  case 'Xmap':
    if (!window->parent())
      event = FL_SHOW;
    break;

  case _UPDATE_: {
    // XXX: should we?
    Fl_X::i(window)->wait_for_expose = 0;

    BRect update;
    if (msg->FindRect("update_rect", &update) == B_OK)
      window->damage(FL_DAMAGE_EXPOSE, (int)update.left, (int)update.top,
                     (int)update.right - update.left + 1,
                     (int)update.bottom - update.top + 1);
    return 1;
  }

  case B_WINDOW_ACTIVATED: {
    bool active;
    if (msg->FindBool("active", &active) != B_OK)
      break;
    event = active ? FL_FOCUS : FL_UNFOCUS;
    break;
  }

  case B_KEY_UP:
  case B_KEY_DOWN: {
    event = msg->what == B_KEY_UP ? FL_KEYUP : FL_KEYDOWN;

    uint32 modifiers;
    if (msg->FindInt32("modifiers", (int32 *)&modifiers) == B_OK)
      update_modifiers(modifiers);
    fprintf(stderr, "modifiers = 0x%08x\n", modifiers);



    static char buffer[1024];
    int8 bytes[10];
    int nbytes;
    unsigned c = 0;
    for (nbytes = 0; msg->FindInt8("byte", nbytes, &bytes[nbytes]) == B_OK; nbytes++);
    bytes[nbytes] = '\0';
    if (nbytes > 0)
      c = fl_utf8decode((const char *)bytes, NULL, &nbytes);
    fprintf(stderr, "bytes[] = '%s', c = 0x%08x %d\n", bytes, c, c);
    if (nbytes)
      ;// should we set e_state from it ?
    const char *text;
    int32 raw_char, key;
    if (msg->FindInt32("raw_char", &raw_char) != B_OK)
      raw_char = 0;
    if (msg->FindInt32("key", &key) != B_OK)
      key = 0;
    Fl::e_keysym = Fl::e_original_keysym = hk2fltk(raw_char, key);
    if (Fl::e_keysym == 0 && nbytes == 1)
      Fl::e_keysym = bytes[0];
    if (modifiers & B_CONTROL_KEY) {
      buffer[0] = (int8)raw_char;
      buffer[1] = '\0';
      Fl::e_length = 2;
      Fl::e_text = buffer;
    fprintf(stderr, "str = '%s'\n", buffer);
    } else if (msg->FindString("bytes", &text) == B_OK) {
      Fl::e_length = strlen(text);
      strlcpy(buffer, text, 1024);
      Fl::e_text = buffer;
    fprintf(stderr, "str = '%s'\n", buffer);
    }

//TODO
    break;
  }

  case B_MOUSE_DOWN:
    if (!event) event = FL_PUSH;
    // FALLTHROUGH
  case B_MOUSE_UP:
    if (!event) event = FL_RELEASE;
    // FALLTHROUGH
  case B_MOUSE_MOVED:
    if (!event) event = FL_MOVE;
  {

    // check for transit
    int32 code;
    if (msg->FindInt32("be:transit", &code) == B_OK && code != B_INSIDE_VIEW) {
      Fl_Window *tw = window;
      while (tw->parent()) tw = tw->window(); // find top level window
      if (code == B_EXITED_VIEW) {
        Fl::belowmouse(0);
        Fl::handle(FL_LEAVE, tw);
      } else if (code == B_ENTERED_VIEW) {
        ;// XXX: anything to do?
      }
    }

    BPoint where;
    if (msg->FindPoint("where", &where) == B_OK) {
      Fl::e_x = (int)where.x;
      Fl::e_y = (int)where.y;
      while (window->parent()) {
        Fl::e_x += window->x();
        Fl::e_y += window->y();
        window = window->window();
      }
    }
    if (msg->FindPoint("screen_where", &where) == B_OK) {
      Fl::e_x_root = (int)where.x;
      Fl::e_y_root = (int)where.y;
    }

    // XXX: update modifiers ??

    static int32 old_buttons = 0, buttons;
    if (msg->FindInt32("buttons", &buttons) == B_OK) {
      // update button state
      Fl::e_state &= 0xff0000; // keep shift key states
      Fl::e_state |= (buttons & 0xff) << 24; // add first 8 buttons

      if (msg->what != B_MOUSE_MOVED) {
        int32 clicks;
        if (msg->FindInt32("clicks", &clicks) == B_OK) {
          if (clicks > 1)
            Fl::e_clicks = clicks - 1;
          else
            Fl::e_clicks = 0;
          fprintf(stderr, "clicks %d\n", clicks);
        }

      	int button;
        for (button = 0; button < 8; button++)
          if ((buttons ^ old_buttons) & (1 << button)) {
            Fl::e_keysym = FL_Button + button;
            break;
          }

        if (msg->what == B_MOUSE_DOWN)
          Fl::e_is_click = 1;
      } else if (!buttons)
        Fl::e_is_click = 0; //XXX other platforms check for > 5pix move
      old_buttons = buttons;
    }


//    fl_xmousewin = window;
//    in_a_window = true;
    break;
  }

  case B_MOUSE_WHEEL_CHANGED: {
    float dx = 0, dy = 0;
    if (msg->FindFloat("be:wheel_delta_x", &dx) == B_OK && dx != 0.0) {
      Fl::e_dx = (int)dx;
      Fl::e_dy = 0;
      if ( Fl::e_dx) Fl::handle( FL_MOUSEWHEEL, window );
    }
    if (msg->FindFloat("be:wheel_delta_y", &dy) == B_OK && dy != 0.0) {
      Fl::e_dx = 0;
      Fl::e_dy = (int)dy;
      if ( Fl::e_dy) Fl::handle( FL_MOUSEWHEEL, window );
    }
    return 0;
  }

  //case B_WINDOW_RESIZED:
  case B_VIEW_RESIZED: { // which should it be?
    int32 width, height;
    //if (window->parent()) break; // ignore child windows
    if (!view ||
        msg->FindInt32("width", &width) != B_OK ||
        msg->FindInt32("height", &height) != B_OK)
      break;
    BRect frame;
    view->LockLooper();
    frame = view->Frame();
    view->ConvertToScreen(&frame);
    view->UnlockLooper();
    int X = (int)frame.left, Y = (int)frame.top;
    int W = (int)width + 1, H = (int)height + 1; // off by 1
    //int W = (int)frame.right - frame.left + 1, H = (int)frame.bottom - frame.top + 1;

    resize_from_system = window;
    fprintf(stderr, "resize(%d, %d, %d, %d)\n", X, Y, W, H);
    window->resize(X, Y, W, H);
    //resize_from_system = NULL;

    break;
  }

  case B_WINDOW_MOVED: {
    BPoint p;
    if (msg->FindPoint("where", &p) != B_OK)
      break;

    resize_from_system = window;
    float dx = p.x - window->x();
    float dy = p.y - window->y();
    window->position((int)p.x, (int)p.y);
    // also update subwindows
    // TODO: actually use a child BView instead so we don't have to hack this way.
    fprintf(stderr, "has parent? %d\n", window->parent());
    if (!window->parent()) {
      Fl_X* i = Fl_X::first;
      while (i) {
      	Fl_Window *w = i->w;
//    fprintf(stderr, "i %p i->xid %p window %p parent %p == %p\n", i, i->xid, w->window(), w->parent(), window);
      	if (i->xid && w->window() == window) {
//      	  fprintf(stderr, "moving subwin\n");
      	  i->xid->MoveBy(dx, dy);
//        Fl_X *p = Fl_X::i(w->window());
//        BWindow *w = i->xid;
//        if (p && p->xid && p->xid != win)
//          p->xid->MoveBy(dx, dy);
      	}
        i = i->next;
      }
    }
    resize_from_system = NULL;

    break;
  }

  case B_COPY:
    break;

  case B_PASTE:
    break;

  default:
    break;
  }

  return Fl::handle(event, window);
}


static char im_enabled = 1;

void Fl::enable_im() {
  fl_open_display();

  Fl_X* i = Fl_X::first;
  while (i) {
  	BView *view = i->xid->ChildAt(0);
  	if (view) {
  	  view->LockLooper();
  	  view->SetFlags(view->Flags() | B_INPUT_METHOD_AWARE);
  	  view->UnlockLooper();
  	}
    i = i->next;
  }

  im_enabled = 1;
}

void Fl::disable_im() {
  fl_open_display();

  Fl_X* i = Fl_X::first;
  while (i) {
  	BView *view = i->xid->ChildAt(0);
  	if (view) {
  	  view->LockLooper();
  	  view->SetFlags(view->Flags() & ~B_INPUT_METHOD_AWARE);
  	  view->UnlockLooper();
  	}
    i = i->next;
  }

  im_enabled = 0;
}

////////////////////////////////////////////////////////////////

int Fl::x()
{
  BScreen s;
  BRect r(s.Frame());

  return r.left;
}

int Fl::y()
{
  BScreen s;
  BRect r(s.Frame());

  return r.top;
}

int Fl::h()
{
  BScreen s;
  BRect r(s.Frame());

  return r.bottom - r.top + 1;
}

int Fl::w()
{
  BScreen s;
  BRect r(s.Frame());

  return r.right - r.left + 1;
}

void Fl::get_mouse(int &x, int &y) {
  BPoint p;
  uint32 buttons;
  // same name, funny isn't it?
  if (::get_mouse(&p, &buttons) != B_OK)
    return;
  x = p.x;
  y = p.y;
}

////////////////////////////////////////////////////////////////
// code used for selections:
#if 0

char *fl_selection_buffer[2];
int fl_selection_length[2];
int fl_selection_buffer_length[2];
char fl_i_own_selection[2];

UINT fl_get_lcid_codepage(LCID id)
{
  char buf[8];
  buf[GetLocaleInfo(id, LOCALE_IDEFAULTANSICODEPAGE, buf, 8)] = 0;
  return atol(buf);
}

// Convert \n -> \r\n
class Lf2CrlfConvert {
  char *out;
  int outlen;
public:
  Lf2CrlfConvert(const char *in, int inlen) {
    outlen = 0;
    const char *i;
    char *o;
    int lencount;
    // Predict size of \r\n conversion buffer
    for ( i=in, lencount = inlen; lencount--; ) {
      if ( *i == '\r' && *(i+1) == '\n' )	// leave \r\n untranslated
	{ i+=2; outlen+=2; }
      else if ( *i == '\n' )			// \n by itself? leave room to insert \r
	{ i++; outlen+=2; }
      else
	{ ++i; ++outlen; }
    }
    // Alloc conversion buffer + NULL
    out = new char[outlen+1];
    // Handle \n -> \r\n conversion
    for ( i=in, o=out, lencount = inlen; lencount--; ) {
      if ( *i == '\r' && *(i+1) == '\n' )	// leave \r\n untranslated
        { *o++ = *i++; *o++ = *i++; }
      else if ( *i == '\n' )			// \n by itself? insert \r
        { *o++ = '\r'; *o++ = *i++; }
      else
        { *o++ = *i++; }
    }
    *o++ = 0;
  }
  ~Lf2CrlfConvert() {
    delete[] out;
  }
  int GetLength() const { return(outlen); }
  const char* GetValue() const { return(out); }
};
#endif

void fl_update_clipboard(void) {
  Fl_Window *w1 = Fl::first_window();
  if (!w1)
    return;
//TODO

  // In case Windows managed to lob of a WM_DESTROYCLIPBOARD during
  // the above.
//  fl_i_own_selection[1] = 1;
}

// call this when you create a selection:
void Fl::copy(const char *stuff, int len, int clipboard, const char *type) {
  if (!stuff || len<0) return;
  if (clipboard >= 2)
    clipboard = 1; // Only on X11 do multiple clipboards make sense.
//TODO
}

// Call this when a "paste" operation happens:
void Fl::paste(Fl_Widget &receiver, int clipboard, const char *type) {
//TODO
}

int Fl::clipboard_contains(const char *type)
{
  int retval = 0;
//TODO
  return retval;
}

#if 0
static void fl_clipboard_notify_target(HWND wnd) {
  if (clipboard_wnd)
    return;

  // We get one fake WM_DRAWCLIPBOARD immediately, which we therefore
  // need to ignore.
  initial_clipboard = true;

  clipboard_wnd = wnd;
  next_clipboard_wnd = SetClipboardViewer(wnd);
}

static void fl_clipboard_notify_untarget(HWND wnd) {
  if (wnd != clipboard_wnd)
    return;

  // We might be called late in the cleanup where Windows has already
  // implicitly destroyed our clipboard window. At that point we need
  // to do some extra work to manually repair the clipboard chain.
  if (IsWindow(wnd))
    ChangeClipboardChain(wnd, next_clipboard_wnd);
  else {
    HWND tmp, head;

    tmp = CreateWindow("STATIC", "Temporary FLTK Clipboard Window", 0,
                       0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    if (tmp == NULL)
      return;

    head = SetClipboardViewer(tmp);
    if (head == NULL)
      ChangeClipboardChain(tmp, next_clipboard_wnd);
    else {
      SendMessage(head, WM_CHANGECBCHAIN, (WPARAM)wnd, (LPARAM)next_clipboard_wnd);
      ChangeClipboardChain(tmp, head);
    }

    DestroyWindow(tmp);
  }

  clipboard_wnd = next_clipboard_wnd = 0;
}

void fl_clipboard_notify_retarget(HWND wnd) {
  // The given window is getting destroyed. If it's part of the
  // clipboard chain then we need to unregister it and find a
  // replacement window.
  if (wnd != clipboard_wnd)
    return;

  fl_clipboard_notify_untarget(wnd);

  if (Fl::first_window())
    fl_clipboard_notify_target(fl_xid(Fl::first_window()));
}
#endif

void fl_clipboard_notify_change() {
#if 0
  // untarget clipboard monitor if no handlers are registered
  if (clipboard_wnd != NULL && fl_clipboard_notify_empty()) {
    fl_clipboard_notify_untarget(clipboard_wnd);
    return;
  }

  // if there are clipboard notify handlers but no window targeted
  // target first window if available
  if (clipboard_wnd == NULL && Fl::first_window())
    fl_clipboard_notify_target(fl_xid(Fl::first_window()));
#endif
}

////////////////////////////////////////////////////////////////
#if 0
void fl_get_codepage()
{
  HKL hkl = GetKeyboardLayout(0);
  TCHAR ld[8];

  GetLocaleInfo (LOWORD(hkl), LOCALE_IDEFAULTANSICODEPAGE, ld, 6);
  DWORD ccp = atol(ld);
  fl_codepage = ccp;
}

HWND fl_capture;

static int mouse_event(Fl_Window *window, int what, int button,
		       WPARAM wParam, LPARAM lParam)
{
  static int px, py, pmx, pmy;
  POINT pt;
  Fl::e_x = pt.x = (signed short)LOWORD(lParam);
  Fl::e_y = pt.y = (signed short)HIWORD(lParam);
  ClientToScreen(fl_xid(window), &pt);
  Fl::e_x_root = pt.x;
  Fl::e_y_root = pt.y;
#ifdef USE_CAPTURE_MOUSE_WIN
  Fl_Window *mouse_window = window;	// save "mouse window"
#endif
  while (window->parent()) {
    Fl::e_x += window->x();
    Fl::e_y += window->y();
    window = window->window();
  }

  ulong state = Fl::e_state & 0xff0000; // keep shift key states
#if 0
  // mouse event reports some shift flags, perhaps save them?
  if (wParam & MK_SHIFT) state |= FL_SHIFT;
  if (wParam & MK_CONTROL) state |= FL_CTRL;
#endif
  if (wParam & MK_LBUTTON) state |= FL_BUTTON1;
  if (wParam & MK_MBUTTON) state |= FL_BUTTON2;
  if (wParam & MK_RBUTTON) state |= FL_BUTTON3;
  Fl::e_state = state;

  switch (what) {
  case 1: // double-click
    if (Fl::e_is_click) {Fl::e_clicks++; goto J1;}
  case 0: // single-click
    Fl::e_clicks = 0;
  J1:
#ifdef USE_CAPTURE_MOUSE_WIN
    if (!fl_capture) SetCapture(fl_xid(mouse_window));  // use mouse window
#else
    if (!fl_capture) SetCapture(fl_xid(window));	// use main window
#endif
    Fl::e_keysym = FL_Button + button;
    Fl::e_is_click = 1;
    px = pmx = Fl::e_x_root; py = pmy = Fl::e_y_root;
    return Fl::handle(FL_PUSH,window);

  case 2: // release:
    if (!fl_capture) ReleaseCapture();
    Fl::e_keysym = FL_Button + button;
    return Fl::handle(FL_RELEASE,window);

  case 3: // move:
  default: // avoid compiler warning
    // MSWindows produces extra events even if mouse does not move, ignore em:
    if (Fl::e_x_root == pmx && Fl::e_y_root == pmy) return 1;
    pmx = Fl::e_x_root; pmy = Fl::e_y_root;
    if (abs(Fl::e_x_root-px)>5 || abs(Fl::e_y_root-py)>5) Fl::e_is_click = 0;
    return Fl::handle(FL_MOVE,window);

  }
}

// convert a MSWindows VK_x to an Fltk (X) Keysym:
// See also the inverse converter in Fl_get_key_win32.cxx
// This table is in numeric order by VK:
static const struct {unsigned short vk, fltk, extended;} vktab[] = {
  {VK_BACK,	FL_BackSpace},
  {VK_TAB,	FL_Tab},
  {VK_CLEAR,	FL_KP+'5',	0xff0b/*XK_Clear*/},
  {VK_RETURN,	FL_Enter,	FL_KP_Enter},
  {VK_SHIFT,	FL_Shift_L,	FL_Shift_R},
  {VK_CONTROL,	FL_Control_L,	FL_Control_R},
  {VK_MENU,	FL_Alt_L,	FL_Alt_R},
  {VK_PAUSE,	FL_Pause},
  {VK_CAPITAL,	FL_Caps_Lock},
  {VK_ESCAPE,	FL_Escape},
  {VK_SPACE,	' '},
  {VK_PRIOR,	FL_KP+'9',	FL_Page_Up},
  {VK_NEXT,	FL_KP+'3',	FL_Page_Down},
  {VK_END,	FL_KP+'1',	FL_End},
  {VK_HOME,	FL_KP+'7',	FL_Home},
  {VK_LEFT,	FL_KP+'4',	FL_Left},
  {VK_UP,	FL_KP+'8',	FL_Up},
  {VK_RIGHT,	FL_KP+'6',	FL_Right},
  {VK_DOWN,	FL_KP+'2',	FL_Down},
  {VK_SNAPSHOT,	FL_Print},	// does not work on NT
  {VK_INSERT,	FL_KP+'0',	FL_Insert},
  {VK_DELETE,	FL_KP+'.',	FL_Delete},
  {VK_LWIN,	FL_Meta_L},
  {VK_RWIN,	FL_Meta_R},
  {VK_APPS,	FL_Menu},
  {VK_SLEEP, FL_Sleep},
  {VK_MULTIPLY,	FL_KP+'*'},
  {VK_ADD,	FL_KP+'+'},
  {VK_SUBTRACT,	FL_KP+'-'},
  {VK_DECIMAL,	FL_KP+'.'},
  {VK_DIVIDE,	FL_KP+'/'},
  {VK_NUMLOCK,	FL_Num_Lock},
  {VK_SCROLL,	FL_Scroll_Lock},
# if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0500)
  {VK_BROWSER_BACK, FL_Back},
  {VK_BROWSER_FORWARD, FL_Forward},
  {VK_BROWSER_REFRESH, FL_Refresh},
  {VK_BROWSER_STOP, FL_Stop},
  {VK_BROWSER_SEARCH, FL_Search},
  {VK_BROWSER_FAVORITES, FL_Favorites},
  {VK_BROWSER_HOME, FL_Home_Page},
  {VK_VOLUME_MUTE, FL_Volume_Mute},
  {VK_VOLUME_DOWN, FL_Volume_Down},
  {VK_VOLUME_UP, FL_Volume_Up},
  {VK_MEDIA_NEXT_TRACK, FL_Media_Next},
  {VK_MEDIA_PREV_TRACK, FL_Media_Prev},
  {VK_MEDIA_STOP, FL_Media_Stop},
  {VK_MEDIA_PLAY_PAUSE, FL_Media_Play},
  {VK_LAUNCH_MAIL, FL_Mail},
#endif
  {0xba,	';'},
  {0xbb,	'='},
  {0xbc,	','},
  {0xbd,	'-'},
  {0xbe,	'.'},
  {0xbf,	'/'},
  {0xc0,	'`'},
  {0xdb,	'['},
  {0xdc,	'\\'},
  {0xdd,	']'},
  {0xde,	'\''},
  {VK_OEM_102,	FL_Iso_Key}
};
static int ms2fltk(WPARAM vk, int extended) {
  static unsigned short vklut[256];
  static unsigned short extendedlut[256];
  if (!vklut[1]) { // init the table
    unsigned int i;
    for (i = 0; i < 256; i++) vklut[i] = tolower(i);
    for (i=VK_F1; i<=VK_F16; i++) vklut[i] = i+(FL_F-(VK_F1-1));
    for (i=VK_NUMPAD0; i<=VK_NUMPAD9; i++) vklut[i] = i+(FL_KP+'0'-VK_NUMPAD0);
    for (i = 0; i < sizeof(vktab)/sizeof(*vktab); i++) {
      vklut[vktab[i].vk] = vktab[i].fltk;
      extendedlut[vktab[i].vk] = vktab[i].extended;
    }
    for (i = 0; i < 256; i++) if (!extendedlut[i]) extendedlut[i] = vklut[i];
  }
  return extended ? extendedlut[vk] : vklut[vk];
}

#if USE_COLORMAP
extern HPALETTE fl_select_palette(void); // in fl_color_win32.cxx
#endif
#endif

/////////////////////////////////////////////////////////////////////////////

static Fl_Window* resize_bug_fix;

extern void fl_save_pen(void);
extern void fl_restore_pen(void);

#if 0
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  // Copy the message to fl_msg so add_handler code can see it, it is
  // already there if this is called by DispatchMessage, but not if
  // Windows calls this directly.
  fl_msg.hwnd = hWnd;
  fl_msg.message = uMsg;
  fl_msg.wParam = wParam;
  fl_msg.lParam = lParam;
  //fl_msg.time = ???
  //fl_msg.pt = ???
  //fl_msg.lPrivate = ???

  Fl_Window *window = fl_find(hWnd);

  if (window) switch (uMsg) {

  case WM_QUIT: // this should not happen?
    Fl::fatal("WM_QUIT message");

  case WM_CLOSE: // user clicked close box
    Fl::handle(FL_CLOSE, window);
    return 0;

  case WM_SYNCPAINT :
  case WM_NCPAINT :
  case WM_ERASEBKGND :
    // Andreas Weitl - WM_SYNCPAINT needs to be passed to DefWindowProc
    // so that Windows can generate the proper paint messages...
    // Similarly, WM_NCPAINT and WM_ERASEBKGND need this, too...
    break;

  case WM_PAINT: {
    Fl_Region R;
    Fl_X *i = Fl_X::i(window);
    i->wait_for_expose = 0;
    char redraw_whole_window = false;
    if (!i->region && window->damage()) {
      // Redraw the whole window...
      i->region = CreateRectRgn(0, 0, window->w(), window->h());
      redraw_whole_window = true;
    }

    // We need to merge WIN32's damage into FLTK's damage.
    R = CreateRectRgn(0,0,0,0);
    int r = GetUpdateRgn(hWnd,R,0);
    if (r==NULLREGION && !redraw_whole_window) {
      XDestroyRegion(R);
      break;
    }

    if (i->region) {
      // Also tell WIN32 that we are drawing someplace else as well...
      CombineRgn(i->region, i->region, R, RGN_OR);
      XDestroyRegion(R);
    } else {
      i->region = R;
    }
    if (window->type() == FL_DOUBLE_WINDOW) ValidateRgn(hWnd,0);
    else ValidateRgn(hWnd,i->region);

    window->clear_damage((uchar)(window->damage()|FL_DAMAGE_EXPOSE));
    // These next two statements should not be here, so that all update
    // is deferred until Fl::flush() is called during idle.  However WIN32
    // apparently is very unhappy if we don't obey it and draw right now.
    // Very annoying!
    fl_GetDC(hWnd); // Make sure we have a DC for this window...
    fl_save_pen();
    i->flush();
    fl_restore_pen();
    window->clear_damage();
    } return 0;

  case WM_LBUTTONDOWN:  mouse_event(window, 0, 1, wParam, lParam); return 0;
  case WM_LBUTTONDBLCLK:mouse_event(window, 1, 1, wParam, lParam); return 0;
  case WM_LBUTTONUP:    mouse_event(window, 2, 1, wParam, lParam); return 0;
  case WM_MBUTTONDOWN:  mouse_event(window, 0, 2, wParam, lParam); return 0;
  case WM_MBUTTONDBLCLK:mouse_event(window, 1, 2, wParam, lParam); return 0;
  case WM_MBUTTONUP:    mouse_event(window, 2, 2, wParam, lParam); return 0;
  case WM_RBUTTONDOWN:  mouse_event(window, 0, 3, wParam, lParam); return 0;
  case WM_RBUTTONDBLCLK:mouse_event(window, 1, 3, wParam, lParam); return 0;
  case WM_RBUTTONUP:    mouse_event(window, 2, 3, wParam, lParam); return 0;

  case WM_MOUSEMOVE:
#ifdef USE_TRACK_MOUSE
    if (track_mouse_win != window) {
      TRACKMOUSEEVENT tme;
      tme.cbSize    = sizeof(TRACKMOUSEEVENT);
      tme.dwFlags   = TME_LEAVE;
      tme.hwndTrack = hWnd;
      _TrackMouseEvent(&tme);
      track_mouse_win = window;
    }
#endif // USE_TRACK_MOUSE
    mouse_event(window, 3, 0, wParam, lParam);
    return 0;

  case WM_MOUSELEAVE:
    if (track_mouse_win == window) { // we left the top level window !
      Fl_Window *tw = window;
      while (tw->parent()) tw = tw->window(); // find top level window
      Fl::belowmouse(0);
      Fl::handle(FL_LEAVE, tw);
    }
    track_mouse_win = 0; // force TrackMouseEvent() restart
    break;

  case WM_SETFOCUS:
    if ((Fl::modal_) && (Fl::modal_ != window)) {
      SetFocus(fl_xid(Fl::modal_));
      return 0;
    }
    Fl::handle(FL_FOCUS, window);
    break;

  case WM_KILLFOCUS:
    Fl::handle(FL_UNFOCUS, window);
    Fl::flush(); // it never returns to main loop when deactivated...
    break;

  case WM_SHOWWINDOW:
    if (!window->parent()) {
      Fl::handle(wParam ? FL_SHOW : FL_HIDE, window);
    }
    break;

  case WM_ACTIVATEAPP:
    // From eric@vfx.sel.sony.com, we should process WM_ACTIVATEAPP
    // messages to restore the correct state of the shift/ctrl/alt/lock
    // keys...  Added control, shift, alt, and meta keys, and changed
    // to use GetAsyncKeyState and do it when wParam is 1
    // (that means we have focus...)
    if (wParam)
    {
      ulong state = 0;
      if (GetAsyncKeyState(VK_CAPITAL)) state |= FL_CAPS_LOCK;
      if (GetAsyncKeyState(VK_NUMLOCK)) state |= FL_NUM_LOCK;
      if (GetAsyncKeyState(VK_SCROLL)) state |= FL_SCROLL_LOCK;
      if (GetAsyncKeyState(VK_CONTROL)&~1) state |= FL_CTRL;
      if (GetAsyncKeyState(VK_SHIFT)&~1) state |= FL_SHIFT;
      if (GetAsyncKeyState(VK_MENU)) state |= FL_ALT;
      if ((GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&~1) state |= FL_META;
      Fl::e_state = state;
      return 0;
    }
    break;

  case WM_INPUTLANGCHANGE:
    fl_get_codepage();
    break;
  case WM_IME_COMPOSITION:
//	if (!fl_is_nt4() && lParam & GCS_RESULTCLAUSE) {
//		HIMC himc = ImmGetContext(hWnd);
//		wlen = ImmGetCompositionStringW(himc, GCS_RESULTSTR,
//			wbuf, sizeof(wbuf)) / sizeof(short);
//		if (wlen < 0) wlen = 0;
//		wbuf[wlen] = 0;
//		ImmReleaseContext(hWnd, himc);
//	}
	break;
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
  case WM_KEYUP:
  case WM_SYSKEYUP:
    // save the keysym until we figure out the characters:
    Fl::e_keysym = Fl::e_original_keysym = ms2fltk(wParam,lParam&(1<<24));
    // See if TranslateMessage turned it into a WM_*CHAR message:
    if (PeekMessageW(&fl_msg, hWnd, WM_CHAR, WM_SYSDEADCHAR, PM_REMOVE))
    {
      uMsg = fl_msg.message;
      wParam = fl_msg.wParam;
      lParam = fl_msg.lParam;
    }
  case WM_DEADCHAR:
  case WM_SYSDEADCHAR:
  case WM_CHAR:
  case WM_SYSCHAR: {
    ulong state = Fl::e_state & 0xff000000; // keep the mouse button state
    // if GetKeyState is expensive we might want to comment some of these out:
    if (GetKeyState(VK_SHIFT)&~1) state |= FL_SHIFT;
    if (GetKeyState(VK_CAPITAL)) state |= FL_CAPS_LOCK;
    if (GetKeyState(VK_CONTROL)&~1) state |= FL_CTRL;
    // Alt gets reported for the Alt-GR switch on foreign keyboards.
    // so we need to check the event as well to get it right:
    if ((lParam&(1<<29)) //same as GetKeyState(VK_MENU)
	&& uMsg != WM_CHAR) state |= FL_ALT;
    if (GetKeyState(VK_NUMLOCK)) state |= FL_NUM_LOCK;
    if ((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&~1) {
      // WIN32 bug?  GetKeyState returns garbage if the user hit the
      // meta key to pop up start menu.  Sigh.
      if ((GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&~1)
	state |= FL_META;
    }
    if (GetKeyState(VK_SCROLL)) state |= FL_SCROLL_LOCK;
    Fl::e_state = state;
    static char buffer[1024];
    if (uMsg == WM_CHAR || uMsg == WM_SYSCHAR) {

      xchar u = (xchar) wParam;
//    Fl::e_length = fl_unicode2utf(&u, 1, buffer);
      Fl::e_length = fl_utf8fromwc(buffer, 1024, &u, 1);
      buffer[Fl::e_length] = 0;


    } else if (Fl::e_keysym >= FL_KP && Fl::e_keysym <= FL_KP_Last) {
      if (state & FL_NUM_LOCK) {
        // Convert to regular keypress...
	buffer[0] = Fl::e_keysym-FL_KP;
	Fl::e_length = 1;
      } else {
        // Convert to special keypress...
	buffer[0] = 0;
	Fl::e_length = 0;
	switch (Fl::e_keysym) {
	  case FL_KP + '0' :
	    Fl::e_keysym = FL_Insert;
	    break;
	  case FL_KP + '1' :
	    Fl::e_keysym = FL_End;
	    break;
	  case FL_KP + '2' :
	    Fl::e_keysym = FL_Down;
	    break;
	  case FL_KP + '3' :
	    Fl::e_keysym = FL_Page_Down;
	    break;
	  case FL_KP + '4' :
	    Fl::e_keysym = FL_Left;
	    break;
	  case FL_KP + '6' :
	    Fl::e_keysym = FL_Right;
	    break;
	  case FL_KP + '7' :
	    Fl::e_keysym = FL_Home;
	    break;
	  case FL_KP + '8' :
	    Fl::e_keysym = FL_Up;
	    break;
	  case FL_KP + '9' :
	    Fl::e_keysym = FL_Page_Up;
	    break;
	  case FL_KP + '.' :
	    Fl::e_keysym = FL_Delete;
	    break;
	  case FL_KP + '/' :
	  case FL_KP + '*' :
	  case FL_KP + '-' :
	  case FL_KP + '+' :
	    buffer[0] = Fl::e_keysym-FL_KP;
	    Fl::e_length = 1;
	    break;
	}
      }
    } else if ((lParam & (1<<31))==0) {
#ifdef FLTK_PREVIEW_DEAD_KEYS
      if ((lParam & (1<<24))==0) { // clear if dead key (always?)
        xchar u = (xchar) wParam;
        Fl::e_length = fl_utf8fromwc(buffer, 1024, &u, 1);
        buffer[Fl::e_length] = 0;
      } else { // set if "extended key" (never printable?)
        buffer[0] = 0;
        Fl::e_length = 0;
      }
#else
      buffer[0] = 0;
      Fl::e_length = 0;
#endif
    }
    Fl::e_text = buffer;
    if (lParam & (1<<31)) { // key up events.
      if (Fl::handle(FL_KEYUP, window)) return 0;
      break;
    }
    // for (int i = lParam&0xff; i--;)
    while (window->parent()) window = window->window();
    if (Fl::handle(FL_KEYBOARD,window)) {
	  if (uMsg==WM_DEADCHAR || uMsg==WM_SYSDEADCHAR)
		Fl::compose_state = 1;
	  return 0;
	}
    break;}

  case WM_MOUSEWHEEL: {
    static int delta = 0; // running total of all motion
    delta += (SHORT)(HIWORD(wParam));
    Fl::e_dx = 0;
    Fl::e_dy = -delta / WHEEL_DELTA;
    delta += Fl::e_dy * WHEEL_DELTA;
    if (Fl::e_dy) Fl::handle(FL_MOUSEWHEEL, window);
    return 0;
  }

// This is only defined on Vista and upwards...
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif
      
  case WM_MOUSEHWHEEL: {
      static int delta = 0; // running total of all motion
      delta += (SHORT)(HIWORD(wParam));
      Fl::e_dy = 0;
      Fl::e_dx = delta / WHEEL_DELTA;
      delta -= Fl::e_dx * WHEEL_DELTA;
      if (Fl::e_dx) Fl::handle(FL_MOUSEWHEEL, window);
      return 0;
    }

  case WM_GETMINMAXINFO:
    Fl_X::i(window)->set_minmax((LPMINMAXINFO)lParam);
    break;

  case WM_SIZE:
    if (!window->parent()) {
      if (wParam == SIZE_MINIMIZED || wParam == SIZE_MAXHIDE) {
	Fl::handle(FL_HIDE, window);
      } else {
	Fl::handle(FL_SHOW, window);
	resize_bug_fix = window;
	window->size(LOWORD(lParam), HIWORD(lParam));
      }
    }
    break;

  case WM_MOVE: {
    resize_bug_fix = window;
    int nx = LOWORD(lParam);
    int ny = HIWORD(lParam);
    if (nx & 0x8000) nx -= 65536;
    if (ny & 0x8000) ny -= 65536;
    window->position(nx, ny); }
    break;

  case WM_SETCURSOR:
    if (LOWORD(lParam) == HTCLIENT) {
      while (window->parent()) window = window->window();
      SetCursor(Fl_X::i(window)->cursor);
      return 0;
    }
    break;

#if USE_COLORMAP
  case WM_QUERYNEWPALETTE :
    fl_GetDC(hWnd);
    if (fl_select_palette()) InvalidateRect(hWnd, NULL, FALSE);
    break;

  case WM_PALETTECHANGED:
    fl_GetDC(hWnd);
    if ((HWND)wParam != hWnd && fl_select_palette()) UpdateColors(fl_gc);
    break;

  case WM_CREATE :
    fl_GetDC(hWnd);
    fl_select_palette();
    break;
#endif

  case WM_DESTROYCLIPBOARD:
    fl_i_own_selection[1] = 0;
    return 1;

  case WM_DISPLAYCHANGE: // occurs when screen configuration (number, position) changes
    Fl::call_screen_init();
    Fl::handle(FL_SCREEN_CONFIGURATION_CHANGED, NULL);
    return 0;

  case WM_CHANGECBCHAIN:
    if ((hWnd == clipboard_wnd) && (next_clipboard_wnd == (HWND)wParam))
      next_clipboard_wnd = (HWND)lParam;
    else
      SendMessage(next_clipboard_wnd, WM_CHANGECBCHAIN, wParam, lParam);
    return 0;

  case WM_DRAWCLIPBOARD:
    // When the clipboard moves between two FLTK windows,
    // fl_i_own_selection will temporarily be false as we are
    // processing this message. Hence the need to use fl_find().
    if (!initial_clipboard && !fl_find(GetClipboardOwner()))
      fl_trigger_clipboard_notify(1);
    initial_clipboard = false;

    if (next_clipboard_wnd)
      SendMessage(next_clipboard_wnd, WM_DRAWCLIPBOARD, wParam, lParam);

    return 0;

  default:
    if (Fl::handle(0,0)) return 0;
    break;
  }


  return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
#endif

////////////////////////////////////////////////////////////////
// This function gets the dimensions of the top/left borders and
// the title bar, if there is one, based on the FL_BORDER, FL_MODAL
// and FL_NONMODAL flags, and on the window's size range.
// It returns the following values:
//
// value | border | title bar
//   0   |  none  |   no
//   1   |  fix   |   yes
//   2   |  size  |   yes

#if 0
static int fake_X_wm_style(const Fl_Window* w,int &X,int &Y, int &bt,int &bx, int &by, DWORD style, DWORD styleEx,
                     int w_maxw, int w_minw, int w_maxh, int w_minh, uchar w_size_range_set) {
  int W, H, xoff, yoff, dx, dy;
  int ret = bx = by = bt = 0;

  int fallback = 1;
  if (!w->parent()) {
    if (fl_xid(w) || style) {
      // The block below calculates the window borders by requesting the
      // required decorated window rectangle for a desired client rectangle.
      // If any part of the function above fails, we will drop to a 
      // fallback to get the best guess which is always available.
      
	 if (!style) {
	     HWND hwnd = fl_xid(w);
          // request the style flags of this window, as WIN32 sees them
          style = GetWindowLong(hwnd, GWL_STYLE);
          styleEx = GetWindowLong(hwnd, GWL_EXSTYLE);
	 }

      RECT r;
      r.left = w->x();
      r.top = w->y();
      r.right = w->x()+w->w();
      r.bottom = w->y()+w->h();
      // get the decoration rectangle for the desired client rectangle
      BOOL ok = AdjustWindowRectEx(&r, style, FALSE, styleEx);
      if (ok) {
        X = r.left;
        Y = r.top;
        W = r.right - r.left;
        H = r.bottom - r.top;
        bx = w->x() - r.left;
        by = r.bottom - w->y() - w->h(); // height of the bootm frame
        bt = w->y() - r.top - by; // height of top caption bar
        xoff = bx;
        yoff = by + bt;
        dx = W - w->w();
        dy = H - w->h();
        if (w_size_range_set && (w_maxw != w_minw || w_maxh != w_minh))
          ret = 2;
        else
          ret = 1;
        fallback = 0;
      }
    }
  }
  // This is the original (pre 1.1.7) routine to calculate window border sizes.
  if (fallback) {
    if (w->border() && !w->parent()) {
      if (w_size_range_set && (w_maxw != w_minw || w_maxh != w_minh)) {
	ret = 2;
	bx = GetSystemMetrics(SM_CXSIZEFRAME);
	by = GetSystemMetrics(SM_CYSIZEFRAME);
      } else {
	ret = 1;
	int padding = GetSystemMetrics(SM_CXPADDEDBORDER);
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0);
	bx = GetSystemMetrics(SM_CXFIXEDFRAME) + (padding ? padding + ncm.iBorderWidth : 0);
	by = GetSystemMetrics(SM_CYFIXEDFRAME) + (padding ? padding + ncm.iBorderWidth : 0);
      }
      bt = GetSystemMetrics(SM_CYCAPTION);
    }
    //The coordinates of the whole window, including non-client area
    xoff = bx;
    yoff = by + bt;
    dx = 2*bx;
    dy = 2*by + bt;
    X = w->x()-xoff;
    Y = w->y()-yoff;
    W = w->w()+dx;
    H = w->h()+dy;
  }

  //Proceed to positioning the window fully inside the screen, if possible
  //Find screen that contains most of the window
  //FIXME: this ought to be the "work area" instead of the entire screen !
  int scr_x, scr_y, scr_w, scr_h;
  Fl::screen_xywh(scr_x, scr_y, scr_w, scr_h, X, Y, W, H);
  //Make border's lower right corner visible
  if (scr_x+scr_w < X+W) X = scr_x+scr_w - W;
  if (scr_y+scr_h < Y+H) Y = scr_y+scr_h - H;
  //Make border's upper left corner visible
  if (X<scr_x) X = scr_x;
  if (Y<scr_y) Y = scr_y;
  //Make client area's lower right corner visible
  if (scr_x+scr_w < X+dx+ w->w()) X = scr_x+scr_w - w->w() - dx;
  if (scr_y+scr_h < Y+dy+ w->h()) Y = scr_y+scr_h - w->h() - dy;
  //Make client area's upper left corner visible
  if (X+xoff < scr_x) X = scr_x-xoff;
  if (Y+yoff < scr_y) Y = scr_y-yoff;
  //Return the client area's top left corner in (X,Y)
  X+=xoff;
  Y+=yoff;

  if (w->fullscreen_active()) {
    bx = by = bt = 0;
  }
  return ret;
}
#endif

int Fl_X::fake_X_wm(const Fl_Window* w,int &X,int &Y, int &bt,int &bx, int &by) {
#if 0
  int val = fake_X_wm_style(w, X, Y, bt, bx, by, 0, 0, w->maxw, w->minw, w->maxh, w->minh, w->size_range_set);
  if (dwmapi_dll) {
    RECT r;
    if (!DwmGetWindowAttribute) DwmGetWindowAttribute = (DwmGetWindowAttribute_type)GetProcAddress(dwmapi_dll, "DwmGetWindowAttribute");
    if (DwmGetWindowAttribute) {
      if ( DwmGetWindowAttribute(fl_xid(w), DWMA_EXTENDED_FRAME_BOUNDS, &r, sizeof(RECT)) == S_OK ) {
        bx = (r.right - r.left - w->w())/2;
        by = bx;
        bt = r.bottom - r.top - w->h() - 2*by;
      }
    }
  }
  return val;
#endif
  return 0;
}

////////////////////////////////////////////////////////////////

void Fl_Window::resize(int X,int Y,int W,int H) {
  int bx, by, bt = 0;
  Fl_Window *parent;
  int is_a_resize = (W != w() || H != h());
  //  printf("Fl_Window::resize(X=%d, Y=%d, W=%d, H=%d), is_a_resize=%d, resize_from_system=%p, this=%p\n",
  //         X, Y, W, H, is_a_resize, resize_from_system, this);
  if (X != x() || Y != y()) set_flag(FORCE_POSITION);
  else if (!is_a_resize) {
    resize_from_system = 0;
    return;
    }
  if ( (resize_from_system!=this) && shown()) {
    if (is_a_resize) {
      if (resizable()) {
        if (W<minw) minw = W; // user request for resize takes priority
        if (maxw && W>maxw) maxw = W; // over a previously set size_range
        if (H<minh) minh = H;
        if (maxh && H>maxh) maxh = H;
        size_range(minw, minh, maxw, maxh);
      } else {
        size_range(W, H, W, H);
      }
      Fl_Group::resize(X,Y,W,H);
      // transmit changes in FLTK coords to cocoa
      //get_window_frame_sizes(bx, by, bt);
      bx = X; by = Y;
      parent = window();
      while (parent) {
        bx += parent->x();
        by += parent->y();
        parent = parent->window();
      }
      //BRect r(0, 0, W - 1, H - 1 + (border()?bt:0));
      //r.OffsetBy(bx, by);
      if (visible_r()) {
        BWindow *w = fl_xid(this);
        w->Lock();
        w->MoveTo(bx, by);
        w->ResizeTo(W, H + (border()?bt:0));
        w->Unlock();
      }
    } else {
      bx = X; by = Y;
      parent = window();
      while (parent) {
        bx += parent->x();
        by += parent->y();
        parent = parent->window();
      }
      BPoint pt(bx, by);
      if (visible_r()) {
        BWindow *w = fl_xid(this);
        w->Lock();
        w->MoveTo(pt);
        w->Unlock();
      }
    }
  }
  else {
    resize_from_system = 0;
    if (is_a_resize) {
      Fl_Group::resize(X,Y,W,H);
      if (shown()) {
        redraw();
      }
    } else {
      x(X); y(Y);
    }
  }
#if 0
  UINT flags = SWP_NOSENDCHANGING | SWP_NOZORDER 
             | SWP_NOACTIVATE | SWP_NOOWNERZORDER;
  int is_a_resize = (W != w() || H != h());
  int resize_from_program = (this != resize_bug_fix);
  if (!resize_from_program) resize_bug_fix = 0;
  if (X != x() || Y != y()) {
    force_position(1);
  } else {
    if (!is_a_resize) return;
    flags |= SWP_NOMOVE;
  }
  if (is_a_resize) {
    Fl_Group::resize(X,Y,W,H);
    if (visible_r()) {
      redraw(); 
      // only wait for exposure if this window has a size - a window 
      // with no width or height will never get an exposure event
      if (i && W>0 && H>0)
        i->wait_for_expose = 1;
    }
  } else {
    x(X); y(Y);
    flags |= SWP_NOSIZE;
  }
  if (!border()) flags |= SWP_NOACTIVATE;
  if (resize_from_program && shown()) {
    if (!resizable()) size_range(w(),h(),w(),h());
    int dummy_x, dummy_y, bt, bx, by;
    //Ignore window managing when resizing, so that windows (and more
    //specifically menus) can be moved offscreen.
    if (Fl_X::fake_X_wm(this, dummy_x, dummy_y, bt, bx, by)) {
      X -= bx;
      Y -= by+bt;
      W += 2*bx;
      H += 2*by+bt;
    }
    // avoid zero size windows. A zero sized window on Win32
    // will cause continouly  new redraw events.
    if (W<=0) W = 1;
    if (H<=0) H = 1;
    SetWindowPos(i->xid, 0, X, Y, W, H, flags);
  }
#endif
}

void Fl_X::make_fullscreen(int X, int Y, int W, int H) {
  int top, bottom, left, right;
  int sx, sy, sw, sh;
#if 0

  top = w->fullscreen_screen_top;
  bottom = w->fullscreen_screen_bottom;
  left = w->fullscreen_screen_left;
  right = w->fullscreen_screen_right;

  if ((top < 0) || (bottom < 0) || (left < 0) || (right < 0)) {
    top = Fl::screen_num(X, Y, W, H);
    bottom = top;
    left = top;
    right = top;
  }

  Fl::screen_xywh(sx, sy, sw, sh, top);
  Y = sy;
  Fl::screen_xywh(sx, sy, sw, sh, bottom);
  H = sy + sh - Y;
  Fl::screen_xywh(sx, sy, sw, sh, left);
  X = sx;
  Fl::screen_xywh(sx, sy, sw, sh, right);
  W = sx + sw - X;

  DWORD flags = GetWindowLong(xid, GWL_STYLE);
  flags = flags & ~(WS_THICKFRAME|WS_CAPTION);
  SetWindowLong(xid, GWL_STYLE, flags);

  // SWP_NOSENDCHANGING is so that we can override size limits
  SetWindowPos(xid, HWND_TOP, X, Y, W, H, SWP_NOSENDCHANGING | SWP_FRAMECHANGED);
#endif
}

void Fl_Window::fullscreen_x() {
  _set_fullscreen();
  i->make_fullscreen(x(), y(), w(), h());
  Fl::handle(FL_FULLSCREEN, this);
}

void Fl_Window::fullscreen_off_x(int X, int Y, int W, int H) {
#if 0
  _clear_fullscreen();
  DWORD style = GetWindowLong(fl_xid(this), GWL_STYLE);
  // Remove the xid temporarily so that Fl_X::fake_X_wm() behaves like it
  // does in Fl_X::make().
  HWND xid = fl_xid(this);
  Fl_X::i(this)->xid = NULL;
  int wx, wy, bt, bx, by;
  switch (Fl_X::fake_X_wm(this, wx, wy, bt, bx, by)) {
  case 0: 
    break;
  case 1: 
    style |= WS_CAPTION; 
    break;
  case 2: 
    if (border()) {
      style |= WS_THICKFRAME | WS_CAPTION; 
    }
    break;
  }
  Fl_X::i(this)->xid = xid;
  // Adjust for decorations (but not if that puts the decorations
  // outside the screen)
  if ((X != x()) || (Y != y())) {
    X -= bx;
    Y -= by+bt;
  }
  W += bx*2;
  H += by*2+bt;
  SetWindowLong(fl_xid(this), GWL_STYLE, style);
  SetWindowPos(fl_xid(this), 0, X, Y, W, H,
               SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED);
  Fl::handle(FL_FULLSCREEN, this);
#endif
}


/*
 * Initialize the given port for redraw and call the window's flush() to actually draw the content
 */ 
void Fl_X::flush()
{
  if (w->as_gl_window()) {
    w->flush();
  } else {
    // [Disable|Enable]Updates() don't really help there
    // [Begin|End]ViewTransaction() neither
    //if (fl_window)
    //  fl_window->DisableUpdates();
    w->flush();
    //if (fl_window)
    //  fl_window->EnableUpdates();
  }
}

////////////////////////////////////////////////////////////////

void fl_fix_focus(); // in Fl.cxx

char fl_show_iconic;	// hack for Fl_Window::iconic()
// int fl_background_pixel = -1; // color to use for background
int fl_disable_transient_for; // secret method of removing TRANSIENT_FOR

Fl_X* Fl_X::make(Fl_Window* w) {
  Fl_Group::current(0); // get rid of very common user bug: forgot end()

  fl_open_display();

  // if the window is a subwindow and our parent is not mapped yet, we
  // mark this window visible, so that mapping the parent at a later
  // point in time will call this function again to finally map the subwindow.
  if (w->parent() && !Fl_X::i(w->window())) {
    w->set_visible();
    return 0L;
  }

  window_look look = B_NO_BORDER_WINDOW_LOOK;
  window_feel feel = B_NORMAL_WINDOW_FEEL;
  uint32 flags = B_ASYNCHRONOUS_CONTROLS;

  if (w->parent()) {
    w->border(0);
    fl_show_iconic = 0;
    //parent = fl_xid(w->window());
  }
  if (w->border()) look = B_TITLED_WINDOW_LOOK;

#if 0
  if (fl_show_iconic && !w->parent()) { // prevent window from being out of work area when created iconized
    int sx, sy, sw, sh;
    Fl::screen_work_area (sx, sy, sw, sh, w->x(), w->y());
    if (w->x() < sx) w->x(sx);
    if (w->y() < sy) w->y(sy);
  }
#endif

  int xp = w->x();
  int yp = w->y();
  int wp = w->w();
  int hp = w->h();
/*
  if (w->size_range_set) {
    if ( w->minh == w->maxh && w->minw == w->maxw) {
      if (w->border()) flags |= B_NOT_RESIZABLE;
    }
  } else {
    if (w->resizable()) {
      Fl_Widget *o = w->resizable();
      int minw = o->w(); if (minw > 100) minw = 100;
      int minh = o->h(); if (minh > 100) minh = 100;
      w->size_range(w->w() - o->w() + minw, w->h() - o->h() + minh, 0, 0);
    } else {
      w->size_range(w->w(), w->h(), w->w(), w->h());
      if (w->border()) flags |= B_NOT_RESIZABLE;
    }
  }
*/
  int xwm = xp, ywm = yp, bt, bx, by;

  if (!fake_X_wm(w, xwm, ywm, bt, bx, by)) {
    // menu windows and tooltips
    if (w->modal()||w->tooltip_window()) {
      feel = B_MODAL_APP_WINDOW_FEEL;//B_MODAL_SUBSET_WINDOW_FEEL;
    }
  }
  if (w->modal()) {
    flags |= B_NOT_MINIMIZABLE;
    feel = B_MODAL_APP_WINDOW_FEEL;//B_MODAL_SUBSET_WINDOW_FEEL;
  }
/*
  if (by+bt) {
    wp += 2*bx;
    hp += 2*by+bt;
  }
  if (w->force_position()) {
    if (!Fl::grab()) {
      xp = xwm; yp = ywm;
      w->x(xp);w->y(yp);
    }
    xp -= bx;
    yp -= by+bt;
  }
*/


  Fl_X* x = new Fl_X;
  x->other_xid = 0;
  x->setwindow(w);
  x->region = 0;
//  x->private_dc = 0;
  x->cursor = NULL;
//  x->custom_cursor = 0;

  BRect frame(0, 0, wp - 1, hp - 1);

  FLView *view = new FLView(frame, "FLView");

  frame.OffsetTo(xp, yp);
  // offset child windows
  if (w->parent() && w->parent()->window()) {
    Fl_Window *p = w->parent()->window();
    BWindow *xid = fl_xid(p);
    if (xid)
      frame.OffsetBy(xid->Frame().LeftTop());
  }

  FLWindow *win = new FLWindow(view, frame, w->label(), look, feel, flags);
  x->xid = win;

  win->AddChild(view);
  view->MakeFocus();

  x->w = w; w->i = x;
  x->wait_for_expose = 1;

  if (!w->parent()) {
    x->next = Fl_X::first;
    Fl_X::first = x;
  } else if (Fl_X::first) {
    x->next = Fl_X::first->next;
    Fl_X::first->next = x;
  }
  else {
    x->next = NULL;
    Fl_X::first = x;
  }

  if (w->size_range_set) w->size_range_();


  if ( w->border() || (!w->modal() && !w->tooltip_window()) ) {
    Fl_Tooltip::enter(0);
  }

  if (w->modal()) Fl::modal_ = w; 

  if (w->parent()) {
    Fl_X *p = Fl_X::i(w->window());
    if (p && p->xid) {
      p->xid->AddToSubset(win);
      // not really helping
      //win->SetFeel(B_FLOATING_SUBSET_WINDOW_FEEL);
    }
  }

  w->set_visible();
  if ( w->border() || (!w->modal() && !w->tooltip_window()) ) Fl::handle(FL_FOCUS, w);

  if (fl_show_iconic) {
    fl_show_iconic = 0;
    w->handle(FL_SHOW); // create subwindows if any
  } else if (w->parent()) { // a subwindow
    // needed if top window was first displayed miniaturized
    FLWindow *pxid = fl_xid(w->top_window());
  } else { // a top-level window
  }

  int old_event = Fl::e_number;
  w->handle(Fl::e_number = FL_SHOW);
  Fl::e_number = old_event;

  x->set_icons();

  win->Show();

  return x;
}

void Fl_X::destroy() {
  if (xid) {
    xid->Lock();
    xid->Quit();
  }
}

void Fl_X::map() {
  if (w && xid) {
    // seems like we're off, like in unittests's schemes test, so force things
    xid->Lock();
    while (xid->IsHidden())
      xid->Show();
    xid->Unlock();
  }
}

void Fl_X::unmap() {
  if (w && xid) {
    // seems like we're off, like in unittests's schemes test, so force things
    xid->Lock();
    while (!xid->IsHidden())
      xid->Hide();
    xid->Unlock();
  }
}


////////////////////////////////////////////////////////////////

//BApplication *fl_display = NULL;

void Fl_Window::size_range_() {
  size_range_set = 1;
}

#if 0
void Fl_X::set_minmax(LPMINMAXINFO minmax)
{
  int td, wd, hd, dummy_x, dummy_y;

  fake_X_wm(w, dummy_x, dummy_y, td, wd, hd);
  wd *= 2;
  hd *= 2;
  hd += td;

  minmax->ptMinTrackSize.x = w->minw + wd;
  minmax->ptMinTrackSize.y = w->minh + hd;
  if (w->maxw) {
    minmax->ptMaxTrackSize.x = w->maxw + wd;
    minmax->ptMaxSize.x = w->maxw + wd;
  }
  if (w->maxh) {
    minmax->ptMaxTrackSize.y = w->maxh + hd;
    minmax->ptMaxSize.y = w->maxh + hd;
  }
}
#endif

////////////////////////////////////////////////////////////////

// returns pointer to the filename, or null if name ends with '/'
const char *fl_filename_name(const char *name) {
  const char *p,*q;
  if (!name) return (0);
  for (p=q=name; *p;) if (*p++ == '/') q = p;
  return q;
}

void Fl_Window::label(const char *name,const char *iname) {
  Fl_Widget::label(name);
  iconlabel_ = iname;
  if (shown() && !parent()) {
    if (!name) name = "";
    int namelen = strlen(name);
    if (!iname) iname = fl_filename_name(name);
    int inamelen = strlen(iname);
    if (i->xid) {
      BString title(name);
      //if (inamelen > 0)
      //  title << " : " << iname;
      i->xid->Lock();
      i->xid->SetTitle(title.String());
      i->xid->Unlock();
    }
  }
}

////////////////////////////////////////////////////////////////
#if 0
static HICON image_to_icon(const Fl_RGB_Image *image, bool is_icon,
                           int hotx, int hoty) {
  BITMAPV5HEADER bi;
  HBITMAP bitmap, mask;
  DWORD *bits;
  HICON icon;

  if (!is_icon) {
    if ((hotx < 0) || (hotx >= image->w()))
      return NULL;
    if ((hoty < 0) || (hoty >= image->h()))
      return NULL;
  }

  memset(&bi, 0, sizeof(BITMAPV5HEADER));

  bi.bV5Size        = sizeof(BITMAPV5HEADER);
  bi.bV5Width       = image->w();
  bi.bV5Height      = -image->h(); // Negative for top-down
  bi.bV5Planes      = 1;
  bi.bV5BitCount    = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask     = 0x00FF0000;
  bi.bV5GreenMask   = 0x0000FF00;
  bi.bV5BlueMask    = 0x000000FF;
  bi.bV5AlphaMask   = 0xFF000000;

  HDC hdc;

  hdc = GetDC(NULL);
  bitmap = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
  ReleaseDC(NULL, hdc);

  if (bits == NULL)
    return NULL;

  const uchar *i = (const uchar*)*image->data();
  for (int y = 0;y < image->h();y++) {
    for (int x = 0;x < image->w();x++) {
      switch (image->d()) {
      case 1:
        *bits = (0xff<<24) | (i[0]<<16) | (i[0]<<8) | i[0];
        break;
      case 2:
        *bits = (i[1]<<24) | (i[0]<<16) | (i[0]<<8) | i[0];
        break;
      case 3:
        *bits = (0xff<<24) | (i[0]<<16) | (i[1]<<8) | i[2];
        break;
      case 4:
        *bits = (i[3]<<24) | (i[0]<<16) | (i[1]<<8) | i[2];
        break;
      }
      i += image->d();
      bits++;
    }
    i += image->ld();
  }

  // A mask bitmap is still needed even though it isn't used
  mask = CreateBitmap(image->w(),image->h(),1,1,NULL);
  if (mask == NULL) {
    DeleteObject(bitmap);
    return NULL;
  }

  ICONINFO ii;

  ii.fIcon    = is_icon;
  ii.xHotspot = hotx;
  ii.yHotspot = hoty;
  ii.hbmMask  = mask;
  ii.hbmColor = bitmap;

  icon = CreateIconIndirect(&ii);

  DeleteObject(bitmap);
  DeleteObject(mask);

  if (icon == NULL)
    return NULL;

  return icon;
}
#endif
////////////////////////////////////////////////////////////////
#if 0
static HICON default_big_icon = NULL;
static HICON default_small_icon = NULL;

static const Fl_RGB_Image *find_best_icon(int ideal_width, 
                                          const Fl_RGB_Image *icons[],
                                          int count) {
  const Fl_RGB_Image *best;

  best = NULL;

  for (int i = 0;i < count;i++) {
    if (best == NULL)
      best = icons[i];
    else {
      if (best->w() < ideal_width) {
        if (icons[i]->w() > best->w())
          best = icons[i];
      } else {
        if ((icons[i]->w() >= ideal_width) &&
            (icons[i]->w() < best->w()))
          best = icons[i];
      }
    }
  }

  return best;
}
#endif

#if 0
void Fl_X::set_default_icons(const Fl_RGB_Image *icons[], int count) {
  const Fl_RGB_Image *best_big, *best_small;

  if (default_big_icon != NULL)
    DestroyIcon(default_big_icon);
  if (default_small_icon != NULL)
    DestroyIcon(default_small_icon);

  default_big_icon = NULL;
  default_small_icon = NULL;

  best_big = find_best_icon(GetSystemMetrics(SM_CXICON), icons, count);
  best_small = find_best_icon(GetSystemMetrics(SM_CXSMICON), icons, count);

  if (best_big != NULL)
    default_big_icon = image_to_icon(best_big, true, 0, 0);

  if (best_small != NULL)
    default_small_icon = image_to_icon(best_small, true, 0, 0);
}
#endif

#if 0
void Fl_X::set_default_icons(HICON big_icon, HICON small_icon) {
  if (default_big_icon != NULL)
    DestroyIcon(default_big_icon);
  if (default_small_icon != NULL)
    DestroyIcon(default_small_icon);

  default_big_icon = NULL;
  default_small_icon = NULL;

  if (big_icon != NULL)
    default_big_icon = CopyIcon(big_icon);
  if (small_icon != NULL)
    default_small_icon = CopyIcon(small_icon);
}
#endif

#if 0
void Fl_X::set_icons() {
  HICON big_icon, small_icon;

  // Windows doesn't copy the icons, so we have to "leak" them when
  // setting, and clean up when we change to some other icons.
  big_icon = (HICON)SendMessage(xid, WM_GETICON, ICON_BIG, 0);
  if ((big_icon != NULL) && (big_icon != default_big_icon))
    DestroyIcon(big_icon);
  small_icon = (HICON)SendMessage(xid, WM_GETICON, ICON_SMALL, 0);
  if ((small_icon != NULL) && (small_icon != default_small_icon))
    DestroyIcon(small_icon);

  big_icon = NULL;
  small_icon = NULL;

  if (w->icon_->count) {
    const Fl_RGB_Image *best_big, *best_small;

    best_big = find_best_icon(GetSystemMetrics(SM_CXICON),
                              (const Fl_RGB_Image **)w->icon_->icons,
                              w->icon_->count);
    best_small = find_best_icon(GetSystemMetrics(SM_CXSMICON),
                                (const Fl_RGB_Image **)w->icon_->icons,
                                w->icon_->count);

    if (best_big != NULL)
      big_icon = image_to_icon(best_big, true, 0, 0);
    if (best_small != NULL)
      small_icon = image_to_icon(best_small, true, 0, 0);
  } else {
    if ((w->icon_->big_icon != NULL) || (w->icon_->small_icon != NULL)) {
      big_icon = w->icon_->big_icon;
      small_icon = w->icon_->small_icon;
    } else {
      big_icon = default_big_icon;
      small_icon = default_small_icon;
    }
  }

  SendMessage(xid, WM_SETICON, ICON_BIG, (LPARAM)big_icon);
  SendMessage(xid, WM_SETICON, ICON_SMALL, (LPARAM)small_icon);
}
#endif

/** Sets the default window icons.

  Convenience function to set the default icons using Windows'
  native HICON icon handles.

  The given icons are copied. You can free the icons immediately after
  this call.

  \param[in] big_icon default large icon for all windows
                      subsequently created
  \param[in] small_icon default small icon for all windows
                        subsequently created

  \see Fl_Window::default_icon(const Fl_RGB_Image *)
  \see Fl_Window::default_icons(const Fl_RGB_Image *[], int)
  \see Fl_Window::icon(const Fl_RGB_Image *)
  \see Fl_Window::icons(const Fl_RGB_Image *[], int)
  \see Fl_Window::icons(HICON, HICON)
 */
#if 0
void Fl_Window::default_icons(HICON big_icon, HICON small_icon) {
  Fl_X::set_default_icons(big_icon, small_icon);
}
#endif

/** Sets the window icons.

  Convenience function to set this window's icons using Windows'
  native HICON icon handles.

  The given icons are copied. You can free the icons immediately after
  this call.

  \param[in] big_icon large icon for this window
  \param[in] small_icon small icon for this windows

  \see Fl_Window::default_icon(const Fl_RGB_Image *)
  \see Fl_Window::default_icons(const Fl_RGB_Image *[], int)
  \see Fl_Window::default_icons(HICON, HICON)
  \see Fl_Window::icon(const Fl_RGB_Image *)
  \see Fl_Window::icons(const Fl_RGB_Image *[], int)
 */
#if 0
void Fl_Window::icons(HICON big_icon, HICON small_icon) {
  free_icons();

  if (big_icon != NULL)
    icon_->big_icon = CopyIcon(big_icon);
  if (small_icon != NULL)
    icon_->small_icon = CopyIcon(small_icon);

  if (i)
    i->set_icons();
}
#endif

////////////////////////////////////////////////////////////////

int Fl_X::set_cursor(Fl_Cursor c) {
  BCursorID n;
  BCursor *new_cursor;

  fl_open_display();

  switch (c) {
  case FL_CURSOR_NONE:    n = B_CURSOR_ID_NO_CURSOR; break;
  case FL_CURSOR_ARROW:   n = B_CURSOR_ID_SYSTEM_DEFAULT; break;
  case FL_CURSOR_CROSS:   n = B_CURSOR_ID_CROSS_HAIR; break;
  case FL_CURSOR_WAIT:    n = B_CURSOR_ID_PROGRESS; break;
  case FL_CURSOR_INSERT:  n = B_CURSOR_ID_I_BEAM; break;
  case FL_CURSOR_HAND:    n = B_CURSOR_ID_GRAB; break;
  case FL_CURSOR_HELP:    n = B_CURSOR_ID_HELP; break;
  case FL_CURSOR_MOVE:    n = B_CURSOR_ID_MOVE; break;
  case FL_CURSOR_N:       n = B_CURSOR_ID_RESIZE_NORTH; break;
  case FL_CURSOR_S:       n = B_CURSOR_ID_RESIZE_SOUTH; break;
  case FL_CURSOR_NS:      n = B_CURSOR_ID_RESIZE_NORTH_SOUTH; break;
  case FL_CURSOR_NE:      n = B_CURSOR_ID_RESIZE_NORTH_EAST; break;
  case FL_CURSOR_SW:      n = B_CURSOR_ID_RESIZE_SOUTH_WEST; break;
  case FL_CURSOR_NESW:    n = B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST; break;
  case FL_CURSOR_E:       n = B_CURSOR_ID_RESIZE_EAST; break;
  case FL_CURSOR_W:       n = B_CURSOR_ID_RESIZE_WEST; break;
  case FL_CURSOR_WE:      n = B_CURSOR_ID_RESIZE_EAST_WEST; break;
  case FL_CURSOR_SE:      n = B_CURSOR_ID_RESIZE_SOUTH_EAST; break;
  case FL_CURSOR_NW:      n = B_CURSOR_ID_RESIZE_NORTH_WEST; break;
  case FL_CURSOR_NWSE:    n = B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST; break;
  default:
    return 0;
  }

  new_cursor = new BCursor(n);
  if (new_cursor == NULL)
    return 0;

  if (cursor != NULL)
    delete cursor;

  cursor = new_cursor;
  //custom_cursor = 0;

  be_app->SetCursor(cursor);

  return 1;
}

int Fl_X::set_cursor(const Fl_RGB_Image *image, int hotx, int hoty) {
//TODO
#if 0
  HCURSOR new_cursor;

  new_cursor = image_to_icon(image, false, hotx, hoty);
  if (new_cursor == NULL)
    return 0;

  if ((cursor != NULL) && custom_cursor)
    DestroyIcon(cursor);

  cursor = new_cursor;
  custom_cursor = 1;

  SetCursor(cursor);

  return 1;
#endif
  return 0;
}

////////////////////////////////////////////////////////////////
// Implement the virtual functions for the base Fl_Window class:

// If the box is a filled rectangle, we can make the redisplay *look*
// faster by using X's background pixel erasing.  We can make it
// actually *be* faster by drawing the frame only, this is done by
// setting fl_boxcheat, which is seen by code in fl_drawbox.cxx:
// For WIN32 it looks like all windows share a background color, so
// I use FL_GRAY for this and only do this cheat for windows that are
// that color.
// Actually it is totally disabled.
// Fl_Widget *fl_boxcheat;
//static inline int can_boxcheat(uchar b) {return (b==1 || (b&2) && b<=15);}

void Fl_Window::show() {
  image(Fl::scheme_bg_);
  if (Fl::scheme_bg_) {
    labeltype(FL_NORMAL_LABEL);
    align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
  } else {
    labeltype(FL_NO_LABEL);
  }
  Fl_Tooltip::exit(this);
  if (!shown()) {
    // if (can_boxcheat(box())) fl_background_pixel = fl_xpixel(color());
    Fl_X::make(this);
  } else {
    // Once again, we would lose the capture if we activated the window.
    i->xid->Lock();
    if (i->xid->IsMinimized()) i->xid->Minimize(false);
    i->xid->Activate();
    i->xid->Unlock();
  }
#ifdef USE_PRINT_BUTTON
  void preparePrintFront(void);
  preparePrintFront();
#endif
}

// Here we ensure only one GetDC is ever in place.
BView *fl_GetDC(FLWindow *w) {
  if (fl_gc) {
    if (w == fl_window  &&  fl_window != NULL) return fl_gc;
    if (fl_window) fl_release_dc(fl_window, fl_gc); // ReleaseDC
  }
  fl_gc = w->View();
  
  fl_save_dc(w, fl_gc);
  fl_window = w;
  

  return fl_gc;
}

// make X drawing go into this window (called by subclass flush() impl.)
void Fl_Window::make_current() {
  fl_GetDC(fl_xid(this));

#if USE_COLORMAP
  // Windows maintains a hardware and software color palette; the
  // SelectPalette() call updates the current soft->hard mapping
  // for all drawing calls, so we must select it here before any
  // code does any drawing...

//  fl_select_palette();
#endif // USE_COLORMAP

  current_ = this;
  fl_clip_region(0);


}

/* Make sure that all allocated fonts are released. This works only if 
   Fl::run() is allowed to exit by closing all windows. Calling 'exit(int)'
   will not automatically free any fonts. */
void fl_free_fonts(void)
{
// remove the Fl_Font_Descriptor chains
  int i;
  Fl_Fontdesc * s;
  Fl_Font_Descriptor * f;
  Fl_Font_Descriptor * ff;
  for (i=0; i<FL_FREE_FONT; i++) {
    s = fl_fonts + i;
    for (f=s->first; f; f=ff) {
      ff = f->next;
      delete f;
      s->first = ff;
    }
  }
}


///////////////////////////////////////////////////////////////////////
//
//  Haiku doesn't have a separate object to represent a drawing context.
//  The DC is an integral part of the BView object (base canvas).
//  So we fake the win32/X11-like DC handling by pushing and popping up
//  the BView state. This has some consequences on threading though.


static int32 dc_count = 0;

void fl_save_dc(FLWindow *w, BView *dc) {
  fprintf(stderr, "fl_save_dc(%p, %p) %d\n", w, dc, dc_count++);
  dc->LockLooper();
  dc->PushState();
  // safety check: ensure w is correct
  if (w && w->View() != dc)
    debugger("bad dc");
  //if (w)
  //  w->DisableUpdates();
  //TODO: save fl_gc_pattern which isn't part of BView's gc!
}

void fl_release_dc(FLWindow *w, BView *dc) {
  fprintf(stderr, "fl_release_dc(%p, %p) %d\n", w, dc, --dc_count);
  //TODO: restore fl_gc_pattern which isn't part of BView's gc!
  // safety check: ensure w is correct
  if (w && w->View() != dc)
    debugger("bad dc");
  //if (w)
  //  w->EnableUpdates();
  dc->PopState();
  dc->UnlockLooper();
}

void fl_cleanup_dc_list(void) {          // clean up the list
  if (fl_gc)
    fl_gc->UnlockLooper();
  fl_gc = NULL;
}

#if 0
Fl_Region XRectangleRegion(int x, int y, int w, int h) {
  if (Fl_Surface_Device::surface() == Fl_Display_Device::display_device()) return CreateRectRgn(x,y,x+w,y+h);
  // because rotation may apply, the rectangle becomes a polygon in device coords
  POINT pt[4] = { {x, y}, {x + w, y}, {x + w, y + h}, {x, y + h} };
  LPtoDP(fl_gc, pt, 4);
  return CreatePolygonRgn(pt, 4, ALTERNATE);
}
#endif

FL_EXPORT Window fl_xid_(const Fl_Window *w) {
  Fl_X *temp = Fl_X::i(w); 
  return temp ? temp->xid : 0;
}

int Fl_Window::decorated_w()
{
  if (!shown() || parent() || !border() || !visible()) return w();
  int X, Y, bt, bx, by;
  Fl_X::fake_X_wm(this, X, Y, bt, bx, by);
  return w() + 2 * bx;
}

int Fl_Window::decorated_h()
{
  if (!shown() || parent() || !border() || !visible()) return h();
  int X, Y, bt, bx, by;
  Fl_X::fake_X_wm(this, X, Y, bt, bx, by);
  return h() + bt + 2 * by;
}

void Fl_Paged_Device::print_window(Fl_Window *win, int x_offset, int y_offset)
{
  if (!win->shown() || win->parent() || !win->border() || !win->visible())
    print_widget(win, x_offset, y_offset);
  else
    draw_decorated_window(win, x_offset, y_offset, this);
}

void Fl_Paged_Device::draw_decorated_window(Fl_Window *win, int x_offset, int y_offset, Fl_Surface_Device *toset)
{
#if 0
  int X, Y, bt, bx, by, ww, wh; // compute the window border sizes
  Fl_X::fake_X_wm(win, X, Y, bt, bx, by);
  ww = win->w() + 2 * bx;
  wh = win->h() + bt + 2 * by;
  Fl_Display_Device::display_device()->set_current(); // make window current
  win->show();
  Fl::check();
  win->make_current();
  HDC save_gc = fl_gc;
  fl_gc = GetDC(NULL); // get the screen device context
  // capture the 4 window sides from screen
  RECT r;
  HRESULT res = S_OK + 1;
  if (DwmGetWindowAttribute) {
    res = DwmGetWindowAttribute(fl_window, DWMA_EXTENDED_FRAME_BOUNDS, &r, sizeof(RECT));
  }
  if (res != S_OK) {
    GetWindowRect(fl_window, &r);
  }
  Window save_win = fl_window;
  fl_window = NULL; // force use of read_win_rectangle() by fl_read_image()
  uchar *top_image = fl_read_image(NULL, r.left, r.top, ww, bt + by);
  uchar *left_image = bx ? fl_read_image(NULL, r.left, r.top, bx, wh) : NULL;
  uchar *right_image = bx ? fl_read_image(NULL, r.right - bx, r.top, bx, wh) : NULL;
  uchar *bottom_image = by ? fl_read_image(NULL, r.left, r.bottom-by, ww, by) : NULL;
  fl_window = save_win;
  ReleaseDC(NULL, fl_gc);  fl_gc = save_gc;
  toset->set_current();
  // print the 4 window sides
  fl_draw_image(top_image, x_offset, y_offset, ww, bt + by, 3); delete[] top_image;
  if (left_image) { fl_draw_image(left_image, x_offset, y_offset, bx, wh, 3); delete left_image; }
  if (right_image) { fl_draw_image(right_image, x_offset + win->w() + bx, y_offset, bx, wh, 3); delete right_image; }
  if (bottom_image) { fl_draw_image(bottom_image, x_offset, y_offset + win->h() + bt + by, ww, by, 3); delete bottom_image; }
  // print the window inner part
  this->print_widget(win, x_offset + bx, y_offset + bt + by);
  fl_gc = GetDC(fl_xid(win));
  ReleaseDC(fl_xid(win), fl_gc);
#endif
}  

#ifdef USE_PRINT_BUTTON
// to test the Fl_Printer class creating a "Print front window" button in a separate window
// contains also preparePrintFront call above
#include <FL/Fl_Printer.H>
#include <FL/Fl_Button.H>
void printFront(Fl_Widget *o, void *data)
{
  Fl_Printer printer;
  o->window()->hide();
  Fl_Window *win = Fl::first_window();
  if(!win) return;
  int w, h;
  if( printer.start_job(1) ) { o->window()->show(); return; }
  if( printer.start_page() ) { o->window()->show(); return; }
  printer.printable_rect(&w,&h);
  int  wh, ww;
  wh = win->decorated_h();
  ww = win->decorated_w();
  // scale the printer device so that the window fits on the page
  float scale = 1;
  if (ww > w || wh > h) {
    scale = (float)w/ww;
    if ((float)h/wh < scale) scale = (float)h/wh;
    printer.scale(scale, scale);
  }
// #define ROTATE 20.0
#ifdef ROTATE
  printer.scale(scale * 0.8, scale * 0.8);
  printer.printable_rect(&w, &h);
  printer.origin(w/2, h/2 );
  printer.rotate(ROTATE);
  printer.print_widget( win, - win->w()/2, - win->h()/2 );
  //printer.print_window_part( win, 0,0, win->w(), win->h(), - win->w()/2, - win->h()/2 );
#else  
  printer.print_window(win);
#endif
  printer.end_page();
  printer.end_job();
  o->window()->show();
}

#include <FL/Fl_Copy_Surface.H>
void copyFront(Fl_Widget *o, void *data)
{
  o->window()->hide();
  Fl_Window *win = Fl::first_window();
  if (!win) return;
  Fl_Copy_Surface *surf = new Fl_Copy_Surface(win->decorated_w() + 1, (int)(win->decorated_h() *0.985));
  surf->set_current();
  surf->draw_decorated_window(win); // draw the window content
  delete surf; // put the window on the clipboard
  Fl_Display_Device::display_device()->set_current();
  o->window()->show();
}

void preparePrintFront(void)
{
  static BOOL first=TRUE;
  if(!first) return;
  first=FALSE;
  static Fl_Window w(0,0,120,60);
  static Fl_Button bp(0,0,w.w(),30, "Print front window");
  bp.callback(printFront);
  static Fl_Button bc(0,30,w.w(),30, "Copy front window");
  bc.callback(copyFront);
  w.end();
  w.show();
}
#endif // USE_PRINT_BUTTON

#endif // FL_DOXYGEN

//
// End of "$Id: Fl_haiku.cxx 10928 2015-11-24 14:26:52Z manolo $".
//
