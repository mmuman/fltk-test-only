//
// "$Id: Fl_RGB_Image.h,v 1.3 2001/12/10 06:25:42 spitzak Exp $"
//
// RGB_Image header file for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-1999 by Bill Spitzak and others.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//
// Please report all bugs and problems to "fltk-bugs@easysw.com".
//

#ifndef Fl_RGB_Image_H
#define Fl_RGB_Image_H

#include "Fl_Image.h"

class FL_API Fl_RGB_Image : public Fl_Image {
  int depth;
  const uchar* data;
public:
  Fl_RGB_Image(const uchar* d, int W, int H, int D=3)
    : data(d) {w = W; h = H; depth = D;}
  void draw(int, int, int, int, Fl_Flags = 0);
  void draw(int x, int y, Fl_Flags f = 0) {draw(x,y,w,h,f);}
};

#endif

//
// End of "$Id: Fl_RGB_Image.h,v 1.3 2001/12/10 06:25:42 spitzak Exp $".
//
