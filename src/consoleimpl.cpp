/*
	Description: stdout viewer

	Author: Marco Costalba (C) 2006

	Copyright: See COPYING file that comes with this distribution

*/
#include <QSettings>
#include <QStatusBar>
#include <QMessageBox>
#include "myprocess.h"
#include "git.h"
#include "consoleimpl.h"

ConsoleImpl::ConsoleImpl(const QString& nm, Git* g) : git(g), actionName(nm) {

	setAttribute(Qt::WA_DeleteOnClose);
	setupUi(this);
	textEditOutput->setCurrentFont(QGit::TYPE_WRITER_FONT);
	QFont f = textLabelCmd->font();
	f.setBold(true);
	textLabelCmd->setFont(f);
	setCaption("\'" + actionName + "\' output window - QGit");
	QSettings settings;
	restoreGeometry(settings.value(QGit::CON_GEOM_KEY).toByteArray());
}

void ConsoleImpl::pushButtonOk_clicked() {

	close();
}

void ConsoleImpl::pushButtonTerminate_clicked() {

	git->cancelProcess(proc);
	procFinished();
}

void ConsoleImpl::closeEvent(QCloseEvent* ce) {

	if (proc && proc->state() == QProcess::Running)
		if (QMessageBox::question(this, "Action output window - QGit",
		    "Action is still running.\nAre you sure you want to close "
		    "the window and leave the action running in background?",
		    "&Yes", "&No", QString(), 1, 1) == 1) {
			ce->ignore();
			return;
		}
	if (QApplication::overrideCursor())
		QApplication::restoreOverrideCursor();

	QSettings settings;
	settings.setValue(QGit::CON_GEOM_KEY, saveGeometry());
	QMainWindow::closeEvent(ce);
}

bool ConsoleImpl::start(const QString& cmd, const QString& cmdArgs) {

	statusBar()->message("Executing \'" + actionName + "\' action...");

	QString t(cmd.section('\n', 1, 0xffffffff, QString::SectionIncludeLeadingSep));

	// in case of a multi-sequence, line arguments are bounded to first command only
	QString txt = cmd.section('\n', 0, 0).append(cmdArgs).append(t);
	textLabelCmd->setText(txt);

	if (t.stripWhiteSpace().isEmpty())
		// any one-line command followed by a newline would fail
		proc = git->runAsync(cmd.stripWhiteSpace(), this);
	else
		proc = git->runAsScript(cmd, this); // wrap in a script

	if (proc.isNull())
		deleteLater();
	else
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	return !proc.isNull();
}

void ConsoleImpl::procReadyRead(const QString& data) {

	QString newParagraph;
	if (QGit::stripPartialParaghraps(data, &newParagraph, &inpBuf))
		// QTextEdit::append() adds a new paragraph,
		// i.e. inserts a LF if not already present.
		textEditOutput->append(newParagraph);
}

void ConsoleImpl::procFinished() {

	textEditOutput->append(inpBuf);
	inpBuf = "";
	QApplication::restoreOverrideCursor();
	statusBar()->message("End of \'" + actionName + "\' execution.");
	pushButtonTerminate->setEnabled(false);
	emit customAction_exited(actionName);
}