/*

Copyright (C) 2012-2013 Michael Goffioul.
Copyright (C) 2012-2013 Jacob Dawid.

This file is part of QTerminal.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not,
see <http://www.gnu.org/licenses/>.

*/

#include "QTerminal.h"

#if defined (Q_OS_WIN32)
# include "win32/QWinTerminalImpl.h"
#else
# include "unix/QUnixTerminalImpl.h"
#endif

QTerminal *
QTerminal::create (QWidget *xparent)
{
#if defined (Q_OS_WIN32)
  return new QWinTerminalImpl (xparent);
#else
  return new QUnixTerminalImpl (xparent);
#endif
}

QList<QColor>
QTerminal::default_colors (void)
{
  static QList<QColor> colors;

  if (colors.isEmpty ())
    {
      colors << QColor(0,0,0)
             << QColor(255,255,255)
             << QColor(192,192,192)
             << QColor(128,128,128);
    }

  return colors;
}

QStringList
QTerminal::color_names (void)
{
  static QStringList names;

  if (names.isEmpty ())
    {
      names << QObject::tr ("foreground")
            << QObject::tr ("background")
            << QObject::tr ("selection")
            << QObject::tr ("cursor");
    }

  return names;
}

void
QTerminal::notice_settings (const QSettings *settings)
{
  // QSettings pointer is checked before emitting.

  // Set terminal font:
  QFont term_font = QFont ();
  term_font.setStyleHint (QFont::TypeWriter);
  term_font.setFamily
    (settings->value ("terminal/fontName", "Courier New").toString ());
  term_font.setPointSize (settings->value ("terminal/fontSize", 10).toInt ());
  setTerminalFont (term_font);

  QString cursorType
    = settings->value ("terminal/cursorType", "ibeam").toString ();

  bool cursorBlinking
    = settings->value ("terminal/cursorBlinking", true).toBool ();

  if (cursorType == "ibeam")
    setCursorType (QTerminal::IBeamCursor, cursorBlinking);
  else if (cursorType == "block")
    setCursorType (QTerminal::BlockCursor, cursorBlinking);
  else if (cursorType == "underline")
    setCursorType (QTerminal::UnderlineCursor, cursorBlinking);

  bool cursorUseForegroundColor
    = settings->value ("terminal/cursorUseForegroundColor", true).toBool ();

  QList<QColor> colors = default_colors ();

  setForegroundColor
    (settings->value ("terminal/color_f",
                      QVariant (colors.at (0))).value<QColor> ());

  setBackgroundColor
    (settings->value ("terminal/color_b",
                      QVariant (colors.at (1))).value<QColor> ());

  setSelectionColor
    (settings->value ("terminal/color_s",
                      QVariant (colors.at (2))).value<QColor> ());

  setCursorColor
    (cursorUseForegroundColor,
     settings->value ("terminal/color_c",
                      QVariant (colors.at (3))).value<QColor> ());
}
