/*

Copyright (C) 2012-2013 Richard Crozier

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

#if !defined (octave_octave_dock_widget_h)
#define octave_octave_dock_widget_h 1

#include <QDockWidget>
#include <QSettings>
#include <QIcon>
#include <QMainWindow>
#include <QMouseEvent>

class octave_dock_widget : public QDockWidget
{
  Q_OBJECT

public:

  octave_dock_widget (QWidget *p = 0);
  virtual ~octave_dock_widget ();

  virtual void connect_visibility_changed (void);
  void make_window (void);
  void make_widget (bool dock=true);
  void set_title (const QString&);

signals:

  /** Custom signal that tells whether a user has clicked away
   *  that dock widget, i.e the active dock widget has
   *  changed. */
  void active_changed (bool active);

protected:

  virtual void closeEvent (QCloseEvent *e)
  {
    emit active_changed (false);
    QDockWidget::closeEvent (e);
  }

  QWidget * focusWidget ();

public slots:

  virtual void focus (void)
  {
    if (! isVisible ())
      setVisible (true);

    setFocus ();
    activateWindow ();
    raise ();
  }

  virtual void handle_visibility (bool visible)
  {
    if (visible && ! isFloating ())
      focus ();
  }

  virtual void notice_settings (const QSettings*)
  {
  }

  QMainWindow *main_win () { return _parent; }

protected slots:

  /** Slot to steer changing visibility from outside. */
  virtual void handle_visibility_changed (bool visible)
  {
    if (visible)
      emit active_changed (true);
  }
  /** slots to handle copy & paste */
  virtual void copyClipboard ()
  {
  }
  virtual void pasteClipboard ()
  {
  }

private slots:

  void change_floating (bool);
  void change_visibility (bool);

private:

  QMainWindow *_parent;  // store the parent since we are reparenting to 0
  QAction *_dock_action;
  bool _floating;

};

#endif
