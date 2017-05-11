/*******************************************************************************************************
 DkWidgets.cpp
 Created on:	17.05.2011
 
 nomacs is a fast and small image viewer with the capability of synchronizing multiple instances
 
 Copyright (C) 2011-2013 Markus Diem <markus@nomacs.org>
 Copyright (C) 2011-2013 Stefan Fiel <stefan@nomacs.org>
 Copyright (C) 2011-2013 Florian Kleber <florian@nomacs.org>

 This file is part of nomacs.

 nomacs is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 nomacs is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 *******************************************************************************************************/

#include "DkWidgets.h"

#include "DkUtils.h"
#include "DkTimer.h"
#include "DkThumbs.h"
#include "DkImageContainer.h"
#include "DkToolbars.h"
#include "DkImageStorage.h"
#include "DkSettings.h"
#include "DkStatusBar.h"
#include "DkActionManager.h"

#pragma warning(push, 0)	// no warnings from includes - begin
#include <QMainWindow>
#include <QObject>
#include <QColor>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QRadioButton>
#include <QAction>
#include <QBoxLayout>
#include <QDialog>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QMouseEvent>
#include <QToolButton>
#include <QComboBox>
#include <QMessageBox>
#include <QStringBuilder>
#include <QPointer>
#include <QTimer>
#include <QMap>
#include <QVector2D>
#include <qmath.h>
#include <QScrollBar>
#include <QFileSystemModel>
#include <QDockWidget>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QToolTip>
#include <QProgressDialog>
#include <QHeaderView>
#include <QMenu>
#include <QScrollArea>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsItem>
#include <QtConcurrentRun>
#include <QMimeData>
#include <QTimeLine>
#include <QGraphicsItemAnimation>
#include <QLineEdit>
#include <QThread>
#include <QFuture>
#include <QFutureWatcher>
#include <qtconcurrentmap.h>
#include <QColor>
#include <QVBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QColorDialog>
#include <QCompleter>
#include <QDirModel>
#include <QSvgRenderer>
#include <QFileDialog>
#include <QInputDialog>
#pragma warning(pop)		// no warnings from includes - end

namespace nmc {

// DkFolderScrollBar --------------------------------------------------------------------
DkFolderScrollBar::DkFolderScrollBar(QWidget* parent) : QSlider(Qt::Horizontal, parent) {
	
	setObjectName("DkFolderScrollBar");
	init();
	mMouseDown = false;
}

DkFolderScrollBar::~DkFolderScrollBar() {
}

// DkWidget stuff
void DkFolderScrollBar::registerAction(QAction* action) {
	connect(this, SIGNAL(visibleSignal(bool)), action, SLOT(setChecked(bool)));
}

void DkFolderScrollBar::block(bool blocked) {
	this->mBlocked = blocked;
	setVisible(false);
}

void DkFolderScrollBar::setDisplaySettings(QBitArray* displayBits) {
	mDisplaySettingsBits = displayBits;
}

bool DkFolderScrollBar::getCurrentDisplaySetting() {

	if (!mDisplaySettingsBits)
		return false;

	if (DkSettingsManager::param().app().currentAppMode < 0 || DkSettingsManager::param().app().currentAppMode >= mDisplaySettingsBits->size()) {
		qDebug() << "[WARNING] illegal app mode: " << DkSettingsManager::param().app().currentAppMode;
		return false;
	}

	return mDisplaySettingsBits->testBit(DkSettingsManager::param().app().currentAppMode);
}

void DkFolderScrollBar::updateDir(QVector<QSharedPointer<DkImageContainerT> > images) {

	setMaximum(images.size()-1);
}

void DkFolderScrollBar::updateFile(int idx) {
	
	if (mMouseDown)
		return;

	if (isVisible()) {
		blockSignals(true);
		QSlider::setValue(idx);
		blockSignals(false);
	}
}

void DkFolderScrollBar::setValue(int idx) {

	QSlider::setValue(idx);
}

void DkFolderScrollBar::mousePressEvent(QMouseEvent *event) {

	blockSignals(true);
	QSlider::mousePressEvent(event);
}

void DkFolderScrollBar::mouseReleaseEvent(QMouseEvent *event) {

	mMouseDown = false;
	blockSignals(false);
	emit valueChanged(value());
	QSlider::mouseReleaseEvent(event);
}

void DkFolderScrollBar::init() {

	setMouseTracking(true);

	mBgCol = (DkSettingsManager::param().app().appMode == DkSettings::mode_frameless) ?
		DkSettingsManager::param().display().bgColorFrameless :
		DkSettingsManager::param().display().hudBgColor;

	mShowing = false;
	mHiding = false;
	mBlocked = false;
	mDisplaySettingsBits = 0;
	mOpacityEffect = 0;

	// painter problems if the widget is a child of another that has the same graphicseffect
	// widget starts on hide
	mOpacityEffect = new QGraphicsOpacityEffect(this);
	mOpacityEffect->setOpacity(0);
	mOpacityEffect->setEnabled(false);
	setGraphicsEffect(mOpacityEffect);

	setVisible(false);
}

void DkFolderScrollBar::show(bool saveSettings) {

	// here is a strange problem if you add a DkWidget to another DkWidget -> painters crash
	if (!mBlocked && !mShowing) {
		mHiding = false;
		mShowing = true;
		setVisible(true, saveSettings);
		animateOpacityUp();
	}
}

void DkFolderScrollBar::hide(bool saveSettings) {

	if (!mHiding) {
		mHiding = true;
		mShowing = false;
		animateOpacityDown();

		// set display bit here too -> since the final call to setVisible takes a few seconds
		if (saveSettings && mDisplaySettingsBits && mDisplaySettingsBits->size() > DkSettingsManager::param().app().currentAppMode) {
			mDisplaySettingsBits->setBit(DkSettingsManager::param().app().currentAppMode, false);
		}
	}
}

void DkFolderScrollBar::setVisible(bool visible, bool saveSettings) {

	if (mBlocked) {
		QWidget::setVisible(false);
		return;
	}

	if (visible && !isVisible() && !mShowing)
		mOpacityEffect->setOpacity(100);

	QWidget::setVisible(visible);
	emit visibleSignal(visible);	// if this gets slow -> put it into hide() or show()

	if (saveSettings && mDisplaySettingsBits && mDisplaySettingsBits->size() > DkSettingsManager::param().app().currentAppMode) {
		mDisplaySettingsBits->setBit(DkSettingsManager::param().app().currentAppMode, visible);
	}
}

void DkFolderScrollBar::animateOpacityUp() {

	if (!mShowing)
		return;

	mOpacityEffect->setEnabled(true);
	if (mOpacityEffect->opacity() >= 1.0f || !mShowing) {
		mOpacityEffect->setOpacity(1.0f);
		mShowing = false;
		mOpacityEffect->setEnabled(false);
		return;
	}

	QTimer::singleShot(20, this, SLOT(animateOpacityUp()));
	mOpacityEffect->setOpacity(mOpacityEffect->opacity()+0.05);
}

void DkFolderScrollBar::animateOpacityDown() {

	if (!mHiding)
		return;

	mOpacityEffect->setEnabled(true);
	if (mOpacityEffect->opacity() <= 0.0f) {
		mOpacityEffect->setOpacity(0.0f);
		mHiding = false;
		setVisible(false, false);	// finally hide the widget
		mOpacityEffect->setEnabled(false);
		return;
	}

	QTimer::singleShot(20, this, SLOT(animateOpacityDown()));
	mOpacityEffect->setOpacity(mOpacityEffect->opacity()-0.05);
}

// DkThumbsSaver --------------------------------------------------------------------
DkThumbsSaver::DkThumbsSaver(QWidget* parent) : DkWidget(parent) {
	mStop = false;
	mCLoadIdx = 0;
	mNumSaved = 0;
}

void DkThumbsSaver::processDir(QVector<QSharedPointer<DkImageContainerT> > images, bool forceSave) {

	if (images.empty())
		return;

	mStop = false;
	mCLoadIdx = 0;
	mNumSaved = 0;

	mPd = new QProgressDialog(tr("\nCreating thumbnails...\n") + images.first()->filePath(), 
		tr("Cancel"), 
		0, 
		(int)images.size(), 
		DkUtils::getMainWindow());
	mPd->setWindowTitle(tr("Thumbnails"));

	//pd->setWindowModality(Qt::WindowModal);

	connect(this, SIGNAL(numFilesSignal(int)), mPd, SLOT(setValue(int)));
	connect(mPd, SIGNAL(canceled()), this, SLOT(stopProgress()));

	mPd->show();

	this->mForceSave = forceSave;
	this->mImages = images;
	loadNext();

}

void DkThumbsSaver::thumbLoaded(bool) {

	mNumSaved++;
	emit numFilesSignal(mNumSaved);

	if (mNumSaved == mImages.size() || mStop) {
		if (mPd) {
			mPd->close();
			mPd->deleteLater();
			mPd = 0;
		}
		mStop = true;
	}
	else
		loadNext();
}

void DkThumbsSaver::loadNext() {
	
	if (mStop)
		return;

	int missing = DkSettingsManager::param().resources().maxThumbsLoading-DkSettingsManager::param().resources().numThumbsLoading;
	int numLoading = mCLoadIdx+missing;
	int force = (mForceSave) ? DkThumbNail::force_save_thumb : DkThumbNail::save_thumb;

	qDebug() << "missing: " << missing << " num loading: " << numLoading;
	qDebug() << "loading bounds: " << mCLoadIdx << " - " << numLoading;

	for (int idx = mCLoadIdx; idx < mImages.size() && idx < numLoading; idx++) {
		mCLoadIdx++;
		connect(mImages.at(idx)->getThumb().data(), SIGNAL(thumbLoadedSignal(bool)), this, SLOT(thumbLoaded(bool)));
		mImages.at(idx)->getThumb()->fetchThumb(force);
	}
}

void DkThumbsSaver::stopProgress() {

	mStop = true;
}

// DkFileSystemModel --------------------------------------------------------------------
DkFileSystemModel::DkFileSystemModel(QObject* parent /* = 0 */) : QFileSystemModel(parent) {

	// some custom settings
	setRootPath(QDir::rootPath());
	setNameFilters(DkSettingsManager::param().app().fileFilters);
	setReadOnly(false);
	//setSupportedDragActions(Qt::CopyAction | Qt::MoveAction);

}

// DkSortFileProxyModel --------------------------------------------------------------------
DkSortFileProxyModel::DkSortFileProxyModel(QObject* parent /* = 0 */) : QSortFilterProxyModel(parent) {

}

bool DkSortFileProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {

	if (left.data().canConvert(QVariant::Url)) {

		QFileInfo lf = left.data().toString();
		QFileInfo rf = right.data().toString();

		// could not find a better way to tell files from dirs appart (isDir() is not what we expect)
		if (lf.suffix().isEmpty() && !rf.suffix().isEmpty())
			return true;
		else if (!lf.suffix().isEmpty() && rf.suffix().isEmpty())
			return false;		


		QString ls = (!lf.fileName().isEmpty()) ? lf.fileName() : lf.absoluteFilePath();	// otherwise e.g. C: is empty
		QString rs = (!rf.fileName().isEmpty()) ? rf.fileName() : rf.absoluteFilePath();

		QString ld = ls.section(QRegExp("[A-Z]:"), 1, -1, QString::SectionIncludeLeadingSep);
		
		// sort by drive letter if present
		if (!ld.isEmpty()) {
			ld.truncate(2);
			ls = ld;
		}

		QString rd = rs.section(QRegExp("[A-Z]:"), 1, -1, QString::SectionIncludeLeadingSep);

		// sort by drive letter if present
		if (!rd.isEmpty()) {
			rd.truncate(2);
			rs = rd;
		}

		return DkUtils::compLogicQString(ls, rs);
	}

	return QSortFilterProxyModel::lessThan(left, right);
}

// DkExplorer --------------------------------------------------------------------
DkExplorer::DkExplorer(const QString& title, QWidget* parent /* = 0 */, Qt::WindowFlags flags /* = 0 */) : DkDockWidget(title, parent, flags) {

	setObjectName("DkExplorer");
	createLayout();
	readSettings();

	connect(fileTree, SIGNAL(clicked(const QModelIndex&)), this, SLOT(fileClicked(const QModelIndex&)));
	
	if (mLoadSelected)
		connect(fileTree->selectionModel(), SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(fileClicked(const QModelIndex&)), Qt::UniqueConnection);
}

DkExplorer::~DkExplorer() {
	
	// save settings
	writeSettings();
}

void DkExplorer::createLayout() {

	fileModel = new DkFileSystemModel(this);
	
	sortModel = new DkSortFileProxyModel(this);
	sortModel->setSourceModel(fileModel);
	sortModel->setSortLocaleAware(true);

	fileTree = new QTreeView(this);
	fileTree->setSortingEnabled(true);
	fileTree->setModel(sortModel);
	fileTree->setDragEnabled(true);
	//fileTree->setContextMenuPolicy(Qt::CustomContextMenu);

	// by default descendingOrder is set
	fileTree->header()->setSortIndicator(0, Qt::AscendingOrder);
	fileTree->header()->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

	setWidget(fileTree);
}

void DkExplorer::setCurrentImage(QSharedPointer<DkImageContainerT> img) {

	if (!img)
		return;

	setCurrentPath(img->filePath());
}

void DkExplorer::setCurrentPath(const QString& filePath) {

	// expand folders
	if (QFileInfo(filePath).isDir())
		fileTree->expand(sortModel->mapFromSource(fileModel->index(filePath)));

	fileTree->setCurrentIndex(sortModel->mapFromSource(fileModel->index(filePath)));
}

void DkExplorer::fileClicked(const QModelIndex &index) const {

	QFileInfo cFile = fileModel->fileInfo(sortModel->mapToSource(index));

	qDebug() << "opening: " << cFile.absoluteFilePath();

	if (DkUtils::isValid(cFile))
		emit openFile(cFile.absoluteFilePath());
	else if (cFile.isDir())
		emit openDir(cFile.absoluteFilePath());
}

void DkExplorer::contextMenuEvent(QContextMenuEvent *event) {

	QMenu* cm = new QMenu(this);

	// enable editing
	QAction* editAction = new QAction(tr("Editable"), this);
	editAction->setCheckable(true);
	editAction->setChecked(!fileModel->isReadOnly());
	connect(editAction, SIGNAL(triggered(bool)), this, SLOT(setEditable(bool)));

	// open selected images
	QAction* selAction = new QAction(tr("Open Selected Image"), this);
	selAction->setCheckable(true);
	selAction->setChecked(mLoadSelected);
	connect(selAction, SIGNAL(triggered(bool)), this, SLOT(loadSelectedToggled(bool)));

	cm->addAction(editAction);
	cm->addAction(selAction);
	cm->addSeparator();

	// adjust sizes
	QAction* sizeAction = new QAction(tr("Adjust Columns"), this);
	connect(sizeAction, SIGNAL(triggered()), this, SLOT(adjustColumnWidth()));

	cm->addAction(sizeAction);
	cm->addSeparator();

	columnActions.clear();	// quick&dirty

	for (int idx = 0; idx < fileModel->columnCount(); idx++) {

		QAction* action = new QAction(fileModel->headerData(idx, Qt::Horizontal).toString(), this);
		action->setCheckable(true);
		action->setChecked(!fileTree->isColumnHidden(idx));
		action->setObjectName(QString::number(idx));

		connect(action, SIGNAL(toggled(bool)), this, SLOT(showColumn(bool)));
		columnActions.push_back(action);

		cm->addAction(action);
	}

	cm->exec(event->globalPos());	
}

void DkExplorer::showColumn(bool show) {

	bool ok = false;
	int idx = QObject::sender()->objectName().toInt(&ok);

	if (!ok)
		return;

	fileTree->setColumnHidden(idx, !show);
}

void DkExplorer::loadSelectedToggled(bool checked) {

	mLoadSelected = checked;

	if (mLoadSelected)
		connect(fileTree->selectionModel(), SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(fileClicked(const QModelIndex&)), Qt::UniqueConnection);
	else
		disconnect(fileTree->selectionModel(), SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(fileClicked(const QModelIndex&)));
}

void DkExplorer::setEditable(bool editable) {
	fileModel->setReadOnly(!editable);	
}

void DkExplorer::adjustColumnWidth() {

	for (int idx = 0; idx < fileTree->model()->columnCount(); idx++)
		fileTree->resizeColumnToContents(idx);
	qDebug() << "size adjusted...";
}

void DkExplorer::closeEvent(QCloseEvent* event) {

	writeSettings();
	DkDockWidget::closeEvent(event);
}

void DkExplorer::writeSettings() {

	QSettings& settings = DkSettingsManager::instance().qSettings();
	settings.beginGroup(objectName());
	
	for (int idx = 0; idx < fileModel->columnCount(QModelIndex()); idx++) {
		QString headerVal = fileModel->headerData(idx, Qt::Horizontal).toString();
		settings.setValue(headerVal + "Size", fileTree->columnWidth(idx));
		settings.setValue(headerVal + "Hidden", fileTree->isColumnHidden(idx));
	}

	settings.setValue("LoadSelected", mLoadSelected);
	settings.setValue("ReadOnly", fileModel->isReadOnly());
	settings.endGroup();
}

void DkExplorer::readSettings() {

	QSettings& settings = DkSettingsManager::instance().qSettings();
	settings.beginGroup(objectName());

	for (int idx = 0; idx < fileModel->columnCount(QModelIndex()); idx++) {
		
		QString headerVal = fileModel->headerData(idx, Qt::Horizontal).toString();
		
		int colWidth = settings.value(headerVal + "Size", -1).toInt();
		if (colWidth != -1) 
			fileTree->setColumnWidth(idx, colWidth);

		bool showCol = idx != 0;	// by default, show the first column only
		fileTree->setColumnHidden(idx, settings.value(headerVal + "Hidden", showCol).toBool());
	}

	mLoadSelected = settings.value("LoadSelected", mLoadSelected).toBool();
	fileModel->setReadOnly(settings.value("ReadOnly", true).toBool());
	settings.endGroup();
}

// DkOverview --------------------------------------------------------------------
DkOverview::DkOverview(QWidget* parent) : QLabel(parent) {

	setObjectName("DkOverview");
	setMinimumSize(0, 0);
	setMaximumSize(200, 200);
	setCursor(Qt::ArrowCursor);
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

void DkOverview::paintEvent(QPaintEvent *event) {

	if (mImg.isNull() || !mImgMatrix || !mWorldMatrix)
		return;

	QPainter painter(this);

	int lm, tm, rm, bm;
	getContentsMargins(&lm, &tm, &rm, &bm);

	QSize viewSize = QSize(width()-lm-rm, height()-tm-bm);	// overview shall take 15% of the mViewport....
	
	if (viewSize.width() > 2 && viewSize.height() > 2) {
	
		QTransform overviewImgMatrix = getScaledImageMatrix();			// matrix that always resizes the image to the current mViewport
		QRectF overviewImgRect = getScaledImageMatrix().mapRect(QRectF(QPointF(), mImg.size()));

		// now render the current view
		QRectF viewRect = mViewPortRect;
		viewRect = mWorldMatrix->inverted().mapRect(viewRect);
		viewRect = mImgMatrix->inverted().mapRect(viewRect);
		viewRect = overviewImgMatrix.mapRect(viewRect);
		viewRect.moveTopLeft(viewRect.topLeft()+QPointF(lm, tm));

		if(viewRect.topLeft().x() < overviewImgRect.topLeft().x()) viewRect.setTopLeft(QPointF(overviewImgRect.topLeft().x(), viewRect.topLeft().y()));
		if(viewRect.topLeft().y() < overviewImgRect.topLeft().y()) viewRect.setTopLeft(QPointF(viewRect.topLeft().x(), overviewImgRect.topLeft().y()));
		if(viewRect.bottomRight().x() > overviewImgRect.bottomRight().x()) viewRect.setBottomRight(QPointF(overviewImgRect.bottomRight().x()-1, viewRect.bottomRight().y()));
		if(viewRect.bottomRight().y() > overviewImgRect.bottomRight().y()) viewRect.setBottomRight(QPointF(viewRect.bottomRight().x(), overviewImgRect.bottomRight().y()-1));		

		//draw the image's location
		painter.setRenderHints(QPainter::SmoothPixmapTransform);
		painter.setBrush(DkSettingsManager::param().display().hudBgColor);
		painter.setPen(QColor(200, 200, 200));
		//painter.drawRect(overviewRect);
		painter.setOpacity(0.8f);
		painter.drawImage(overviewImgRect, imgT, QRect(0, 0, imgT.width(), imgT.height()));

		QColor col = DkSettingsManager::param().display().highlightColor;
		col.setAlpha(255);
		painter.setPen(col);
		col.setAlpha(50);
		painter.setBrush(col);

		if (viewRect.width()+1 < overviewImgRect.width() || viewRect.height()+1 < overviewImgRect.height())	// draw viewrect if we do not see all parts of the image
			painter.drawRect(viewRect);

	}
	painter.end();

	QWidget::paintEvent(event);
}

void DkOverview::mousePressEvent(QMouseEvent *event) {
	
	mEnterPos = event->pos();
	mPosGrab = event->pos();
}

void DkOverview::mouseReleaseEvent(QMouseEvent *event) {

	QPointF dxy = mEnterPos-QPointF(event->pos());

	if (dxy.manhattanLength() < 4) {
		
		int lm, tm, rm, bm;
		getContentsMargins(&lm, &tm, &rm, &bm);
		
		// move to the current position
		QRectF viewRect = mViewPortRect;
		viewRect = mWorldMatrix->inverted().mapRect(viewRect);
		viewRect = mImgMatrix->inverted().mapRect(viewRect);
		viewRect = getScaledImageMatrix().mapRect(viewRect);
		QPointF currentViewPoint = viewRect.center();

		float panningSpeed = (float)-(mWorldMatrix->m11()/(getScaledImageMatrix().m11()/mImgMatrix->m11()));

		QPointF cPos = event->pos()-QPointF(lm, tm);
		QPointF lDxy = (cPos - currentViewPoint)/mWorldMatrix->m11()*panningSpeed;
		emit moveViewSignal(lDxy);

		if (event->modifiers() == DkSettingsManager::param().global().altMod)
			emit sendTransformSignal();
	}

}

void DkOverview::mouseMoveEvent(QMouseEvent *event) {

	if (event->buttons() != Qt::LeftButton)
		return;

	float panningSpeed = (float)-(mWorldMatrix->m11()/(getScaledImageMatrix().m11()/mImgMatrix->m11()));

	QPointF cPos = event->pos();
	QPointF dxy = (cPos - mPosGrab)/mWorldMatrix->m11()*panningSpeed;
	mPosGrab = cPos;
	emit moveViewSignal(dxy);

	if (event->modifiers() == DkSettingsManager::param().global().altMod)
		emit sendTransformSignal();

}

void DkOverview::resizeEvent(QResizeEvent* event) {

	QWidget::resizeEvent(event);
}

QRectF DkOverview::getImageRect() const {
	
	QRectF imgRect = QRectF(QPoint(), size());			// get the overview rect

	if ((float)imgT.width()/imgT.height() < (float)imgRect.width()/imgRect.height())
		imgRect.setWidth(width() * (float)height()/(float)imgT.height());
	else
		imgRect.setHeight(height() * (float)width()/(float)imgT.width());

	//imgRect = imgRect.toRect();	// force round

	return imgRect;
}

void DkOverview::resizeImg() {

	if (mImg.isNull())
		return;

	//QRectF overviewRect = getImageRect();
	QTransform overviewImgMatrix = getScaledImageMatrix();			// matrix that always resizes the image to the current mViewport
	
	// is the overviewImgMatrix empty?
	if (overviewImgMatrix.isIdentity())
		return;
	
	//if (overviewRect.width() <= 1|| overviewRect.height() <= 1)
	//	return;

	// fast downscaling
	imgT = mImg.scaled(maximumWidth()*2, maximumHeight()*2, Qt::KeepAspectRatio, Qt::FastTransformation);
	imgT = imgT.scaled(maximumWidth(), maximumHeight(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QTransform DkOverview::getScaledImageMatrix() {

	if (mImg.isNull())
		return QTransform();

	int lm, tm, rm, bm;
	getContentsMargins(&lm, &tm, &rm, &bm);

	QSize iSize = QSize(width()-lm-rm, height()-tm-bm);	// inner size

	if (iSize.width() < 2 || iSize.height() < 2)
		return QTransform();

	// the image resizes as we zoom
	QRectF imgRect = QRectF(QPoint(lm, tm), mImg.size());
	float ratioImg = (float)(imgRect.width()/imgRect.height());
	float ratioWin = (float)(iSize.width())/(float)(iSize.height());

	QTransform imgMatrix;
	float s;
	if (imgRect.width() == 0 || imgRect.height() == 0)
		s = 1.0f;
	else
		s = (ratioImg > ratioWin) ? (float)(iSize.width()/imgRect.width()) : (float)(iSize.height()/imgRect.height());

	imgMatrix.scale(s, s);

	QRectF imgViewRect = imgMatrix.mapRect(imgRect);
	imgMatrix.translate((iSize.width()-imgViewRect.width())*0.5f/s, (iSize.height()-imgViewRect.height())*0.5f/s);

	return imgMatrix;
}


// DkZoomWidget --------------------------------------------------------------------
DkZoomWidget::DkZoomWidget(QWidget* parent) : DkFadeLabel(parent) {

	mAutoHide = false;
	setObjectName("DkZoomWidget");
	createLayout();

	setMinimumSize(70, 0);
	setMaximumSize(200, 240);
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	QMetaObject::connectSlotsByName(this);
}

void DkZoomWidget::createLayout() {

	mOverview = new DkOverview(this);

	mSlZoom = new QSlider(Qt::Horizontal, this);
	mSlZoom->setObjectName("slZoom");
	mSlZoom->setCursor(Qt::ArrowCursor);
	mSlZoom->setMinimum(0);	// add a mapping here
	mSlZoom->setMaximum(100);

	QString styleString = "QDoubleSpinBox{margin: 0px; padding: 0px; color: " + 
		DkUtils::colorToString(DkSettingsManager::param().display().hudFgdColor) + 
		"; background-color: rgba(0,0,0,0); border: none; selection-background-color: " +
		DkUtils::colorToString(DkSettingsManager::param().display().highlightColor) + ";}";
	//styleString += "QDoubleSpinBox::up-arrow, QDoubleSpinBox::down-arrow {width: 0px; heihgt: 0px;}";
	//styleString += "QDoubleSpinBox::up-bottom, QDoubleSpinBox::down-bottom {width: 0px; heihgt: 0px;}";

	mSbZoom = new QDoubleSpinBox(this);
	mSbZoom->setObjectName("sbZoom");
	mSbZoom->setStyleSheet(styleString);
	mSbZoom->setButtonSymbols(QAbstractSpinBox::NoButtons);
	mSbZoom->setSuffix("%");
	mSbZoom->setDecimals(0);
	mSbZoom->setMinimum(0.2);
	mSbZoom->setValue(100);
	mSbZoom->setMaximum(6000);

	QLabel* sliderWidget = new QLabel(this);
	sliderWidget->setObjectName("DkOverviewSliderWidget");
	QHBoxLayout* sliderLayout = new QHBoxLayout(sliderWidget);
	sliderLayout->setContentsMargins(10,0,0,0);
	sliderLayout->setSpacing(10);
	sliderLayout->addWidget(mSlZoom);
	sliderLayout->addWidget(mSbZoom);

	QVBoxLayout* layout = new QVBoxLayout(this);
	//mLayout->setContentsMargins(10,10,10,10);
	layout->setSpacing(0);
	layout->addWidget(mOverview);
	layout->addWidget(sliderWidget);
}

void DkZoomWidget::on_sbZoom_valueChanged(double zoomLevel) {
	updateZoom((float)zoomLevel);
	mAutoHide = false;
	emit zoomSignal((float)zoomLevel/100.0f);
}

void DkZoomWidget::on_slZoom_valueChanged(int zoomLevel) {
	float level = (zoomLevel > 50) ? (zoomLevel-50.0f)/50.0f * (float)mSbZoom->maximum() + 200.0f : zoomLevel*4.0f;
	if (level < 0.2f) level = 0.2f;
	mAutoHide = false;
	updateZoom(level);
	emit zoomSignal(level/100.0f);
}

void DkZoomWidget::updateZoom(float zoomLevel) {

	mSlZoom->blockSignals(true);
	mSbZoom->blockSignals(true);
	
	int slVal = (zoomLevel > 200.0f) ? qRound(zoomLevel/mSbZoom->maximum()*50.0f + 50.0f) : qRound(zoomLevel*0.25f);
	mSlZoom->setValue(slVal);
	mSbZoom->setValue(zoomLevel);
	mSlZoom->blockSignals(false);
	mSbZoom->blockSignals(false);
}

DkOverview* DkZoomWidget::getOverview() const {
	return mOverview;
}

void DkZoomWidget::setVisible(bool visible, bool autoHide /* = false */) {
	
	if (!isVisible() && visible)
		this->mAutoHide = autoHide;

	if (!visible)
		autoHide = false;

	DkFadeLabel::setVisible(visible);
}

bool DkZoomWidget::isAutoHide() const {
	return mAutoHide;
}

// DkButton --------------------------------------------------------------------
DkButton::DkButton(QWidget* parent) : QPushButton(parent) {

}

DkButton::DkButton(const QString& text, QWidget* parent) : QPushButton(text, parent) {

}

DkButton::DkButton(const QIcon& icon, const QString& text, QWidget* parent) : QPushButton(icon, text, parent) {

	checkedIcon = icon;
	setText(text);

	init();
}

DkButton::DkButton(const QIcon& checkedIcon, const QIcon& uncheckedIcon, const QString& text, QWidget* parent) : QPushButton(checkedIcon, text, parent) {

	this->checkedIcon = checkedIcon;
	this->uncheckedIcon = uncheckedIcon;
	this->setCheckable(true);
	setText(text);
	
	init();
}

void DkButton::init() {

	setIcon(checkedIcon);

	if (!checkedIcon.availableSizes().empty())
		this->setMaximumSize(checkedIcon.availableSizes()[0]);	// crashes if the image is empty!!
	
	mouseOver = false;
	keepAspectRatio = true;
}

void DkButton::setFixedSize(QSize size) {
	mySize = size;
	this->setMaximumSize(size);
}

void DkButton::paintEvent(QPaintEvent*) {

 	QPainter painter(this);
	QPoint offset;
	QSize s;
	float opacity = 1.0f;

	if (!isEnabled())
		opacity = 0.5f;
	else if(!mouseOver)
		opacity = 0.7f;

	painter.setOpacity(opacity);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	if (!mySize.isEmpty()) {
		
		offset = QPoint(qRound((float)(size().width()-mySize.width())*0.5f), qRound((float)(size().height()-mySize.height())*0.5f));
		s = mySize;
	}
	else
		s = this->size();

	// scale to parent label
	QRect r = (keepAspectRatio) ? QRect(offset, checkedIcon.actualSize(s)) : QRect(offset, s);	// actual size preserves the aspect ratio
	QPixmap pm2draw;

	if (isChecked() || !isCheckable())
		pm2draw = checkedIcon.pixmap(s);
	else
		pm2draw = uncheckedIcon.pixmap(s);

	if (this->isDown()) {
		QPixmap effect = createSelectedEffect(&pm2draw);
		painter.drawPixmap(r, effect);
	}

	painter.drawPixmap(r, pm2draw);
	painter.end();
}

QPixmap DkButton::createSelectedEffect(QPixmap* pm) {

	if (!pm || pm->isNull())
		return QPixmap();

	return DkImage::colorizePixmap(*pm, DkSettingsManager::param().display().highlightColor, 1.0f);
}

void DkButton::focusInEvent(QFocusEvent*) {
	mouseOver = true;
}

void DkButton::focusOutEvent(QFocusEvent*) {
	mouseOver = false;
}

void DkButton::enterEvent(QEvent*) {
	mouseOver = true;
}

void DkButton::leaveEvent(QEvent*) {
	mouseOver = false;
}

// star label --------------------------------------------------------------------
DkRatingLabel::DkRatingLabel(int rating, QWidget* parent, Qt::WindowFlags flags) : DkWidget(parent, flags) {

	setObjectName("DkRatingLabel");
	mRating = rating;
	init();

	int iconSize = 16;

	mLayout = new QBoxLayout(QBoxLayout::LeftToRight);
	mLayout->setContentsMargins(0,0,0,0);
	mLayout->setSpacing(3);
	mLayout->addStretch();
	
	for (int idx = 0; idx < mStars.size(); idx++) {
		mStars[idx]->setFixedSize(QSize(iconSize, iconSize));
		mLayout->addWidget(mStars[idx]);
	}
	
	setLayout(mLayout);
}

void DkRatingLabel::init() {

	QPixmap starDark = QPixmap(":/nomacs/img/star-dark.png");
	QPixmap starWhite = QPixmap(":/nomacs/img/star-white.png");

	mStars.resize(5);
	
	mStars[rating_1] = new DkButton(starWhite, starDark, tr("one star"), this);
	mStars[rating_1]->setCheckable(true);
	connect(mStars[rating_1], SIGNAL(released()), this, SLOT(rating1()));

	mStars[rating_2] = new DkButton(starWhite, starDark, tr("two stars"), this);
	mStars[rating_2]->setCheckable(true);
	connect(mStars[rating_2], SIGNAL(released()), this, SLOT(rating2()));

	mStars[rating_3] = new DkButton(starWhite, starDark, tr("three star"), this);
	mStars[rating_3]->setCheckable(true);
	connect(mStars[rating_3], SIGNAL(released()), this, SLOT(rating3()));

	mStars[rating_4] = new DkButton(starWhite, starDark, tr("four star"), this);
	mStars[rating_4]->setCheckable(true);
	connect(mStars[rating_4], SIGNAL(released()), this, SLOT(rating4()));

	mStars[rating_5] = new DkButton(starWhite, starDark, tr("five star"), this);
	mStars[rating_5]->setCheckable(true);
	connect(mStars[rating_5], SIGNAL(released()), this, SLOT(rating5()));

}

// DkRatingLabelBg --------------------------------------------------------------------
DkRatingLabelBg::DkRatingLabelBg(int rating, QWidget* parent, Qt::WindowFlags flags) : DkRatingLabel(rating, parent, flags) {

	setCursor(Qt::ArrowCursor);

	mHideTimer = new QTimer(this);
	mHideTimer->setInterval(mTimeToDisplay);
	mHideTimer->setSingleShot(true);

	// we want a margin
	mLayout->setContentsMargins(10,4,10,4);
	mLayout->setSpacing(4);
	
	mActions.resize(6);

	mActions[rating_0] = new QAction(tr("no rating"), this);
	mActions[rating_0]->setShortcut(Qt::Key_0);
	connect(mActions[rating_0], SIGNAL(triggered()), this, SLOT(rating0()));

	mActions[rating_1] = new QAction(tr("one star"), this);
	mActions[rating_1]->setShortcut(Qt::Key_1);
	connect(mActions[rating_1], SIGNAL(triggered()), this, SLOT(rating1()));

	mActions[rating_2] = new QAction(tr("two stars"), this);
	mActions[rating_2]->setShortcut(Qt::Key_2);
	connect(mActions[rating_2], SIGNAL(triggered()), this, SLOT(rating2()));

	mActions[rating_3] = new QAction(tr("three stars"), this);
	mActions[rating_3]->setShortcut(Qt::Key_3);
	connect(mActions[rating_3], SIGNAL(triggered()), this, SLOT(rating3()));

	mActions[rating_4] = new QAction(tr("four stars"), this);
	mActions[rating_4]->setShortcut(Qt::Key_4);
	connect(mActions[rating_4], SIGNAL(triggered()), this, SLOT(rating4()));

	mActions[rating_5] = new QAction(tr("five stars"), this);
	mActions[rating_5]->setShortcut(Qt::Key_5);
	connect(mActions[rating_5], SIGNAL(triggered()), this, SLOT(rating5()));

	mStars[rating_1]->addAction(mActions[rating_1]);
	mStars[rating_2]->addAction(mActions[rating_2]);
	mStars[rating_3]->addAction(mActions[rating_3]);
	mStars[rating_4]->addAction(mActions[rating_4]);
	mStars[rating_5]->addAction(mActions[rating_5]);
	
	connect(mHideTimer, SIGNAL(timeout()), this, SLOT(hide()));
}

DkRatingLabelBg::~DkRatingLabelBg() {}

void DkRatingLabelBg::changeRating(int newRating) {
	DkRatingLabel::changeRating(newRating);
	show();
	mHideTimer->start();
}

QVector<QAction*> DkRatingLabelBg::getActions() const {
	return mActions;
}

void DkRatingLabelBg::paintEvent(QPaintEvent *event) {

	QPainter painter(this);
	painter.fillRect(QRect(QPoint(), this->size()), mBgCol);
	painter.end();

	DkRatingLabel::paintEvent(event);
}

// title info --------------------------------------------------------------------
DkFileInfoLabel::DkFileInfoLabel(QWidget* parent) : DkFadeLabel(parent) {

	setObjectName("DkFileInfoLabel");
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);

	mTitleLabel = new QLabel(this);
	mTitleLabel->setMouseTracking(true);
	mTitleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	mDateLabel = new QLabel(this);
	mDateLabel->setMouseTracking(true);
	mDateLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	mRatingLabel = new DkRatingLabel(0, this);
	setMinimumWidth(110);
	setCursor(Qt::ArrowCursor);
	
	createLayout();
}

void DkFileInfoLabel::createLayout() {

	mLayout = new QBoxLayout(QBoxLayout::TopToBottom, this);
	mLayout->setSpacing(2);

	mLayout->addWidget(mTitleLabel);
	mLayout->addWidget(mDateLabel);
	mLayout->addWidget(mRatingLabel);
	//mLayout->addStretch();
}

void DkFileInfoLabel::setVisible(bool visible, bool saveSettings) {

	// nothing to display??
	if (!DkSettingsManager::param().slideShow().display.testBit(DkSettings::display_file_name) &&
		!DkSettingsManager::param().slideShow().display.testBit(DkSettings::display_creation_date) &&
		!DkSettingsManager::param().slideShow().display.testBit(DkSettings::display_file_rating) && visible) {
			
			QMessageBox infoDialog(DkUtils::getMainWindow());
			infoDialog.setWindowTitle(tr("Info Box"));
			infoDialog.setText(tr("All information fields are currently hidden.\nDo you want to show them again?"));
			infoDialog.setIcon(QMessageBox::Information);
			infoDialog.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
			infoDialog.setDefaultButton(QMessageBox::Yes);
			infoDialog.show();
			int choice = infoDialog.exec();

			if (choice == QMessageBox::No) {
				DkFadeLabel::setVisible(false);
				return;
			}
			else {
				DkSettingsManager::param().slideShow().display.setBit(DkSettings::display_file_name, true);
				DkSettingsManager::param().slideShow().display.setBit(DkSettings::display_creation_date, true);
				DkSettingsManager::param().slideShow().display.setBit(DkSettings::display_file_rating, true);
			}
	}

	DkFadeLabel::setVisible(visible, saveSettings);
	mTitleLabel->setVisible(DkSettingsManager::param().slideShow().display.testBit(DkSettings::display_file_name));
	mDateLabel->setVisible(DkSettingsManager::param().slideShow().display.testBit(DkSettings::display_creation_date));
	mRatingLabel->setVisible(DkSettingsManager::param().slideShow().display.testBit(DkSettings::display_file_rating));

	int height = 32;
	if (mTitleLabel->isVisible())
		height += mTitleLabel->sizeHint().height();
	if (mDateLabel->isVisible())
		height += mDateLabel->sizeHint().height();
	if (mRatingLabel->isVisible())
		height += mRatingLabel->sizeHint().height();

	qDebug() << "my minimum height: " << height;
	setMinimumHeight(height);
	updateWidth();
}

void DkFileInfoLabel::setEdited(bool edited) {

	if (!isVisible() || !edited)
		return;

	QString cFileName = mTitleLabel->text() + "*";
	this->mTitleLabel->setText(cFileName);

}

DkRatingLabel* DkFileInfoLabel::getRatingLabel() {
	return mRatingLabel;
}

void DkFileInfoLabel::updateInfo(const QString& filePath, const QString& attr, const QString& date, const int rating) {

	mFilePath = filePath;
	updateTitle(filePath, attr);
	updateDate(date);
	updateRating(rating);

	updateWidth();
}

void DkFileInfoLabel::updateTitle(const QString& filePath, const QString& attr) {
	
	updateDate();
	mTitleLabel->setText(QFileInfo(filePath).fileName() + " " + attr);
	mTitleLabel->setAlignment(Qt::AlignRight);

	updateWidth();
}

void DkFileInfoLabel::updateDate(const QString& date) {

	QString dateConverted = DkUtils::convertDateString(date, QFileInfo(mFilePath));

	mDateLabel->setText(dateConverted);
	mDateLabel->setAlignment(Qt::AlignRight);

	updateWidth();
}

void DkFileInfoLabel::updateRating(const int rating) {
	
	mRatingLabel->setRating(rating);
}

void DkFileInfoLabel::updateWidth() {

	int width = 20;		// mar
	width += qMax(qMax(mTitleLabel->sizeHint().width(), mDateLabel->sizeHint().width()), mRatingLabel->sizeHint().width());
	
	if (width < minimumWidth())
		setMinimumWidth(width);
	
	setMaximumWidth(width);
}

// player --------------------------------------------------------------------
DkPlayer::DkPlayer(QWidget* parent) : DkWidget(parent) {

	init();
	createLayout();
}

void DkPlayer::createLayout() {

	int height = 50;

	previousButton = new QPushButton(QIcon(":/nomacs/img/player-previous.svg"), "", this);
	//previousButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	previousButton->setIconSize(QSize(2*height, height));
	previousButton->setMaximumHeight(height);
	previousButton->setToolTip(tr("Show previous image"));
	previousButton->setObjectName("DkPlayerButton");
	previousButton->setFlat(true);
	connect(previousButton, SIGNAL(pressed()), this, SLOT(previous()));

	QIcon icon;
	icon.addPixmap(QIcon(":/nomacs/img/player-pause.svg").pixmap(2*height), QIcon::Normal, QIcon::On);
	icon.addPixmap(QIcon(":/nomacs/img/player-play.svg").pixmap(2*height), QIcon::Normal, QIcon::Off);
	playButton = new QPushButton(icon, "", this);
	//playButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	playButton->setIconSize(QSize(2*height, height));
	playButton->setMaximumHeight(height);
	playButton->setToolTip(tr("Play/Pause"));
	playButton->setObjectName("DkPlayerButton");
	playButton->setFlat(true);
	playButton->setCheckable(true);
	playButton->setChecked(false);
	playButton->addAction(actions[play_action]);
	connect(playButton, SIGNAL(clicked(bool)), this, SLOT(play(bool)));

	nextButton = new QPushButton(QIcon(":/nomacs/img/player-next.svg"), "",  this);
	//nextButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	nextButton->setIconSize(QSize(2*height, height));
	nextButton->setMaximumHeight(height);
	nextButton->setToolTip(tr("Show next image"));
	nextButton->setObjectName("DkPlayerButton");
	nextButton->setFlat(true);
	connect(nextButton, SIGNAL(pressed()), this, SLOT(next()));

	// now add to mLayout
	container = new QWidget(this);
	QHBoxLayout* layout = new QHBoxLayout(container);
	layout->setContentsMargins(0,0,0,0);
	layout->addStretch();
	layout->addWidget(previousButton);
	layout->addWidget(playButton);
	layout->addWidget(nextButton);
	layout->addStretch();

	QVBoxLayout* l = new QVBoxLayout(this);
	l->setContentsMargins(0, 0, 0, 0);
	l->addWidget(container);
	l->addStretch();

	//setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

}

void DkPlayer::init() {
	
	setObjectName("DkPlayer");

	// slide show
	int timeToDisplayPlayer = 3000;
	timeToDisplay = qRound(DkSettingsManager::param().slideShow().time*1000);
	playing = false;
	displayTimer = new QTimer(this);
	displayTimer->setInterval(timeToDisplay);
	displayTimer->setSingleShot(true);
	connect(displayTimer, SIGNAL(timeout()), this, SLOT(autoNext()));

	hideTimer = new QTimer(this);
	hideTimer->setInterval(timeToDisplayPlayer);
	hideTimer->setSingleShot(true);
	connect(hideTimer, SIGNAL(timeout()), this, SLOT(hide()));

	actions.resize(1);
	actions[play_action] = new QAction(tr("play"), this);
	connect(actions[play_action], SIGNAL(triggered()), this, SLOT(togglePlay()));
	connect(DkActionManager::instance().action(DkActionManager::menu_view_slideshow), SIGNAL(triggered()), this, SLOT(togglePlay()));
}

void DkPlayer::play(bool play) {

	if (play != playing)	// emulate a click
		playButton->setChecked(play);

	playing = play;

	if (play) {
		displayTimer->start();
		hideTimer->start();
	}
	else
		displayTimer->stop();
}

void DkPlayer::togglePlay() {

	show();
	playing = !playing;
	playButton->click();
}

void DkPlayer::startTimer() {
	if (playing) {
		displayTimer->setInterval(qRound(DkSettingsManager::param().slideShow().time*1000));	// if it was updated...
		displayTimer->start();
	}
}

void DkPlayer::autoNext() {
	emit nextSignal();
}

void DkPlayer::next() {
	hideTimer->stop();
	emit nextSignal();
}

void DkPlayer::previous() {
	hideTimer->stop();
	emit previousSignal();
}

bool DkPlayer::isPlaying() const {
	return playing;
}

void DkPlayer::setTimeToDisplay(int ms) {

	timeToDisplay = ms;
	displayTimer->setInterval(ms);
}

void DkPlayer::show(int ms) {		
	
	if (ms > 0 && !hideTimer->isActive()) {
		hideTimer->setInterval(ms);
		hideTimer->start();
	}

	bool showPlayer = getCurrentDisplaySetting();

	DkWidget::show();

	// automatic showing, don't store it in the display bits
	if (ms > 0 && mDisplaySettingsBits && mDisplaySettingsBits->size() > DkSettingsManager::param().app().currentAppMode) {
		mDisplaySettingsBits->setBit(DkSettingsManager::param().app().currentAppMode, showPlayer);
	}
}
 
// DkTransformRectangle --------------------------------------------------------------------
DkTransformRect::DkTransformRect(int idx, DkRotatingRect* rect, QWidget* parent, Qt::WindowFlags f) : QWidget(parent, f) {

	this->parentIdx = idx;
	this->size = QSize(12, 12);
	this->rect = rect;

	init();

	this->resize(size);
	setCursor(Qt::CrossCursor);
}

void DkTransformRect::init() {

}

void DkTransformRect::draw(QPainter *painter) {

	QPen penNoStroke;
	penNoStroke.setWidth(0);
	penNoStroke.setColor(QColor(0,0,0,0));
	
	QPen pen;
	//pen.setWidth(1);
	pen.setColor(QColor(255,255,0,100));

	QRectF visibleRect(QPointF(), QSizeF(5,5));
	QRectF whiteRect(QPointF(), QSize(7,7));
	visibleRect.moveCenter(geometry().center());
	whiteRect.moveCenter(geometry().center());

	// draw the control point
	painter->setWorldMatrixEnabled(false);
	painter->setPen(penNoStroke);
	painter->setBrush(QColor(0, 0, 0, 0));
	painter->drawRect(geometry());	// invisible mRect for mouseevents...
	//painter->setPen(pen);
	painter->setBrush(QColor(255,255,255, 100));
	painter->drawRect(whiteRect);
	painter->setBrush(QColor(0,0,0));
	painter->drawRect(visibleRect);
	painter->setWorldMatrixEnabled(true);
}

void DkTransformRect::mousePressEvent(QMouseEvent *event) {
	
	if (event->buttons() == Qt::LeftButton) {
		posGrab = event->globalPos();
		initialPos = geometry().topLeft();

		emit updateDiagonal(parentIdx);
	}
	qDebug() << "mouse pressed control point";
	QWidget::mousePressEvent(event);
}

void DkTransformRect::mouseMoveEvent(QMouseEvent *event) {

	if (event->buttons() == Qt::LeftButton) {
		
		QPointF pt = initialPos+event->globalPos()-posGrab;
		emit ctrlMovedSignal(parentIdx, pt, event->modifiers(), true);
		qDebug() << "accepted false...";
	}

	QWidget::mouseMoveEvent(event);
}

void DkTransformRect::mouseReleaseEvent(QMouseEvent *event) {

	QWidget::mouseReleaseEvent(event);
}

void DkTransformRect::enterEvent(QEvent*) {

	if (rect)
		setCursor(rect->cpCursor(parentIdx));
}

// DkEditableRectangle --------------------------------------------------------------------
DkEditableRect::DkEditableRect(const QRectF& rect, QWidget* parent, Qt::WindowFlags f) : DkWidget(parent, f) {

	mRect = rect;
	mRotatingCursor = QCursor(QPixmap(":/nomacs/img/rotating-cursor.png"));
	
	setAttribute(Qt::WA_MouseTracking);

	mPen = QPen(QColor(0, 0, 0, 255), 1);
	mPen.setCosmetic(true);
	mBrush = (DkSettingsManager::param().app().appMode == DkSettings::mode_frameless) ?
		DkSettingsManager::param().display().bgColorFrameless :
		DkSettingsManager::param().display().hudBgColor;

	for (int idx = 0; idx < 8; idx++) {
		mCtrlPoints.push_back(new DkTransformRect(idx, &this->mRect, this));
		mCtrlPoints[idx]->hide();
		connect(mCtrlPoints[idx], SIGNAL(ctrlMovedSignal(int, const QPointF&, Qt::KeyboardModifiers, bool)), this, SLOT(updateCorner(int, const QPointF&, Qt::KeyboardModifiers, bool)));
		connect(mCtrlPoints[idx], SIGNAL(updateDiagonal(int)), this, SLOT(updateDiagonal(int)));
	}
}

void DkEditableRect::reset() {

	mRect = QRectF();
	//for (int idx = 0; idx < ctrlPoints.size(); idx++)
	//	ctrlPoints[idx]->reset();

}

QPointF DkEditableRect::map(const QPointF &pos) {

	QPointF posM = pos;
	if (mWorldTform) posM = mWorldTform->inverted().map(posM);
	if (mImgTform)	posM = mImgTform->inverted().map(posM);
	
	return posM;
}

QPointF DkEditableRect::clipToImage(const QPointF &pos) {
	
	if (!mImgRect)
		return QPointF(pos);

	QRectF imgViewRect(*mImgRect);
	if (mWorldTform) imgViewRect = mWorldTform->mapRect(imgViewRect);

	float x = (float)pos.x();
	float y = (float)pos.y();
	float magicMargin = 5;	// 5 px for now

	if (qAbs(x - imgViewRect.left()) < magicMargin)
		x = (float)imgViewRect.left();
	if (qAbs(x - imgViewRect.right()) < magicMargin)
		x = (float)imgViewRect.right();

	if (qAbs(y - imgViewRect.top()) < magicMargin)
		y = (float)imgViewRect.top();
	if (qAbs(y - imgViewRect.bottom()) < magicMargin)
		y = (float)imgViewRect.bottom();

	return QPointF(x,y);		// round
}

QPointF DkEditableRect::clipToImageForce(const QPointF &pos) {

	if (!mImgRect)
		return QPointF(pos);

	QRectF imgViewRect(*mImgRect);
	if (mWorldTform) imgViewRect = mWorldTform->mapRect(imgViewRect);

	float x = (float)pos.x();
	float y = (float)pos.y();

	if (x < imgViewRect.left())
		x = (float)imgViewRect.left();
	if (x > imgViewRect.right())
		x = (float)imgViewRect.right();

	if (y < imgViewRect.top())
		y = (float)imgViewRect.top();
	if (y > imgViewRect.bottom())
		y = (float)imgViewRect.bottom();

	return QPointF(x,y);		// round
}

void DkEditableRect::updateDiagonal(int idx) {

	// we need to store the old diagonal in order to enable "keep aspect ratio"
	if (mRect.isEmpty())
		mOldDiag = DkVector(-1.0f, -1.0f);
	else
		mOldDiag = mRect.getDiagonal(idx);
}

void DkEditableRect::setFixedDiagonal(const DkVector& diag) {

	mFixedDiag = diag;

	qDebug() << "after rotating: " << mFixedDiag.toQPointF();

	// don't update in that case
	if (diag.x == 0 || diag.y == 0)
		return;
	else
		mFixedDiag.rotate(-mRect.getAngle());

	QPointF c = mRect.getCenter();

	if (!mRect.getPoly().isEmpty()) 
		mRect.updateCorner(0, mRect.getPoly().at(0), mFixedDiag);

	mRect.setCenter(c);
	update();
}

void DkEditableRect::setPanning(bool panning) {
	this->mPanning = panning;
	setCursor(Qt::OpenHandCursor);
	qDebug() << "panning set...";
}

void DkEditableRect::updateCorner(int idx, const QPointF& point, Qt::KeyboardModifiers modifiers, bool changeState) {

	if (changeState)
		mState = scaling;

	DkVector diag = (modifiers & Qt::ShiftModifier || mFixedDiag.x != 0 && mFixedDiag.y != 0) ? mOldDiag : DkVector();	// braces
	QPointF p = point;
	
	if ((modifiers & Qt::ControlModifier) == 0)
		p = clipToImage(point);

	mRect.updateCorner(idx, map(p), diag);

	// edge control -> remove aspect ratio constraint
	if (idx >= 4 && idx < 8)
		emit aRatioSignal(QPointF(0,0));

	update();
}

void DkEditableRect::paintEvent(QPaintEvent *event) {

	// create path
	QPainterPath path;
	QRect canvas(geometry().x()-1, geometry().y()-1, geometry().width()+1, geometry().height()+1);
	path.addRect(canvas);
	
	QPolygonF p;
	if (!mRect.isEmpty()) {
		// TODO: directly map the points (it's easier and not slower at all)
		p = mRect.getClosedPoly();
		p = mTtform.map(p);
		p = mRtform.map(p); 
		p = mTtform.inverted().map(p);
		if (mImgTform) p = mImgTform->map(p);
		if (mWorldTform) p = mWorldTform->map(p);
		QPolygon pr = p.toPolygon();	// round coordinates
		path.addPolygon(pr);
	}

	// now draw
	QPainter painter(this);

	painter.setPen(mPen);
	painter.setBrush(mBrush);
	painter.drawPath(path);

	//for (int idx = 0; idx < p.size()-1; idx++) {
	//	
	//	QPointF c = p[idx];
	//	painter.setBrush(QColor(255, 255, 255));
	//	painter.setPen(QColor(255, 255, 255));
	//	painter.drawText(c+QPointF(10,10), QString::number(idx));
	//}

	drawGuide(&painter, p, mPaintMode);
	
	//// debug
	//painter.drawPoint(mRect.getCenter());

	// this changes the painter -> do it at the end
	if (!mRect.isEmpty()) {
		
		for (int idx = 0; idx < mCtrlPoints.size(); idx++) {
			
			QPointF cp;
			
			if (idx < 4) {
				QPointF c = p[idx];
				cp = c-mCtrlPoints[idx]->getCenter();
			}
			// paint control points in the middle of the edge
			else if (idx >= 4) {
				QPointF s = mCtrlPoints[idx]->getCenter();

				QPointF lp = p[idx % 4];
				QPointF rp = p[(idx+1) % 4];

				QVector2D lv = QVector2D(lp-s);
				QVector2D rv = QVector2D(rp-s);

				cp = (lv + 0.5*(rv - lv)).toPointF();
			}

			mCtrlPoints[idx]->move(qRound(cp.x()), qRound(cp.y()));
			mCtrlPoints[idx]->draw(&painter);
		}
	}
 
	painter.end();

	QWidget::paintEvent(event);
}

void DkEditableRect::drawGuide(QPainter* painter, const QPolygonF& p, int paintMode) {

	if (p.isEmpty() || paintMode == no_guide)
		return;

	QColor col = painter->pen().color();
	col.setAlpha(150);
	QPen pen = painter->pen();
	QPen cPen = pen;
	cPen.setColor(col);
	painter->setPen(cPen);

	// vertical
	DkVector lp = p[1]-p[0];	// parallel to drawing
	DkVector l9 = p[3]-p[0];	// perpendicular to drawing

	int nLines = (paintMode == rule_of_thirds) ? 3 : qRound(l9.norm()/20.0f);
	DkVector offset = l9;
	offset.normalize();
	offset *= l9.norm()/nLines;

	DkVector offsetVec = offset;

	for (int idx = 0; idx < (nLines-1); idx++) {

		// step through & paint
		QLineF l = QLineF(DkVector(p[1]+offsetVec).toQPointF(), DkVector(p[0]+offsetVec).toQPointF());
		painter->drawLine(l);
		offsetVec += offset;
	}

	// horizontal
	lp = p[3]-p[0];	// parallel to drawing
	l9 = p[1]-p[0];	// perpendicular to drawing

	nLines = (paintMode == rule_of_thirds) ? 3 : qRound(l9.norm()/20);
	offset = l9;
	offset.normalize();
	offset *= l9.norm()/nLines;

	offsetVec = offset;

	for (int idx = 0; idx < (nLines-1); idx++) {

		// step through & paint
		QLineF l = QLineF(DkVector(p[3]+offsetVec).toQPointF(), DkVector(p[0]+offsetVec).toQPointF());
		painter->drawLine(l);
		offsetVec += offset;
	}

	painter->setPen(pen);	// revert painter

}

// make events callable
void DkEditableRect::mousePressEvent(QMouseEvent *event) {

	// panning -> redirect to mViewport
	if (event->buttons() == Qt::LeftButton && 
		(event->modifiers() == DkSettingsManager::param().global().altMod || mPanning)) {
		event->setModifiers(Qt::NoModifier);	// we want a 'normal' action in the mViewport
		event->ignore();
		return;
	}

	mPosGrab = map(QPointF(event->pos()));
	mClickPos = QPointF(event->pos());

	if (mRect.isEmpty()) {
		mState = initializing;
		setAngle(0);
	}
	else if (mRect.getPoly().containsPoint(mPosGrab, Qt::OddEvenFill)) {
		mState = moving;
	}
	else {
		mState = rotating;
	}

}

void DkEditableRect::mouseMoveEvent(QMouseEvent *event) {

	// panning -> redirect to mViewport
	if (event->modifiers() == DkSettingsManager::param().global().altMod ||
		mPanning) {
		
		if (event->buttons() != Qt::LeftButton)
			setCursor(Qt::OpenHandCursor);
		else
			setCursor(Qt::ClosedHandCursor);

		event->setModifiers(Qt::NoModifier);
		event->ignore();
		update();
		return;
	}

	QPointF posM = map(QPointF(event->pos()));
	
	if (event->buttons() != Qt::LeftButton && !mRect.isEmpty()) {
		// show rotating - moving
		if (mRect.getPoly().containsPoint(map(event->pos()), Qt::OddEvenFill))
			setCursor(Qt::SizeAllCursor);
		else
			setCursor(mRotatingCursor);
	}
	else if (mRect.isEmpty())
		setCursor(Qt::CrossCursor);

	// additionally needed for showToolTip
	double angle = 0;

	if (mState == initializing && event->buttons() == Qt::LeftButton) {

		QPointF clipPos = clipToImageForce(QPointF(event->pos()));

		if (!mImgRect || !mRect.isEmpty() || clipPos == QPointF(event->pos())) {
			
			if (mRect.isEmpty()) {

				for (int idx = 0; idx < mCtrlPoints.size(); idx++)
					mCtrlPoints[idx]->show();

				QPointF p = map(clipToImageForce(mClickPos));
				mRect.setAllCorners(p);
			}
			
			DkVector diag;
			
			// when initializing shift should make the mRect a square
			if (event->modifiers() == Qt::ShiftModifier)
				diag = DkVector(1.0f, 1.0f);
			else
				diag = mFixedDiag;
			mRect.updateCorner(2, map(clipPos), diag);
			update();
		}
 
	}
	else if (mState == moving && event->buttons() == Qt::LeftButton) {
		
		QPointF dxy = posM-mPosGrab;
		mRtform.translate(dxy.x(), dxy.y());
		mPosGrab = posM;
		update();
	}
	else if (mState == rotating && event->buttons() == Qt::LeftButton) {

		DkVector c(mRect.getCenter());
		DkVector xt(mPosGrab);
		DkVector xn(posM);

		// compute the direction vector;
		xt = c-xt;
		xn = c-xn;
		angle = xn.angle() - xt.angle();

		// just rotate in CV_PI*0.25 steps if shift is pressed
		if (event->modifiers() == Qt::ShiftModifier) {
			double angleRound = DkMath::normAngleRad(angle+mRect.getAngle(), -CV_PI*0.125, CV_PI*0.125);
			angle -= angleRound;
		}
					
		setAngle(angle, false);
	}

	if (event->buttons() == Qt::LeftButton && mState != moving) {

		QPolygonF p = mRect.getPoly();

		float sAngle = DkMath::getReadableAngle(mRect.getAngle() + angle);
		int height = qRound(DkVector(p[1]-p[0]).norm());
		int width = qRound(DkVector(p[3]-p[0]).norm());

		if (mShowInfo) {
			QToolTip::showText(event->globalPos(),
				QString::number(width) + " x " +
				QString::number(height) + " px\n" +
				QString::number(sAngle) + dk_degree_str,
				this);
		}

		DkStatusBarManager::instance().setMessage(QString::number(width) + " x " + QString::number(height) + " px | " + QString::number(sAngle) + dk_degree_str);
	}

	//QWidget::mouseMoveEvent(event);
}

void DkEditableRect::mouseReleaseEvent(QMouseEvent *event) {

	// panning -> redirect to mViewport
	if (event->buttons() == Qt::LeftButton && 
		(event->modifiers() == DkSettingsManager::param().global().altMod || mPanning)) {
		setCursor(Qt::OpenHandCursor);
		event->setModifiers(Qt::NoModifier);
		event->ignore();
		return;
	}

	mState = do_nothing;

	applyTransform();
	//QWidget::mouseReleaseEvent(event);
}

void DkEditableRect::wheelEvent(QWheelEvent* event) {

	QWidget::wheelEvent(event);
	update();	// this is an extra update - however we get rendering errors otherwise?!
}

void DkEditableRect::applyTransform() {

	// apply transform
	mRect.transform(mTtform, mRtform);

	mRtform.reset();	
	mTtform.reset();
	update();

}

void DkEditableRect::keyPressEvent(QKeyEvent *event) {

	if (event->key() == Qt::Key_Alt)
		setCursor(Qt::OpenHandCursor);

	QWidget::keyPressEvent(event);
}

void DkEditableRect::keyReleaseEvent(QKeyEvent *event) {

	qDebug() << "key pressed rect";
	QWidget::keyPressEvent(event);
}

void DkEditableRect::setPaintHint(int paintMode /* = DkCropToolBar::no_guide */) {

	qDebug() << "painting mode: " << paintMode;
	this->mPaintMode = paintMode;
	update();
}

void DkEditableRect::setShadingHint(bool) {

	QColor col = mBrush.color();
	col = QColor(255-col.red(), 255-col.green(), 255-col.blue(), col.alpha());
	mBrush.setColor(col);

	col = mPen.color();
	col = QColor(255-col.red(), 255-col.green(), 255-col.blue(), col.alpha());
	mPen.setColor(col);

	update();
}

void DkEditableRect::setShowInfo(bool showInfo) {
	this->mShowInfo = showInfo;
}

void DkEditableRect::setAngle(double angle, bool apply) {

	DkVector c(mRect.getCenter());

	if (!mTtform.isTranslating())
		mTtform.translate(-c.x, -c.y);
	
	mRtform.reset();
	if (apply)
		mRtform.rotateRadians(angle-mRect.getAngle());
	else
		mRtform.rotateRadians(angle);
	
	if (apply)
		applyTransform();
	else {
		emit angleSignal(mRect.getAngle()+angle);
		update();
	}

}

void DkEditableRect::setVisible(bool visible) {

	if (!visible) {
		
		mRect = DkRotatingRect();
		for (int idx = 0; idx < mCtrlPoints.size(); idx++)
			mCtrlPoints[idx]->hide();
	}
	else {
		//setFocus(Qt::ActiveWindowFocusReason);
		setCursor(Qt::CrossCursor);
	}

	DkWidget::setVisible(visible);
}

// DkEditableRect --------------------------------------------------------------------
DkCropWidget::DkCropWidget(QRectF rect /* = QRect */, QWidget* parent /* = 0*/, Qt::WindowFlags f /* = 0*/) : DkEditableRect(rect, parent, f) {

	cropToolbar = 0;
}

void DkCropWidget::createToolbar() {

	cropToolbar = new DkCropToolBar(tr("Crop Toolbar"), this);

	connect(cropToolbar, SIGNAL(cropSignal(bool)), this, SLOT(crop(bool)));
	connect(cropToolbar, SIGNAL(cancelSignal()), this, SIGNAL(cancelSignal()));
	connect(cropToolbar, SIGNAL(aspectRatio(const DkVector&)), this, SLOT(setFixedDiagonal(const DkVector&)));
	connect(cropToolbar, SIGNAL(angleSignal(double)), this, SLOT(setAngle(double)));
	connect(cropToolbar, SIGNAL(panSignal(bool)), this, SLOT(setPanning(bool)));
	connect(cropToolbar, SIGNAL(paintHint(int)), this, SLOT(setPaintHint(int)));
	connect(cropToolbar, SIGNAL(shadingHint(bool)), this, SLOT(setShadingHint(bool)));
	connect(cropToolbar, SIGNAL(showInfo(bool)), this, SLOT(setShowInfo(bool)));
	connect(this, SIGNAL(angleSignal(double)), cropToolbar, SLOT(angleChanged(double)));
	connect(this, SIGNAL(aRatioSignal(const QPointF&)), cropToolbar, SLOT(setAspectRatio(const QPointF&)));

	cropToolbar->loadSettings();	// need to this manually after connecting the slots

}

DkCropToolBar* DkCropWidget::getToolbar() const {
	return cropToolbar;
}

void DkCropWidget::crop(bool cropToMetadata) {

	if (!cropToolbar)
		return;

	if (!mRect.isEmpty())
		emit cropImageSignal(mRect, cropToolbar->getColor(), cropToMetadata);

	setVisible(false);
	setWindowOpacity(0);
}

void DkCropWidget::setVisible(bool visible) {

	if (visible && !cropToolbar)
		createToolbar();

	emit showToolbar(cropToolbar, visible);
	DkEditableRect::setVisible(visible);
}


// DkAnimagionLabel --------------------------------------------------------------------
DkAnimationLabel::DkAnimationLabel(QString animationPath, QWidget* parent) : DkLabel(parent) {

	init(animationPath, QSize());
}

DkAnimationLabel::DkAnimationLabel(QString animationPath, QSize size, QWidget* parent) : DkLabel(parent) {

	init(animationPath, size);
}

DkAnimationLabel::~DkAnimationLabel() {
}

void DkAnimationLabel::init(const QString& animationPath, const QSize& size) {
	
	setObjectName("DkAnimationLabel");

	mSvg = QSharedPointer<QSvgRenderer>(new QSvgRenderer(animationPath));
	connect(mSvg.data(), SIGNAL(repaintNeeded()), this, SLOT(update()));

	QSize s = size;
	if(s.isEmpty())
		s = mSvg->defaultSize();

	setFixedSize(s);
	hide();
}

void DkAnimationLabel::showTimed(int time) {
	
	DkLabel::showTimed(time);
}


void DkAnimationLabel::hide() {
	
	DkLabel::hide();
}

void DkAnimationLabel::paintEvent(QPaintEvent* ev) {
	
	if (mSvg) {
		QPainter p(this);
		mSvg->render(&p, QRect(QPoint(), size()));
	}

	DkLabel::paintEvent(ev);
}

// Image histogram  -------------------------------------------------------------------
DkHistogram::DkHistogram(QWidget *parent) : DkWidget(parent){
	
	setObjectName("DkHistogram");
	setMinimumWidth(265);
	setMinimumHeight(142);
	setCursor(Qt::ArrowCursor);
}

DkHistogram::~DkHistogram() {
}

/**
 * Paints the image histogram
 **/
void DkHistogram::paintEvent(QPaintEvent*) {

	QPainter painter(this);
	painter.fillRect(1, 1, width(), height(), mBgCol);

	const int margin = 5;
	const int TEXT_SIZE = 12 + 1;  //FIXME just guessed
	const int NUM_TEXT_LINES = 2;
	const int textHeight = NUM_TEXT_LINES * TEXT_SIZE + margin + 5;

	int binHeight = height() - margin * 2 - textHeight;
	int binBottom = height() - margin - textHeight;

	//draw Histogram
	if(mIsPainted && mMaxValue > 0){
		
		for(int x = 0; x < 256; x++){
			
			// get bounded values
			int rLineHeight = qMax(qMin(qRound((float)mHist[0][x] * binHeight * mScaleFactor/mMaxValue), binHeight), 0);
			int gLineHeight = qMax(qMin(qRound((float)mHist[1][x] * binHeight * mScaleFactor/mMaxValue), binHeight), 0);
			int bLineHeight = qMax(qMin(qRound((float)mHist[2][x] * binHeight * mScaleFactor/mMaxValue), binHeight), 0);
			int maxLineHeight = qMax(qMax(rLineHeight, gLineHeight), bLineHeight); // (rLineHeight > gLineHeight) ? ((rLineHeight > bLineHeight) ? rLineHeight : bLineHeight) : ((gLineHeight > bLineHeight) ? gLineHeight : bLineHeight);

			painter.setCompositionMode(QPainter::CompositionMode_Clear);
			painter.setPen(Qt::black);
			painter.drawLine(QPoint(x + margin, binBottom), QPoint(x + margin, binBottom - maxLineHeight));

			painter.setCompositionMode(QPainter::CompositionMode_Screen);
			painter.setPen(Qt::red);
			painter.drawLine(QPoint(x+margin, binBottom), QPoint(x+margin, binBottom - rLineHeight));
			painter.setPen(Qt::green);
			painter.drawLine(QPoint(x+margin, binBottom), QPoint(x+margin, binBottom - gLineHeight));
			painter.setPen(Qt::blue);
			painter.drawLine(QPoint(x+margin, binBottom), QPoint(x+margin, binBottom - bLineHeight));
		}
	}

	//draw histogram text
	double activeRatio = (double)mNumNonZeroPixels / (double)mNumPixels;
	double megaPixels = (double)mNumPixels * 10.0e-7;
	painter.setPen(QColor(180, 180, 180));

	QString histText1("Pixels: %1 MPix: %2");
	painter.drawText(QPoint(margin, height() - 2 * TEXT_SIZE),
					 histText1.arg(mNumPixels, 10, 10, QChar(' '))
                              .arg(megaPixels, 10, 'f', -1,  QChar(' ')));
	if (mMinBinValue < 256) {
		//gray image statistics
		QString histText2("Min: %1 Max: %2 Value Count: %3");
		painter.drawText(QPoint(margin, height() - 1 * TEXT_SIZE),
						 histText2.arg(mMinBinValue, 5, 10, QChar(' '))
								  .arg(mMaxBinValue, 5, 10, QChar(' '))
								  .arg(mNumDistinctValues, 5, 10, QChar(' ')));
	}else{
		//color image statistics
		double blackPct = 100.0 * (double)mNumZeroPixels / (double)mNumPixels;
		double whitePct = 100.0 * (double)mNumSaturatedPixels / (double)mNumPixels;
		double goodPct = 100.0 * (double)(mNumPixels - mNumZeroPixels - mNumSaturatedPixels) / (double)mNumPixels;

		QString histText2("Black: %1 Good: %3 White: %2");
		painter.drawText(QPoint(margin, height() - 1 * TEXT_SIZE),
						 histText2.arg(blackPct, 5, 'f', 2, QChar(' '))
								  .arg(whitePct, 5, 'f', 2, QChar(' '))
								  .arg(goodPct, 5, 'f', 2, QChar(' ')));

	}
}

/**
 * Goes through the image and counts pixels values. They are used to create the image histogram.
 * @param currently displayed image
 **/ 
void DkHistogram::drawHistogram(QImage imgQt) {

	if (!isVisible() || imgQt.isNull()) {
		setPainted(false);
		return;
	}

	DkTimer dt;

	//clear histogram values
	mNumNonZeroPixels = 0;
	mNumZeroPixels = 0;
	mNumSaturatedPixels = 0;
	mMaxBinValue = -1;
	mMinBinValue = 256;
	mMaxValue = 0;
	mNumPixels = imgQt.width() * imgQt.height();

	for (int idx = 0; idx < 256; idx++) {
		mHist[0][idx] = 0;
		mHist[1][idx] = 0;
		mHist[2][idx] = 0;
	}

	// count pixel- and total values, for
	// 8 bit images
	if (imgQt.depth() == 8) {

		qDebug() << "8 bit histogram -------------------";

		for (int rIdx = 0; rIdx < imgQt.height(); rIdx++) {

			const unsigned char* pixel = imgQt.constScanLine(rIdx);

			for (int cIdx = 0; cIdx < imgQt.width(); cIdx++, pixel++) {

				mHist[0][*pixel]++;
				mHist[1][*pixel]++;
				mHist[2][*pixel]++;

				if (*pixel) mNumNonZeroPixels++;
				if (*pixel == 255) mNumSaturatedPixels++;
				if (*pixel < mMinBinValue) mMinBinValue = *pixel;
				if (*pixel > mMaxBinValue) mMaxBinValue = *pixel;
			}
		}
	}
	// 24 bit images
	else if (imgQt.depth() == 24) {
		
		qDebug() << "24 bit histogram -------------------";

		// TODO: not tested!!
		for (int rIdx = 0; rIdx < imgQt.height(); rIdx++) {

			const unsigned char* pixel = imgQt.constScanLine(rIdx);
			for (int cIdx = 0; cIdx < imgQt.width(); cIdx++) {

				// If I understood the api correctly, the first bits are 0 if we have 24bpp & < 8 bits per channel
				mHist[0][*pixel]++; pixel++;
				mHist[1][*pixel]++; pixel++;
				mHist[2][*pixel]++; pixel++;

			}
		}
	}
	// 32 bit images
	else if (imgQt.depth() == 32) {
		
		for (int rIdx = 0; rIdx < imgQt.height(); rIdx++) {
		
			const QRgb* pixel = (QRgb*)(imgQt.constScanLine(rIdx));
	
			for (int cIdx = 0; cIdx < imgQt.width(); cIdx++, pixel++) {
				size_t pixR = static_cast<size_t>(qRed(*pixel));
				size_t pixG = static_cast<size_t>(qGreen(*pixel));
				size_t pixB = static_cast<size_t>(qBlue(*pixel));

				mHist[0][pixR]++;
				mHist[1][pixG]++;
				mHist[2][pixB]++;

				if (pixR == 0 && pixG == 0 && pixB == 0) {
					mNumZeroPixels++;
				}

				if (pixR || pixG || pixB) {
					mNumNonZeroPixels++;
				}

				if (pixR == 255 && pixG == 255 && pixB == 255) {
					mNumSaturatedPixels++;
				}
			}
		}
	}

	// determine extreme values from the histogram
	mNumDistinctValues = 0;

    for (int idx = 0; idx < 256; idx++) {
        if (mHist[0][idx] > mMaxValue)
            mMaxValue = mHist[0][idx];
        if (mHist[1][idx] > mMaxValue)
            mMaxValue = mHist[1][idx];
        if (mHist[2][idx] > mMaxValue)
            mMaxValue = mHist[2][idx];

        if (mHist[0][idx] || mHist[1][idx] || mHist[2][idx]){
            mNumDistinctValues++;
        }
    }

	setPainted(true);
	qDebug() << "drawing the histogram took me: " << dt;
	update();
}

/**
 * Clears the histogram panel
 **/ 
void DkHistogram::clearHistogram() {

	setPainted(false);
	update();
}

void DkHistogram::setPainted(bool isPainted) {

	this->mIsPainted = isPainted;
}

void DkHistogram::setMaxHistogramValue(int maxValue) {

	if (maxValue == 0)
		setPainted(false);

	this->mMaxValue = maxValue;
}

/**
 * Updates histogram values.
 * @param values to be copied
 **/ 
void DkHistogram::updateHistogramValues(int histValues[][256]) {

	for(int i = 0; i < 256; i++) {
		this->mHist[0][i] = histValues[0][i];
		this->mHist[1][i] = histValues[1][i];
		this->mHist[2][i] = histValues[2][i];
	}
}

/**
 * Mouse events for scaling the histogram - enlarge the histogram between the bottom axis and the cursor position
 **/ 
void DkHistogram::mousePressEvent(QMouseEvent *event) {

	// always propagate mouse events
	if (event->buttons() != Qt::LeftButton)
		DkWidget::mousePressEvent(event);
}

void DkHistogram::mouseMoveEvent(QMouseEvent *event) {

	if (event->buttons() == Qt::LeftButton) {
		
		float cp = (float)(height() - event->pos().y());
		
		if (cp > 0) {
			mScaleFactor = height() / cp;
			update();
		}
	}
	else
		DkWidget::mouseMoveEvent(event);

}

void DkHistogram::mouseReleaseEvent(QMouseEvent *event) {
	
	mScaleFactor = 1;
	update();

	if (event->buttons() != Qt::LeftButton)
		DkWidget::mouseReleaseEvent(event);
}

// DkFileInfo --------------------------------------------------------------------
DkFileInfo::DkFileInfo() {
	mFileExists = false;
	mUsed = false;
}

DkFileInfo::DkFileInfo(const QFileInfo& fileInfo) {

	mFileInfo = fileInfo;
}

bool DkFileInfo::exists() const {
	return mFileExists;
}

void DkFileInfo::setExists(bool fileExists) {
	mFileExists = fileExists;
}

bool DkFileInfo::inUse() const {
	return mUsed;
}

void DkFileInfo::setInUse(bool inUse) {
	mUsed = inUse;
}

QString DkFileInfo::getFilePath() const {
	return mFileInfo.absoluteFilePath();
}


// DkFileLabel --------------------------------------------------------------------
DkFolderLabel::DkFolderLabel(const DkFileInfo& fileInfo, QWidget* parent /* = 0 */, Qt::WindowFlags f /* = 0 */) : QLabel(parent, f) {

	// we don't use the file labels anymore
	// + isDir() might hang - if we try to get an unavailable network resource on windows
	//QFileInfo fInfo(fileInfo.getFilePath());
	//if (fInfo.isDir())
		setText(fileInfo.getFilePath());
	//else
	//	setText(fInfo.fileName());

	this->fileInfo = fileInfo;
	setObjectName("DkFileLabel");
}

void DkFolderLabel::mousePressEvent(QMouseEvent *ev) {

	emit loadFileSignal(fileInfo.getFilePath());

	QLabel::mousePressEvent(ev);
}

// DkImageLabel --------------------------------------------------------------------
DkImageLabel::DkImageLabel(const QString& filePath, int thumbSize, QWidget* parent /* = 0 */, Qt::WindowFlags f /* = 0 */) : QLabel(parent, f) {

	mThumbSize = thumbSize;

	thumb = QSharedPointer<DkThumbNailT>(new DkThumbNailT(filePath));
	connect(thumb.data(), SIGNAL(thumbLoadedSignal()), this, SIGNAL(labelLoaded()));
	connect(thumb.data(), SIGNAL(thumbLoadedSignal()), this, SLOT(thumbLoaded()));

	setFixedSize(mThumbSize, mThumbSize);
	setMouseTracking(true);

	QFileInfo fInfo(filePath);
	setStatusTip(fInfo.fileName());
	setToolTip(fInfo.fileName());
	
	createLayout();
}

void DkImageLabel::createLayout() {

	imageLabel = new QLabel(this);
	imageLabel->setFixedSize(mThumbSize, mThumbSize);
	imageLabel->setScaledContents(true);
	imageLabel->setStyleSheet("QLabel{margin: 0 0 0 0; padding: 0 0 0 0; border: 1px solid " + DkUtils::colorToString(DkSettingsManager::param().display().hudBgColor) + ";}");

	QColor cA = DkSettingsManager::param().display().highlightColor;
	cA.setAlpha(100);
	highLightLabel = new QLabel(this);
	highLightLabel->setFixedSize(mThumbSize, mThumbSize);
	highLightLabel->setStyleSheet("QLabel{background: " + DkUtils::colorToString(cA) + "; border: 1px solid " + DkUtils::colorToString(DkSettingsManager::param().display().highlightColor) + ";}");
	highLightLabel->hide();

	removeFileButton = new QPushButton(QIcon(":/nomacs/img/close.svg"), tr(""), this);
	removeFileButton->setIconSize(QSize(12, 12));
	removeFileButton->setFixedSize(QSize(12, 12));
	removeFileButton->setFlat(true);
	removeFileButton->setObjectName("removeFileButton");
	removeFileButton->move(width()-17, 5);
	removeFileButton->hide();

	connect(removeFileButton, SIGNAL(clicked()), this, SLOT(removeFileFromList()));
}

bool DkImageLabel::hasFile() const {

	return !thumb->getImage().isNull();
}

QSharedPointer<DkThumbNailT> DkImageLabel::getThumb() const {
	
	return thumb;
}

void DkImageLabel::thumbLoaded() {

	if (thumb->getImage().isNull()) {
		qDebug() << QFileInfo(thumb->getFilePath()).fileName() << " not loaded...";
		return;
	}
	
	QPixmap pm = QPixmap::fromImage(thumb->getImage());

	QRect r(QPoint(), pm.size());

	if (r.width() > r.height()) {
		r.setX(qFloor((r.width()-r.height())*0.5f));
		r.setWidth(r.height());
	}
	else {
		r.setY(qFloor((r.height()-r.width())*0.5f));
		r.setHeight(r.width());
	}
	pm = pm.copy(r);

	imageLabel->setPixmap(pm);
}

void DkImageLabel::removeFileFromList() {

	imageLabel->hide();
	//highLightLabel->hide();
	removeFileButton->hide();
	highLightLabel->setStyleSheet("QLabel{background: " + DkUtils::colorToString(DkSettingsManager::param().display().hudBgColor) + "; border: 1px solid black;}");
	highLightLabel->show();

	for (int idx = 0; idx < DkSettingsManager::param().global().recentFiles.size(); idx++) {

		if (thumb->getFilePath() == DkSettingsManager::param().global().recentFiles.at(idx))
			DkSettingsManager::param().global().recentFiles.removeAt(idx);
	}
}

void DkImageLabel::enterEvent(QEvent *ev) {
	
	if (imageLabel->isVisible()) {
		highLightLabel->show();
		removeFileButton->show();
	}
	QLabel::enterEvent(ev);
}

void DkImageLabel::leaveEvent(QEvent *ev) {
	
	if (imageLabel->isVisible())
		highLightLabel->hide();
	removeFileButton->hide();
	QLabel::leaveEvent(ev);
}

void DkImageLabel::mousePressEvent(QMouseEvent *ev) {

	emit loadFileSignal(thumb->getFilePath());

	QLabel::mousePressEvent(ev);
}


// Recent Files Widget --------------------------------------------------------------------
DkRecentFilesWidget::DkRecentFilesWidget(QWidget* parent /* = 0 */) : DkWidget(parent) {

	setObjectName("DkRecentFilesWidget");
	rFileIdx = 0;

	createLayout();

	//connect(&fileWatcher, SIGNAL(finished()), this, SLOT(updateFiles()));
}

DkRecentFilesWidget::~DkRecentFilesWidget() {

	//fileWatcher.blockSignals(true);
	//fileWatcher.cancel();
	//fileWatcher.waitForFinished();
}

void DkRecentFilesWidget::createLayout() {

	filesWidget = new QWidget(this);
	filesLayout = new QGridLayout(filesWidget);
	filesLayout->setAlignment(Qt::AlignTop);
	//filesWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

	//folderWidget = new QWidget(this);
	//folderLayout = new QVBoxLayout(folderWidget);
	
	//filesTitle = new QLabel(tr("Recent Files"), this);
	//filesTitle->setObjectName("DkRecentFilesTitle");
	//filesTitle->setStyleSheet("QLabel{font-size: 15pt;}" + filesTitle->styleSheet());

	//folderTitle = new QLabel(tr("Recent Folders"), this);
	//folderTitle->setObjectName("DkRecentFilesTitle");
	//folderTitle->setStyleSheet("QLabel{font-size: 15pt;}" + folderTitle->styleSheet());

	bgLabel = new QLabel(this);
	bgLabel->setObjectName("bgLabel");
	
	QHBoxLayout* layout = new QHBoxLayout(this);
	layout->setContentsMargins(0,0,0,0);
	layout->addWidget(bgLabel);
	
	QHBoxLayout* hLayout = new QHBoxLayout(bgLabel);
	hLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	hLayout->addWidget(filesWidget);
	//hLayout->addWidget(folderWidget);
	//hLayout->addStretch();

	setCustomStyle();
}

void DkRecentFilesWidget::setCustomStyle(bool imgLoadedStyle) {

	setProperty("imageLoaded", imgLoadedStyle);
	style()->unpolish(bgLabel);
	//style()->unpolish(filesTitle);	// don't know why I need to unpolish every widget on it's own
	//style()->unpolish(folderTitle);
	style()->unpolish(this);
	ensurePolished();
}

void DkRecentFilesWidget::setVisible(bool visible, bool saveSettings) {
	
	if (visible && !isVisible()) {
		updateFileList();
	}

	DkWidget::setVisible(saveSettings);
}

void DkRecentFilesWidget::hide(bool saveSettings) {
	
	QWidget::hide();	// no animation effect on hide
	DkWidget::hide(saveSettings);
}

void DkRecentFilesWidget::updateFileList() {

	//delete folderLayout;
	delete filesLayout;
	rFileIdx = 0;
	numActiveLabels = 0;

	filesLayout = new QGridLayout(filesWidget);
	filesLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

	//folderLayout = new QVBoxLayout(folderWidget);

	filesWidget->setFixedHeight(1080);
	//folderWidget->setFixedHeight(1080);

	//filesLayout->setSpacing(10);
	//folderLayout->setSpacing(0);

	for (int idx = 0; idx < fileLabels.size(); idx++) {
		delete fileLabels.at(idx);
	}

	//for (int idx = 0; idx < folderLabels.size(); idx++) {
	//	delete folderLabels.at(idx);
	//}

	//filesTitle->hide();
	//folderTitle->hide();

	fileLabels.clear();
	//folderLabels.clear();
	recentFiles.clear();
	//recentFolders.clear();

	for (const QString& filePath : DkSettingsManager::param().global().recentFiles)
		recentFiles.append(QFileInfo(filePath));
	//for (int idx = 0; idx < DkSettingsManager::param().global().recentFolders.size(); idx++)
	//	recentFolders.append(QFileInfo(DkSettingsManager::param().global().recentFolders.at(idx)));

	updateFiles();
	//updateFolders();
}

void DkRecentFilesWidget::updateFiles() {

	int columns = 5;//qFloor(width()/(mThumbSize*2.0));

	if (fileLabels.empty()) {
		//filesTitle->show();
		//filesLayout->setRowStretch(recentFiles.size()+2, 100);
		//filesLayout->addWidget(filesTitle, 0, 0, 1, columns, Qt::AlignRight);
	}

	// show current
	if (rFileIdx < fileLabels.size() && fileLabels.at(rFileIdx)->hasFile()) {
		fileLabels.at(rFileIdx)->show();
		filesLayout->addWidget(fileLabels.at(rFileIdx), qFloor((float)numActiveLabels/columns)+2, numActiveLabels%columns);
		numActiveLabels++;
	}
	else if (rFileIdx < fileLabels.size()) {
		fileLabels.at(rFileIdx)->hide();

		// remove files which we can't load to keep the list clean...
		DkSettingsManager::param().global().recentFiles.removeAll(fileLabels.at(rFileIdx)->getThumb()->getFilePath());		// remove recent files which we could not load...
	}

	if (!fileLabels.empty())
		rFileIdx++;

	// load next
	if ((rFileIdx/(float)columns*mThumbSize < filesWidget->height()-200 || !(rFileIdx+1 % columns)) && rFileIdx < recentFiles.size()) {
		DkImageLabel* cLabel = new DkImageLabel(recentFiles.at(rFileIdx).absoluteFilePath(), mThumbSize, this);
		cLabel->hide();
		cLabel->setStyleSheet(QString("QLabel{background-color: rgba(0,0,0,0), border: solid 1px black;}"));
		
		fileLabels.append(cLabel);
		connect(cLabel, SIGNAL(labelLoaded()), this, SLOT(updateFiles()));
		connect(cLabel, SIGNAL(loadFileSignal(const QString&)), this, SIGNAL(loadFileSignal(const QString&)));
		cLabel->getThumb()->fetchThumb(DkThumbNailT::force_exif_thumb);
	}
	//else {
	//	qDebug() << "[Recent Files] loading stopped at" << rFileIdx << "history size:" << recentFiles.size();
	//}

	update();
}

//void DkRecentFilesWidget::updateFolders() {
//
//	//folderTitle->show();
//	//folderLayout->addWidget(folderTitle);
//	//folderLayout->addSpacerItem(new QSpacerItem(10, 10));
//
//	int cHeight = 0;
//
//	for (DkFileInfo& fi : recentFolders) {
//
//		DkFolderLabel* fLabel = new DkFolderLabel(fi, this);
//		connect(fLabel, SIGNAL(loadFileSignal(const QString&)), this, SIGNAL(loadFileSignal(const QString&)));
//		folderLayout->addWidget(fLabel);
//		folderLabels.append(fLabel);
//
//		cHeight += fLabel->height();
//		if (cHeight > folderWidget->height())
//			break;
//	}
//
//	folderLayout->addStretch();
//}

// DkDirectoryEdit --------------------------------------------------------------------
DkDirectoryEdit::DkDirectoryEdit(QWidget* parent /* = 0 */) : QLineEdit(parent) {
	setObjectName("DkWarningEdit");
	connect(this, SIGNAL(textChanged(QString)), this, SLOT(lineEditChanged(QString)));

	QCompleter *completer = new QCompleter(this);
	QDirModel* model = new QDirModel(completer);
	model->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
	completer->setModel(model);
	setCompleter(completer);
}

DkDirectoryEdit::DkDirectoryEdit(const QString& content, QWidget* parent /* = 0 */) : QLineEdit(parent) {
	
	setObjectName("DkWarningEdit");
	connect(this, SIGNAL(textChanged(const QString&)), this, SLOT(lineEditChanged(const QString&)));
	setText(content);

	QCompleter *completer = new QCompleter(this);
	QDirModel* model = new QDirModel(completer);
	model->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
	completer->setModel(model);
	setCompleter(completer);
}

void DkDirectoryEdit::lineEditChanged(const QString& path) {
	
	setProperty("error", !existsDirectory(path));
	style()->unpolish(this);
	style()->polish(this);
	update();
	
	// converting to QDir since D:/img == D:/img/ then
	if (QDir(path).absolutePath() != QDir(mOldPath).absolutePath() && existsDirectory(path)) {
		mOldPath = path;
		emit directoryChanged(path);
	}
}

bool DkDirectoryEdit::existsDirectory(const QString& path) {
	
	return QDir(path).exists();
}


// DkDirectoryChooser --------------------------------------------------------------------
DkDirectoryChooser::DkDirectoryChooser(const QString& dirPath, QWidget* parent) : QWidget(parent) {
	
	createLayout(dirPath);
	QMetaObject::connectSlotsByName(this);
}

void DkDirectoryChooser::createLayout(const QString& dirPath) {

	mDirEdit = new DkDirectoryEdit(dirPath, this);
	mDirEdit->setObjectName("dirEdit");

	QPushButton* dirButton = new QPushButton(tr("..."), this);
	dirButton->setObjectName("dirButton");

	QHBoxLayout* layout = new QHBoxLayout(this);
	layout->setAlignment(Qt::AlignLeft);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(mDirEdit);
	layout->addWidget(dirButton);

	connect(mDirEdit, SIGNAL(textChanged(const QString&)), this, SIGNAL(directoryChanged(const QString&)));
}

void DkDirectoryChooser::on_dirButton_clicked() {

	QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open an Image Directory"), mDirEdit->text());

	if (dirPath.isEmpty())
		return;

	mDirEdit->setText(dirPath);
}

// DkListWidget --------------------------------------------------------------------
DkListWidget::DkListWidget(QWidget* parent) : QListWidget(parent) {

	setAcceptDrops(true);
	setDragEnabled(true);
	setMinimumHeight(100);
	setDropIndicatorShown(true);

	setSelectionMode(QAbstractItemView::ExtendedSelection);

}

void DkListWidget::startDrag(Qt::DropActions) {

	QListWidget::startDrag(Qt::MoveAction);
}

bool DkListWidget::isEmpty() const {
	return count() == 0;
}

void DkListWidget::setEmptyText(const QString & text) {
	mEmptyText = text;
}

void DkListWidget::dropEvent(QDropEvent *event) {

	if (event->source() != this)
		QListWidget::dropEvent(event);

	// workaround for the empty rows
	emit dataDroppedSignal();
}

void DkListWidget::paintEvent(QPaintEvent *event) {

	QListView::paintEvent(event);

	if (model() && model()->rowCount(rootIndex())) 
		return;

	// The view is empty.
	QPainter p(viewport());
	p.setPen(Qt::NoPen);
	p.setBrush(QBrush(QColor(200,200,200), Qt::BDiagPattern));
	p.drawRect(QRect(QPoint(), size()));

	p.setPen(QPen(QColor(100,100,100)));
	p.drawText(rect(), Qt::AlignCenter, mEmptyText);
}

// DkProgressBar --------------------------------------------------------------------
DkProgressBar::DkProgressBar(QWidget* parent) : QProgressBar(parent) {

	initPoints();
	mTimer.setInterval(20);
	connect(&mTimer, SIGNAL(timeout()), this, SLOT(update()));

	mShowTimer.setInterval(3000);
	connect(&mShowTimer, SIGNAL(timeout()), this, SLOT(show()));
}

void DkProgressBar::setVisible(bool visible) {

	if (visible)
		mTimer.start();
	else
		mTimer.stop();

	if (visible && !isVisible())
		initPoints();

	QProgressBar::setVisible(visible);
}

void DkProgressBar::setVisibleTimed(bool visible, int time) {

	// nothing todo?
	if (visible && mShowTimer.isActive())
		return;

	// hide
	if (isVisible() && !visible)
		hide();

	// just stop the timer, if it is active & the process is done already
	if (!visible && mShowTimer.isActive())
		mShowTimer.stop();

	// start the timer
	if (!isVisible() && visible && time > 0) {
		mShowTimer.setInterval(time);
		mShowTimer.start();
	}

	// simply show
	if (visible && !isVisible() && time <= 0)
		show();
}

void DkProgressBar::paintEvent(QPaintEvent *) {

	QStyleOption opt;
	opt.init(this);

	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
	p.setPen(Qt::NoPen);

	if (parentWidget() && DkUtils::getMainWindow()->isFullScreen())
		p.fillRect(QRect(QPoint(), size()), DkSettingsManager::param().slideShow().backgroundColor);

	p.setBrush(DkSettingsManager::param().display().highlightColor);

	if (value() != minimum()) {

		int rv = qRound((value() - minimum()) / (double)(maximum() - minimum())*width());

		// draw the current status bar
		QRect r(QPoint(), size());
		r.setRight(rv);

		p.drawRect(r);
	}

	bool stillVisible = false;

	// draw points
	for (double& pt : mPoints) {

		animatePoint(pt);

		QRect r(QPoint(), QSize(height(), height()));
		r.moveLeft(qRound(pt*width()));

		p.drawRect(r);

		if (pt < 0.99)
			stillVisible = true;
	}

	if (!stillVisible)
		initPoints();

}

void DkProgressBar::initPoints() {

	mPoints.clear();

	int m = 7;
	for (int idx = 1; idx < m; idx++) {
		mPoints.append((double)idx / m * 0.1);
	}
}

void DkProgressBar::animatePoint(double& xVal) {


	double speed = xVal > 0.5 ? std::abs(1.0 - xVal) : std::abs(xVal);
	speed *= .05;

	xVal += speed;
}

// DkGenericProfileWidget --------------------------------------------------------------------
DkGenericProfileWidget::DkGenericProfileWidget(const QString& name, QWidget* parent) : DkNamedWidget(name, parent) {

}

DkGenericProfileWidget::~DkGenericProfileWidget() {

	// save settings
	for (int idx = 0; idx < mProfileList->count(); idx++) {
		saveSettings(mProfileList->itemText(idx));
	}
}

void DkGenericProfileWidget::init() {

	createLayout();

	connect(mSaveButton, SIGNAL(clicked()), this, SLOT(saveSettings()));
	connect(mDeleteButton, SIGNAL(clicked()), this, SLOT(deleteCurrentSetting()));
	connect(mProfileList, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(loadSettings(const QString&)));

	activate(false);	// inits gui states
}

void DkGenericProfileWidget::createLayout() {

	QPixmap pm = QPixmap(":/nomacs/img/save.svg");
	mSaveButton = new QPushButton(this);
	mSaveButton->setIcon(QIcon(pm));
	//mSaveButton->setIconSize(pm.size());
	mSaveButton->setFlat(true);

	pm = QPixmap(":/nomacs/img/trash.svg");
	mDeleteButton = new QPushButton(this);
	mDeleteButton->setIcon(QIcon(pm));
	//mDeleteButton->setIconSize(pm.size());
	mDeleteButton->setFlat(true);

	mProfileList = new QComboBox(this);
	mProfileList->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);

	QAction* delGradientAction = new QAction(tr("Set As Default"), mProfileList);
	connect(delGradientAction, SIGNAL(triggered()), this, SLOT(setDefaultModel()));

	mProfileList->addAction(delGradientAction);
	mProfileList->setContextMenuPolicy(Qt::ActionsContextMenu);

	QStringList modelStrings = loadProfileStrings();

	if (!modelStrings.empty()) {
		mProfileList->addItems(loadProfileStrings());
		mProfileList->setCurrentText(loadDefaultProfileString());
	}

	QHBoxLayout* layout = new QHBoxLayout(this);
	layout->setAlignment(Qt::AlignTop);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(mProfileList);
	layout->addWidget(mSaveButton);
	layout->addWidget(mDeleteButton);
}

QSettings& DkGenericProfileWidget::settings() const {

	return DkSettingsManager::instance().qSettings();
}

QStringList DkGenericProfileWidget::loadProfileStrings() const {

	QSettings& s = settings();

	s.beginGroup(mSettingsGroup);
	QStringList modelStrings = s.childGroups();
	s.endGroup();

	qDebug() << "profile settings of group: " << mSettingsGroup;

	return modelStrings;
}

void DkGenericProfileWidget::deleteCurrentSetting() {

	QString modelName = mProfileList->currentText();

	QSettings& s = settings();

	s.beginGroup(mSettingsGroup);
	s.beginGroup(modelName);
	s.remove("");	// remove all group entries
	s.endGroup();
	s.endGroup();

	// update list
	mProfileList->removeItem(mProfileList->currentIndex());
}

void DkGenericProfileWidget::saveSettings() const {

	// default mode is overwrite (UI is asking anyway)
	QString dName = mProfileList->currentText().isEmpty() ? "Profile 1" : mProfileList->currentText();

	bool ok;
	QString text = QInputDialog::getText(
		DkUtils::getMainWindow(), 
		tr("Profile Name"),
		tr("Profile Name:"), 
		QLineEdit::Normal,
		dName, &ok);

	if (!ok || text.isEmpty())
		return;	// user canceled

				// is the model name unique?
	if (mProfileList->findText(text) != -1) {

		QMessageBox::StandardButton button = QMessageBox::information(
			DkUtils::getMainWindow(), 
			tr("Profile Already Exists"), 
			tr("Do you want to overwrite %1?").arg(text), 
			QMessageBox::Yes | QMessageBox::No);

		if (button == QMessageBox::No) {
			saveSettings(); // start over
			return;
		}
	}

	saveSettings(text);
}

void DkGenericProfileWidget::saveSettings(const QString& name) const {

	if (mProfileList->findText(name) == -1)
		mProfileList->addItem(name);

	int idx = mProfileList->findText(name);
	if (idx != -1)
		mProfileList->setCurrentIndex(idx);
}

void DkGenericProfileWidget::activate(bool active) {

	mProfileList->setVisible(active);
	mSaveButton->setVisible(active);
	mDeleteButton->setVisible(active);
	mEmpty = !active;

	if (active)
		loadSettings(mProfileList->currentText());

	update();
}

void DkGenericProfileWidget::setDefaultModel() const {

	QSettings& s = settings();
	s.beginGroup(mSettingsGroup);
	s.setValue("DefaultProfileString", mProfileList->currentText());
	s.endGroup();
}

QString DkGenericProfileWidget::loadDefaultProfileString() const {

	QSettings& s = settings();
	s.beginGroup(mSettingsGroup);
	QString defaultString = s.value("DefaultProfileString", "").toString();
	s.endGroup();

	return defaultString;
}

void DkGenericProfileWidget::paintEvent(QPaintEvent* ev) {

	if (mEmpty) {

		// The view is empty.
		QPainter p(this);
		p.setPen(Qt::NoPen);
		p.setBrush(QBrush(QColor(200,200,200), Qt::BDiagPattern));
		p.drawRect(QRect(QPoint(), size()));

		p.setPen(QPen(QColor(100,100,100)));
		p.drawText(rect(), Qt::AlignCenter, tr("No Profiles"));
	}
	else
		DkNamedWidget::paintEvent(ev);
}

// DkTabEntryWidget --------------------------------------------------------------------
DkTabEntryWidget::DkTabEntryWidget(const QIcon& icon, const QString& text, QWidget* parent) : QPushButton(text, parent) {

	setObjectName("DkTabEntryWidget");

	QPixmap pm = DkImage::colorizePixmap(icon.pixmap(100), QColor(255, 255, 255));
	setIcon(pm);
	setIconSize(pm.size());

	setFlat(true);
	setCheckable(true);
}

void DkTabEntryWidget::paintEvent(QPaintEvent *event) {

	// fixes stylesheets which are not applied to custom widgets
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

	QPushButton::paintEvent(event);
}

}


