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

#include "preferences.hh"
#include "file-chooser.hh"

#include <QtGui>

// Config Dialog

ConfigDialog::ConfigDialog()
  : QDialog()
{
  m_contentsWidget = new QListWidget;
  m_contentsWidget->setViewMode(QListView::IconMode);
  m_contentsWidget->setIconSize(QSize(96, 84));
  m_contentsWidget->setMovement(QListView::Static);
  m_contentsWidget->setMaximumWidth(128);
  m_contentsWidget->setSpacing(12);

  m_pagesWidget = new QStackedWidget;
  m_pagesWidget->addWidget(new OptionsPage);
  m_pagesWidget->addWidget(new DisplayPage);

  QPushButton *closeButton = new QPushButton(tr("Close"));

  createIcons();
  m_contentsWidget->setCurrentRow(0);

  connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));

  QHBoxLayout *horizontalLayout = new QHBoxLayout;
  horizontalLayout->addWidget(m_contentsWidget);
  horizontalLayout->addWidget(m_pagesWidget, 1);

  QHBoxLayout *buttonsLayout = new QHBoxLayout;
  buttonsLayout->addStretch(1);
  buttonsLayout->addWidget(closeButton);

  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->addLayout(horizontalLayout);
  mainLayout->addStretch(1);
  mainLayout->addSpacing(12);
  mainLayout->addLayout(buttonsLayout);
  setLayout(mainLayout);

  setWindowTitle(tr("Preferences"));
}

void ConfigDialog::createIcons()
{
  QListWidgetItem *optionsButton = new QListWidgetItem(m_contentsWidget);
  //optionsButton->setIcon(QIcon::fromTheme("preferences-system"));
  optionsButton->setText(tr("Options"));
  optionsButton->setTextAlignment(Qt::AlignHCenter);
  optionsButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

  QListWidgetItem *displayButton = new QListWidgetItem(m_contentsWidget);
  //displayButton->setIcon(QIcon::fromTheme("preferences-columns"));
  displayButton->setText(tr("Display"));
  displayButton->setTextAlignment(Qt::AlignHCenter);
  displayButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

  connect(m_contentsWidget,
          SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
          this, SLOT(changePage(QListWidgetItem*,QListWidgetItem*)));
}

void ConfigDialog::changePage(QListWidgetItem *current,
                              QListWidgetItem *previous)
{
  if (!current)
    current = previous;

  m_pagesWidget->setCurrentIndex(m_contentsWidget->row(current));
}

void ConfigDialog::closeEvent(QCloseEvent *event)
{
  for( int i = 0 ; i < m_pagesWidget->count() ; ++i )
    {
      m_pagesWidget->widget(i)->close();
    }
}

// Display Page

DisplayPage::DisplayPage(QWidget *parent)
  : QWidget(parent)
{
  QGroupBox *displayColumnsGroupBox = new QGroupBox(tr("Display Columns"));

  m_artistCheckBox = new QCheckBox(tr("Artist"));
  m_titleCheckBox = new QCheckBox(tr("Title"));
  m_pathCheckBox = new QCheckBox(tr("Path"));
  m_albumCheckBox = new QCheckBox(tr("Album"));
  m_lilypondCheckBox = new QCheckBox(tr("Lilypond"));
  m_coverCheckBox = new QCheckBox(tr("Cover"));
  m_langCheckBox = new QCheckBox(tr("Language"));

  QVBoxLayout *displayColumnsLayout = new QVBoxLayout;
  displayColumnsLayout->addWidget(m_artistCheckBox);
  displayColumnsLayout->addWidget(m_titleCheckBox);
  displayColumnsLayout->addWidget(m_pathCheckBox);
  displayColumnsLayout->addWidget(m_albumCheckBox);
  displayColumnsLayout->addWidget(m_lilypondCheckBox);
  displayColumnsLayout->addWidget(m_coverCheckBox);
  displayColumnsLayout->addWidget(m_langCheckBox);
  displayColumnsGroupBox->setLayout(displayColumnsLayout);

  QGroupBox *displayWindowsGroupBox = new QGroupBox(tr("Display windows"));
  m_compilationLogCheckBox = new QCheckBox(tr("Compilation log"));

  QVBoxLayout *displayWindowsLayout = new QVBoxLayout;
  displayWindowsLayout->addWidget(m_compilationLogCheckBox);
  displayWindowsGroupBox->setLayout(displayWindowsLayout);

  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->addWidget(displayColumnsGroupBox);
  mainLayout->addWidget(displayWindowsGroupBox);
  mainLayout->addStretch(1);
  setLayout(mainLayout);

  readSettings();
}

void DisplayPage::readSettings()
{
  QSettings settings;
  settings.beginGroup("display");
  m_artistCheckBox->setChecked(settings.value("artist", true).toBool());
  m_titleCheckBox->setChecked(settings.value("title", true).toBool());
  m_pathCheckBox->setChecked(settings.value("path", false).toBool());
  m_albumCheckBox->setChecked(settings.value("album", true).toBool());
  m_lilypondCheckBox->setChecked(settings.value("lilypond", false).toBool());
  m_coverCheckBox->setChecked(settings.value("cover", true).toBool());
  m_langCheckBox->setChecked(settings.value("lang", false).toBool());
  m_compilationLogCheckBox->setChecked(settings.value("log", false).toBool());
  settings.endGroup();
}

void DisplayPage::writeSettings()
{
  QSettings settings;
  settings.beginGroup("display");
  settings.setValue("artist", m_artistCheckBox->isChecked());
  settings.setValue("title", m_titleCheckBox->isChecked());
  settings.setValue("path", m_pathCheckBox->isChecked());
  settings.setValue("album", m_albumCheckBox->isChecked());
  settings.setValue("lilypond", m_lilypondCheckBox->isChecked());
  settings.setValue("cover", m_coverCheckBox->isChecked());
  settings.setValue("lang", m_langCheckBox->isChecked());
  settings.setValue("log", m_compilationLogCheckBox->isChecked());
  settings.endGroup();
}

void DisplayPage::closeEvent(QCloseEvent *event)
{
  writeSettings();
  event->accept();
}

// Option Page

OptionsPage::OptionsPage(QWidget *parent)
  : QWidget(parent)
  , m_workingPath(0)
  , m_workingPathValid(0)
{
  m_workingPathValid = new QLabel;

  m_workingPath = new CFileChooser();
  m_workingPath->setType(CFileChooser::DirectoryChooser);
  m_workingPath->setCaption(tr("Songbook path"));

  connect(m_workingPath, SIGNAL(pathChanged(const QString&)),
          this, SLOT(checkWorkingPath(const QString&)));

  readSettings();

  // working path
  QGroupBox *workingPathGroupBox
    = new QGroupBox(tr("Directory for Patacrep Songbook"));

  checkWorkingPath(m_workingPath->path());

  QLayout *workingPathLayout = new QVBoxLayout;
  workingPathLayout->addWidget(m_workingPath);
  workingPathLayout->addWidget(m_workingPathValid);
  workingPathGroupBox->setLayout(workingPathLayout);

  // check application
  QGroupBox *checkApplicationGroupBox
    = new QGroupBox(tr("Check application dependencies"));

  QPushButton *checkApplicationButton = new QPushButton(tr("Check"));
  connect(checkApplicationButton, SIGNAL(clicked()),
          this, SLOT(checkApplication()));
  m_lilypondLabel = new QLabel(tr("<a href=\"http://lilypond.org/\">lilypond</a>: <font color=orange>%1</font>"));
  m_gitLabel = new QLabel(tr("<a href=\"http://git-scm.com/\">git</a>: <font color=orange>%1</font>"));

  QBoxLayout *checkApplicationLayout = new QVBoxLayout;
  checkApplicationLayout->addWidget(m_lilypondLabel);
  checkApplicationLayout->addWidget(m_gitLabel);
  checkApplicationLayout->addWidget(checkApplicationButton);
  checkApplicationGroupBox->setLayout(checkApplicationLayout);

  // main layout
  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->addWidget(workingPathGroupBox);
  mainLayout->addWidget(checkApplicationGroupBox);
  mainLayout->addStretch(1);
  setLayout(mainLayout);

  checkApplication();
}

void OptionsPage::readSettings()
{
  QSettings settings;
  m_workingPath->setPath(settings.value("workingPath", QString("%1/songbook/").arg(QDir::home().path())).toString());
}

void OptionsPage::writeSettings()
{
  QSettings settings;
  settings.setValue("workingPath", m_workingPath->path());
}

void OptionsPage::closeEvent(QCloseEvent *event)
{
  writeSettings();
  event->accept();
}

void OptionsPage::checkWorkingPath(const QString &path)
{
  QDir directory(path);

  bool error = true;
  bool warning = true;

  QString message;

  if (!directory.exists())
    {
      message = tr("the directory does not exist");
    }
  else if (!directory.exists("makefile"))
    {
      message = tr("makefile not found");
    }
  else if (!directory.exists("songbook.py"))
    {
      message = tr("songbook software (songbook.py) not found");
    }
  else if (!directory.exists("songs"))
    {
      message = tr("songs/ directory not found");
    }
  else if (!directory.exists("img"))
    {
      message = tr("img/ directory not found");
    }
  else if (!directory.exists("lilypond"))
    {
      error = false;
      message = tr("lilypond/ directory not found");
    }
  else if (!directory.exists("utils"))
    {
      error = false;
      message = tr("utils/ directory not found");
    }
  else
    {
      error = false;
      warning = false;
      message = tr("The directory is valid");
    }
  
  QString mask("<font color=%1>%2%3.</font>");
  if (error)
    {
      mask = mask.arg("red").arg("Error: ");
    }
  else if (warning)
    {
      mask = mask.arg("orange").arg("Warning: ");
    }
  else
    {
      mask = mask.arg("green").arg("");
    }
  m_workingPathValid->setText(mask.arg(message));
}


void OptionsPage::checkApplication()
{
  QProcess *process;

  process = new QProcess(m_gitLabel);
  connect(process, SIGNAL(error(QProcess::ProcessError)),
          this, SLOT(processError(QProcess::ProcessError)));

  process->start("git", QStringList() << "--version");
  if (process->waitForFinished())
    {
      QRegExp rx("git version ([^\n]+)");
      rx.indexIn(process->readAllStandardOutput().data());
      m_gitLabel->setText(m_gitLabel->text().replace("orange","green").arg(rx.cap(1)));
    }

  process = new QProcess(m_lilypondLabel);
  connect(process, SIGNAL(error(QProcess::ProcessError)),
          this, SLOT(processError(QProcess::ProcessError)));

  process->start("lilypond", QStringList() << "--version");
  if (process->waitForFinished())
    {
      QRegExp rx("GNU LilyPond ([^\n]+)");
      rx.indexIn(process->readAllStandardOutput().data());
      m_lilypondLabel->setText(m_lilypondLabel->text().replace("orange","green").arg(rx.cap(1)));
    }
}

void OptionsPage::processError(QProcess::ProcessError error)
{
  QProcess *process = qobject_cast< QProcess* >(QObject::sender());
  if (process)
    {
      QLabel *label =  qobject_cast< QLabel* >(process->parent());
      if (label)
        label->setText(label->text().arg("not found"));
    }
}
