// Copyright (C) 2009 Romain Goffe, Alexandre Dupas
//
// Songbook Creator is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// Songbook Creator is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
// MA  02110-1301, USA.
//******************************************************************************
#include <QtGui>
#include <QtSql>
#include <QtAlgorithms>
#include <QDebug>

#include <assert.h>
#include "utils/utils.hh"
#include "label.hh"
#include "mainwindow.hh"
#include "preferences.hh"
#include "library.hh"
#include "songbook.hh"
#include "build-engine/resize-covers.hh"
#include "build-engine/latex-preprocessing.hh"
#include "build-engine/make-songbook.hh"
#include "build-engine/download.hh"
#include "song-editor.hh"
#include "highlighter.hh"
#include "dialog-new-song.hh"
#include "filter-lineedit.hh"
#include "songSortFilterProxyModel.hh"
#include "tab-widget.hh"

using namespace SbUtils;

//******************************************************************************
CMainWindow::CMainWindow()
  : QMainWindow()
  , m_library()
  , m_proxyModel(new CSongSortFilterProxyModel)
  , m_songbook(new CSongbook())
  , m_sbInfoSelection(new CLabel)
  , m_sbInfoTitle(new CLabel)
  , m_sbInfoAuthors(new CLabel)
  , m_sbInfoStyle(new CLabel)
  , m_view(new QTableView(this))
  , m_progressBar(new QProgressBar(this))
  , m_cover(new QPixmap)
{
  setWindowTitle("Patacrep Songbook Client");
  setWindowIcon(QIcon(":/icons/patacrep.png"));

  m_isToolbarDisplayed = true;
  m_isStatusbarDisplayed = true;
  m_first = true;

  readSettings();

  // main document and title
  songbook()->setWorkingPath(workingPath());
  connect(songbook(), SIGNAL(wasModified(bool)),
          this, SLOT(setWindowModified(bool)));
  connect(this, SIGNAL(workingPathChanged(QString)),
	  songbook(), SLOT(setWorkingPath(QString)));
  updateTitle(songbook()->filename());

  // compilation log
  m_log = new QTextEdit;
  m_log->setMaximumHeight(150);
  m_log->setReadOnly(true);
  new CHighlighter(m_log->document());
  
  // no data info widget
  m_noDataInfo = new QTextEdit;
  m_noDataInfo->setReadOnly(true);
  m_noDataInfo->setMaximumHeight(150);
  m_noDataInfo->
    setHtml(QString(tr("<table><tr><td valign=middle>  "
		       "<img src=\":/icons/attention.png\" />  </td><td>"
		       "<p>The directory <b>%1</b> does not contain any song file (\".sg\").<br/><br/> "
		       "You may :<ul><li>select a valid directory in the menu <i>Edit/Preferences</i></li>"
		       "<li>use the menu <i>Library/Download</i> to get the latest git snapshot</li>"
		       "<li>manually download the latest tarball on "
		       "<a href=\"http://www.patacrep.com/static1/downloads\">"
		       "patacrep.com</a></li></ul>"
		       "</p></td></tr></table>")).arg(workingPath()));
  m_noDataInfo->hide();

  // toolbar (for the build button)
  m_toolbar = new QToolBar;
  current_toolbar = m_toolbar;
  m_toolbar->setMovable(false);
  this->setUnifiedTitleAndToolBarOnMac(true);

  createActions();
  createMenus();

  m_toolbar->addAction(m_newAct);
  m_toolbar->addAction(m_openAct);
  m_toolbar->addAction(m_saveAct);
  m_toolbar->addAction(m_saveAsAct);
  m_toolbar->addSeparator();
  m_toolbar->addAction(m_buildAct);
  m_toolbar->addSeparator();
  m_toolbar->addAction(m_selectAllAct);
  m_toolbar->addAction(m_unselectAllAct);
  m_toolbar->addAction(m_invertSelectionAct);
  m_toolbar->addSeparator();
  m_toolbar->addAction(m_selectEnglishAct);
  m_toolbar->addAction(m_selectFrenchAct);
  m_toolbar->addAction(m_selectSpanishAct);

  //Connection to database
  connectDb();
  refreshLibrary();

  // filtering related widgets
  CFilterLineEdit *filterLineEdit = new CFilterLineEdit;
  m_proxyModel->setFilterKeyColumn(-1);
  filterLineEdit->setVisible(true);
  connect(filterLineEdit, SIGNAL(textChanged(QString)),
	  this, SLOT(filterChanged()));

  QWidget* stretchWidget = new QWidget;
  stretchWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_toolbar->addWidget(stretchWidget);
  m_toolbar->addWidget(filterLineEdit);
  m_toolbar->setContextMenuPolicy(Qt::PreventContextMenu);

  //artist autocompletion in the filter bar
  QCompleter *completer = new QCompleter;
  completer->setModel(library());
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setCompletionMode(QCompleter::InlineCompletion);
  filterLineEdit->setCompleter(completer);

  addToolBar(m_toolbar);
  
  connect(selectionModel(), SIGNAL(selectionChanged(const QItemSelection & , 
						    const QItemSelection & )),
	  this, SLOT(selectionChanged(const QItemSelection & , const QItemSelection & )));

  
  //Layouts
  QBoxLayout *mainLayout = new QVBoxLayout;
  QBoxLayout *dataLayout = new QVBoxLayout;
  QBoxLayout *centerLayout = new QHBoxLayout;
  QBoxLayout *leftLayout = new QVBoxLayout;
  leftLayout->addWidget(new QLabel(tr("<b>Song</b>")));
  leftLayout->addLayout(songInfo());
  leftLayout->addWidget(new QLabel(tr("<b>Songbook</b>")));
  leftLayout->addLayout(songbookInfo());
  leftLayout->addStretch();
  dataLayout->addWidget(view());
  dataLayout->addWidget(m_noDataInfo);
  centerLayout->addLayout(leftLayout);
  centerLayout->addLayout(dataLayout);
  centerLayout->setStretch(1,2);
  mainLayout->addLayout(centerLayout);
  mainLayout->addWidget(log());

  QWidget* libraryTab = new QWidget;
  QBoxLayout *libraryLayout = new QVBoxLayout;
  libraryLayout->addLayout(mainLayout);
  libraryTab->setLayout(libraryLayout);

  // place elements into the main window
  m_mainWidget = new CTabWidget;
  m_mainWidget->setTabsClosable(true);
  m_mainWidget->setMovable(true);
  m_mainWidget->setSelectionBehaviorOnAdd(CTabWidget::SelectNew);
  connect( m_mainWidget, SIGNAL(tabCloseRequested(int)),
	   this, SLOT(closeTab(int)) );
  connect( m_mainWidget, SIGNAL(currentChanged(int)),
	   this, SLOT(changeTab(int)) );
  m_mainWidget->addTab(libraryTab, tr("Library"));
  setCentralWidget(m_mainWidget);

  // status bar with an embedded progress bar on the right
  progressBar()->setTextVisible(false);
  progressBar()->setRange(0, 0);
  progressBar()->hide();
  statusBar()->addPermanentWidget(progressBar());

  applySettings();
  selectionChanged();
  songbook()->panel();
  updateSongbookLabels();
}
//------------------------------------------------------------------------------
CMainWindow::~CMainWindow()
{
  delete m_library;
  delete m_songbook;

  {  // close db connection
    QSqlDatabase db = QSqlDatabase::database();
    db.close();
  }
  QSqlDatabase::removeDatabase(QString());
}

void CMainWindow::switchToolBar(QToolBar * toolbar)
{
  if (toolbar != current_toolbar)
    {
      toolbar->setContextMenuPolicy(Qt::PreventContextMenu); // avoid 'jump' on MacOS
      addToolBar(toolbar);
      toolbar->setVisible(true);
      current_toolbar->setVisible(false);
      removeToolBar(current_toolbar);
      current_toolbar = toolbar;
    }
}
//------------------------------------------------------------------------------
void CMainWindow::readSettings()
{
  QSettings settings;

  resize(settings.value("mainWindow/size", QSize(800,600)).toSize());

  setWorkingPath( settings.value("workingPath", QString("%1/songbook").arg(QDir::home().path())).toString() );

  settings.beginGroup("display");
  m_displayColumnArtist = settings.value("artist", true).toBool();
  m_displayColumnTitle = settings.value("title", true).toBool();
  m_displayColumnPath = settings.value("path", false).toBool();
  m_displayColumnAlbum = settings.value("album", true).toBool();
  m_displayColumnLilypond = settings.value("lilypond", false).toBool();
  m_displayColumnCover = settings.value("cover", true).toBool();
  m_displayColumnLang = settings.value("lang", false).toBool();
  m_displayCompilationLog = settings.value("log", false).toBool();
  settings.endGroup();
}
//------------------------------------------------------------------------------
void CMainWindow::writeSettings()
{
  QSettings settings;
  settings.setValue("mainWindow/size", size());
}
//------------------------------------------------------------------------------
void CMainWindow::applySettings()
{
  view()->setColumnHidden(0,!m_displayColumnArtist);
  view()->setColumnHidden(1,!m_displayColumnTitle);
  view()->setColumnHidden(2,!m_displayColumnLilypond);
  view()->setColumnHidden(3,!m_displayColumnPath);
  view()->setColumnHidden(4,!m_displayColumnAlbum);
  view()->setColumnHidden(5,!m_displayColumnCover);
  view()->setColumnHidden(6,!m_displayColumnLang);
  view()->setColumnWidth(0,250);
  view()->setColumnWidth(1,350);
  view()->setColumnWidth(4,250);
  log()->setVisible(m_displayCompilationLog);
}
//------------------------------------------------------------------------------
void CMainWindow::templateSettings()
{
  QDialog *dialog = new QDialog;
  dialog->setWindowTitle(tr("Songbook settings"));
  QVBoxLayout *layout = new QVBoxLayout;

  QScrollArea *songbookScrollArea = new QScrollArea();
  songbookScrollArea->setMinimumWidth(400);
  songbookScrollArea->setWidget(songbook()->panel());
  songbookScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  
  QDialogButtonBox *buttonBox = new QDialogButtonBox;
  
  QPushButton *button = new QPushButton(tr("Reset"));
  connect( button, SIGNAL(clicked()), songbook(), SLOT(reset()) );
  buttonBox->addButton(button, QDialogButtonBox::ResetRole);
  
  button = new QPushButton(tr("Ok"));
  button->setDefault(true);
  connect( button, SIGNAL(clicked()), dialog, SLOT(accept()) );
  buttonBox->addButton(button, QDialogButtonBox::ActionRole);

  connect( dialog, SIGNAL(accepted()), this, SLOT(updateSongbookLabels()) );
  
  layout->addWidget(songbookScrollArea);
  layout->addWidget(buttonBox);
  dialog->setLayout(layout);
  dialog->show();  
}
//------------------------------------------------------------------------------
void CMainWindow::updateSongbookLabels()
{
  m_sbInfoTitle->setText(songbook()->title());
  m_sbInfoAuthors->setText(songbook()->authors());
  m_sbInfoStyle->setText(songbook()->style());
}
//------------------------------------------------------------------------------
void CMainWindow::updateView()
{
  view()->sortByColumn(1, Qt::AscendingOrder);
  view()->sortByColumn(0, Qt::AscendingOrder);
  view()->show();
}
//------------------------------------------------------------------------------
void CMainWindow::filterChanged()
{
  QObject *object = QObject::sender();

  if (QLineEdit *lineEdit = qobject_cast< QLineEdit* >(object))
    {
      QRegExp expression = QRegExp(lineEdit->text(), Qt::CaseInsensitive, QRegExp::FixedString);
      m_proxyModel->setFilterRegExp(expression);
    }
  else
    {
      qWarning() << "Unknown caller to filterChanged.";
    }
}
//------------------------------------------------------------------------------
void CMainWindow::selectionChanged()
{
  QItemSelection invalid;
  selectionChanged(invalid, invalid);
}
//------------------------------------------------------------------------------
void CMainWindow::selectionChanged(const QItemSelection & , const QItemSelection & )
{
  m_sbNbSelected = selectionModel()->selectedRows().size();
  m_sbNbTotal = library()->rowCount();
  m_sbInfoSelection->setText(QString(tr("%1/%2"))
			     .arg(m_sbNbSelected).arg(m_sbNbTotal) );
  if(m_sbNbTotal==0)
    m_noDataInfo->show();
  else
    m_noDataInfo->hide();
}
//------------------------------------------------------------------------------
void CMainWindow::createActions()
{
  m_newSongAct = new QAction(tr("New Song"), this);
#if QT_VERSION >= 0x040600
  m_newSongAct->setIcon(QIcon::fromTheme("document-new"));
#endif
  m_newSongAct->setStatusTip(tr("Write a new song"));
  connect(m_newSongAct, SIGNAL(triggered()), this, SLOT(newSong()));

  m_newAct = new QAction(tr("New"), this);
#if QT_VERSION >= 0x040600
  m_newAct->setIcon(QIcon::fromTheme("folder-new"));
#endif
  m_newAct->setShortcut(QKeySequence::New);
  m_newAct->setStatusTip(tr("Create a new songbook"));
  connect(m_newAct, SIGNAL(triggered()), this, SLOT(newSongbook()));

  m_openAct = new QAction(tr("Open..."), this);
#if QT_VERSION >= 0x040600
  m_openAct->setIcon(QIcon::fromTheme("document-open"));
#endif
  m_openAct->setShortcut(QKeySequence::Open);
  m_openAct->setStatusTip(tr("Open a songbook"));
  connect(m_openAct, SIGNAL(triggered()), this, SLOT(open()));

  m_saveAct = new QAction(tr("Save"), this);
  m_saveAct->setShortcut(QKeySequence::Save);
#if QT_VERSION >= 0x040600
  m_saveAct->setIcon(QIcon::fromTheme("document-save"));
#endif
  m_saveAct->setStatusTip(tr("Save the current songbook"));
  connect(m_saveAct, SIGNAL(triggered()), this, SLOT(save()));

  m_saveAsAct = new QAction(tr("Save As..."), this);
  m_saveAsAct->setShortcut(QKeySequence::SaveAs);
#if QT_VERSION >= 0x040600
  m_saveAsAct->setIcon(QIcon::fromTheme("document-save-as"));
#endif
  m_saveAsAct->setStatusTip(tr("Save the current songbook with a different name"));
  connect(m_saveAsAct, SIGNAL(triggered()), this, SLOT(saveAs()));

  m_documentationAct = new QAction(tr("Online documentation"), this);
  m_documentationAct->setShortcut(QKeySequence::HelpContents);
  m_documentationAct->setIcon(QIcon::fromTheme("help-contents"));
  m_documentationAct->setStatusTip(tr("Download documentation pdf file "));
  connect(m_documentationAct, SIGNAL(triggered()), this, SLOT(documentation()));

  m_aboutAct = new QAction(tr("&About"), this);
#if QT_VERSION >= 0x040600
  m_aboutAct->setIcon(QIcon::fromTheme("help-about"));
#endif
  m_aboutAct->setStatusTip(tr("About this application"));
  connect(m_aboutAct, SIGNAL(triggered()), this, SLOT(about()));

  m_exitAct = new QAction(tr("Quit"), this);
  m_exitAct->setShortcut(QKeySequence::Close);
#if QT_VERSION >= 0x040600
  m_exitAct->setIcon(QIcon::fromTheme("application-exit"));
  m_exitAct->setShortcut(QKeySequence::Quit);
#endif
  m_exitAct->setStatusTip(tr("Quit the program"));
  connect(m_exitAct, SIGNAL(triggered()), this, SLOT(close()));

  m_preferencesAct = new QAction(tr("&Preferences"), this);
  m_preferencesAct->setStatusTip(tr("Configure the application"));
  connect(m_preferencesAct, SIGNAL(triggered()), SLOT(preferences()));

  m_selectAllAct = new QAction(tr("Select all"), this);
  m_selectAllAct->setIcon(QIcon(":/icons/select-all.png"));
  m_selectAllAct->setStatusTip(tr("Select all songs in the library"));
  connect(m_selectAllAct, SIGNAL(triggered()), SLOT(selectAll()));

  m_unselectAllAct = new QAction(tr("Unselect all"), this);
  m_unselectAllAct->setIcon(QIcon(":/icons/unselect-all.png"));
  m_unselectAllAct->setStatusTip(tr("Unselect all songs in the library"));
  connect(m_unselectAllAct, SIGNAL(triggered()), SLOT(unselectAll()));

  m_invertSelectionAct = new QAction(tr("Invert Selection"), this);
  m_invertSelectionAct->setIcon(QIcon(":/icons/invert-selection.png"));
  m_invertSelectionAct->setStatusTip(tr("Invert currently selected songs in the library"));
  connect(m_invertSelectionAct, SIGNAL(triggered()), SLOT(invertSelection()));

  m_selectEnglishAct = new QAction(tr("english"), this);
  m_selectEnglishAct->setStatusTip(tr("Select/Unselect songs in english"));
  m_selectEnglishAct->setIcon(QIcon::fromTheme("flag-en",QIcon(":/icons/en.png")));
  m_selectEnglishAct->setCheckable(true);
  connect(m_selectEnglishAct, SIGNAL(triggered(bool)), SLOT(selectLanguage(bool)));

  m_selectFrenchAct = new QAction(tr("french"), this);
  m_selectFrenchAct->setStatusTip(tr("Select/Unselect songs in french"));
  m_selectFrenchAct->setIcon(QIcon::fromTheme("flag-fr",QIcon(":/icons/fr.png")));
  m_selectFrenchAct->setCheckable(true);
  connect(m_selectFrenchAct, SIGNAL(triggered(bool)), SLOT(selectLanguage(bool)));

  m_selectSpanishAct = new QAction(tr("spanish"), this);
  m_selectSpanishAct->setStatusTip(tr("Select/Unselect songs in spanish"));
  m_selectSpanishAct->setIcon(QIcon::fromTheme("flag-es",QIcon(":/icons/es.png")));
  m_selectSpanishAct->setCheckable(true);
  connect(m_selectSpanishAct, SIGNAL(triggered(bool)), SLOT(selectLanguage(bool)));

  m_adjustColumnsAct = new QAction(tr("Auto Adjust Columns"), this);
  m_adjustColumnsAct->setStatusTip(tr("Adjust columns to contents"));
  connect(m_adjustColumnsAct, SIGNAL(triggered()),
          view(), SLOT(resizeColumnsToContents()));

  m_refreshLibraryAct = new QAction(tr("Update"), this);
  m_refreshLibraryAct->setStatusTip(tr("Update current song list from \".sg\" files"));
  connect(m_refreshLibraryAct, SIGNAL(triggered()), this, SLOT(refreshLibrary()));

  m_rebuildLibraryAct = new QAction(tr("Rebuild"), this);
  m_rebuildLibraryAct->setStatusTip(tr("Rebuild the current song list from \".sg\" files"));
  connect(m_rebuildLibraryAct, SIGNAL(triggered()), this, SLOT(rebuildLibrary()));

  m_builder = new CDownload(this);
  m_downloadDbAct = new QAction(tr("Download"),this);
  m_downloadDbAct->setStatusTip(tr("Download songs from remote location"));
#if QT_VERSION >= 0x040600
  m_downloadDbAct->setIcon(QIcon::fromTheme("folder-remote"));
#endif
  connect(m_downloadDbAct, SIGNAL(triggered()), m_builder, SLOT(dialog()));

  m_toolbarViewAct = new QAction(tr("Toolbar"),this);
  m_toolbarViewAct->setStatusTip(tr("Show or hide the toolbar in the current window"));
  m_toolbarViewAct->setCheckable(true);
  m_toolbarViewAct->setChecked(m_isToolbarDisplayed);
  connect(m_toolbarViewAct, SIGNAL(toggled(bool)), this, SLOT(setToolbarDisplayed(bool)));

  m_statusbarViewAct = new QAction(tr("Statusbar"),this);
  m_statusbarViewAct->setStatusTip(tr("Show or hide the statusbar in the current window"));
  m_statusbarViewAct->setCheckable(true);
  m_statusbarViewAct->setChecked(m_isStatusbarDisplayed);
  connect(m_statusbarViewAct, SIGNAL(toggled(bool)), this, SLOT(setStatusbarDisplayed(bool)));

  m_builder = new CResizeCovers(this);
  m_resizeCoversAct = new QAction( tr("Resize covers"), this);
  m_resizeCoversAct->setStatusTip(tr("Ensure that covers are correctly resized"));
  connect(m_resizeCoversAct, SIGNAL(triggered()), m_builder, SLOT(dialog()));

  m_builder = new CLatexPreprocessing(this);
  m_checkerAct = new QAction( tr("LaTeX Preprocessing"), this);
  m_checkerAct->setStatusTip(tr("Check for common mistakes in songs (e.g spelling, chords, LaTeX typo ...)"));
  connect(m_checkerAct, SIGNAL(triggered()), m_builder, SLOT(dialog()));

  m_buildAct = new QAction(tr("Build PDF"), this);
#if QT_VERSION >= 0x040600
  m_buildAct->setIcon(QIcon::fromTheme("document-export"));
#endif
  m_buildAct->setStatusTip(tr("Generate pdf from selected songs"));
  connect(m_buildAct, SIGNAL(triggered()), this, SLOT(build()));

  m_builder = new CMakeSongbook(this);
  m_builder->setProcessOptions(QStringList() << "clean");
  m_cleanAct = new QAction(tr("Clean"), this);
#if QT_VERSION >= 0x040600
  m_cleanAct->setIcon(QIcon::fromTheme("edit-clear"));
#endif
  m_cleanAct->setStatusTip(tr("Clean LaTeX temporary files"));
  connect(m_cleanAct, SIGNAL(triggered()), m_builder, SLOT(action()));

}
//------------------------------------------------------------------------------
void CMainWindow::setToolbarDisplayed( bool value )
{
  if( m_isToolbarDisplayed != value && m_toolbar )
    {
      m_isToolbarDisplayed = value;
      m_toolbar->setVisible(value);
    }
}
//------------------------------------------------------------------------------
bool CMainWindow::isToolbarDisplayed( )
{
  return m_isToolbarDisplayed;
}
//------------------------------------------------------------------------------
void CMainWindow::setStatusbarDisplayed( bool value )
{
  m_isStatusbarDisplayed = value;
  statusBar()->setVisible(value);
}
//------------------------------------------------------------------------------
bool CMainWindow::isStatusbarDisplayed( )
{
  return m_isStatusbarDisplayed;
}
//------------------------------------------------------------------------------
void CMainWindow::connectDb()
{
  //Connect to database
  QString path = QString("%1/.cache/songbook-client").arg(QDir::home().path());
  QDir dbdir; dbdir.mkpath( path );
  QString dbpath = QString("%1/patacrep.db").arg(path);
  bool exist = QFile::exists(dbpath); 

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
  db.setDatabaseName(dbpath);
  if (!db.open())
    {
      QMessageBox::critical(this, tr("Cannot open database"),
			    tr("Unable to establish a database connection.\n"
			       "This application needs SQLite support. "
			       "Click Cancel to exit."), QMessageBox::Cancel);
    }
  if (!exist)
    {
      QSqlQuery query;
      query.exec("create table songs ( artist text, "
		 "title text, "
		 "lilypond bool, "
		 "path text, "
		 "album text, "
		 "cover text, "
		 "lang text)");
    }

  // Initialize the song library
  m_library = new CLibrary(this);
  library()->setWorkingPath(workingPath());

  m_proxyModel->setSourceModel(library());
  m_proxyModel->setDynamicSortFilter(true);

  view()->setModel(m_proxyModel);
  view()->setShowGrid( false );
  view()->setAlternatingRowColors(true);
  view()->setSelectionMode(QAbstractItemView::MultiSelection);
  view()->setSelectionBehavior(QAbstractItemView::SelectRows);
  view()->setEditTriggers(QAbstractItemView::NoEditTriggers);
  view()->setSortingEnabled(true);
  view()->verticalHeader()->setVisible(false);

  connect(library(), SIGNAL(wasModified()),
          this, SLOT(updateView()));
  connect(library(), SIGNAL(wasModified()),
          this, SLOT(selectionChanged()));
}
//------------------------------------------------------------------------------
void CMainWindow::rebuildLibrary()
{
  //Drop table songs and recreate
  QSqlQuery query("delete from songs");
  refreshLibrary();
}
//------------------------------------------------------------------------------
void CMainWindow::refreshLibrary()
{
  QStringList filter = QStringList() << "*.sg";
  QString path = QString("%1/songs/").arg(workingPath());
  
  QDirIterator i(path, filter, QDir::NoFilter, QDirIterator::Subdirectories);
  uint count = 0;
  while(i.hasNext())
    {
      ++count;
      i.next();
    }
  progressBar()->show();
  progressBar()->setTextVisible(true);
  progressBar()->setRange(0, count);

  library()->retrieveSongs();
  progressBar()->setTextVisible(false);
  progressBar()->hide();
  statusBar()->showMessage(tr("Building database from \".sg\" files completed."));
}
//------------------------------------------------------------------------------
void CMainWindow::closeEvent(QCloseEvent *event)
{
  writeSettings();
  event->accept();
}
//------------------------------------------------------------------------------
void CMainWindow::createMenus()
{
  m_fileMenu = menuBar()->addMenu(tr("&Songbook"));
  m_fileMenu->addAction(m_newAct);
  m_fileMenu->addAction(m_openAct);
  m_fileMenu->addAction(m_saveAct);
  m_fileMenu->addAction(m_saveAsAct);
  m_fileMenu->addSeparator();
  m_fileMenu->addAction(m_buildAct);
  m_fileMenu->addAction(m_cleanAct);
  m_fileMenu->addSeparator();
  m_fileMenu->addAction(m_exitAct);

  m_editMenu = menuBar()->addMenu(tr("&Edit"));
  m_editMenu->addAction(m_selectAllAct);
  m_editMenu->addAction(m_unselectAllAct);
  m_editMenu->addAction(m_invertSelectionAct);
  m_editMenu->addSeparator();
  m_editMenu->addAction(m_preferencesAct);

  m_dbMenu = menuBar()->addMenu(tr("&Library"));
  m_dbMenu->addAction(m_newSongAct);
  m_dbMenu->addSeparator();
  m_dbMenu->addAction(m_downloadDbAct);
  m_dbMenu->addAction(m_refreshLibraryAct);
  m_dbMenu->addAction(m_rebuildLibraryAct);

  m_viewMenu = menuBar()->addMenu(tr("&View"));
  m_viewMenu->addAction(m_toolbarViewAct);
  m_viewMenu->addAction(m_statusbarViewAct);
  m_viewMenu->addAction(m_adjustColumnsAct);

  m_viewMenu = menuBar()->addMenu(tr("&Tools"));
  m_viewMenu->addAction(m_resizeCoversAct);
  m_viewMenu->addAction(m_checkerAct);

  m_helpMenu = menuBar()->addMenu(tr("&Help"));
  m_helpMenu->addAction(m_documentationAct);
  m_helpMenu->addAction(m_aboutAct);
}
//------------------------------------------------------------------------------
QGridLayout * CMainWindow::songInfo()
{
  CLabel *artistLabel = new CLabel();
  artistLabel->setElideMode(Qt::ElideRight);
  artistLabel->setFixedWidth(175);
  CLabel *titleLabel = new CLabel();
  titleLabel->setElideMode(Qt::ElideRight);
  titleLabel->setFixedWidth(175);
  CLabel *albumLabel = new CLabel();
  albumLabel->setElideMode(Qt::ElideRight);
  albumLabel->setFixedWidth(175);
  
  QDialogButtonBox *buttonBox = new QDialogButtonBox;
  QPushButton *editButton = new QPushButton(tr("Edit"));
  QPushButton *deleteButton = new QPushButton(tr("Delete"));
  editButton->setDefault(true);
  buttonBox->addButton(editButton, QDialogButtonBox::ActionRole);
  buttonBox->addButton(deleteButton, QDialogButtonBox::NoRole);

  connect(editButton, SIGNAL(clicked()), SLOT(songEditor()));
  connect(deleteButton, SIGNAL(clicked()), SLOT(deleteSong()));

  QGridLayout *layout = new QGridLayout;
  m_coverLabel.setAlignment(Qt::AlignTop);
  layout->addWidget(&m_coverLabel,0,0,4,1);
  layout->addWidget(new QLabel(tr("<i>Title:</i>")),0,1,1,1,Qt::AlignLeft);
  layout->addWidget(titleLabel,0,2,1,1);
  layout->addWidget(new QLabel(tr("<i>Artist:</i>")),1,1,1,1,Qt::AlignLeft);
  layout->addWidget(artistLabel,1,2,1,1);
  layout->addWidget(new QLabel(tr("<i>Album:</i>")),2,1,1,1,Qt::AlignLeft);
  layout->addWidget(albumLabel,2,2,1,1);
  layout->addWidget(buttonBox,3,1,1,2);
  layout->setColumnStretch(2,1);
  
  //Data mapper
  m_mapper = new QDataWidgetMapper();
  m_mapper->setModel(m_proxyModel);
  m_mapper->addMapping(artistLabel, 0, QByteArray("text"));
  m_mapper->addMapping(titleLabel, 1, QByteArray("text"));
  m_mapper->addMapping(albumLabel, 4, QByteArray("text"));
  updateCover(QModelIndex());

  connect(view(), SIGNAL(clicked(const QModelIndex &)),
          m_mapper, SLOT(setCurrentModelIndex(const QModelIndex &)));
  connect(view(), SIGNAL(clicked(const QModelIndex &)),
          SLOT(updateCover(const QModelIndex &)));

  return layout;
}
//------------------------------------------------------------------------------
QGridLayout * CMainWindow::songbookInfo()
{
  QPushButton* button = new QPushButton(tr("Settings"));
  connect(button, SIGNAL(clicked()), this, SLOT(templateSettings()));
  
  QGridLayout* layout = new QGridLayout;
  layout->addWidget(new QLabel(tr("<i>Title:</i>")),0,0,1,1);
  layout->addWidget(m_sbInfoTitle,0,1,1,2);
  layout->addWidget(new QLabel(tr("<i>Authors:</i>")),1,0,1,1);
  layout->addWidget(m_sbInfoAuthors,1,1,1,2);
  layout->addWidget(new QLabel(tr("<i>Style:</i>")),2,0,1,1);
  layout->addWidget(m_sbInfoStyle,2,1,1,2);
  layout->addWidget(new QLabel(tr("<i>Selection:</i>")),3,0,1,1);
  layout->addWidget(m_sbInfoSelection,3,1,1,2);
  layout->addWidget(button,4,2,1,1);

  m_sbInfoTitle->setElideMode(Qt::ElideRight);
  m_sbInfoTitle->setFixedWidth(250);
  m_sbInfoAuthors->setElideMode(Qt::ElideRight);
  m_sbInfoAuthors->setFixedWidth(250);
  m_sbInfoStyle->setElideMode(Qt::ElideRight);
  m_sbInfoStyle->setFixedWidth(250);

  return layout;
}
//------------------------------------------------------------------------------
void CMainWindow::updateCover(const QModelIndex & index)
{
  if (!selectionModel()->hasSelection())
    {
#if QT_VERSION >= 0x040600
      m_cover = new QPixmap(QIcon::fromTheme("image-missing").pixmap(128,128));
#else
      m_cover = new QPixmap;
#endif
      m_coverLabel.setPixmap(*m_cover);
      return;
    }

  // do not retrieve last clicked item but last selected item
  QModelIndex lastIndex = selectionModel()->selectedRows().last();
  selectionModel()->setCurrentIndex(lastIndex, QItemSelectionModel::NoUpdate);
  if (lastIndex != index)
    m_mapper->setCurrentModelIndex(lastIndex);

  QString coverpath = library()->record(m_proxyModel->mapToSource(lastIndex).row()).field("cover").value().toString();
  if (QFile::exists(coverpath))
    m_cover->load(coverpath);
  else
#if QT_VERSION >= 0x040600
    m_cover = new QPixmap(QIcon::fromTheme("image-missing").pixmap(128,128));
#else
  m_cover = new QPixmap;
#endif

  m_coverLabel.setPixmap(m_cover->scaled(128, 128, Qt::IgnoreAspectRatio));
}
//------------------------------------------------------------------------------
void CMainWindow::preferences()
{
  ConfigDialog dialog;
  dialog.exec();
  readSettings();
  applySettings();
}
//------------------------------------------------------------------------------
void CMainWindow::documentation()
{
  QDesktopServices::openUrl(QUrl(QString("http://www.patacrep.com/data/documents/doc.pdf")));
}
//------------------------------------------------------------------------------
void CMainWindow::about()
{
  QString version = tr("0.4 (January 2011)");
  QMessageBox::about(this, 
		     tr("About Patacrep Songbook Client"),
		     QString
		     (tr("<br>This program is a client for building and customizing the songbooks available on"
			 " <a href=\"http::www.patacrep.com\">www.patacrep.com</a> </br>"
			 "<br><b>Version:</b> %1 </br>"
			 "<br><b>Authors:</b> Crep (R.Goffe), Lohrun (A.Dupas) </br>")).arg(version));
}
//------------------------------------------------------------------------------
void CMainWindow::selectAll()
{
  view()->selectAll();
  view()->setFocus();
}
//------------------------------------------------------------------------------
void CMainWindow::unselectAll()
{
  view()->clearSelection();
}
//------------------------------------------------------------------------------
void CMainWindow::invertSelection()
{
  QModelIndexList indexes = selectionModel()->selectedRows();
  QModelIndex index;

  view()->selectAll();

  foreach(index, indexes)
    {
      selectionModel()->select(index, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
    }
}
//------------------------------------------------------------------------------
void CMainWindow::selectLanguage(bool selection)
{
  QString language = qobject_cast< QAction* >(QObject::sender())->text();
  QList<QModelIndex> indexes;
  QModelIndex index;

  indexes = m_proxyModel->match(m_proxyModel->index(0,6), Qt::ToolTipRole, language, -1);

  QItemSelectionModel::SelectionFlags flag = (selection ? QItemSelectionModel::Select : QItemSelectionModel::Deselect) | QItemSelectionModel::Rows;

  foreach(index, indexes)
    {
      selectionModel()->select(index, flag);
    }  
  view()->setFocus();
}
//------------------------------------------------------------------------------
QStringList CMainWindow::getSelectedSongs()
{
  QStringList songsPath;
  QModelIndexList indexes = selectionModel()->selectedRows();
  QModelIndex index;

  qSort(indexes.begin(), indexes.end());

  foreach(index, indexes)
    {
      songsPath << library()->record(m_proxyModel->mapToSource(index).row()).field("path").value().toString();
    }

  return songsPath;
}
//------------------------------------------------------------------------------
void CMainWindow::build()
{
  if(getSelectedSongs().isEmpty())
    {
      if(QMessageBox::question(this, windowTitle(), 
			       QString(tr("You did not select any song. \n "
					  "Do you want to build the songbook with all songs ?")), 
			       QMessageBox::Yes, 
			       QMessageBox::No, 
			       QMessageBox::NoButton) == QMessageBox::No)
	return;
      else
	selectAll();
    }
  
  save(true);
  
  switch(songbook()->checkFilename())
    {
    case WrongDirectory:
      statusBar()->showMessage(tr("The songbook is not in the working directory. Build aborted."));
      return;
    case WrongExtension:
      statusBar()->showMessage(tr("Wrong filename: songbook does not have \".sb\" extension. Build aborted."));
      return;
    default:
      break;
    }

  QString target = QString("%1.pdf")
    .arg(QFileInfo(songbook()->filename()).baseName());
  
  m_builder = new CMakeSongbook(this);

  //force a make clean
  m_builder->setProcessOptions(QStringList() << "clean");
  m_builder->action();
  m_builder->process()->waitForFinished();
  
  m_builder->setProcessOptions(QStringList() << target);
  m_builder->action();
}
//------------------------------------------------------------------------------
void CMainWindow::newSongbook()
{
  songbook()->reset();
  updateTitle(songbook()->filename());
}
//------------------------------------------------------------------------------
void CMainWindow::open()
{
  QString filename = QFileDialog::getOpenFileName(this,
                                                  tr("Open"),
                                                  workingPath(),
                                                  tr("Songbook (*.sb)"));
  songbook()->load(filename);
  QStringList songlist = songbook()->songs();
  QString path = QString("%1/songs/").arg(workingPath());
  songlist.replaceInStrings(QRegExp("^"),path);

  view()->clearSelection();

  QList<QModelIndex> indexes;
  QString str;
  foreach(str, songlist)
    {
      indexes = m_proxyModel->match( m_proxyModel->index(0,3), Qt::MatchExactly, str );
      if (!indexes.isEmpty())
        selectionModel()->select(indexes[0], QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }

  updateTitle(songbook()->filename());
}
//------------------------------------------------------------------------------
void CMainWindow::save(bool forced)
{
  if(songbook()->filename().isEmpty() || songbook()->filename().endsWith("default.sb"))
    {
      if(forced)
	songbook()->setFilename(QString("%1/default.sb").arg(workingPath()));
      else if(!songbook()->filename().isEmpty())
	saveAs();
    }

  updateSongsList();
  songbook()->save(songbook()->filename());
  updateTitle(songbook()->filename());
}
//------------------------------------------------------------------------------
void CMainWindow::saveAs()
{
  QFileDialog* dialog = new QFileDialog;
  QString filename = dialog->getSaveFileName(this, tr("Save as"), workingPath(),
					     tr("Songbook (*.sb)"));

  if (!filename.isEmpty())
    songbook()->setFilename(filename);

  if(dialog->result())
    save();
}
//------------------------------------------------------------------------------
void CMainWindow::updateSongsList()
{
  QStringList songlist = getSelectedSongs();
  QString path = QString("%1/songs/").arg(workingPath());
  songlist.replaceInStrings(path, QString());
  songbook()->setSongs(songlist);
}
//------------------------------------------------------------------------------
void CMainWindow::updateTitle(const QString &filename)
{
  QString text = filename.isEmpty() ? tr("New songbook") : filename;
  setWindowTitle(tr("%1 - %2[*]")
                 .arg(QCoreApplication::applicationName())
                 .arg(text));
}
//------------------------------------------------------------------------------
const QString CMainWindow::workingPath()
{
  if (!QDir( m_workingPath ).exists())
    m_workingPath = QDir::currentPath();
  return m_workingPath;
 }
//------------------------------------------------------------------------------
void CMainWindow::setWorkingPath(QString dirname)
{
  while(dirname.endsWith("/"))
    dirname.remove(-1,1);
  
  if ( dirname != m_workingPath)
    {
      m_workingPath = dirname;
      emit(workingPathChanged(dirname));

      if(!m_first)
	rebuildLibrary();
    }
  m_first = false;
}
//------------------------------------------------------------------------------
QProgressBar * CMainWindow::progressBar() const
{
  return m_progressBar;
}
//------------------------------------------------------------------------------
CSongbook * CMainWindow::songbook() const
{
  return m_songbook;
}
//------------------------------------------------------------------------------
QTableView * CMainWindow::view() const
{
  return m_view;
}
//------------------------------------------------------------------------------
CLibrary * CMainWindow::library() const
{
  return m_library;
}
//------------------------------------------------------------------------------
QItemSelectionModel * CMainWindow::selectionModel()
{
  while (library()->canFetchMore())
    library()->fetchMore();

  return view()->selectionModel();
}
//------------------------------------------------------------------------------
void CMainWindow::songEditor()
{
  if (!selectionModel()->hasSelection())
    {
      statusBar()->showMessage(tr("Please select a song to edit."));
      return;
    }

  int row = m_proxyModel->mapToSource(selectionModel()->currentIndex()).row();
  QSqlRecord record = library()->record(row);
  QString path = record.field("path").value().toString();
  QString title = record.field("title").value().toString();

  songEditor(path, title);
}
//------------------------------------------------------------------------------
void CMainWindow::songEditor(const QString &path, const QString &title)
{
  if (m_editors.contains(path))
    {
      m_mainWidget->setCurrentWidget(m_editors[path]);
      return;
    }

  CSongEditor* editor = new CSongEditor();
  editor->setPath(path);
  if (title == QString())
    {
      QFileInfo fileInfo(path);
      editor->setWindowTitle(fileInfo.fileName());
    }
  else
    {
      editor->setWindowTitle(title);
    }

  connect(editor, SIGNAL(labelChanged(const QString&)),
	  m_mainWidget, SLOT(changeTabText(const QString&)));

  m_mainWidget->addTab(editor);
  m_editors.insert(path, editor);
}
//------------------------------------------------------------------------------
void CMainWindow::newSong()
{
  CDialogNewSong *dialog = new CDialogNewSong(this);
  if (dialog->exec() == QDialog::Accepted)
    {
      songEditor(dialog->path(), dialog->title());
    }
  delete dialog;
}
//------------------------------------------------------------------------------
void CMainWindow::deleteSong()
{
  if (!selectionModel()->hasSelection())
    {
      statusBar()->showMessage(tr("Please select a song to remove."));
      return;
    }

  QString path = library()->record(m_proxyModel->mapToSource(selectionModel()->currentIndex()).row()).field("path").value().toString();

  deleteSong(path);
}
//------------------------------------------------------------------------------
void CMainWindow::deleteSong(const QString &path)
{
  if (QMessageBox::question(this, this->windowTitle(),
			    QString(tr("Are you sure you want to permanently remove the file %1 ?")).arg(path),
			    QMessageBox::Yes,
			    QMessageBox::No,
			    QMessageBox::NoButton) == QMessageBox::Yes)
    {
      //remove entry in database
      library()->removeSong(path);

      //removal on disk
      QFile file(path);
      QFileInfo fileinfo(file);
      QString tmp = fileinfo.canonicalPath();
      if (file.remove())
	{
	  QDir dir;
	  dir.rmdir(tmp); //remove dir if empty
	  //once deleted move selection in the model
	  updateCover(selectionModel()->currentIndex());
	  m_mapper->setCurrentModelIndex(selectionModel()->currentIndex());
	}
    }
}
//------------------------------------------------------------------------------
void CMainWindow::closeTab(int index)
{
  CSongEditor *editor = qobject_cast< CSongEditor* >(m_mainWidget->widget(index));
  if (editor)
    {
      if (editor->document()->isModified())
	{
	  QMessageBox::StandardButton answer = 
	    QMessageBox::question(this,
				  tr("Close"),
				  tr("There is unsaved modification in the current editor, do you really want to close it?"),
				  QMessageBox::Ok | QMessageBox::Cancel,
				  QMessageBox::Cancel);
	  if (answer != QMessageBox::Ok)
	    return;
	}
      m_editors.remove(editor->path());
      m_mainWidget->closeTab(index);
    }
}
//------------------------------------------------------------------------------
void CMainWindow::changeTab(int index)
{
  CSongEditor *editor = qobject_cast< CSongEditor* >(m_mainWidget->widget(index));

  if (editor)
    {
      switchToolBar(editor->toolbar());
      m_saveAct->setShortcutContext(Qt::WidgetShortcut);
    }
  else
    {
      switchToolBar(m_toolbar);
      m_saveAct->setShortcutContext(Qt::WindowShortcut);
    }
}
//------------------------------------------------------------------------------
QTextEdit* CMainWindow::log() const
{
  return m_log;
}
