//
// "$Id: fl_dnd_haiku.cxx 9979 2013-09-20 03:36:02Z greg.ercolano $"
//
// Drag & Drop code for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2010 by Bill Spitzak and others.
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

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/x.H>
#include "flstring.h"


extern char fl_i_own_selection[2];
extern char *fl_selection_buffer[2];

extern int (*fl_local_grab)(int); // in Fl.cxx

int Fl::dnd() {
#warning TODO
  return 1;
}


//
// End of "$Id: fl_dnd_haiku.cxx 9979 2013-09-20 03:36:02Z greg.ercolano $".
//
