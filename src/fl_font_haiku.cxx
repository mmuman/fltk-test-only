//
// "$Id: fl_font_haiku.cxx 10226 2014-08-19 12:36:12Z AlbrechtS $"
//
// Haiku font selection routines for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2016 by Bill Spitzak and others.
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

#include <FL/Fl_Printer.H>

#include <Font.h>
#include <String.h>

static int fl_angle_ = 0;

// XXX: use a family/style separator char (/ or -) ?
#define SEP ' '

#ifndef FL_DOXYGEN
Fl_Font_Descriptor::Fl_Font_Descriptor(const char* name, Fl_Fontsize fsize) {
  font_family family;
  font_style style;
  int32 styles = 0;
  BString n;
  int32 idx = -1;

  font = new BFont();

  // try to find the family name from the complete name
  for (n.SetTo(name); (idx = n.FindLast(SEP)) > -1; n.Truncate(idx)) {
    strlcpy(family, n.String(), sizeof(family));
    styles = count_font_styles(family);
    if (styles > 0)
      break;
  }

  if (styles < 1) {
    strlcpy(style, name + idx + 1, sizeof(style));
    //XXX: use B_REGULAR_FACE when empty?
    font->SetFamilyAndStyle(family, style);
  }

  font->SetSize(fsize);
  angle = fl_angle_;
  //XXX: what's the unit?
  font->SetRotation(angle);
  size = fsize;
}

Fl_Font_Descriptor::~Fl_Font_Descriptor() {
  if (this == fl_graphics_driver->font_descriptor()) fl_graphics_driver->font_descriptor(NULL);
  delete font;
}

////////////////////////////////////////////////////////////////

// WARNING: if you add to this table, you must redefine FL_FREE_FONT
// in Enumerations.H & recompile!!
static Fl_Fontdesc built_in_table[] = { // full font names used before 10.5
  {"DejaVu Sans Book"},
  {"DejaVu Sans Bold"},
  {"DejaVu Sans Oblique"},
  {"DejaVu Sans Bold Oblique"},
  {"DejaVu Serif Book"},
  {"DejaVu Serif Bold"},
  {"DejaVu Serif Italic"},
  {"DejaVu Serif Bold Italic"},
  {"Bitstream Charter"},
  {"Bitstream Charter Bold"},
  {"Bitstream Charter Italic"},
  {"Bitstream Charter Bold Italic"},
  {"VL Gothic regular"},//XXX
  {"DejaVu Sans Mono Book"},
  {"DejaVu Sans Mono Bold"},
  {"VL Gothic regular"}//XXX
};

Fl_Fontdesc* fl_fonts = built_in_table;

static Fl_Font_Descriptor* find(Fl_Font fnum, Fl_Fontsize size, int angle) {
  Fl_Fontdesc* s = fl_fonts+fnum;
  if (!s->name) s = fl_fonts; // use 0 if fnum undefined
  Fl_Font_Descriptor* f;
  for (f = s->first; f; f = f->next)
    if (f->size == size && f->angle == angle) return f;

  fl_open_display();

  f = new Fl_Font_Descriptor(s->name, size);
  f->next = s->first;
  s->first = f;
  return f;
}

////////////////////////////////////////////////////////////////
// Public interface:

static void fl_font(Fl_Graphics_Driver *driver, Fl_Font fnum, Fl_Fontsize size, int angle) {
  if (fnum==-1) { // just make sure that we will load a new font next time
    fl_angle_ = 0;
    driver->Fl_Graphics_Driver::font(0, 0);
    return;
  }
  if (fnum == driver->Fl_Graphics_Driver::font() && size == driver->size() && angle == fl_angle_) return;
  fl_angle_ = angle;
  driver->Fl_Graphics_Driver::font(fnum, size);
  driver->font_descriptor( find(fnum, size, angle) );
}

void Fl_Haiku_Graphics_Driver::font(Fl_Font fnum, Fl_Fontsize size) {
  fl_font(this, fnum, size, 0);
}

int Fl_Haiku_Graphics_Driver::height() {
  Fl_Font_Descriptor *fl_fontsize = font_descriptor();
  if (fl_fontsize == NULL)
    return -1;
  font_height height;
  fl_fontsize->font->GetHeight(&height);
  return (height.ascent + height.descent);
}

int Fl_Haiku_Graphics_Driver::descent() {
  Fl_Font_Descriptor *fl_fontsize = font_descriptor();
  if (fl_fontsize == NULL || fl_fontsize->font == NULL)
    return -1;
  font_height height;
  fl_fontsize->font->GetHeight(&height);
  return height.descent;
}

double Fl_Haiku_Graphics_Driver::width(const char* c, int n) {
  Fl_Font_Descriptor *fl_fontsize = font_descriptor();
  if (!fl_fontsize || !fl_fontsize->font) return -1.0;
  double w = 0.0;
  //XXX: is n a byte length or a char count!??
  w = fl_fontsize->font->StringWidth(c, n);
  return w;
}

double Fl_Haiku_Graphics_Driver::width(unsigned int c) {
  Fl_Font_Descriptor *fl_fontsize = font_descriptor();
  if (!fl_fontsize || !fl_fontsize->font) return -1.0;
  double w = 0.0;
  char buf[10];
  int n;
  n = fl_utf8encode(c, buf);
  w = fl_fontsize->font->StringWidth(buf, n);
  return w;
}

// Function to determine the extent of the "inked" area of the glyphs in a string
void Fl_Haiku_Graphics_Driver::text_extents(const char *c, int n, int &dx, int &dy, int &w, int &h) {

  Fl_Font_Descriptor *fl_fontsize = font_descriptor();
  if (!fl_fontsize || !fl_fontsize->font) { // no valid font, nothing to measure
    w = 0; h = 0;
    dx = dy = 0;
    return;
  }

  // XXX: what should we pass here?
  escapement_delta delta = { 0.0, 0.0 };

  BString str(c, n);
  const char *strings[] = { str.String() };
  BRect boxes[1];
  font_metric_mode mode = B_SCREEN_METRIC;
  fl_fontsize->font->GetBoundingBoxesForStrings(strings, 1, mode, &delta, boxes);

  dx = boxes[0].left;
  dy = boxes[0].top;
  w = boxes[0].right - boxes[0].left + 1;
  h = boxes[0].bottom - boxes[0].top + 1;

  return;
} // fl_text_extents

void Fl_Haiku_Graphics_Driver::draw(const char* str, int n, int x, int y) {
  // avoid a crash if no font has been selected by user yet !
  if (!font_descriptor()) font(FL_HELVETICA, FL_NORMAL_SIZE);

// XXX:  COLORREF oldColor = SetTextColor(fl_gc, fl_RGB());

//  fl_gc->PushState();
  drawing_mode mode = fl_gc->DrawingMode();
  fl_gc->SetDrawingMode(B_OP_OVER);
  BFont font;
  fl_gc->GetFont(&font);
  fl_gc->SetFont(font_descriptor()->font);
  BPoint p(x, y);
  fl_gc->DrawString(str, n, p);

  fl_gc->SetFont(&font);
  fl_gc->SetDrawingMode(mode);
//  SetTextColor(fl_gc, oldColor); // restore initial state
//  fl_gc->PopState();
  //fl_gc->SetFont(old);
}

void Fl_Haiku_Graphics_Driver::draw(int angle, const char* str, int n, int x, int y) {
  // avoid a crash if no font has been selected by user yet !
  if (!font_descriptor()) font(FL_HELVETICA, FL_NORMAL_SIZE);

  fl_font(this, Fl_Graphics_Driver::font(), size(), angle);
// XXX:  COLORREF oldColor = SetTextColor(fl_gc, fl_RGB());

//  fl_gc->PushState();
  drawing_mode mode = fl_gc->DrawingMode();
  fl_gc->SetDrawingMode(B_OP_OVER);
  BFont font;
  fl_gc->GetFont(&font);
  fl_gc->SetFont(font_descriptor()->font);

  BPoint p(x, y);
  fl_gc->DrawString(str, n, p);

  fl_gc->SetFont(&font);
  fl_gc->SetDrawingMode(mode);
//  SetTextColor(fl_gc, oldColor); // restore initial state
//  fl_gc->PopState();
  //fl_gc->SetFont(old);

  fl_font(this, Fl_Graphics_Driver::font(), size(), 0);
}

void Fl_Haiku_Graphics_Driver::rtl_draw(const char* c, int n, int x, int y) {
  int dx, dy, w, h;
  text_extents(c, n, dx, dy, w, h);
  draw(c, n, x - w - dx, y);
}
#endif
//
// End of "$Id: fl_font_haiku.cxx 10226 2014-08-19 12:36:12Z AlbrechtS $".
//
