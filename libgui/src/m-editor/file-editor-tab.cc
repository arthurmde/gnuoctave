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

#ifdef HAVE_QSCINTILLA

#if defined (HAVE_QSCI_QSCILEXEROCTAVE_H)
#define HAVE_LEXER_OCTAVE
#include <Qsci/qscilexeroctave.h>
#elif defined (HAVE_QSCI_QSCILEXERMATLAB_H)
#define HAVE_LEXER_MATLAB
#include <Qsci/qscilexermatlab.h>
#endif
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerbash.h>
#include <Qsci/qscilexerperl.h>
#include <Qsci/qscilexerbatch.h>
#include <Qsci/qscilexerdiff.h>
#include <Qsci/qsciprinter.h>
#include "resource-manager.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QPrintDialog>

#include "file-editor-tab.h"
#include "file-editor.h"

#include "file-ops.h"

#include "debug.h"
#include "octave-qt-link.h"
#include "version.h"

// Make parent null for the file editor tab so that warning
// WindowModal messages don't affect grandparents.
file_editor_tab::file_editor_tab (const QString& directory_arg)
{
  QString directory = directory_arg;
  _lexer_apis = 0;
  _app_closing = false;

  // Make sure there is a slash at the end of the directory name
  // for identification when saved later.
  if (directory.count () && directory.at (directory.count () - 1) != '/')
    directory.append ("/");

  _file_name = directory;
  _file_system_watcher.setObjectName ("_qt_autotest_force_engine_poller");

  _edit_area = new octave_qscintilla (this);
  // Connect signal for command execution to a slot of this tab which in turn
  // emits a signal connected to the main window.
  // Direct connection is not possible because tab's parent is null.
  connect (_edit_area,
           SIGNAL (execute_command_in_terminal_signal (const QString&)),
           this,
           SLOT (execute_command_in_terminal (const QString&)));

  connect (_edit_area, 
           SIGNAL (cursorPositionChanged (int, int)),
           this,
           SLOT (handle_cursor_moved (int,int)));

  // create statusbar for row/col indicator
  _status_bar = new QStatusBar (this);

  _row_indicator = new QLabel ("", this);
  _row_indicator->setMinimumSize (30,0);
  QLabel *row_label = new QLabel (tr ("Line:"), this);
  _col_indicator = new QLabel ("", this);
  _col_indicator->setMinimumSize (25,0);
  QLabel *col_label = new QLabel (tr ("Col:"), this);
  _status_bar->addPermanentWidget (row_label, 0);
  _status_bar->addPermanentWidget (_row_indicator, 0);
  _status_bar->addPermanentWidget (col_label, 0);
  _status_bar->addPermanentWidget (_col_indicator, 0);

  // Leave the find dialog box out of memory until requested.
  _find_dialog = 0;
  _find_dialog_is_visible = false;

  // symbols
  _edit_area->setMarginType (1, QsciScintilla::SymbolMargin);
  _edit_area->setMarginSensitivity (1, true);
  _edit_area->markerDefine (QsciScintilla::RightTriangle, bookmark);
  _edit_area->markerDefine (QPixmap (":/actions/icons/redled.png"),
                            breakpoint);
  _edit_area->markerDefine (QPixmap (":/actions/icons/bookmark.png"),
                            debugger_position);

  connect (_edit_area, SIGNAL (marginClicked (int, int,
                                              Qt::KeyboardModifiers)),
           this, SLOT (handle_margin_clicked (int, int,
                                              Qt::KeyboardModifiers)));

  // line numbers
  _edit_area->setMarginsForegroundColor (QColor (96, 96, 96));
  _edit_area->setMarginsBackgroundColor (QColor (232, 232, 220));
  _edit_area->setMarginType (2, QsciScintilla::TextMargin);

  // code folding
  _edit_area->setMarginType (3, QsciScintilla::SymbolMargin);
  _edit_area->setFolding (QsciScintilla::BoxedTreeFoldStyle , 3);

  // other features
  _edit_area->setBraceMatching (QsciScintilla::StrictBraceMatch);
  _edit_area->setAutoIndent (true);
  _edit_area->setIndentationWidth (2);
  _edit_area->setIndentationsUseTabs (false);

  _edit_area->setUtf8 (true);

  // auto completion
  _edit_area->autoCompleteFromAll ();
  _edit_area->setAutoCompletionSource (QsciScintilla::AcsAll);

  QVBoxLayout *edit_area_layout = new QVBoxLayout ();
  edit_area_layout->addWidget (_edit_area);
  edit_area_layout->addWidget (_status_bar);
  edit_area_layout->setMargin (0);
  setLayout (edit_area_layout);

  // connect modified signal
  connect (_edit_area, SIGNAL (modificationChanged (bool)),
           this, SLOT (update_window_title (bool)));

  connect (_edit_area, SIGNAL (copyAvailable (bool)),
           this, SLOT (handle_copy_available (bool)));

  connect (&_file_system_watcher, SIGNAL (fileChanged (const QString&)),
           this, SLOT (file_has_changed (const QString&)));

  QSettings *settings = resource_manager::get_settings ();
  if (settings)
    notice_settings (settings);
}

file_editor_tab::~file_editor_tab (void)
{
  // Destroy items attached to _edit_area.
  QsciLexer *lexer = _edit_area->lexer ();
  if (lexer)
    {
      delete lexer;
      _edit_area->setLexer (0);
    }
  if (_find_dialog)
    {
      delete _find_dialog;
      _find_dialog = 0;
    }

  // Destroy _edit_area.
  delete _edit_area;
}

void
file_editor_tab::closeEvent (QCloseEvent *e)
{
  // ignore close event if file is not saved and user cancels
  // closing this window
  if (check_file_modified () == QMessageBox::Cancel)
    e->ignore ();
  else
    e->accept ();
}

void
file_editor_tab::execute_command_in_terminal (const QString& command)
{
  emit execute_command_in_terminal_signal (command); // connected to main window
}

void
file_editor_tab::set_file_name (const QString& fileName)
{
  // update tracked file if we really have a file on disk
  QStringList trackedFiles = _file_system_watcher.files ();
  if (!trackedFiles.isEmpty ())
    _file_system_watcher.removePath (_file_name);
  if (!fileName.isEmpty ())
    _file_system_watcher.addPath (fileName);
  _file_name = fileName;

  // update lexer after _file_name change
  update_lexer ();

  // update the file editor with current editing directory
  emit editor_state_changed (_copy_available, _file_name);
  // add the new file to the mru list

  emit mru_add_file (_file_name);
}

// valid_file_name (file): checks whether "file" names a file
// by default, "file" is empty, then _file_name is checked
bool
file_editor_tab::valid_file_name (const QString& file)
{
  QString file_name;
  if (file.isEmpty ())
    file_name = _file_name;
  else
    file_name = file;
  return (! file_name.isEmpty ()
          && file_name.at (file_name.count () - 1) != '/');
}

void
file_editor_tab::handle_margin_clicked (int margin, int line,
                                        Qt::KeyboardModifiers state)
{
  if (margin == 1)
    {
      unsigned int markers_mask = _edit_area->markersAtLine (line);

      if (state & Qt::ControlModifier)
        {
          if (markers_mask && (1 << bookmark))
            _edit_area->markerDelete (line, bookmark);
          else
            _edit_area->markerAdd (line, bookmark);
        }
      else
        {
          if (markers_mask && (1 << breakpoint))
            request_remove_breakpoint (line);
          else
            request_add_breakpoint (line);
        }
    }
}

void
file_editor_tab::update_lexer ()
{
  if (_lexer_apis)
    _lexer_apis->cancelPreparation ();  // stop preparing if apis exists

  QsciLexer *lexer = _edit_area->lexer ();
  delete lexer;
  lexer = 0;

  if (_file_name.endsWith (".m")
      || _file_name.endsWith ("octaverc"))
    {
#if defined (HAVE_LEXER_OCTAVE)
      lexer = new QsciLexerOctave ();
#elif defined (HAVE_LEXER_MATLAB)
      lexer = new QsciLexerMatlab ();
#endif
    }

  if (! lexer)
    {
      if (_file_name.endsWith (".c")
          || _file_name.endsWith (".cc")
          || _file_name.endsWith (".cpp")
          || _file_name.endsWith (".cxx")
          || _file_name.endsWith (".c++")
          || _file_name.endsWith (".h")
          || _file_name.endsWith (".hh")
          || _file_name.endsWith (".hpp")
          || _file_name.endsWith (".h++"))
        {
          lexer = new QsciLexerCPP ();
        }
      else if (_file_name.endsWith (".pl"))
        {
          lexer = new QsciLexerPerl ();
        }
      else if (_file_name.endsWith (".bat"))
        {
          lexer = new QsciLexerBatch ();
        }
      else if (_file_name.endsWith (".diff"))
        {
          lexer = new QsciLexerDiff ();
        }
      else if (! valid_file_name ())
        {
          // new, no yet named file: let us assume it is octave
#if defined (HAVE_LEXER_OCTAVE)
          lexer = new QsciLexerOctave ();
#elif defined (HAVE_LEXER_MATLAB)
          lexer = new QsciLexerMatlab ();
#else
          lexer = new QsciLexerBash ();
#endif
        }
      else
        {
          // other or no extension
          lexer = new QsciLexerBash ();
        }
    }

  _lexer_apis = new QsciAPIs(lexer);
  if (_lexer_apis)
    {
      // get path to prepared api info
      QDesktopServices desktopServices;
      QString prep_apis_path
        = desktopServices.storageLocation (QDesktopServices::HomeLocation)
          + "/.config/octave/"  + QString(OCTAVE_VERSION) + "/qsci/";
      _prep_apis_file = prep_apis_path + lexer->lexer () + ".pap";

      if (!_lexer_apis->loadPrepared (_prep_apis_file))
        {
          // no prepared info loaded, prepare and save if possible

          // create raw apis info
          QString keyword;
          QStringList keyword_list;
          int i,j;
          for (i=1; i<=3; i++) // test the first 5 keyword sets
            {
              keyword = QString(lexer->keywords (i));           // get list
              keyword_list = keyword.split (QRegExp ("\\s+"));  // split
              for (j = 0; j < keyword_list.size (); j++)        // add to API
                _lexer_apis->add (keyword_list.at (j));
            }

          // dsiconnect slot for saving prepared info if already connected
          disconnect (_lexer_apis, SIGNAL (apiPreparationFinished ()), 0, 0);
          // check whether path for prepared info exists or can be created
          if (QDir("/").mkpath (prep_apis_path))
            {
              // path exists, apis info can be saved there
              connect (_lexer_apis, SIGNAL (apiPreparationFinished ()),
                       this, SLOT (save_apis_info ()));
            }
          _lexer_apis->prepare ();  // prepare apis info
        }
    }

  QSettings *settings = resource_manager::get_settings ();
  if (settings)
    lexer->readSettings (*settings);

  _edit_area->setLexer (lexer);

  // fix line number width with respect to the font size of the lexer
  if (settings->value ("editor/showLineNumbers", true).toBool ())
    auto_margin_width ();
  else
    _edit_area->setMarginWidth (2,0);

}

void
file_editor_tab::save_apis_info ()
{
  _lexer_apis->savePrepared (_prep_apis_file);
}

QString
file_editor_tab::comment_string (const QString& lexer)
{
  if (lexer == "octave" || lexer == "matlab")
    return QString("%");
  else if (lexer == "perl" || lexer == "bash" || lexer == "diff")
    return QString("#");
  else if (lexer == "cpp")
    return ("//");
  else if (lexer == "batch")
    return ("REM ");
  else
    return ("%");  // should never happen
}

// slot for fetab_set_focus: sets the focus to the current edit area
void
file_editor_tab::set_focus (const QWidget *ID)
{
  if (ID != this)
    return;
  _edit_area->setFocus ();
}

void
file_editor_tab::undo (const QWidget *ID)
{
  if (ID != this)
    return;

  _edit_area->undo ();
}

void
file_editor_tab::redo (const QWidget *ID)
{
  if (ID != this)
    return;

  _edit_area->redo ();
}

void
file_editor_tab::copy (const QWidget *ID)
{
  if (ID != this)
    return;

  _edit_area->copy ();
}

void
file_editor_tab::cut (const QWidget *ID)
{
  if (ID != this)
    return;

  _edit_area->cut ();
}

void
file_editor_tab::paste (const QWidget *ID)
{
  if (ID != this)
    return;

  _edit_area->paste ();
}

void
file_editor_tab::context_help (const QWidget *ID, bool doc)
{
  if (ID != this)
    return;

  _edit_area->context_help_doc (doc);
}

void
file_editor_tab::context_edit (const QWidget *ID)
{
  if (ID != this)
    return;

  _edit_area->context_edit ();
}

void
file_editor_tab::save_file (const QWidget *ID)
{
  if (ID != this)
    return;

  save_file (_file_name);
}
void

file_editor_tab::save_file (const QWidget *ID, const QString& fileName,
                            bool remove_on_success)
{
  if (ID != this)
    return;

  save_file (fileName, remove_on_success);
}

void
file_editor_tab::save_file_as (const QWidget *ID)
{
  if (ID != this)
    return;

  save_file_as ();
}

void
file_editor_tab::print_file (const QWidget *ID)
{
  if (ID != this)
    return;

  QsciPrinter *printer = new QsciPrinter (QPrinter::HighResolution);

  QPrintDialog printDlg (printer, this);

  if (printDlg.exec () == QDialog::Accepted)
    printer->printRange (_edit_area);

  delete printer;
}

void
file_editor_tab::run_file (const QWidget *ID)
{
  if (ID != this)
    return;

  if (_edit_area->isModified ())
    save_file (_file_name);

  QFileInfo info (_file_name);
  emit run_file_signal (info);
}

void
file_editor_tab::context_run (const QWidget *ID)
{
  if (ID != this)
    return;

  _edit_area->context_run ();
}

void
file_editor_tab::toggle_bookmark (const QWidget *ID)
{
  if (ID != this)
    return;

  int line, cur;
  _edit_area->getCursorPosition (&line, &cur);

  if (_edit_area->markersAtLine (line) && (1 << bookmark))
    _edit_area->markerDelete (line, bookmark);
  else
    _edit_area->markerAdd (line, bookmark);
}

void
file_editor_tab::next_bookmark (const QWidget *ID)
{
  if (ID != this)
    return;

  int line, cur;
  _edit_area->getCursorPosition (&line, &cur);

  if (_edit_area->markersAtLine (line) && (1 << bookmark))
    line++; // we have a breakpoint here, so start search from next line

  int nextline = _edit_area->markerFindNext (line, (1 << bookmark));

  _edit_area->setCursorPosition (nextline, 0);
}

void
file_editor_tab::previous_bookmark (const QWidget *ID)
{
  if (ID != this)
    return;

  int line, cur;
  _edit_area->getCursorPosition (&line, &cur);

  if (_edit_area->markersAtLine (line) && (1 << bookmark))
    line--; // we have a breakpoint here, so start search from prev line

  int prevline = _edit_area->markerFindPrevious (line, (1 << bookmark));

  _edit_area->setCursorPosition (prevline, 0);
}

void
file_editor_tab::remove_bookmark (const QWidget *ID)
{
  if (ID != this)
    return;

  _edit_area->markerDeleteAll (bookmark);
}

void
file_editor_tab::add_breakpoint_callback (const bp_info& info)
{
  bp_table::intmap line_info;
  line_info[0] = info.line;

  if (octave_qt_link::file_in_path (info.file, info.dir))
    bp_table::add_breakpoint (info.function_name, line_info);
}

void
file_editor_tab::remove_breakpoint_callback (const bp_info& info)
{
  bp_table::intmap line_info;
  line_info[0] = info.line;

  if (octave_qt_link::file_in_path (info.file, info.dir))
    bp_table::remove_breakpoint (info.function_name, line_info);
}

void
file_editor_tab::remove_all_breakpoints_callback (const bp_info& info)
{
  if (octave_qt_link::file_in_path (info.file, info.dir))
    bp_table::remove_all_breakpoints_in_file (info.function_name, true);
}

file_editor_tab::bp_info::bp_info (const QString& fname, int l)
  : line (l), file (fname.toStdString ())
{
  QFileInfo file_info (fname);

  QString q_dir = file_info.absolutePath ();
  QString q_function_name = file_info.fileName ();

  // We have to cut off the suffix, because octave appends it.
  q_function_name.chop (file_info.suffix ().length () + 1);

  dir = q_dir.toStdString ();
  function_name = q_function_name.toStdString ();

  // Is the last component of DIR @foo?  If so, strip it and prepend it
  // to the name of the function.

  size_t pos = dir.rfind (file_ops::dir_sep_chars ());

  if (pos != std::string::npos && pos < dir.length () - 1)
    {
      if (dir[pos+1] == '@')
        {
          function_name = file_ops::concat (dir.substr (pos+1), function_name);

          dir = dir.substr (0, pos);
        }
    }
}

void
file_editor_tab::request_add_breakpoint (int line)
{
  bp_info info (_file_name, line+1);

  octave_link::post_event
    (this, &file_editor_tab::add_breakpoint_callback, info);
}

void
file_editor_tab::request_remove_breakpoint (int line)
{
  bp_info info (_file_name, line+1);

  octave_link::post_event
    (this, &file_editor_tab::remove_breakpoint_callback, info);
}

void
file_editor_tab::toggle_breakpoint (const QWidget *ID)
{
  if (ID != this)
    return;

  int line, cur;
  _edit_area->getCursorPosition (&line, &cur);

  if (_edit_area->markersAtLine (line) && (1 << breakpoint))
    request_remove_breakpoint (line);
  else
    request_add_breakpoint (line);
}

void
file_editor_tab::next_breakpoint (const QWidget *ID)
{
  if (ID != this)
    return;

  int line, cur;
  _edit_area->getCursorPosition (&line, &cur);

  if (_edit_area->markersAtLine (line) && (1 << breakpoint))
    line++; // we have a breakpoint here, so start search from next line

  int nextline = _edit_area->markerFindNext (line, (1 << breakpoint));

  _edit_area->setCursorPosition (nextline, 0);
}

void
file_editor_tab::previous_breakpoint (const QWidget *ID)
{
  if (ID != this)
    return;

  int line, cur, prevline;
  _edit_area->getCursorPosition (&line, &cur);

  if (_edit_area->markersAtLine (line) && (1 << breakpoint))
    line--; // we have a breakpoint here, so start search from prev line

  prevline = _edit_area->markerFindPrevious (line, (1 << breakpoint));

  _edit_area->setCursorPosition (prevline, 0);
}

void
file_editor_tab::remove_all_breakpoints (const QWidget *ID)
{
  if (ID != this)
    return;

  bp_info info (_file_name);

  octave_link::post_event
    (this, &file_editor_tab::remove_all_breakpoints_callback, info);
}

void
file_editor_tab::comment_selected_text (const QWidget *ID)
{
  if (ID != this)
    return;

  do_comment_selected_text (true);
}

void
file_editor_tab::uncomment_selected_text (const QWidget *ID)
{
  if (ID != this)
    return;

  do_comment_selected_text (false);
}

void
file_editor_tab::handle_find_dialog_finished (int)
{
  // Find dialog is going to hide.  Save location of window for
  // when it is reshown.
  _find_dialog_geometry = _find_dialog->geometry ();
  _find_dialog_is_visible = false;
}

void
file_editor_tab::find (const QWidget *ID)
{
  if (ID != this)
    return;

  // The find_dialog feature doesn't need a slot for return info.
  // Rather than Qt::DeleteOnClose, let the find feature hang about
  // in case it contains useful information like previous searches
  // and so on.  Perhaps one find dialog for the whole editor is
  // better, but individual find dialogs has the nice feature of
  // retaining position per file editor tabs, which can be undocked.

  if (!_find_dialog)
    {
      _find_dialog = new find_dialog (_edit_area,
                                      qobject_cast<QWidget *> (sender ()));
      connect (_find_dialog, SIGNAL (finished (int)),
               this, SLOT (handle_find_dialog_finished (int)));
      _find_dialog->setWindowModality (Qt::NonModal);
      _find_dialog_geometry = _find_dialog->geometry ();
    }

  if (!_find_dialog->isVisible ())
    {
      _find_dialog->setGeometry (_find_dialog_geometry);
      _find_dialog->show ();
      _find_dialog_is_visible = true;
    }

  _find_dialog->activateWindow ();
  _find_dialog->init_search_text ();

}

void
file_editor_tab::goto_line (const QWidget *ID, int line)
{
  if (ID != this)
    return;

  if (line <= 0)  // ask for desired line
    {
      bool ok = false;
      int index;
      _edit_area->getCursorPosition (&line, &index);
      line = QInputDialog::getInt (_edit_area, tr ("Goto line"),
                                   tr ("Line number"), line+1, 1,
                                   _edit_area->lines (), 1, &ok);
      if (ok)
        {
          _edit_area->setCursorPosition (line-1, 0);
          center_current_line ();
        }
    }
  else  // go to given line without dialog
    _edit_area->setCursorPosition (line-1, 0);
}


void
file_editor_tab::do_comment_selected_text (bool comment)
{
  QString comment_str = comment_string (_edit_area->lexer ()->lexer ());
  _edit_area->beginUndoAction ();

  if (_edit_area->hasSelectedText ())
    {
      int lineFrom, lineTo, colFrom, colTo;
      _edit_area->getSelection (&lineFrom, &colFrom, &lineTo, &colTo);

      if (colTo == 0)  // the beginning of last line is not selected
        lineTo--;        // stop at line above

      for (int i = lineFrom; i <= lineTo; i++)
        {
          if (comment)
            _edit_area->insertAt (comment_str, i, 0);
          else
            {
              QString line (_edit_area->text (i));
              if (line.startsWith (comment_str))
                {
                  _edit_area->setSelection (i, 0, i, comment_str.length ());
                  _edit_area->removeSelectedText ();
                }
            }
        }
      //set selection on (un)commented section
      _edit_area->setSelection (lineFrom, 0, lineTo,
                                _edit_area->text (lineTo).length ());
    }
  else
    {
      int cpline, col;
      _edit_area->getCursorPosition (&cpline, &col);
      if (comment)
        _edit_area->insertAt (comment_str, cpline, 0);
      else
        {
          QString line (_edit_area->text (cpline));
          if (line.startsWith (comment_str))
            {
              _edit_area->setSelection (cpline, 0, cpline, comment_str.length ());
              _edit_area->removeSelectedText ();
            }
        }
    }
  _edit_area->endUndoAction ();
}

void
file_editor_tab::update_window_title (bool modified)
{
  QString title ("");
  QString tooltip ("");

  if (! valid_file_name ())
    title = tr ("<unnamed>");
  else
    {
      if (_long_title)
        title = _file_name;
      else
        {
          QFileInfo file (_file_name);
          title = file.fileName ();
          tooltip = _file_name;
        }
    }

  if (modified)
    emit file_name_changed (title.prepend ("* "), tooltip);
  else
    emit file_name_changed (title, tooltip);
}

void
file_editor_tab::handle_copy_available (bool enableCopy)
{
  _copy_available = enableCopy;
  emit editor_state_changed (_copy_available, QDir::cleanPath (_file_name));
}

// show_dialog: shows a modal or non modal dialog depeding on the closing
//              of the app
void
file_editor_tab::show_dialog (QDialog *dlg)
{
  dlg->setAttribute (Qt::WA_DeleteOnClose);
  if (_app_closing)
    dlg->exec ();
  else
    {
      dlg->setWindowModality (Qt::WindowModal);
      dlg->show ();
    }
}

int
file_editor_tab::check_file_modified ()
{
  int decision = QMessageBox::Yes;
  if (_edit_area->isModified ())
    {
      activateWindow ();
      raise ();
      // File is modified but not saved, ask user what to do.  The file
      // editor tab can't be made parent because it may be deleted depending
      // upon the response.  Instead, change the _edit_area to read only.
      QMessageBox::StandardButtons buttons = QMessageBox::Save |
                                             QMessageBox::Discard;
      QString available_actions;

      if (_app_closing)
        available_actions = tr ("Do you want to save or discard the changes?");
      else
        {
          buttons = buttons | QMessageBox::Cancel;  // cancel is allowed
          available_actions
            = tr ("Do you want to cancel closing, save or discard the changes?");
        }

      QString file;
      if (valid_file_name ())
          file = _file_name;
      else
          file = tr ("<unnamed>");

      QMessageBox* msgBox
        = new QMessageBox (QMessageBox::Warning, tr ("Octave Editor"),
                           tr ("The file\n"
                               "%1\n"
                               "is about to be closed but has been modified.\n"
                               "%2").
                           arg (file). arg (available_actions),
                           buttons, qobject_cast<QWidget *> (parent ()));

      msgBox->setDefaultButton (QMessageBox::Save);
      _edit_area->setReadOnly (true);
      connect (msgBox, SIGNAL (finished (int)),
               this, SLOT (handle_file_modified_answer (int)));

      show_dialog (msgBox);

      return QMessageBox::Cancel;
    }
  else
    {
      // Nothing was modified, just remove from editor.
      emit tab_remove_request ();
    }

  return decision;
}

void
file_editor_tab::handle_file_modified_answer (int decision)
{
  if (decision == QMessageBox::Save)
    {
      // Save file, then remove from editor.
      save_file (_file_name, true);
    }
  else if (decision == QMessageBox::Discard)
    {
      // User doesn't want to save, just remove from editor.
      emit tab_remove_request ();
    }
  else
    {
      // User canceled, allow editing again.
      _edit_area->setReadOnly (false);
    }
}

void
file_editor_tab::set_modified (bool modified)
{
  _edit_area->setModified (modified);
}

QString
file_editor_tab::load_file (const QString& fileName)
{
  // get the absolute path
  QFileInfo file_info = QFileInfo (fileName);
  QString file_to_load;
  if (file_info.exists ())
    file_to_load = file_info.canonicalFilePath ();
  else
    file_to_load = fileName;
  QFile file (file_to_load);
  if (!file.open (QFile::ReadOnly))
    return file.errorString ();

  QTextStream in (&file);
  QApplication::setOverrideCursor (Qt::WaitCursor);
  _edit_area->setText (in.readAll ());
  QApplication::restoreOverrideCursor ();

  _copy_available = false;     // no selection yet available
  set_file_name (file_to_load);
  update_window_title (false); // window title (no modification)
  _edit_area->setModified (false); // loaded file is not modified yet

  return QString ();
}

void
file_editor_tab::new_file (const QString &commands)
{
  update_window_title (false); // window title (no modification)
  _edit_area->setText (commands);
  _edit_area->setModified (false); // new file is not modified yet
}

void
file_editor_tab::save_file (const QString& saveFileName, bool remove_on_success)
{
  // If it is a new file with no name, signal that saveFileAs
  // should be performed.
  if (! valid_file_name (saveFileName))
    {
      save_file_as (remove_on_success);
      return;
    }
  // get the absolute path (if existing)
  QFileInfo file_info = QFileInfo (saveFileName);
  QString file_to_save;
  if (file_info.exists ())
    file_to_save = file_info.canonicalFilePath ();
  else
    file_to_save = saveFileName;
  QFile file (file_to_save);

  // stop watching file
  QStringList trackedFiles = _file_system_watcher.files ();
  if (trackedFiles.contains (file_to_save))
    _file_system_watcher.removePath (file_to_save);

  // open the file for writing
  if (!file.open (QIODevice::WriteOnly))
    {
      // Unsuccessful, begin watching file again if it was being
      // watched previously.
      if (trackedFiles.contains (file_to_save))
        _file_system_watcher.addPath (file_to_save);

      // Create a NonModal message about error.
      QMessageBox* msgBox
        = new QMessageBox (QMessageBox::Critical,
                           tr ("Octave Editor"),
                           tr ("Could not open file %1 for write:\n%2.").
                           arg (file_to_save).arg (file.errorString ()),
                           QMessageBox::Ok, 0);
      show_dialog (msgBox);

      return;
    }

  // save the contents into the file
  QTextStream out (&file);
  QApplication::setOverrideCursor (Qt::WaitCursor);
  out << _edit_area->text ();
  out.flush ();
  QApplication::restoreOverrideCursor ();
  file.flush ();
  file.close ();

  // file exists now
  file_info = QFileInfo (file);
  file_to_save = file_info.canonicalFilePath ();

  // save file name after closing file as set_file_name starts watching again
  set_file_name (file_to_save);   // make absolute

  // set the window title to actual file name (not modified)
  update_window_title (false);

  // files is save -> not modified
  _edit_area->setModified (false);

  if (remove_on_success)
    {
      emit tab_remove_request ();
      return;  // Don't touch member variables after removal
    }
}

void
file_editor_tab::save_file_as (bool remove_on_success)
{
  // Simply put up the file chooser dialog box with a slot connection
  // then return control to the system waiting for a file selection.

  // If the tab is removed in response to a QFileDialog signal, the tab
  // can't be a parent.
  QFileDialog* fileDialog;
  if (remove_on_success)
    {
      // If tab is closed, "this" cannot be parent in which case modality
      // has no effect.  Disable editing instead.
      _edit_area->setReadOnly (true);
      fileDialog = new QFileDialog ();
    }
  else
    fileDialog = new QFileDialog (this);

  // Giving trouble under KDE (problem is related to Qt signal handling on unix,
  // see https://bugs.kde.org/show_bug.cgi?id=260719 ,
  // it had/has no effect on Windows, though)
  fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);

  if (valid_file_name ())
    {
      fileDialog->selectFile (_file_name);
    }
  else
    {
      fileDialog->selectFile ("");

      if (_file_name.isEmpty ())
        fileDialog->setDirectory (QDir::currentPath ());
      else
        {
          // The file name is actually the directory name from the
          // constructor argument.
          fileDialog->setDirectory (_file_name);
        }
    }

  fileDialog->setNameFilter (tr ("Octave Files (*.m);;All Files (*)"));
  fileDialog->setDefaultSuffix ("m");
  fileDialog->setAcceptMode (QFileDialog::AcceptSave);
  fileDialog->setViewMode (QFileDialog::Detail);

  if (remove_on_success)
    {
      connect (fileDialog, SIGNAL (fileSelected (const QString&)),
               this, SLOT (handle_save_file_as_answer_close (const QString&)));

      connect (fileDialog, SIGNAL (rejected ()),
               this, SLOT (handle_save_file_as_answer_cancel ()));
    }
  else
    {
      connect (fileDialog, SIGNAL (fileSelected (const QString&)),
               this, SLOT (handle_save_file_as_answer (const QString&)));
    }

  show_dialog (fileDialog);
}

void
file_editor_tab::handle_save_file_as_answer (const QString& saveFileName)
{
  if (saveFileName == _file_name)
    {
      // same name as actual file, save it as "save" would do
      save_file (saveFileName);
    }
  else
    {
      // Have editor check for conflict, do not delete tab after save.
      emit editor_check_conflict_save (saveFileName, false);
    }
}

void
file_editor_tab::handle_save_file_as_answer_close (const QString& saveFileName)
{
  // saveFileName == _file_name can not happen, because we only can get here
  // when we close a tab and _file_name is not a valid file name yet

  // Have editor check for conflict, delete tab after save.
  emit editor_check_conflict_save (saveFileName, true);
}

void
file_editor_tab::handle_save_file_as_answer_cancel ()
{
  // User canceled, allow editing again.
  _edit_area->setReadOnly (false);
}

void
file_editor_tab::file_has_changed (const QString&)
{
  // Prevent popping up multiple message boxes when the file has
  // been changed multiple times by temporarily removing from the
  // file watcher.
  QStringList trackedFiles = _file_system_watcher.files ();
  if (!trackedFiles.isEmpty ())
    _file_system_watcher.removePath (_file_name);

  if (QFile::exists (_file_name))
    {
      // Create a WindowModal message that blocks the edit area
      // by making _edit_area parent.
      QMessageBox* msgBox
        = new QMessageBox (QMessageBox::Warning,
                           tr ("Octave Editor"),
                           tr ("It seems that \'%1\' has been modified by another application. Do you want to reload it?").
                           arg (_file_name),
                           QMessageBox::Yes | QMessageBox::No, this);

      connect (msgBox, SIGNAL (finished (int)),
               this, SLOT (handle_file_reload_answer (int)));

      msgBox->setWindowModality (Qt::WindowModal);
      msgBox->setAttribute (Qt::WA_DeleteOnClose);
      msgBox->show ();
    }
  else
    {
      QString modified = "";
      if (_edit_area->isModified ())
        modified = tr ("\n\nWarning: The contents in the editor is modified!");

      // Create a WindowModal message. The file editor tab can't be made
      // parent because it may be deleted depending upon the response.
      // Instead, change the _edit_area to read only.
      QMessageBox* msgBox
        = new QMessageBox (QMessageBox::Warning, tr ("Octave Editor"),
                           tr ("It seems that the file\n"
                               "%1\n"
                               "has been deleted or renamed. Do you want to save it now?%2").
                           arg (_file_name).arg (modified),
                           QMessageBox::Save | QMessageBox::Close, 0);

      _edit_area->setReadOnly (true);

      connect (msgBox, SIGNAL (finished (int)),
               this, SLOT (handle_file_resave_answer (int)));

      msgBox->setWindowModality (Qt::WindowModal);
      msgBox->setAttribute (Qt::WA_DeleteOnClose);
      msgBox->show ();
    }
}

void
file_editor_tab::notice_settings (const QSettings *settings)
{
  // QSettings pointer is checked before emitting.

  update_lexer ();

  //highlight current line color
  QVariant default_var = QColor (240, 240, 240);
  QColor setting_color = settings->value ("editor/highlight_current_line_color",
                                          default_var).value<QColor> ();
  _edit_area->setCaretLineBackgroundColor (setting_color);
  _edit_area->setCaretLineVisible
    (settings->value ("editor/highlightCurrentLine", true).toBool ());

  if (settings->value ("editor/codeCompletion", true).toBool ())  // auto compl.
    {
      bool match_keywords = settings->value
                            ("editor/codeCompletion_keywords",true).toBool ();
      bool match_document = settings->value
                            ("editor/codeCompletion_document",true).toBool ();

      QsciScintilla::AutoCompletionSource source = QsciScintilla::AcsNone;
      if (match_keywords)
        if (match_document)
          source = QsciScintilla::AcsAll;
        else
          source = QsciScintilla::AcsAPIs;
      else if (match_document)
        source = QsciScintilla::AcsDocument;
      _edit_area->setAutoCompletionSource (source);

      _edit_area->setAutoCompletionReplaceWord
        (settings->value ("editor/codeCompletion_replace",false).toBool ());
      _edit_area->setAutoCompletionCaseSensitivity
        (settings->value ("editor/codeCompletion_case",true).toBool ());
      _edit_area->setAutoCompletionThreshold
        (settings->value ("editor/codeCompletion_threshold",2).toInt ());
    }
  else
    _edit_area->setAutoCompletionThreshold (-1);

  if (settings->value ("editor/show_white_space",false).toBool ())
    if (settings->value ("editor/show_white_space_indent",false).toBool ())
      _edit_area->setWhitespaceVisibility (QsciScintilla::WsVisibleAfterIndent);
    else
      _edit_area->setWhitespaceVisibility (QsciScintilla::WsVisible);
  else
    _edit_area->setWhitespaceVisibility (QsciScintilla::WsInvisible);

  if (settings->value ("editor/showLineNumbers", true).toBool ())
    {
      _edit_area->setMarginLineNumbers (2, true);
      auto_margin_width ();
      connect (_edit_area, SIGNAL (linesChanged ()),
               this, SLOT (auto_margin_width ()));
    }
  else
    {
      _edit_area->setMarginLineNumbers (2, false);
      disconnect (_edit_area, SIGNAL (linesChanged ()), 0, 0);
    }

  _edit_area->setAutoIndent
        (settings->value ("editor/auto_indent",true).toBool ());
  _edit_area->setTabIndents
        (settings->value ("editor/tab_indents_line",false).toBool ());
  _edit_area->setBackspaceUnindents
        (settings->value ("editor/backspace_unindents_line",false).toBool ());
  _edit_area->setIndentationGuides
        (settings->value ("editor/show_indent_guides",false).toBool ());

  _edit_area->setTabWidth
        (settings->value ("editor/tab_width",2).toInt ());

  _long_title = settings->value ("editor/longWindowTitle", false).toBool ();
  update_window_title (_edit_area->isModified ());

}

void
file_editor_tab::auto_margin_width ()
{
  _edit_area->setMarginWidth (2, "1"+QString::number (_edit_area->lines ()));
}

void
file_editor_tab::conditional_close (const QWidget *ID, bool app_closing)
{
  if (ID != this)
    return;

  _app_closing = app_closing;
  close ();
}

void
file_editor_tab::change_editor_state (const QWidget *ID)
{
  if (ID != this)
    {
      // Widget may be going out of focus.  If so, record location.
      if (_find_dialog)
        {
          if (_find_dialog->isVisible ())
            {
              _find_dialog_geometry = _find_dialog->geometry ();
              _find_dialog->hide ();
            }
        }
      return;
    }

  if (_find_dialog && _find_dialog_is_visible)
    {
      _find_dialog->setGeometry (_find_dialog_geometry);
      _find_dialog->show ();
    }

  emit editor_state_changed (_copy_available, QDir::cleanPath (_file_name));
}

void
file_editor_tab::file_name_query (const QWidget *ID)
{
  // A zero (null pointer) means that all file editor tabs
  // should respond, otherwise just the desired file editor tab.
  if (ID != this && ID != 0)
    return;

  // Unnamed files shouldn't be transmitted.
  if (!_file_name.isEmpty ())
    emit add_filename_to_list (_file_name, this);
}

void
file_editor_tab::handle_file_reload_answer (int decision)
{
  if (decision == QMessageBox::Yes)
    {
      // reload: file is readded to the file watcher in set_file_name ()
      load_file (_file_name);
    }
  else
    {
      // do not reload: readd to the file watche
      _file_system_watcher.addPath (_file_name);
    }
}

void
file_editor_tab::handle_file_resave_answer (int decision)
{
  // check decision of user in dialog
  if (decision == QMessageBox::Save)
    {
      save_file (_file_name);  // readds file to watcher in set_file_name ()
      _edit_area->setReadOnly (false);  // delete read only flag
    }
  else
    {
      // Definitely close the file.
      // Set modified to false to prevent the dialog box when the close event
      // is posted. If the user cancels the close in this dialog the tab is
      // left open with a non-existing file.
      _edit_area->setModified (false);
      close ();
    }
}

void
file_editor_tab::insert_debugger_pointer (const QWidget *ID, int line)
{
  if (ID != this || ID == 0)
    return;

  if (line > 0)
    {
      _edit_area->markerAdd (line-1, debugger_position);
      center_current_line ();
    }
}

void
file_editor_tab::delete_debugger_pointer (const QWidget *ID, int line)
{
  if (ID != this || ID == 0)
    return;

  if (line > 0)
    _edit_area->markerDelete (line-1, debugger_position);
}

void
file_editor_tab::do_breakpoint_marker (bool insert, const QWidget *ID, int line)
{
  if (ID != this || ID == 0)
    return;

  if (line > 0)
    {
      if (insert)
        _edit_area->markerAdd (line-1, breakpoint);
      else
        _edit_area->markerDelete (line-1, breakpoint);
    }
}


void
file_editor_tab::center_current_line ()
{
  long int visible_lines
    = _edit_area->SendScintilla (QsciScintillaBase::SCI_LINESONSCREEN);

  if (visible_lines > 2)
    {
      int line, index;
      _edit_area->getCursorPosition (&line, &index);

      int first_line = _edit_area->firstVisibleLine ();
      first_line = first_line + (line - first_line - (visible_lines-1)/2);

      _edit_area->SendScintilla (2613,first_line); // SCI_SETFIRSTVISIBLELINE
    }
}

void 
file_editor_tab::handle_cursor_moved (int line, int col)
{
  _row_indicator->setNum (line+1);
  _col_indicator->setNum (col+1);
}

#endif
