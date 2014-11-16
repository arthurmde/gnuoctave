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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QApplication>
#include <QClipboard>
#include <QVBoxLayout>
#include <QMenu>
#include <QScrollBar>

#include "error.h"

#include "cmd-hist.h"

#include "history-dock-widget.h"

history_dock_widget::history_dock_widget (QWidget *p)
  : octave_dock_widget (p)
{
  setObjectName ("HistoryDockWidget");
  setStatusTip (tr ("Browse and search the command history."));

  connect (this, SIGNAL (command_create_script (const QString&)),
           p, SLOT (new_file (const QString&)));

  connect (this, SIGNAL (information (const QString&)),
           p, SLOT (report_status_message (const QString&)));

  connect (this, SIGNAL (command_double_clicked (const QString&)),
           p, SLOT (execute_command_in_terminal (const QString&)));

  construct ();
}

void
history_dock_widget::construct ()
{
  _history_model = new QStringListModel ();
  _sort_filter_proxy_model.setSourceModel (_history_model);
  _history_list_view = new QListView (this);
  _history_list_view->setModel (&_sort_filter_proxy_model);
  _history_list_view->setAlternatingRowColors (true);
  _history_list_view->setEditTriggers (QAbstractItemView::NoEditTriggers);
  _history_list_view->setStatusTip (
    tr ("Double-click a command to transfer it to the terminal."));
  _history_list_view->setSelectionMode (QAbstractItemView::ExtendedSelection);
  _history_list_view->setContextMenuPolicy (Qt::CustomContextMenu);
  connect (_history_list_view,
           SIGNAL (customContextMenuRequested (const QPoint &)), this,
           SLOT (ctxMenu (const QPoint &)));

  _filter_line_edit = new QLineEdit (this);
  _filter_line_edit->setStatusTip (
    tr ("Enter text to filter the command history."));
  QVBoxLayout *vbox_layout = new QVBoxLayout ();

  setWindowIcon (QIcon (":/actions/icons/logo.png"));
  set_title (tr ("Command History"));
  setWidget (new QWidget ());

  vbox_layout->addWidget (_history_list_view);
  vbox_layout->addWidget (_filter_line_edit);
  vbox_layout->setMargin (2);

  widget ()->setLayout (vbox_layout);

  connect (_filter_line_edit, SIGNAL (textEdited (QString)),
           &_sort_filter_proxy_model, SLOT (setFilterWildcard (QString)));

  connect (_history_list_view, SIGNAL (doubleClicked (QModelIndex)),
           this, SLOT (handle_double_click (QModelIndex)));

  setFocusProxy (_filter_line_edit);
}

void history_dock_widget::ctxMenu (const QPoint &xpos)
{
  QMenu menu (this);
  menu.addAction (tr ("Copy"), this, SLOT (handle_contextmenu_copy (bool)));
  menu.addAction (tr ("Evaluate"), this,
                  SLOT (handle_contextmenu_evaluate (bool)));
  menu.addAction (tr ("Create script"), this,
                  SLOT (handle_contextmenu_create_script (bool)));
  menu.exec (_history_list_view->mapToGlobal (xpos));
}

void history_dock_widget::handle_contextmenu_copy (bool)
{
  QString text;
  QItemSelectionModel *selectionModel = _history_list_view->selectionModel ();
  QModelIndexList rows = selectionModel->selectedRows ();
  QModelIndexList::iterator it;
  bool prev_valid_row = false;
  for (it = rows.begin (); it != rows.end (); it++)
    {
      if ((*it).isValid ())
        {
          if (prev_valid_row)
            text += "\n";
          text += (*it).data ().toString ();
          prev_valid_row = true;
        }
    }
  QApplication::clipboard ()->setText (text);
}

void history_dock_widget::handle_contextmenu_evaluate (bool)
{
  QItemSelectionModel *selectionModel = _history_list_view->selectionModel ();
  QModelIndexList rows = selectionModel->selectedRows ();
  QModelIndexList::iterator it;
  for (it = rows.begin () ; it != rows.end (); it++)
    {
      if ((*it).isValid ())
        emit command_double_clicked ((*it).data ().toString ());
    }
}

void
history_dock_widget::handle_contextmenu_create_script (bool)
{
  QString text;
  QItemSelectionModel *selectionModel = _history_list_view->selectionModel ();
  QModelIndexList rows = selectionModel->selectedRows ();

  bool prev_valid_row = false;
  for (QModelIndexList::iterator it = rows.begin (); it != rows.end (); it++)
    {
      if ((*it).isValid ())
        {
          if (prev_valid_row)
            text += "\n";
          text += (*it).data ().toString ();
          prev_valid_row = true;
        }
    }

  if (text.length () > 0)
    emit command_create_script (text);
}


void
history_dock_widget::handle_double_click (QModelIndex modelIndex)
{
  emit command_double_clicked (modelIndex.data ().toString ());
}

void
history_dock_widget::set_history (const QStringList& hist)
{
  _history_model->setStringList (hist);
  _history_list_view->scrollToBottom ();
}

void
history_dock_widget::append_history (const QString& hist_entry)
{
  QStringList lst = _history_model->stringList ();
  lst.append (hist_entry);

  QScrollBar *scroll_bar = _history_list_view->verticalScrollBar ();

  bool at_bottom = scroll_bar->maximum () - scroll_bar->value () < 1;

  _history_model->setStringList (lst);

  // Scroll if slider position at bottom.
  if (at_bottom)
    _history_list_view->scrollToBottom ();
}

void
history_dock_widget::clear_history (void)
{
  _history_model->setStringList (QStringList ());
}

void
history_dock_widget::copyClipboard ()
{
  if (_history_list_view->hasFocus ())
    handle_contextmenu_copy (true);
  if (_filter_line_edit->hasFocus () && _filter_line_edit->hasSelectedText ())
    {
      QClipboard *clipboard = QApplication::clipboard ();
      clipboard->setText ( _filter_line_edit->selectedText ());
    }
}

void
history_dock_widget::pasteClipboard ()
{
  if (_filter_line_edit->hasFocus ())
    {
      QClipboard *clipboard = QApplication::clipboard ();
      QString str =  clipboard->text ();
      if (str.length () > 0)
        _filter_line_edit->insert (str);
    }
}

