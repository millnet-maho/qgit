/*
	Description: file viewer window

	Author: Marco Costalba (C) 2005-2006

	Copyright: See COPYING file that comes with this distribution

*/
#include <QStatusBar>
#include "mainimpl.h"
#include "git.h"
#include "annotate.h"
#include "listview.h"
#include "filecontent.h"
#include "fileview.h"

#define MAX_LINE_NUM 5

FileView::FileView(MainImpl* mi, Git* g) : Domain(mi, g, false) {

	container = new QWidget(NULL); // will be reparented to m()->tabWdg
	fileTab = new Ui_TabFile();
	fileTab->setupUi(container);
	fileTab->histListView->setup(this, git);

	m()->tabWdg->addTab(container, "File");
	tabPosition = m()->tabWdg->count() - 1;

	textEditFile = new FileContent(this, git, fileTab->textEditFile);

	// an empty string turn off the special-value text display
	fileTab->spinBoxRevision->setSpecialValueText(" ");

	clear(true); // init some stuff

	connect(git, SIGNAL(loadCompleted(const FileHistory*, const QString&)),
	        this, SLOT(on_loadCompleted(const FileHistory*, const QString&)));

	connect(m(), SIGNAL(repaintListViews(const QFont&)),
	        fileTab->histListView, SLOT(on_repaintListViews(const QFont&)));

	connect(fileTab->histListView, SIGNAL(contextMenu(const QString&, int)),
	        this, SLOT(on_contextMenu(const QString&, int)));

	connect(textEditFile, SIGNAL(annotationAvailable(bool)),
	        this, SLOT(on_annotationAvailable(bool)));

	connect(textEditFile, SIGNAL(fileAvailable(bool)),
	        this, SLOT(on_fileAvailable(bool)));

	connect(textEditFile, SIGNAL(revIdSelected(int)),
	        this, SLOT(on_revIdSelected(int)));

	connect(fileTab->toolButtonCopy, SIGNAL(clicked()),
	        this, SLOT(on_toolButtonCopy_clicked()));

	connect(fileTab->toolButtonShowAnnotate, SIGNAL(toggled(bool)),
	        this, SLOT(on_toolButtonShowAnnotate_toggled(bool)));

	connect(fileTab->toolButtonFindAnnotate, SIGNAL(toggled(bool)),
	        this, SLOT(on_toolButtonFindAnnotate_toggled(bool)));

	connect(fileTab->toolButtonRangeFilter, SIGNAL(toggled(bool)),
	        this, SLOT(on_toolButtonRangeFilter_toggled(bool)));

	connect(fileTab->toolButtonPin, SIGNAL(toggled(bool)),
	        this, SLOT(on_toolButtonPin_toggled(bool)));

	connect(fileTab->toolButtonHighlightText, SIGNAL(toggled(bool)),
	        this, SLOT(on_toolButtonHighlightText_toggled(bool)));

	connect(fileTab->spinBoxRevision, SIGNAL(valueChanged(int)),
	        this, SLOT(on_spinBoxRevision_valueChanged(int)));
}

FileView::~FileView() {

	if (!parent())
		return;

	// remove before to delete, avoids a Qt warning in QInputContext()
	m()->tabWdg->removePage(container);

	delete textEditFile; // delete this before fileTab->textEditFile
	delete fileTab;
	delete container;

	m()->statusBar()->clear(); // cleanup any pending progress info
	QApplication::restoreOverrideCursor();
}

void FileView::clear(bool complete) {

	Domain::clear(complete);

	if (complete) {
		int idx = m()->tabWdg->indexOf(container);
		m()->tabWdg->setTabText(idx, "File");
		fileTab->toolButtonCopy->setEnabled(false);
	}
	textEditFile->clear();

	annotateAvailable = fileAvailable = false;
	updateEnabledButtons();

	fileTab->toolButtonPin->setEnabled(false);
	fileTab->toolButtonPin->setOn(false); // should not trigger an update
	fileTab->spinBoxRevision->setEnabled(false);
	fileTab->spinBoxRevision->setValue(fileTab->spinBoxRevision->minValue()); // clears the box
}

bool FileView::goToCurrentAnnotation() {

	SCRef ids = fileTab->histListView->currentText(QGit::ANN_ID_COL);
	int id = (!ids.isEmpty() ? ids.toInt() : 0);
	textEditFile->goToAnnotation(id);
	return (id != 0);
}

void FileView::updateSpinBoxValue() {

	SCRef ids = fileTab->histListView->currentText(QGit::ANN_ID_COL);
	if (    ids.isEmpty()
	    || !fileTab->spinBoxRevision->isEnabled()
	    ||  fileTab->spinBoxRevision->value() == ids.toInt())
		return;

	fileTab->spinBoxRevision->setValue(ids.toInt()); // emit QSpinBox::valueChanged()
}

bool FileView::doUpdate(bool force) {

	if (st.fileName().isEmpty())
		return false;

	if (st.isChanged(StateInfo::FILE_NAME) || force) {

		clear(false);
		model()->setFileName(st.fileName());

		int idx = m()->tabWdg->indexOf(container);
		m()->tabWdg->setTabText(idx, st.fileName());

		QApplication::restoreOverrideCursor();
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		m()->statusBar()->message("Retrieving history of '" + st.fileName() + "'...");
		git->startFileHistory(model());

	} else if (fileTab->histListView->update() || st.sha().isEmpty()) {

		updateSpinBoxValue();
		m()->statusBar()->message(git->getRevInfo(st.sha()));
	}
	if (!fileTab->toolButtonPin->isOn())
		textEditFile->update();

	return true; // always accept new state
}

// ************************************ SLOTS ********************************

void FileView::updateEnabledButtons() {

	QToolButton* copy = fileTab->toolButtonCopy;
	QToolButton* showAnnotate = fileTab->toolButtonShowAnnotate;
	QToolButton* findAnnotate = fileTab->toolButtonFindAnnotate;
	QToolButton* rangeFilter = fileTab->toolButtonRangeFilter;
	QToolButton* highlight = fileTab->toolButtonHighlightText;

	// first enable
	copy->setEnabled(fileAvailable);
	showAnnotate->setEnabled(annotateAvailable);
	findAnnotate->setEnabled(annotateAvailable);
	rangeFilter->setEnabled(annotateAvailable);
	highlight->setEnabled(fileAvailable && git->isTextHighlighter());

	// then disable
	if (!showAnnotate->isOn())
		findAnnotate->setEnabled(false);

	if (highlight->isOn()) {
		findAnnotate->setEnabled(false);
		rangeFilter->setEnabled(false);
	}
	if (rangeFilter->isOn())
		highlight->setEnabled(false);

	// special case: reset range filter when changing file
	if (!annotateAvailable && rangeFilter->isOn())
		rangeFilter->toggle();
}

void FileView::on_toolButtonCopy_clicked() {

	textEditFile->copySelection();
}

void FileView::on_toolButtonShowAnnotate_toggled(bool b) {

	updateEnabledButtons();
	textEditFile->setShowAnnotate(b);

	if (b && fileTab->toolButtonFindAnnotate->isOn())
		goToCurrentAnnotation();
}

void FileView::on_toolButtonFindAnnotate_toggled(bool b) {

	updateEnabledButtons();
	if (b)
		goToCurrentAnnotation();
}

void FileView::on_toolButtonPin_toggled(bool b) {
// button is enabled and togglable only if st.sha() is found

	fileTab->spinBoxRevision->setDisabled(b);

	if (!b) {
		updateSpinBoxValue();
		textEditFile->update(true);
	}
}

void FileView::on_toolButtonRangeFilter_toggled(bool b) {

	updateEnabledButtons();
	if (b) {
		if (!textEditFile->annotateAvailable()) {
			dbs("ASSERT in on_toolButtonRangeFilter_toggled: annotate not available");
			return;
		}
		if (!fileTab->textEditFile->hasSelectedText()) {
			m()->statusBar()->message("Please select some text");
			return;
		}
	}
	bool rangeFilterActive = textEditFile->rangeFilter(b);
	filterOnRange(rangeFilterActive);
}

void FileView::on_toolButtonHighlightText_toggled(bool b) {

	updateEnabledButtons();
	textEditFile->setHighlightSource(b);
}

void FileView::on_spinBoxRevision_valueChanged(int id) {

	if (id != fileTab->spinBoxRevision->minValue()) {

		SCRef selRev(fileTab->histListView->getSha(id));
		if (st.sha() != selRev) { // to avoid looping
			st.setSha(selRev);
			st.setSelectItem(true);
			UPDATE();
		}
	}
}

void FileView::on_loadCompleted(const FileHistory* f, const QString&) {

	QApplication::restoreOverrideCursor();

	if (f != model())
		return;

	fileTab->histListView->showIdValues();
	int maxId = model()->rowCount();
	if (maxId == 0) {
		m()->statusBar()->clear();
		return;
	}
	fileTab->spinBoxRevision->setMaxValue(maxId);
	fileTab->toolButtonPin->setEnabled(true);
	fileTab->spinBoxRevision->setEnabled(true);

	UPDATE();

	updateProgressBar(0);
	textEditFile->startAnnotate(model());
}

void FileView::showAnnotation() {

	if (  !fileTab->toolButtonPin->isOn()
	    && fileTab->toolButtonShowAnnotate->isEnabled()
	    && fileTab->toolButtonShowAnnotate->isOn()) {

		textEditFile->setShowAnnotate(true);

		if (   fileTab->toolButtonFindAnnotate->isEnabled()
		    && fileTab->toolButtonFindAnnotate->isOn())

			goToCurrentAnnotation();
	}
}

void FileView::on_annotationAvailable(bool b) {

	annotateAvailable = b;
	updateEnabledButtons();

	if (b)
		showAnnotation(); // in case annotation got ready after file
}

void FileView::on_fileAvailable(bool b) {

	fileAvailable = b;
	updateEnabledButtons();

	if (b) {
		// code range is independent from annotation
		if (fileTab->toolButtonRangeFilter->isOn())
			textEditFile->goToRangeStart();

		showAnnotation(); // in case file got ready after annotation
	}
}

void FileView::on_revIdSelected(int id) {

	if (id != 0 && fileTab->spinBoxRevision->isEnabled())
		fileTab->spinBoxRevision->setValue(id);
}

// ******************************* data events ****************************

bool FileView::event(QEvent* e) {

	if (e->type() == (int)QGit::ANN_PRG_EV) {
		updateProgressBar(((AnnotateProgressEvent*)e)->myData().toInt());
		return true;
	}
	return Domain::event(e);
}

void FileView::updateProgressBar(int annotatedNum) {

	uint tot = model()->rowCount();
	if (tot == 0)
		return;

	int cc = (annotatedNum * 100) / tot;
	int idx = (annotatedNum * 40) / tot;
	QString head("Annotating '" + st.fileName() + "' [");
	QString tail("] " + QString::number(cc) + " %");
	QString done, toDo;
	done.fill('.', idx);
	toDo.fill(' ', 40 - idx);
	m()->statusBar()->message(head + done + toDo + tail);
}

void FileView::filterOnRange(bool isOn) {
// TODO integrate with mainimpl function

// 	Q3ListView* hv = fileTab->histListView; FIXME
// 	Q3ListViewItemIterator it(hv);
// 	bool evenLine = false;
// 	int visibleCnt = 0;
// 	RangeInfo r;
// 	while (it.current()) {
// 		ListViewItem* item = static_cast<ListViewItem*>(it.current());
//
// 		if (isOn) {
// 			if (!textEditFile->getRange(item->sha(), &r))
// 				continue;
//
// 			if (r.modified) {
// 				item->setEven(evenLine);
// 				evenLine = !evenLine;
// 				visibleCnt++;
// 			} else
// 				item->setVisible(false);
// 		} else {
// 			item->setEven(evenLine);
// 			evenLine = !evenLine;
// 			if (!item->isVisible())
// 				item->setVisible(true);
// 		}
// 		++it;
// 	}
// 	QString msg;
// 	if (isOn)
// 		msg = QString("Found %1 matches. Toggle filter "
// 		              "button to remove the filter").arg(visibleCnt);
// 	else
// 		hv->ensureItemVisible(hv->currentItem());
//
// 	m()->statusBar()->message(msg);
}