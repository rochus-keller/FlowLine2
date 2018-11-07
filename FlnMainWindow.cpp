/*
* Copyright 2010-2018 Rochus Keller <mailto:me@rochus-keller.info>
*
* This file is part of the FlowLine2 application.
*
* The following is the license that applies to this copy of the
* application. For a license to use the application under conditions
* other than those described here, please email to me@rochus-keller.info.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include "FlnMainWindow.h"
#include "FlowLine2App.h"
#include "FuncTree.h"
#include "EpkObjects.h"
#include "EpkProcs.h"
#include "EpkCtrl.h"
#include "EpkView.h"
#include "EpkLinkViewCtrl.h"
#include "FlnFolderCtrl.h"
#include "SysTree.h"
#include "AllocViewCtrl.h"
#include "EpkLuaBinding.h"
#include <CrossLine/DocTabWidget.h>
#include <Gui2/AutoShortcut.h>
#include <Oln2/OutlineUdbCtrl.h>
#include <Udb/Database.h>
#include <WorkTree/AttrViewCtrl.h>
#include <WorkTree/TextViewCtrl.h>
#include <WorkTree/SceneOverview.h>
#include <WorkTree/SearchView.h>
#include <WorkTree/RefByViewCtrl.h>
#include <QSettings>
#include <QFileInfo>
#include <QCloseEvent>
#include <QApplication>
#include <QMessageBox>
#include <QFontDialog>
#include <QDockWidget>
#include <QDesktopServices>
#include <QTreeView>
#include <Script/CodeEditor.h>
#include <Script/Terminal2.h>
using namespace Fln;

class _MyDockWidget : public QDockWidget
{
public:
    _MyDockWidget( const QString& title, QWidget* p ):QDockWidget(title,p){}
    virtual void setVisible( bool visible )
    {
        QDockWidget::setVisible( visible );
        if( visible )
            raise();
    }
};

static inline QDockWidget* createDock( QMainWindow* parent, const QString& title, quint32 id, bool visi )
{
    QDockWidget* dock = new _MyDockWidget( title, parent );
    if( id )
        dock->setObjectName( QString( "{%1" ).arg( id, 0, 16 ) );
    else
        dock->setObjectName( QChar('[') + title );
    dock->setAllowedAreas( Qt::AllDockWidgetAreas );
    dock->setFeatures( QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable );
    dock->setFloating( false );
    dock->setVisible( visi );
    return dock;
}

MainWindow::MainWindow(Udb::Transaction *txn)
    : QMainWindow(0), d_txn(txn),d_fullScreen(false),d_selectLock(false),d_pushBackLock(false)
{
    setAttribute( Qt::WA_DeleteOnClose );
    Q_ASSERT( txn != 0 );

	d_tab = new Oln::DocTabWidget( this, false );
	Oln::DocTabWidget::attrText = Udb::ContentObject::AttrText;
    d_tab->setCloserIcon( ":/FlowLine2/Images/close.png" );
    setCentralWidget( d_tab );
    connect( d_tab, SIGNAL( currentChanged(int) ), this, SLOT(onTabChanged(int) ) );
	connect( d_tab, SIGNAL(closing(int)), this, SLOT(saveEditor()) );

    setDockNestingEnabled(true);
    setCorner( Qt::BottomRightCorner, Qt::BottomDockWidgetArea );
    setCorner( Qt::BottomLeftCorner, Qt::LeftDockWidgetArea );
    setCorner( Qt::TopRightCorner, Qt::RightDockWidgetArea );
	setCorner( Qt::TopLeftCorner, Qt::LeftDockWidgetArea );

    setupAttrView();
    setupTextView();
    setupLinkView();
    setupFunctions();
    setupFolders();
    setupAllocView();
    setupOverview();
    setupSearchView();
    setupSysTree();
	setupTerminal();
	setupItemRefView();

    QSettings set;
    QVariant state = set.value( "MainFrame/State/" + d_txn->getDb()->getDbUuid().toString() ); // Da DB-individuelle Docks
    if( !state.isNull() )
        restoreState( state.toByteArray() );

    if( set.value( "MainFrame/State/FullScreen" ).toBool() )
    {
        toFullScreen( this );
        d_fullScreen = true;
    }else
    {
        showNormal();
        d_fullScreen = false;
    }

    setCaption();
	setWindowIcon( qApp->windowIcon() ); // Ist noetig, da ansonsten auf Linux das Fenster keine Ikone hat!

    // Tab Menu
    Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_tab, true );
	pop->addCommand( tr("Forward Tab"), d_tab, SLOT(onDocSelect()), tr("CTRL+TAB") );
	pop->addCommand( tr("Backward Tab"), d_tab, SLOT(onDocSelect()), tr("CTRL+SHIFT+TAB") );
	pop->addCommand( tr("Close Tab"), d_tab, SLOT(onCloseDoc()), tr("CTRL+W") );
    pop->addCommand( tr("Close All"), d_tab, SLOT(onCloseAll()) );
    pop->addCommand( tr("Close All Others"), d_tab, SLOT(onCloseAllButThis()) );
    addTopCommands( pop );
	    
	new Gui2::AutoShortcut( tr("CTRL+TAB"), this, d_tab, SLOT(onDocSelect()) );
	new Gui2::AutoShortcut( tr("CTRL+SHIFT+TAB"), this, d_tab, SLOT(onDocSelect()) );
	new Gui2::AutoShortcut( tr("CTRL+W"), this, d_tab, SLOT(onCloseDoc()) );
	    
    new Gui2::AutoShortcut( tr("F11"), this, this, SLOT(onFullScreen()) );
    new Gui2::AutoShortcut( tr("CTRL+F"), this, this, SLOT(onSearch()) );
    new Gui2::AutoShortcut( tr("CTRL+Q"), this, this, SLOT(close()) );
    new Gui2::AutoShortcut( tr("ALT+LEFT"), this,  this, SLOT(onGoBack()) );
    new Gui2::AutoShortcut( tr("ALT+RIGHT"), this,  this, SLOT(onGoForward()) );
    new Gui2::AutoShortcut( tr("ALT+UP"), this, this,  SLOT(onShowSuperTask()) );
    new Gui2::AutoShortcut( tr("ALT+DOWN"), this,  this, SLOT(onShowSubTask()) );
    new Gui2::AutoShortcut( tr("ALT+HOME"), this,  this, SLOT(onFollowAlias()) );

	onFollowObject( Epk::Index::getRoot(d_txn).getValueAsObj(Epk::Root::AttrAutoOpen) );
}

MainWindow::~MainWindow()
{
    Q_ASSERT( d_txn );
	delete d_txn;
}

void MainWindow::showOid(quint64 oid)
{
	Udb::Obj o = d_txn->getObject( oid );
	if( !o.isNull() )
		onFollowObject( o );
}

void MainWindow::onTabChanged(int i)
{
    if( i < 0 ) // bei remove des letzen Tab kommt -1
    {
        d_ov->setObserved( 0 );
        d_lv->setObserved( 0 );
    }else
    {
        if( Epk::EpkView* v = dynamic_cast<Epk::EpkView*>( d_tab->widget( i ) ) )
        {
            d_ov->setObserved( v );
            d_lv->setObserved( v );
            Udb::Obj o = v->getCtrl()->getSingleSelection();
            if( o.getType() == Epk::ConFlow::TID )
                onFuncSelected( o );
            else if( !o.isNull() )
                d_ft->showObject( o );
        }else
        {
            d_ov->setObserved( 0 );
            d_lv->setObserved( 0 );
        }
    }
}

void MainWindow::setCaption()
{
    QFileInfo info( d_txn->getDb()->getFilePath() );
    setWindowTitle( tr("%1 - FlowLine2").arg( info.baseName() ) );
}

void MainWindow::toFullScreen(QMainWindow * w)
{
#ifdef _WIN32
    w->showFullScreen();
#else
    w->setWindowFlags( Qt::Window | Qt::FramelessWindowHint  );
    w->showMaximized();
    //w->setWindowIcon( qApp->windowIcon() );
#endif
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings set;
    set.setValue("MainFrame/State/" + d_txn->getDb()->getDbUuid().toString(), saveState() );
    QMainWindow::closeEvent( event );
    if( event->isAccepted() )
        emit closing();
}

void MainWindow::onFullScreen()
{
    CHECKED_IF( true, d_fullScreen );
    if( d_fullScreen )
    {
#ifndef _WIN32
        setWindowFlags( Qt::Window );
#endif
        showNormal();
        d_fullScreen = false;
    }else
    {
        toFullScreen( this );
        d_fullScreen = true;
    }
    QSettings set;
    set.setValue( "MainFrame/State/FullScreen", d_fullScreen );
}

void MainWindow::onSetAppFont()
{
    ENABLED_IF( true );
    QFont f1 = QApplication::font();
    bool ok;
	QFont f2 = QFontDialog::getFont( &ok, f1, this, tr("Select Application Font - FlowLine" ) );
    if( !ok )
        return;
    f1.setFamily( f2.family() );
    f1.setPointSize( f2.pointSize() );
    QApplication::setFont( f1 );
    FlowLine2App::inst()->setAppFont( f1 );
    QSettings set;
    set.setValue( "App/Font", f1 );
}

void MainWindow::onAbout()
{
    ENABLED_IF( true );

	QMessageBox::about( this, tr("About FlowLine"),
		tr("<html><body><p>Release: <strong>%1</strong>   Date: <strong>%2</strong> </p>"
		   "<p>FlowLine can be used for functional and process analysis.</p>"
		   "<p>Author: Rochus Keller, me@rochus-keller.info</p>"
		   "<p>Copyright (c) 2010-%3</p>"
		   "<h4>Additional Credits:</h4>"
		   "<p>Qt GUI Toolkit 4.4 (c) 1995-2008 Trolltech AS, 2008 Nokia Corporation<br>"
		   "Sqlite 3.5, <a href='http://sqlite.org/copyright.html'>dedicated to the public domain by the authors</a><br>"
		   "<a href='http://www.sourceforge.net/projects/clucene'>CLucene</a> Copyright (C) 2003-2006 Ben van Klinken and the CLucene Team<br>"
		   "<a href='http://code.google.com/p/fugue-icons-src/'>Fugue Icons</a>  2012 by Yusuke Kamiyamane<br>"
		   "<a href='https://www.graphviz.org'>Graphviz</a>  (libGv libCgraph) 1991-2016 by AT&T, John Ellson et al.<br>"
		   "Lua 5.1 by R. Ierusalimschy, L. H. de Figueiredo & W. Celes (c) 1994-2006 Tecgraf, PUC-Rio<p>"
		   "<h4>Terms of use:</h4>"
		   "<p>This version of FlowLine is freeware, i.e. it can be used for free by anyone. "
		   "The software and documentation are provided \"as is\", without warranty of any kind, "
		   "expressed or implied, including but not limited to the warranties of merchantability, "
		   "fitness for a particular purpose and noninfringement. In no event shall the author or "
		   "copyright holders be liable for any claim, damages or other liability, whether in an action "
		   "of contract, tort or otherwise, arising from, out of or in connection with the software or the "
		   "use or other dealings in the software.</p></body></html>" )
						.arg( FlowLine2App::s_version ).arg( FlowLine2App::s_date ).arg( QDate::currentDate().year() ) );
}

void MainWindow::addTopCommands(Gui2::AutoMenu * pop)
{
    Q_ASSERT( pop != 0 );
    pop->addSeparator();
    Gui2::AutoMenu* sub = new Gui2::AutoMenu( tr("Go" ), pop );
    pop->addMenu( sub );
    sub->addCommand( tr("Back"),  this, SLOT(onGoBack()), tr("ALT+LEFT") );
    sub->addCommand( tr("Forward"), this, SLOT(onGoForward()), tr("ALT+RIGHT") );
    sub->addCommand( tr("Search..."),  this, SLOT(onSearch()), tr("CTRL+F") );
    if( QMenu* sub2 = createPopupMenu() )
    {
        sub2->setTitle( tr("Show Window") );
        pop->addMenu( sub2 );
    }
    sub = new Gui2::AutoMenu( tr("Configuration" ), pop );
    pop->addMenu( sub );
	sub->addCommand( tr("Set Application Font..."), this, SLOT(onSetAppFont()) );
	sub->addCommand( "Set Script Font...", this, SLOT(onSetScriptFont()) );
	sub->addCommand( tr("Full Screen"), this, SLOT(onFullScreen()), tr("F11") )->setCheckable(true);
	sub->addCommand( tr("Update Indices..."), this, SLOT(onRebuildIndices()) );

	pop->addCommand( tr("About FlowLine..."), this, SLOT(onAbout()) );
    pop->addSeparator();
    pop->addAction( tr("Quit"), this, SLOT(close()), tr("CTRL+Q") );

}

void MainWindow::setupAttrView()
{
    QDockWidget* dock = createDock( this, tr("Object Attributes"), 0, true );
    d_atv = Wt::AttrViewCtrl::create( dock, d_txn );
    dock->setWidget( d_atv->getWidget() );
    addDockWidget( Qt::BottomDockWidgetArea, dock );
    connect( d_atv,SIGNAL(signalFollowObject(Udb::Obj)), this,SLOT(onFollowObject(Udb::Obj)) );
}

void MainWindow::setupTextView()
{
    QDockWidget* dock = createDock( this, tr("Object Text"), 0, true );
    d_tv = Wt::TextViewCtrl::create( dock, d_txn );
    Gui2::AutoMenu* pop = d_tv->createPopup();
    addTopCommands( pop );
    dock->setWidget( d_tv->getWidget() );
    addDockWidget( Qt::BottomDockWidgetArea, dock );
    connect( d_tv,SIGNAL(signalFollowObject(Udb::Obj)), this,SLOT(onFollowObject(Udb::Obj)) );
	connect( d_tv,SIGNAL(signalItemActivated(Udb::Obj)), this, SLOT(onItemActivated(Udb::Obj)) );
}

void MainWindow::setupOverview()
{
    QDockWidget* dock = createDock( this, tr("Overview"), 0, true );
    d_ov = new Wt::SceneOverview( dock );
    dock->setWidget( d_ov );
    addDockWidget( Qt::LeftDockWidgetArea, dock );
}

void MainWindow::setupLinkView()
{
	QDockWidget* dock = createDock( this, tr("Control Flows"), 0, true );
    d_lv = Epk::EpkLinkViewCtrl::create( dock, d_txn );
    Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_lv->getWidget(), true );
    d_lv->addCommands( pop );
    connect( d_lv, SIGNAL(signalSelect(Udb::Obj,bool)), this, SLOT(onLinkSelected(Udb::Obj,bool)) );
    addTopCommands( pop );
    dock->setWidget( d_lv->getWidget() );
    addDockWidget( Qt::BottomDockWidgetArea, dock );
}

void MainWindow::setupAllocView()
{
    QDockWidget* dock = createDock( this, tr("Allocations"), 0, true );
    d_alloc = AllocViewCtrl::create( dock, d_txn );
    Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_alloc->getWidget(), true );
    d_alloc->addCommands( pop );
    connect( d_alloc, SIGNAL(signalSelect(Udb::Obj)), this, SLOT(onAllocSelected(Udb::Obj)) );
    addTopCommands( pop );
    dock->setWidget( d_alloc->getWidget() );
    addDockWidget( Qt::BottomDockWidgetArea, dock );
}

void MainWindow::setupFunctions()
{
    QDockWidget* dock = createDock( this, tr("Function Tree"), 0, true );

    Epk::FuncDomain root = Epk::FuncDomain::getOrCreateRoot( d_txn );
    root.setText( root.formatObjectTitle() );
    d_txn->commit();
    d_ft = FuncTreeCtrl::create( dock, root );
    Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_ft->getTree(), true );
    pop->addCommand( tr("Open Diagram"), this, SLOT(onOpenEpkDiagram()) );
    pop->addSeparator();
    d_ft->addCommands( pop );
    addTopCommands( pop );
    connect( d_ft, SIGNAL(signalSelected(Udb::Obj)), this, SLOT(onFuncSelected(Udb::Obj)) );
    connect( d_ft, SIGNAL(signalDblClicked(Udb::Obj)), this, SLOT(onFuncDblClicked(Udb::Obj)));
    dock->setWidget( d_ft->getTree() );
    addDockWidget( Qt::LeftDockWidgetArea, dock );
}

void MainWindow::setupFolders()
{
    QDockWidget* dock = createDock( this, tr("Documents"), 0, true );

    Udb::RootFolder root = Udb::RootFolder::getOrCreate( d_txn );
    root.setText( Epk::Procs::formatObjectTitle(root) );
    d_txn->commit();
    d_fldr = FolderCtrl::create( dock, root );
    Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_fldr->getTree(), true );
    pop->addCommand( tr("Open Document"), this, SLOT(onOpenDocument()) );
    pop->addSeparator();
    d_fldr->addCommands( pop );
    addTopCommands( pop );
    connect( d_fldr, SIGNAL(signalSelected(Udb::Obj)), this, SLOT(onFldrSelected(Udb::Obj)) );
    connect( d_fldr, SIGNAL(signalDblClicked(Udb::Obj)), this, SLOT( onFldrDblClicked(Udb::Obj)));
    dock->setWidget( d_fldr->getTree() );
    addDockWidget( Qt::LeftDockWidgetArea, dock );
}

void MainWindow::setupSysTree()
{
    QDockWidget* dock = createDock( this, tr("System Tree"), 0, true );

    Epk::SystemElement root = Epk::SystemElement::getOrCreateRoot( d_txn );
    root.setText( Epk::Procs::formatObjectTitle(root) );
    d_txn->commit();
    d_sys = SysTreeCtrl::create( dock, root );
    Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_sys->getTree(), true );
    d_sys->addCommands( pop );
    addTopCommands( pop );
    connect( d_sys, SIGNAL(signalSelected(Udb::Obj)), this, SLOT(onSysSelected(Udb::Obj)) );
    dock->setWidget( d_sys->getTree() );
    addDockWidget( Qt::RightDockWidgetArea, dock );
}

void MainWindow::setupSearchView()
{
    d_sv = new Wt::SearchView( this, d_txn );
    QDockWidget* dock = createDock( this, tr("Search" ), 0, false );
    dock->setWidget( d_sv );
    addDockWidget( Qt::RightDockWidgetArea, dock );

    connect( d_sv, SIGNAL(signalShowItem(Udb::Obj)), this, SLOT(onSearchSelected(Udb::Obj) ) );
    connect( d_sv, SIGNAL(signalOpenItem(Udb::Obj) ), this, SLOT(onFollowObject(Udb::Obj) ) );

    Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_sv, true );
    pop->addCommand( tr("Show Item"), d_sv, SLOT(onOpen()), tr("CTRL+O"), true );
    pop->addCommand( tr("Clear"), d_sv, SLOT(onClearSearch()) );
    pop->addSeparator();
    pop->addCommand( tr("Copy"), d_sv, SLOT(onCopyRef()), tr("CTRL+C"), true );
    pop->addSeparator();
    pop->addCommand( tr("Update Index..."), d_sv, SLOT(onUpdateIndex()) );
    pop->addCommand( tr("Rebuild Index..."), d_sv, SLOT(onRebuildIndex()) );
	addTopCommands( pop );
}

void MainWindow::setupTerminal()
{
	QDockWidget* dock = createDock( this, tr("Lua Terminal"), 0, false );
	Lua::Terminal2* term = new Lua::Terminal2( dock );
	dock->setWidget( term );
	addDockWidget( Qt::BottomDockWidgetArea, dock );
}

static QString _formatTitle(const Udb::Obj & o )
{
	return Epk::Procs::formatObjectTitle(o,true);
}

void MainWindow::setupItemRefView()
{
	QDockWidget* dock = createDock( this, tr("Referenced by"), 0, false );

	d_rbv = Wt::RefByViewCtrl::create( dock, d_txn );
	Oln::RefByItemMdl::formatTitle = _formatTitle;
	dock->setWidget( d_rbv->getWidget() );
	addDockWidget( Qt::RightDockWidgetArea, dock );

	connect( d_rbv, SIGNAL(signalFollowObject(Udb::Obj)), this,SLOT(onFollowObject(Udb::Obj)) );

	Gui2::AutoMenu* pop = new Gui2::AutoMenu( d_rbv->getWidget(), true );
	d_rbv->addCommands( pop );
	addTopCommands( pop );
}

void MainWindow::onFuncSelected(const Udb::Obj & o)
{
    if( d_selectLock )
        return;
    d_selectLock = true;
    d_atv->setObj( o );
    d_tv->setObj( o );
	d_rbv->setObj( o );
	d_lv->setObj( o );
    d_alloc->setObj( o );
    pushBack( o );
    showInEpkDiagram( o, false );
    d_selectLock = false;
}

void MainWindow::onFuncDblClicked(const Udb::Obj & doc)
{
    openEpkDiagram( doc );
}

void MainWindow::onSysSelected(const Udb::Obj & o)
{
    if( d_selectLock )
        return;
    d_selectLock = true;
    d_atv->setObj( o );
    d_tv->setObj( o );
	d_rbv->setObj( o );
	pushBack( o );
    d_selectLock = false;
}

void MainWindow::onOpenEpkDiagram()
{
    Udb::Obj doc = d_ft->getSelectedObject(true);
    ENABLED_IF( Epk::Procs::isDiagram( doc.getType() ) );

    openEpkDiagram( doc );
}

static Epk::EpkCtrl* _getCurrentEpkDiagram(Oln::DocTabWidget* tab)
{
    Epk::EpkView* v = dynamic_cast<Epk::EpkView*>( tab->getCurrentTab() );
    if( v )
        return v->getCtrl();
    else
        return 0;
}

void Fln::MainWindow::onFollowObject(const Udb::Obj & o)
{
    if( o.isNull( true, true ) )
        return;
    const Udb::Atom type = o.getType();
	if( Epk::Procs::isValidAggregate( Epk::Function::TID, type ) && type != Oln::OutlineItem::TID )
    {
        if( type == Epk::Function::TID )
            d_ft->showObject(o);
        else
            onFuncSelected( o );
        if( o.getValue( Epk::Function::AttrElemCount ).getUInt32() == 0 && type != Epk::FuncDomain::TID )
            openEpkDiagram( o.getParent(), o );
        else
            openEpkDiagram( o );
    }else if( type == Epk::ConFlow::TID )
    {
        onLinkSelected( o, false );
        d_lv->setObj( o.getParent() );
        d_lv->showLink( o );
    }else if( type == Epk::Diagram::TID )
    {
        openEpkDiagram( o );
        d_fldr->showObject( o );
	}else if( type == Oln::OutlineItem::TID )
    {
        Oln::OutlineItem item(o);
		Udb::Obj home = item.getHome();
		if( home.getType() == Oln::Outline::TID )
			openOutline( home, o );
		else
			onFollowObject( home );
		d_tv->setObj( o );
	}else if( type == Oln::Outline::TID )
    {
        openOutline( o );
        d_fldr->showObject( o );
    }else if( type == Udb::Folder::TID )
        d_fldr->showObject( o );
	else if( type == Udb::ScriptSource::TID )
	{
		openScript( o );
		d_fldr->showObject( o );
	}else
    {
        pushBack( o );
        d_atv->setObj( o );
        d_tv->setObj( o );
		d_rbv->setObj( o );
    }
}

void MainWindow::showInEpkDiagram(const Udb::Obj &select, bool checkAllOpenDiagrams)
{
    if( !select.isNull() )
    {
        // Wenn das Item im aktuell angezeigten Diagramm vorhanden ist, nehme das
        Epk::EpkCtrl* c = _getCurrentEpkDiagram(d_tab);
        if( c && c->focusOn( select ) )
            return;
    }
    if( !checkAllOpenDiagrams )
        return;
    const int pos = d_tab->findDoc( (select.getType()==Epk::ConFlow::TID)?
                                        select.getParent().getParent():
                                        select.getParent() );
    if( pos != -1 )
    {
        // Wenn der Parent des Items aktuell offen ist, nehme das
        if( Epk::EpkView* v = dynamic_cast<Epk::EpkView*>( d_tab->widget(pos) ) )
        {
            if( v->getCtrl()->focusOn( select ) )
                d_tab->setCurrentIndex( pos );
            return;
        }
    }
    // else schaue in allen anderen offenen Diagrammen
    for( int i = 0; i < d_tab->count(); i++ )
    {
        if( Epk::EpkView* v = dynamic_cast<Epk::EpkView*>( d_tab->widget(i) ) )
        {
            if( v->getCtrl()->focusOn( select ) )
                d_tab->setCurrentIndex( i );
            return;
        }
    }
}

void MainWindow::openEpkDiagram(const Udb::Obj & doc, const Udb::Obj &select)
{
    if( doc.isNull() || !Epk::Procs::isDiagram(doc.getType()) )
        return;
    if( d_tab->showDoc( doc ) != -1 )
    {
        // Wenn der Parent des Items aktuell offen ist, nehme das
        Epk::EpkCtrl* c = _getCurrentEpkDiagram(d_tab);
        if( c )
            c->focusOn( select );
        return;
    }
    // Ansonsten ffne den Parent des Diagramms
    QApplication::setOverrideCursor( Qt::WaitCursor );

    Epk::EpkCtrl* ctrl = Epk::EpkCtrl::create( d_tab, doc );
    Gui2::AutoMenu* pop = new Gui2::AutoMenu( ctrl->getView(), true );
    pop->addCommand( tr( "Show Subfunction" ), this, SLOT( onShowSubTask() ), tr("ALT+DOWN") );
    pop->addCommand( tr( "Show Superfunction" ), this, SLOT( onShowSuperTask() ), tr("ALT+UP") );
    pop->addCommand( tr( "Follow Alias" ), this, SLOT( onFollowAlias() ), tr("ALT+HOME") );
    pop->addSeparator();
    ctrl->addCommands( pop );
    addTopCommands( pop );
    connect( ctrl, SIGNAL(signalSelectionChanged()), this, SLOT(onPdmSelection()) );

    d_tab->addDoc( ctrl->getView(), doc, Epk::Procs::formatObjectTitle( doc ) );
    ctrl->focusOn( select );

    QApplication::restoreOverrideCursor();
}

void MainWindow::pushBack(const Udb::Obj & o)
{
    if( d_pushBackLock )
        return;
    if( o.isNull( true, true ) )
        return;
	Epk::LuaBinding::setCurrentObject( o );
    if( !d_backHisto.isEmpty() && d_backHisto.last() == o.getOid() )
        return; // o ist bereits oberstes Element auf dem Stack.
    d_backHisto.removeAll( o.getOid() );
    d_backHisto.push_back( o.getOid() );
}

void MainWindow::onGoBack()
{
    // d_backHisto.last() ist Current
    ENABLED_IF( d_backHisto.size() > 1 );

    d_pushBackLock = true;
    d_forwardHisto.push_back( d_backHisto.last() );
    d_backHisto.pop_back();
    onFollowObject( d_txn->getObject( d_backHisto.last() ) );
    d_pushBackLock = false;
}

void MainWindow::onGoForward()
{
    ENABLED_IF( !d_forwardHisto.isEmpty() );

    Udb::Obj cur = d_txn->getObject( d_forwardHisto.last() );
    d_forwardHisto.pop_back();
    onFollowObject( cur );
}

void MainWindow::onShowSubTask()
{
    Epk::EpkCtrl* ctrl = _getCurrentEpkDiagram(d_tab);
    if( ctrl == 0 )
        return;
    Udb::Obj sel = ctrl->getSingleSelection();
    ENABLED_IF( sel.getType() == Epk::Function::TID );
    d_ft->showObject( sel );
    openEpkDiagram( sel );
}

void MainWindow::onShowSuperTask()
{
    Epk::EpkCtrl* ctrl = _getCurrentEpkDiagram(d_tab);
    if( ctrl == 0 )
        return;
    const quint32 type = ctrl->getDiagram().getParent().getType();
    ENABLED_IF( ctrl && ( type == Epk::Function::TID || type == Epk::Diagram::TID ) );
    d_ft->showObject( ctrl->getDiagram().getParent() );
    openEpkDiagram( ctrl->getDiagram().getParent(), ctrl->getDiagram() );
}

void MainWindow::onFollowAlias()
{
    Epk::EpkCtrl* ctrl = _getCurrentEpkDiagram(d_tab);
    if( ctrl == 0 )
        return;
    Udb::Obj sel = ctrl->getSingleSelection();
    ENABLED_IF( !sel.isNull() && !sel.getParent().equals( ctrl->getDiagram() ) );
    d_ft->showObject( sel );
    openEpkDiagram( sel.getParent(), sel );
}

void MainWindow::onPdmSelection()
{
    Epk::EpkCtrl* ctrl = dynamic_cast<Epk::EpkCtrl*>( sender() );
    Q_ASSERT( ctrl != 0 );
    Udb::Obj o = ctrl->getSingleSelection();
    if( !o.isNull( ) )
    {
        if( o.getType() == Epk::ConFlow::TID )
            onFuncSelected( o );
        else if( o.getType() == Epk::Event::TID || o.getType() == Epk::Connector::TID )
            onFuncSelected(o); // da ja Events und Connectors auch nicht mehr in d_ft vertreten sind
        else
            d_ft->showObject( o );
    }
}

void MainWindow::onLinkSelected(const Udb::Obj & o, bool open )
{
    const Udb::Atom type = o.getType();
    if( type == Epk::ConFlow::TID )
    {
        if( open )
            openEpkDiagram( o.getParent().getParent(), o );
        onFuncSelected( o );
    }else
    {
        if( type == Epk::Function::TID )
            d_ft->showObject( o );
        else
            onFuncSelected( o );
        if( open )
            openEpkDiagram( o.getParent(), o );
    }
}

void MainWindow::onSearchSelected(const Udb::Obj & o )
{
    const Udb::Atom type = o.getType();
    if( type == Epk::Function::TID )
        d_ft->showObject( o );
    else if( Epk::Procs::isDiagNode( type ) )
        onFuncSelected( o );
    else
    {
		Udb::Obj home = o;
		if( o.getType() == Oln::OutlineItem::TID )
			home = Oln::OutlineItem(o).getHome();
		d_atv->setObj( home );
        d_tv->setObj( o );
		d_rbv->setObj( o );
		d_fldr->showObject(home);
		Epk::LuaBinding::setCurrentObject( o );
	}
}

void MainWindow::onAllocSelected(const Udb::Obj & o)
{
    if( d_selectLock )
        return;
    d_selectLock = true;
    d_atv->setObj( o );
    d_tv->setObj( o );
	d_rbv->setObj( o );
	pushBack( o );
	d_selectLock = false;
}

void MainWindow::handleExecute()
{
	Lua::CodeEditor* e = dynamic_cast<Lua::CodeEditor*>( d_tab->currentWidget() );
	ENABLED_IF( !Lua::Engine2::getInst()->isExecuting() && e != 0 );
	Lua::Engine2::getInst()->executeCmd( e->text().toLatin1(), ( e->getName().isEmpty() ) ?
											 QByteArray("#Editor") : e->getName().toLatin1() );
}

void MainWindow::saveEditor()
{
	Lua::CodeEditor* e = dynamic_cast<Lua::CodeEditor*>( d_tab->currentWidget() );
	if( e == 0 || !e->document()->isModified() )
		return;
	Udb::ScriptSource script = d_tab->getCurrentObj();
	if( script.isNull() )
		return;
	script.setSource( e->getText() );
	script.commit();
	e->document()->setModified(false);
}

void MainWindow::onSetScriptFont()
{
	ENABLED_IF( true );

	bool ok;
	QFont f = Lua::CodeEditor::defaultFont();
	Lua::CodeEditor* e = dynamic_cast<Lua::CodeEditor*>( d_tab->currentWidget() );
	if( e )
		f = e->font();
	f = QFontDialog::getFont( &ok, f, this );
	if( !ok )
		return;
	QSettings set;
	set.setValue( "LuaEditor/Font", QVariant::fromValue(f) );
	set.sync();
	for( int i = 0; i < d_tab->count(); i++ )
		dynamic_cast<Lua::CodeEditor*>( d_tab->widget( i ) )->setFont(f);
}

void MainWindow::onItemActivated(const Udb::Obj & item)
{
	d_rbv->setObj( item );
	pushBack(item);
}

void MainWindow::onItemActivated(quint64 id)
{
	onItemActivated(d_txn->getObject(id));
}

void MainWindow::onRebuildIndices()
{
	ENABLED_IF(true);
	if( QMessageBox::warning( this, tr("Update Indices - FlowLine"),
		tr("This operation can take some Time. Do you want to continue?" ),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No ) == QMessageBox::No )
		return;

	QApplication::setOverrideCursor( Qt::WaitCursor );
	Oln::OutlineItem::updateAllRefs( d_txn );
	QApplication::restoreOverrideCursor();
}

void MainWindow::onAutoStart()
{
	Udb::Obj oln = d_tab->getCurrentObj();
	Udb::Obj root = Epk::Index::getRoot( d_txn );
	CHECKED_IF( !oln.isNull(), root.getValueAsObj(Epk::Root::AttrAutoOpen).equals(oln) );

	if( root.getValueAsObj(Epk::Root::AttrAutoOpen).equals(oln) )
		root.clearValue(Epk::Root::AttrAutoOpen);
	else
		root.setValue(Epk::Root::AttrAutoOpen,oln);
	root.commit();
}

void MainWindow::onOpenDocument()
{
    Udb::Obj doc = d_fldr->getSelectedObject();
    ENABLED_IF( !doc.isNull() );

    const Udb::Atom type = doc.getType();
    if( type == Epk::Diagram::TID )
        openEpkDiagram( doc );
	else if( type == Oln::Outline::TID )
        openOutline( doc );
}

void MainWindow::openOutline(const Udb::Obj &doc, const Udb::Obj &select)
{
    if( doc.isNull() )
        return;
    const int pos = d_tab->showDoc( doc );
    if( pos != -1 )
    {
        const QObjectList& ol = d_tab->widget(pos)->children();
        for( int i = 0; i < ol.size(); i++ )
        {
            if( Oln::OutlineUdbCtrl* ctrl = dynamic_cast<Oln::OutlineUdbCtrl*>( ol[i] ) )
            {
                if( ctrl->gotoItem( select.getOid() ) )
                    ctrl->getTree()->expand( ctrl->getMdl()->getIndex( select.getOid() ) );
                return;
            }
        }
        return;
    }
    QApplication::setOverrideCursor( Qt::WaitCursor );

    Oln::OutlineUdbCtrl* oln = Oln::OutlineUdbCtrl::create( d_tab, d_txn );
    oln->getDeleg()->setShowIcons( true );

    connect( oln, SIGNAL(sigLinkActivated(quint64) ), this, SLOT(onFollowOID(quint64) ) );
    connect( oln, SIGNAL(sigUrlActivated(QUrl) ), this, SLOT(onUrlActivated(QUrl)) );
	connect( oln, SIGNAL(sigCurrentChanged(quint64)), this, SLOT(onItemActivated(quint64)) );

    Gui2::AutoMenu* pop = new Gui2::AutoMenu( oln->getTree(), true );
    Gui2::AutoMenu* sub = new Gui2::AutoMenu( tr("Item" ), pop );
    pop->addMenu( sub );
    oln->addItemCommands( sub );
    sub = new Gui2::AutoMenu( tr("Text" ), pop );
    pop->addMenu( sub );
    oln->addTextCommands( sub );
	pop->addCommand( tr("Open on Start"), this, SLOT(onAutoStart()) );
	pop->addSeparator();
    oln->addOutlineCommands( pop );
    addTopCommands( pop );

    oln->setOutline( doc );

    d_tab->addDoc( oln->getTree(), doc, Epk::Procs::formatObjectTitle( doc ) );
    oln->gotoItem( select.getOid() );
	QApplication::restoreOverrideCursor();
}

void MainWindow::openScript(const Udb::Obj &doc)
{
	if( doc.isNull() )
		return;
	const int pos = d_tab->showDoc( doc );
	if( pos != -1 )
	{
		return;
	}
	Udb::ScriptSource script(doc);

	Lua::CodeEditor* e = new Lua::CodeEditor( d_tab );

	Gui2::AutoMenu* pop = new Gui2::AutoMenu( e, true );
	pop->addCommand( "&Execute",this, SLOT(handleExecute()), tr("CTRL+E"), true );
	pop->addSeparator();
	pop->addCommand( "Undo", e, SLOT(handleEditUndo()), tr("CTRL+Z"), true );
	pop->addCommand( "Redo", e, SLOT(handleEditRedo()), tr("CTRL+Y"), true );
	pop->addSeparator();
	pop->addCommand( "Cut", e, SLOT(handleEditCut()), tr("CTRL+X"), true );
	pop->addCommand( "Copy", e, SLOT(handleEditCopy()), tr("CTRL+C"), true );
	pop->addCommand( "Paste", e, SLOT(handleEditPaste()), tr("CTRL+V"), true );
	pop->addCommand( "Select all", e, SLOT(handleEditSelectAll()), tr("CTRL+A"), true  );
	pop->addSeparator();
	pop->addCommand( "Find...", e, SLOT(handleFind()), tr("CTRL+F"), true );
	pop->addCommand( "Find again", e, SLOT(handleFindAgain()), tr("F3"), true );
	pop->addCommand( "Replace...", e, SLOT(handleReplace()), tr("CTRL+R"), true );
	pop->addCommand( "&Goto...", e, SLOT(handleGoto()), tr("CTRL+G"), true );
	pop->addSeparator();
	pop->addCommand( "Indent", e, SLOT(handleIndent()) );
	pop->addCommand( "Unindent", e, SLOT(handleUnindent()) );
	pop->addCommand( "Set Indentation Level...", e, SLOT(handleSetIndent()) );
	pop->addSeparator();
	pop->addCommand( "Print...", e, SLOT(handlePrint()) );
	pop->addCommand( "Export PDF...", e, SLOT(handleExportPdf()) );
	addTopCommands( pop );

	QSettings set;
	const QVariant v = set.value( "LuaEditor/Font" );
	if( !v.isNull() )
		e->setFont( v.value<QFont>() );

	e->setText( script.getSource() );
	e->setName( script.getText() );
	e->document()->setModified( false );
	d_tab->addDoc( e, doc, script.getText() );
	connect( e, SIGNAL(blockCountChanged(int)), this, SLOT(saveEditor() ) );
}

void MainWindow::onFldrSelected(const Udb::Obj & o)
{
    if( d_selectLock )
        return;
    d_selectLock = true;
    d_atv->setObj( o );
	d_tv->setObj( o );
	d_rbv->setObj( o );
	pushBack( o );
    d_selectLock = false;
}

void MainWindow::onFldrDblClicked(const Udb::Obj & doc)
{
    if( doc.getType() == Epk::Diagram::TID )
        openEpkDiagram( doc );
	else if( doc.getType() == Oln::Outline::TID )
        openOutline( doc );
	else if( doc.getType() == Udb::ScriptSource::TID )
		openScript( doc );
}

void MainWindow::onFollowOID(quint64 oid)
{
    Udb::Obj obj = d_txn->getObject( oid );
    if( !obj.isNull() )
        onFollowObject( obj );
}

void MainWindow::onUrlActivated(const QUrl & url)
{
    QDesktopServices::openUrl( url );
}

void MainWindow::onSearch()
{
    ENABLED_IF( true );

    d_sv->parentWidget()->show();
    d_sv->parentWidget()->raise();
    d_sv->onNew();
}


