//
// "$Id: fl_set_fonts_haiku.cxx 9326 2012-04-05 14:30:19Z AlbrechtS $"
//
// Haiku font utilities for the Fast Light Tool Kit (FLTK).
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

// This function fills in the FLTK font table with all the fonts that
// are found on the system.  It tries to place the fonts into families
// and to sort them so the first 4 in a family are normal, bold, italic,
// and bold italic.
#include <FL/fl_utf8.h>

#include <String.h>

// Bug: older versions calculated the value for *ap as a side effect of
// making the name, and then forgot about it. To avoid having to change
// the header files I decided to store this value in the last character
// of the font name array.
#define ENDOFBUFFER 127 // sizeof(Fl_Font.fontname)-1

// keep in sync with fl_font_haiku.cxx
#define SEP ' '

// turn a stored font name into a pretty name:
const char* Fl::get_font_name(Fl_Font fnum, int* ap) {
  Fl_Fontdesc *f = fl_fonts + fnum;
  if (!f->fontname[0]) {
    strlcpy(f->fontname, f->name, ENDOFBUFFER);
  }
  if (ap) *ap = f->fontname[ENDOFBUFFER];
  return f->fontname;
}

static int fl_free_font = FL_FREE_FONT;

Fl_Font Fl::set_fonts(const char* xstarname) {
  (void)xstarname;
  // TODO: check for "encoding" (or rather for latin Unicode block)
  if (fl_free_font > FL_FREE_FONT)
    return (Fl_Font)fl_free_font; // if already called
  fl_open_display();

  int32 count = count_font_families();
  int32 i, j;
  for (i = 0; i < count; i++) {
    font_family family;
    if (get_font_family(i, &family) != B_OK)
      continue;
    int32 scount = count_font_styles(family);
    BList styles;
    // reserve the first 4 slots for predefined faces
    styles.AddItem(NULL);
    styles.AddItem(NULL);
    styles.AddItem(NULL);
    styles.AddItem(NULL);
    for (j = 0; j < scount; j++) {
      font_style style;
      uint16 face;
      int attributes = 0;
      if (get_font_style(family, j, &style, &face) != B_OK)
        continue;
      // XXX: should we check for == instead?
      if (face & B_BOLD_FACE)
        attributes |= FL_BOLD;
      if (face & B_ITALIC_FACE)
        attributes |= FL_ITALIC;
      // FL_BOLD_ITALIC = FL_BOLD | FL_ITALIC (for now)
      int32 at = 5;
      if (face == B_REGULAR_FACE)
        at = 0;
      else if (face == B_BOLD_FACE)
        at = 1;
      else if (face == B_ITALIC_FACE)
        at = 2;
      else if (face == (B_BOLD_FACE | B_ITALIC_FACE))
        at = 3;

      BString pretty(family);
      pretty << SEP << style;

      // if it's a built-in, populate the attributes, then skip adding it again
      for (int k=0; k<FL_FREE_FONT; k++) {
        Fl_Fontdesc* d = &fl_fonts[k];
        if (pretty != d->name)
          continue;
        strlcpy(d->fontname, d->name, ENDOFBUFFER);
        d->fontname[ENDOFBUFFER] = (char)attributes;
        at = -1;
        break;
      }

      // uncomment to keep only the 4 usual types
      //if (at == 5) at = -1;

      if (at < 0)
        continue;
      else if (at < 5 && styles.ItemAt(at) == NULL) {
        styles.ReplaceItem(at, new BString(pretty));
      } else
        styles.AddItem(new BString(pretty));
    }

    for (j = 0; j < styles.CountItems(); j++) {
      BString *name = (BString *)styles.ItemAt(j);
      if (name == NULL)
        continue;
        Fl::set_font((Fl_Font)(fl_free_font), strdup(name->String()));
        Fl_Fontdesc* d = &fl_fonts[j];
        strlcpy(d->fontname, d->name, ENDOFBUFFER);
        if (j < 4) // index and attribute values match
          d->fontname[ENDOFBUFFER] = (char)j;
        fl_free_font++;
    }
  }

  return (Fl_Font)fl_free_font;
}

static int array[128];
int Fl::get_font_sizes(Fl_Font fnum, int*& sizep) {
  if (fnum >= fl_free_font)
    return 0;
  Fl_Fontdesc *s = fl_fonts+fnum;
  if (!s->name) s = fl_fonts; // empty slot in table, use entry 0
  int cnt = 0;

  // Haiku supports all font size 
  array[0] = 0;
  sizep = array;
  cnt = 1;

  return cnt;
}

//
// End of "$Id: fl_set_fonts_haiku.cxx 9326 2012-04-05 14:30:19Z AlbrechtS $".
//
