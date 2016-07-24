//
// "$Id: fl_draw_image_haiku.cxx 10560 2015-02-08 06:48:19Z manolo $"
//
// Haiku image drawing code for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2012 by Bill Spitzak and others.
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

////////////////////////////////////////////////////////////////

#include <config.h>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Printer.H>
#include <FL/x.H>

/*
 * draw an image based on the input parameters
 *
 * buf:       image source data
 * X, Y:      position (in buffer?!)
 * W, H:      size of picture (in pixel?)
 * delta:     distance from pixel to pixel in buf in bytes
 * linedelta: distance from line to line in buf in bytes
 * mono:      if set, pixel is one byte - if zero, pixel is 3 byte
 * cb:        callback to copy image data into (RGB?) buffer
 *   buf:       pointer to first byte in image source
 *   x, y:      position in buffer
 *   w:         width (in bytes?)
 *   dst:       destination buffer
 * userdata:  ?
 */
static void innards(const uchar *buf, int X, int Y, int W, int H,
		    int delta, int linedelta, int mono,
		    Fl_Draw_Image_Cb cb, void* userdata)
{
fprintf(stderr, "%s(%p, %d, %d, %d, %d, %d, %d, %d, %p, %p)\n", __PRETTY_FUNCTION__, buf, X, Y, W, H, delta, linedelta, mono, cb, userdata);
  bool alpha = !!(abs(delta) & FL_IMAGE_WITH_ALPHA);
  if (alpha)
    delta ^= FL_IMAGE_WITH_ALPHA; //XXX is it really valid anyway??
  //XXX: we probably dont support negative delta anyway here
  if (!linedelta) linedelta = W*delta;

  const void *array = buf;
  uchar *tmpBuf = 0;
  if (cb || Fl_Surface_Device::surface() != Fl_Display_Device::display_device()) {
    tmpBuf = new uchar[ H*W*delta ];
    if (cb) {
      for (int i=0; i<H; i++) {
	cb(userdata, 0, i, W, tmpBuf+i*W*delta);
      }
    } else {
      uchar *p = tmpBuf;
      for (int i=0; i<H; i++) {
	memcpy(p, buf+i*linedelta, W*delta);
	p += W*delta;
	}
    }
    array = (void*)tmpBuf;
    linedelta = W*delta;
  }
  // create an image context
  color_space space = B_RGB32;
  int32 bpr = B_ANY_BYTES_PER_ROW;
  switch (/*abs(*/delta/*)*/) {
  case 1: space = B_GRAY8; bpr = linedelta; break;
  case 2: space = B_RGBA32; alpha = true; break;
  case 3: space = B_RGB24; bpr = linedelta; break;
  case 4: space = B_RGBA32; bpr = linedelta; alpha = true; break;
  default: fprintf(stderr, "draw_image: invalid delta %d\n", delta); break; //XXX
  }

  BRect frame(0, 0, W-1, H-1);

  // try with the wanted linedelta...
  BBitmap *img = new BBitmap(frame, 0, space, bpr);
  // try again with any bytes per row
  if (img->InitCheck() != B_OK) {
    delete img;
    bpr = B_ANY_BYTES_PER_ROW;
    img = new BBitmap(frame, 0, space, bpr);
  }
    fprintf(stderr, "bm st %08lx ld %d bpr %ld\n", img->InitCheck(), linedelta, img->BytesPerRow());
	fprintf(stderr, "space %08x delta %d\n", space, delta);

  bpr = img->BytesPerRow();

  img->LockBits();

  switch (abs(delta)) {
  case 1:
  case 3:
  case 4:
    if (linedelta == bpr)
      memcpy(img->Bits(), array, linedelta*H);
    else
      for (int l = 0; l < H; l++)
        memcpy(((uchar *)img->Bits()) + l * bpr, ((uchar *)array) + l * linedelta, linedelta);
    break;
  case 2:
  {
  	const uchar *s = (const uchar *)array;
  	uchar *d = (uchar *)img->Bits();
    for (int l = 0; l < H; l++, s += linedelta, d += bpr) {
      for (int i = 0; i < W; i++) {
      	// i * delta actually but we optimize it out
        d[i * 4 + 0] = s[i * 2];
        d[i * 4 + 1] = s[i * 2];
        d[i * 4 + 2] = s[i * 2];
        d[i * 4 + 3] = s[i * 2 + 1];
      }
    }
  }
    break;
  }

  img->UnlockBits();

  delete [] tmpBuf;

  drawing_mode mode = alpha?B_OP_COPY:B_OP_ALPHA;

//  fl_gc->PushState();
  drawing_mode old_mode = fl_gc->DrawingMode();
  fl_gc->SetDrawingMode(mode);

  BRect rect(X, Y,  X+W-1, Y+H-1);
  fl_gc->DrawBitmap(img, rect);

  fl_gc->SetDrawingMode(old_mode);
  //fl_gc->PopState();

  delete img;
}

void Fl_Haiku_Graphics_Driver::draw_image(const uchar* buf, int x, int y, int w, int h, int d, int l){
  innards(buf,x,y,w,h,d,l,(d<3&&d>-3),0,0);
}
void Fl_Haiku_Graphics_Driver::draw_image(Fl_Draw_Image_Cb cb, void* data,
		   int x, int y, int w, int h,int d) {
  innards(0,x,y,w,h,d,0,(d<3&&d>-3),cb,data);
}
void Fl_Haiku_Graphics_Driver::draw_image_mono(const uchar* buf, int x, int y, int w, int h, int d, int l){
  innards(buf,x,y,w,h,d,l,1,0,0);
}
void Fl_Haiku_Graphics_Driver::draw_image_mono(Fl_Draw_Image_Cb cb, void* data,
		   int x, int y, int w, int h,int d) {
  innards(0,x,y,w,h,d,0,1,cb,data);
}

void fl_rectf(int x, int y, int w, int h, uchar r, uchar g, uchar b) {
  fl_color(r,g,b);
  fl_rectf(x,y,w,h);
}

//
// End of "$Id: fl_draw_image_haiku.cxx 10560 2015-02-08 06:48:19Z manolo $".
//
