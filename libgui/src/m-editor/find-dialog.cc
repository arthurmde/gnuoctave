/****************************************************************************

Find dialog derived from an example from Qt Toolkit (license below (**))

Copyright (C) 2012-2013 Torsten <ttl@justmail.de>
Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
 All rights reserved.
 Contact: Nokia Corporation (qt-info@nokia.com)

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

** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial Usage
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
** $QT_END_LICENSE$
**
****************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_QSCINTILLA

#include <QtGui>
#include <QIcon>
#include "find-dialog.h"

find_dialog::find_dialog (QsciScintilla* edit_area, QWidget *p)
  : QDialog (p)
{
  setWindowTitle ("Find and Replace");
  setWindowIcon (QIcon(":/actions/icons/find.png"));

  _search_label = new QLabel (tr ("Find &what:"));
  _search_line_edit = new QLineEdit;
  _search_label->setBuddy (_search_line_edit);
  _replace_label = new QLabel (tr ("Re&place with:"));
  _replace_line_edit = new QLineEdit;
  _replace_label->setBuddy (_replace_line_edit);

  _case_check_box = new QCheckBox (tr ("Match &case"));
  _from_start_check_box = new QCheckBox (tr ("Search from &start"));
  _wrap_check_box = new QCheckBox (tr ("&Wrap while searching"));
  _wrap_check_box->setChecked(true);
  _find_next_button = new QPushButton (tr ("&Find Next"));
  _find_prev_button = new QPushButton (tr ("Find &Previous"));
  _replace_button = new QPushButton (tr ("&Replace"));
  _replace_all_button = new QPushButton (tr ("Replace &All"));

  _more_button = new QPushButton (tr ("&More..."));
  _more_button->setCheckable (true);
  _more_button->setAutoDefault (false);

  _button_box = new QDialogButtonBox (Qt::Vertical);
  _button_box->addButton (_find_next_button, QDialogButtonBox::ActionRole);
  _button_box->addButton (_find_prev_button, QDialogButtonBox::ActionRole);
  _button_box->addButton (_replace_button, QDialogButtonBox::ActionRole);
  _button_box->addButton (_replace_all_button, QDialogButtonBox::ActionRole);
  _button_box->addButton (_more_button, QDialogButtonBox::ActionRole);
  _button_box->addButton (QDialogButtonBox::Close);

  _extension = new QWidget (this);
  _whole_words_check_box = new QCheckBox (tr ("&Whole words"));
  _regex_check_box = new QCheckBox (tr ("Regular E&xpressions"));
  _backward_check_box = new QCheckBox (tr ("Search &backward"));
  _search_selection_check_box = new QCheckBox (tr ("Search se&lection"));
  _search_selection_check_box->setCheckable (false); // TODO: Not implemented.
  _search_selection_check_box->setEnabled (false);

  _edit_area = edit_area;
  connect (_find_next_button,   SIGNAL (clicked ()),
           this,                SLOT (find_next ()));
  connect (_find_prev_button,   SIGNAL (clicked ()),
           this,                SLOT (find_prev ()));
  connect (_more_button,        SIGNAL (toggled (bool)),
           _extension,          SLOT (setVisible (bool)));
  connect (_replace_button,     SIGNAL (clicked ()),
           this,                SLOT (replace ()));
  connect (_replace_all_button, SIGNAL (clicked ()),
           this,                SLOT (replace_all ()));
  connect (_backward_check_box, SIGNAL (stateChanged (int)),
           this,                SLOT (handle_backward_search_changed (int)));
  connect (_button_box,         SIGNAL (rejected ()),
           this,                SLOT (close ()));

  QVBoxLayout *extension_layout = new QVBoxLayout ();
  extension_layout->setMargin (0);
  extension_layout->addWidget (_whole_words_check_box);
  extension_layout->addWidget (_backward_check_box);
  extension_layout->addWidget (_search_selection_check_box);
  _extension->setLayout (extension_layout);

  QGridLayout *top_left_layout = new QGridLayout;
  top_left_layout->addWidget (_search_label, 1, 1);
  top_left_layout->addWidget (_search_line_edit, 1, 2);
  top_left_layout->addWidget (_replace_label, 2, 1);
  top_left_layout->addWidget (_replace_line_edit, 2, 2);

  QVBoxLayout *left_layout = new QVBoxLayout;
  left_layout->addLayout (top_left_layout);
  left_layout->insertStretch (1, 5);
  left_layout->addWidget (_case_check_box);
  left_layout->addWidget (_from_start_check_box);
  left_layout->addWidget (_wrap_check_box);
  left_layout->addWidget (_regex_check_box);

  QGridLayout *main_layout = new QGridLayout;
  main_layout->setSizeConstraint (QLayout::SetFixedSize);
  main_layout->addLayout (left_layout, 0, 0);
  main_layout->addWidget (_button_box, 0, 1);
  main_layout->addWidget (_extension, 1, 0);
  setLayout (main_layout);

  _extension->hide ();
  _find_next_button->setDefault (true);
  _find_result_available = false;

  // move dialog to side of the parent if there is room on the desktop to do so.
  QWidget * desktop = QApplication::desktop ();
  int xp = p->x () + p->frameGeometry ().width ();
  int yp=  p->y ();
  if (desktop != 0 && sizeHint ().isValid ())
    {
      if (xp + sizeHint ().width () > desktop->width ())
        xp = desktop->width () - sizeHint ().width ();
    }

  move (xp, yp);

}

// set text of "search from start" depending on backward search
void
find_dialog::handle_backward_search_changed (int backward)
{
  if (backward)
    _from_start_check_box->setText (tr ("Search from end"));
  else
    _from_start_check_box->setText (tr ("Search from start"));
}

// initialize search text with selected text if this is in one single line
void
find_dialog::init_search_text ()
{
  if (_edit_area->hasSelectedText ())
    {
      int lbeg, lend, cbeg, cend;
      _edit_area->getSelection(&lbeg,&cbeg,&lend,&cend);
      if (lbeg == lend)
        _search_line_edit->setText (_edit_area->selectedText ());
    }
}

void
find_dialog::find_next ()
{
  find (!_backward_check_box->isChecked ());
}

void
find_dialog::find_prev ()
{
  find (_backward_check_box->isChecked ());
}

void
find_dialog::find (bool forward)
{
  int line = -1, col = -1;
  bool do_wrap = _wrap_check_box->isChecked ();
  bool do_forward = true;

  if (_find_result_available)
    {
      // we found a match last time, cursor is at the end of the match
      if (!forward)
        {
          // backward: go back one position or we will find the same again
          do_forward = false;
          _edit_area->getCursorPosition (&line,&col);
          if (col > 0)
            _edit_area->setCursorPosition (line,--col);
        }
    }

  _find_result_available = false;

  if (_from_start_check_box->isChecked ())
    {
      line = 0;
      col  = 0;
      if (_backward_check_box->isChecked ())
        do_wrap = true;
    }

  if (_edit_area)
    {
      _find_result_available
        = _edit_area->findFirst (_search_line_edit->text (),
                                _regex_check_box->isChecked (),
                                _case_check_box->isChecked (),
                                _whole_words_check_box->isChecked (),
                                do_wrap,
                                do_forward,
                                line,col,
                                true
#ifdef HAVE_QSCI_VERSION_2_6_0
                                , true
#endif
                                );
    }
  if (_find_result_available)
    _from_start_check_box->setChecked (0);
  else
    no_matches_message ();
}


void
find_dialog::replace ()
{
  if (_edit_area)
    {
      _edit_area->replace (_replace_line_edit->text ());
      if (!_edit_area->findNext())
        no_matches_message ();
    }
}

void
find_dialog::replace_all ()
{
  int count = 0;

  // check whether find & replace srings are different (avoid endless loop!)
  int strDiff;
  Qt::CaseSensitivity cs;
  if (_case_check_box->isChecked())
    {
      cs = Qt::CaseSensitive;
    }
  else
    {
      cs = Qt::CaseInsensitive;
    }
  strDiff = QString::compare (_search_line_edit->text(),
                              _replace_line_edit->text(), cs);

  // replace all if strings are different
  if (_edit_area && strDiff )
    {
      find (!_backward_check_box->isChecked ());  // find first occurence
      while (_find_result_available)   // while search string is found
        {
          _edit_area->replace (_replace_line_edit->text ());   // replace
          count++;                                             // inc counter
          _find_result_available = _edit_area->findNext();     // and find next
        }
      QMessageBox msg_box (QMessageBox::Information, tr ("Replace Result"),
                           tr ("%1 items replaced").arg(count),
                           QMessageBox::Ok, this);
      msg_box.exec ();
    }
  // TODO: Show number of replaced strings
}

void
find_dialog::no_matches_message ()
{
  QMessageBox msg_box (QMessageBox::Information, tr ("Find Result"),
                       tr ("No more matches found"), QMessageBox::Ok, this);
  msg_box.exec ();
}


#endif
