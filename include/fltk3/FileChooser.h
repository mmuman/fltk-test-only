//
// "$Id$"
//
// fltk3::FileChooser dialog for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2011 by Bill Spitzak and others.
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
// Please report all bugs and problems on the following page:
//
//     http://www.fltk.org/str.php
//

// generated by Fast Light User Interface Designer (fluid) version 1.0300

#ifndef Fltk3_File_Chooser_H
#define Fltk3_File_Chooser_H
#include <fltk3/run.h>
#include <fltk3/DoubleWindow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fltk3/Group.h>
#include <fltk3/Choice.h>
#include <fltk3/MenuButton.h>
#include <fltk3/Button.h>
#include <fltk3/Preferences.h>
#include <fltk3/TiledGroup.h>
#include <fltk3/FileBrowser.h>
#include <fltk3/Box.h>
#include <fltk3/CheckButton.h>
#include <fltk3/FileInput.h>
#include <fltk3/ReturnButton.h>
#include <fltk3/ask.h>

class Fl_File_Chooser;

namespace fltk3 {
  
  class FLTK3_EXPORT FileChooser {
    friend class ::Fl_File_Chooser;
  public:
    enum { SINGLE = 0, MULTI = 1, CREATE = 2, DIRECTORY = 4 }; 
  private:
    static fltk3::Preferences prefs_; 
    void (*callback_)(fltk3::FileChooser*, void *); 
    void *data_; 
    char directory_[FLTK3_PATH_MAX]; 
    char pattern_[FLTK3_PATH_MAX]; 
    char preview_text_[2048]; 
    int type_; 
    void favoritesButtonCB(); 
    void favoritesCB(fltk3::Widget *w); 
    void fileListCB(); 
    void fileNameCB(); 
    void newdir(); 
    static void previewCB(fltk3::FileChooser *fc); 
    void showChoiceCB(); 
    void update_favorites(); 
    void update_preview(); 
  public:
    FileChooser(const char *d, const char *p, int t, const char *title);
  private:
    fltk3::DoubleWindow *window;
    void cb_window_i(fltk3::DoubleWindow*, void*);
    static void cb_window(fltk3::DoubleWindow*, void*);
    fltk3::Choice *showChoice;
    void cb_showChoice_i(fltk3::Choice*, void*);
    static void cb_showChoice(fltk3::Choice*, void*);
    fltk3::MenuButton *favoritesButton;
    void cb_favoritesButton_i(fltk3::MenuButton*, void*);
    static void cb_favoritesButton(fltk3::MenuButton*, void*);
  public:
    fltk3::Button *newButton;
  private:
    void cb_newButton_i(fltk3::Button*, void*);
    static void cb_newButton(fltk3::Button*, void*);
    void cb__i(fltk3::TiledGroup*, void*);
    static void cb_(fltk3::TiledGroup*, void*);
    fltk3::FileBrowser *fileList;
    void cb_fileList_i(fltk3::FileBrowser*, void*);
    static void cb_fileList(fltk3::FileBrowser*, void*);
    fltk3::Widget *previewBox;
  public:
    fltk3::FileBrowser *browser() { return fileList; }
    fltk3::CheckButton *previewButton;
  private:
    void cb_previewButton_i(fltk3::CheckButton*, void*);
    static void cb_previewButton(fltk3::CheckButton*, void*);
  public:
    fltk3::CheckButton *showHiddenButton;
  private:
    void cb_showHiddenButton_i(fltk3::CheckButton*, void*);
    static void cb_showHiddenButton(fltk3::CheckButton*, void*);
    fltk3::FileInput *fileName;
    void cb_fileName_i(fltk3::FileInput*, void*);
    static void cb_fileName(fltk3::FileInput*, void*);
    fltk3::ReturnButton *okButton;
    void cb_okButton_i(fltk3::ReturnButton*, void*);
    static void cb_okButton(fltk3::ReturnButton*, void*);
    fltk3::Button *cancelButton;
    void cb_cancelButton_i(fltk3::Button*, void*);
    static void cb_cancelButton(fltk3::Button*, void*);
    fltk3::DoubleWindow *favWindow;
    fltk3::FileBrowser *favList;
    void cb_favList_i(fltk3::FileBrowser*, void*);
    static void cb_favList(fltk3::FileBrowser*, void*);
    fltk3::Button *favUpButton;
    void cb_favUpButton_i(fltk3::Button*, void*);
    static void cb_favUpButton(fltk3::Button*, void*);
    fltk3::Button *favDeleteButton;
    void cb_favDeleteButton_i(fltk3::Button*, void*);
    static void cb_favDeleteButton(fltk3::Button*, void*);
    fltk3::Button *favDownButton;
    void cb_favDownButton_i(fltk3::Button*, void*);
    static void cb_favDownButton(fltk3::Button*, void*);
    fltk3::Button *favCancelButton;
    void cb_favCancelButton_i(fltk3::Button*, void*);
    static void cb_favCancelButton(fltk3::Button*, void*);
    fltk3::ReturnButton *favOkButton;
    void cb_favOkButton_i(fltk3::ReturnButton*, void*);
    static void cb_favOkButton(fltk3::ReturnButton*, void*);
  public:
    ~FileChooser();
    void callback(void (*cb)(fltk3::FileChooser *, void *), void *d = 0);
    void color(fltk3::Color c);
    fltk3::Color color();
    int count(); 
    void directory(const char *d); 
    char * directory();
    void filter(const char *p); 
    const char * filter();
    int filter_value();
    void filter_value(int f);
    void hide();
    void iconsize(uchar s);
    uchar iconsize();
    void label(const char *l);
    const char * label();
    void ok_label(const char *l);
    const char * ok_label();
    void preview(int e); 
    int preview() const { return previewButton->value(); }; 
  private:
    void showHidden(int e); 
    void remove_hidden_files(); 
  public:
    void rescan(); 
    void rescan_keep_filename(); 
    void show();
    int shown();
    void textcolor(fltk3::Color c);
    fltk3::Color textcolor();
    void textfont(fltk3::Font f);
    fltk3::Font textfont();
    void textsize(fltk3::Fontsize s);
    fltk3::Fontsize textsize();
    void type(int t);
    int type();
    void * user_data() const;
    void user_data(void *d);
    const char *value(int f = 1); 
    void value(const char *filename); 
    int visible();
    /**
     [standard text may be customized at run-time]
     */
    static const char *add_favorites_label; 
    /**
     [standard text may be customized at run-time]
     */
    static const char *all_files_label; 
    /**
     [standard text may be customized at run-time]
     */
    static const char *custom_filter_label; 
    /**
     [standard text may be customized at run-time]
     */
    static const char *existing_file_label; 
    /**
     [standard text may be customized at run-time]
     */
    static const char *favorites_label; 
    /**
     [standard text may be customized at run-time]
     */
    static const char *filename_label; 
    /**
     [standard text may be customized at run-time]
     */
    static const char *filesystems_label; 
    /**
     [standard text may be customized at run-time]
     */
    static const char *manage_favorites_label; 
    /**
     [standard text may be customized at run-time]
     */
    static const char *new_directory_label; 
    /**
     [standard text may be customized at run-time]
     */
    static const char *new_directory_tooltip; 
    /**
     [standard text may be customized at run-time]
     */
    static const char *preview_label; 
    /**
     [standard text may be customized at run-time]
     */
    static const char *save_label; 
    /**
     [standard text may be customized at run-time]
     */
    static const char *show_label; 
    /** 
     [standard text may be customized at run-time]
     */
    static const char *hidden_label;
    /**
     the sort function that is used when loading
     the contents of a directory.
     */
    static fltk3::FileSortF *sort; 
  private:
    fltk3::Widget* ext_group; 
  public:
    fltk3::Widget* add_extra(fltk3::Widget* gr);
  };
  
  FLTK3_EXPORT char *dir_chooser(const char *message,const char *fname,int relative=0);
  FLTK3_EXPORT char *file_chooser(const char *message,const char *pat,const char *fname,int relative=0);
  FLTK3_EXPORT void file_chooser_callback(void (*cb)(const char*));
  FLTK3_EXPORT void file_chooser_ok_label(const char*l);
  
}

#endif

//
// End of "$Id$".
//
