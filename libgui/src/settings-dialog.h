/*

Copyright (C) 2011-2013 Jacob Dawid

This file is part of Octave.

Octave is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version.

Octave is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with Octave; see the file COPYING.  If not, see
<http://www.gnu.org/licenses/>.

*/

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>

#include "color-picker.h"

#ifdef HAVE_QSCINTILLA
class QsciLexer;
#endif

namespace Ui
{
  class settings_dialog;
}

class settings_dialog:public QDialog
{
  Q_OBJECT public:
  explicit settings_dialog (QWidget * parent,
                            const QString& desired_tab = QString ());
  ~settings_dialog ();
  void write_changed_settings ();

private:
  Ui::settings_dialog * ui;
#ifdef HAVE_QSCINTILLA
  void read_lexer_settings (QsciLexer *lexer, QSettings *settings);
  void write_lexer_settings (QsciLexer *lexer, QSettings *settings);
  int  get_valid_lexer_styles (QsciLexer *lexer, int styles[]);
  enum { MaxLexerStyles = 64,
         MaxStyleNumber = 128 };
#endif

  void read_workspace_colors (QSettings *settings);
  void write_workspace_colors (QSettings *settings);

  void read_terminal_colors (QSettings *settings);
  void write_terminal_colors (QSettings *settings);

  color_picker *_editor_current_line_color;
};

#endif // SETTINGSDIALOG_H
