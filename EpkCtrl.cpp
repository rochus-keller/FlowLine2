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

#include "EpkCtrl.h"
#include "EpkView.h"
#include "EpkItemMdl.h"
#include "EpkProcs.h"
#include "EpkItems.h"
#include "EpkObjects.h"
#include "EpkStream.h"
#include <Gui2/UiFunction.h>
#include <QtGui/QGraphicsItem>
#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>
#include <QtGui/QHBoxLayout>
#include <QtGui/QCheckBox>
#include <QtGui/QRadioButton>
#include <QtGui/QGroupBox>
#include <QtGui/QTextEdit>
#include <QtGui/QKeyEvent>
#include <QtCore/QSet>
#include <QtGui/QVBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QListWidget>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QPushButton>
#include <QtGui/QSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <Udb/Database.h>
#include <Udb/Transaction.h>
#include <Udb/Idx.h>
#include <QtGui/QApplication>
#include <QtGui/QClipboard>
#include <QtDebug>
#include "EpkLayouter.h"
//#include "FlowLine2App.h"
using namespace Epk;

const char* EpkCtrl::s_mimeEpkItems = "application/flowline2/epk-items";

static EpkLayouter s_layouter;

class _FlowChartViewTextEdit : public QTextEdit
{
public:
    bool d_ok;
    EpkNode* d_item;

    _FlowChartViewTextEdit(QWidget* p, EpkNode* item ):QTextEdit(p),d_item(item),d_ok(true)
    {
        setWindowFlags( Qt::Popup );
        setText( item->getText() );
        selectAll();
        setAcceptRichText( false );
        //setLineWrapMode( QTextEdit::NoWrap );
    }
    void hideEvent ( QHideEvent * e )
    {
        if( d_ok )
        {
            EpkItemMdl* mdl = dynamic_cast<EpkItemMdl*>( d_item->scene() );
            if( mdl )
                mdl->setItemText( d_item, toPlainText().simplified() );
        }
        QTextEdit::hideEvent( e );
        deleteLater();
    }
    void keyPressEvent ( QKeyEvent * e )
    {
        if( e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return )
            close();
        else if( e->key() == Qt::Key_Escape )
        {
            d_ok = false;
            close();
        }else
            QTextEdit::keyPressEvent( e );
    }
};

EpkCtrl::EpkCtrl( EpkView* view, EpkItemMdl * mdl ):
    QObject( view ),d_mdl(mdl)
{
    Q_ASSERT( view != 0 );
    Q_ASSERT( mdl != 0 );
}

EpkView* EpkCtrl::getView() const
{
    return static_cast<EpkView*>( parent() );
}

EpkCtrl *EpkCtrl::create(QWidget *parent, const Udb::Obj &doc)
{
    EpkView* view = new EpkView( parent );
    EpkItemMdl* mdl = new EpkItemMdl( view );
    //mdl->setChartFont( d_chartFont );
    mdl->setDiagram( doc );
    view->setScene( mdl );
    EpkCtrl* ctrl = new EpkCtrl( view, mdl );
    connect( view, SIGNAL(signalDblClick(QPoint)), ctrl, SLOT(onDblClick(QPoint)) );
    connect( mdl, SIGNAL(signalCreateLink(Udb::Obj,Udb::Obj,QPolygonF)),
             ctrl, SLOT(onCreateLink(Udb::Obj,Udb::Obj,QPolygonF)));
    connect( mdl, SIGNAL(signalCreateSuccLink(Udb::Obj,int,QPointF,QPolygonF)),
             ctrl, SLOT(onCreateSuccLink(Udb::Obj,int,QPointF,QPolygonF)) );
    connect( mdl, SIGNAL( selectionChanged() ), ctrl, SLOT( onSelectionChanged() ) );
    connect( view, SIGNAL(signalClick(QPoint)), ctrl, SLOT(onSelectionChanged()) );
    connect( mdl, SIGNAL(signalDrop(QByteArray,QPointF)),
             ctrl,SLOT(onDrop(QByteArray,QPointF)), Qt::QueuedConnection );
    return ctrl;
}

void EpkCtrl::addCommands(Gui2::AutoMenu *pop)
{
    pop->addCommand( tr( "New Function" ), this, SLOT( onAddFunction() ), tr("CTRL+T"), true );
    pop->addCommand( tr( "New Event" ), this, SLOT( onAddEvent() ), tr("CTRL+M"), true );
    pop->addCommand( tr( "New Connector" ), this, SLOT( onAddConnector() ) );
    pop->addCommand( tr( "New Note" ), this, SLOT( onAddNote() ), tr("CTRL+N"), true );
    pop->addCommand( tr( "New Frame" ), this, SLOT( onAddFrame() ), tr("CTRL+SHIFT+N"), true );
    pop->addCommand( tr("Toggle Function/Event"), this, SLOT(onToggleFuncEvent()) );
	pop->addCommand( tr("Pin Items"), this, SLOT(onPinItems()), tr("CTRL+I"), true );
	pop->addCommand( tr("Unpin Items"), this, SLOT(onUnpinItems()), tr("CTRL+U"), true );
	Gui2::AutoMenu* sub;

    sub = new Gui2::AutoMenu( tr("Set Connector Type" ), pop );
    pop->addMenu( sub );
    sub->addCommand( Procs::formatConnType( Connector::Unspecified ),
                     this, SLOT(onSetConnType()) )->setData( Connector::Unspecified );
    sub->addCommand( Procs::formatConnType( Connector::And ),
                     this, SLOT(onSetConnType()) )->setData( Connector::And );
    sub->addCommand( Procs::formatConnType( Connector::Or ),
                     this, SLOT(onSetConnType()) )->setData( Connector::Or );
    sub->addCommand( Procs::formatConnType( Connector::Xor ),
                     this, SLOT(onSetConnType()) )->setData( Connector::Xor );
    sub->addCommand( Procs::formatConnType( Connector::Start ),
                     this, SLOT(onSetConnType()) )->setData( Connector::Start );
    sub->addCommand( Procs::formatConnType( Connector::Finish ),
                     this, SLOT(onSetConnType()) )->setData( Connector::Finish );

    pop->addSeparator();
    pop->addCommand( tr( "Start Link" ), this, SLOT( onLink() ) );
    pop->addCommand( tr( "New Handle" ), this, SLOT( onInsertHandle() ), tr("Insert"), true );

    pop->addSeparator();
    sub = new Gui2::AutoMenu( tr("Add Existing" ), pop );
    pop->addMenu( sub );
    sub->addCommand( tr( "Hidden Functions/Events..."), this, SLOT(onSelectHiddenSchedObjs()), tr("CTRL+H"), true );
    sub->addCommand( tr( "Linked Functions/Event..."), this, SLOT(onExtendDiagram()), tr("CTRL+E"), true );
    sub->addCommand( tr( "Connecting Functions/Event..."), this, SLOT(onShowShortestPath()), tr("CTRL+SHIFT+E"), true );
    sub->addCommand( tr( "Hidden Flows..."), this, SLOT(onSelectHiddenLinks()), tr("CTRL+SHIFT+H"), true );

    sub = new Gui2::AutoMenu( tr("Select" ), pop );
    pop->addMenu( sub );
    sub->addCommand( tr( "All" ), this, SLOT( onSelectAll() ), tr("CTRL+A"), true );
    sub->addCommand( tr( "Rightwards" ), this, SLOT( onSelectRightward() ), tr("CTRL+SHIFT+Right"), true );
    sub->addCommand( tr( "Leftwards" ), this, SLOT( onSelectLeftward() ), tr("CTRL+SHIFT+Left"), true );
    sub->addCommand( tr( "Upwards" ), this, SLOT( onSelectUpward() ), tr("CTRL+SHIFT+Up"), true );
    sub->addCommand( tr( "Downwards" ), this, SLOT( onSelectDownward() ), tr("CTRL+SHIFT+Down"), true );

    pop->addCommand( tr( "Layout..." ), this, SLOT( onLayout() ), tr("CTRL+SHIFT+L"), true );
    sub = new Gui2::AutoMenu( tr("Direction" ), pop );
    pop->addMenu( sub );
    sub->addCommand( Procs::formatDirection( Function::TopToBottom ),
                     this, SLOT(onSetDirection()) )->setData( Function::TopToBottom );
    sub->addCommand( Procs::formatDirection( Function::LeftToRight ),
                     this, SLOT(onSetDirection()) )->setData( Function::LeftToRight );

    pop->addSeparator();
    pop->addCommand( tr( "Cut..." ), this, SLOT( onCut() ), tr( "CTRL+X" ), true );
    pop->addCommand( tr( "Copy" ), this, SLOT( onCopy() ), tr( "CTRL+C" ), true );
    pop->addCommand( tr( "Paste" ), this, SLOT( onPaste() ), tr( "CTRL+V" ), true );
    pop->addCommand( tr("Edit Attributes..."), this, SLOT(onEditAttrs()), tr("CTRL+Return"), true );

    sub = new Gui2::AutoMenu( tr("Export" ), pop );
    pop->addMenu( sub );
    sub->addCommand( tr( "To File..." ), this, SLOT( onExportToFile() ) );
    sub->addCommand( tr( "To Clipboard" ), this, SLOT( onExportToClipboard() ) );

    pop->addSeparator();

    pop->addCommand( tr( "Remove from diagram..." ), this, SLOT( onRemoveItems() ), tr("CTRL+D"), true );
    pop->addCommand( tr( "Remove all Aliasses..." ), this, SLOT( onRemoveAllAliasses() ) );
    pop->addCommand( tr( "Delete permanently..." ), this, SLOT( onDeleteItems() ), tr("CTRL+SHIFT+D"), true );

    pop->addSeparator();

    pop->addCommand( tr("Show Ident."), this, SLOT(onShowId()) )->setCheckable(true);
    pop->addCommand( tr("Mark Alias"), this, SLOT(onMarkAlias()) )->setCheckable(true);
}

Udb::Obj EpkCtrl::getSingleSelection() const
{
    DiagItem o = d_mdl->getSingleSelection();
    if( !o.isNull() )
        return o.getOrigObject();
    else
        return Udb::Obj();
}

void EpkCtrl::onAddItem( quint32 type, int kind )
{
    ENABLED_IF( !d_mdl->isReadOnly() && d_mdl->getDiagram().getType() != Diagram::TID &&
                   Procs::isValidAggregate( d_mdl->getDiagram().getType(), type ) );

    Udb::Obj diag = d_mdl->getDiagram();
    Udb::Obj obj = Procs::createObject( type, diag );
    if( type == Connector::TID )
        obj.setValue( Connector::AttrConnType, Stream::DataCell().setUInt8(kind) );
    DiagItem item = DiagItem::create( diag, obj, d_mdl->getStart( true ) );
    obj.commit();
    d_mdl->selectObject(item);
}
void EpkCtrl::pasteItemRefs(const QMimeData *data, const QPointF& where )
{
    if( !data->hasFormat( Udb::Obj::s_mimeObjectRefs ) )
        return;
    Udb::Obj diagram = d_mdl->getDiagram();
    QList<Udb::Obj> objs = Udb::Obj::readObjectRefs( data, diagram.getTxn() );
    QList<Udb::Obj> done = Procs::addItemsToDiagram( diagram, objs, where );
    diagram.commit();
    Procs::addItemLinksToDiagram( diagram, done );
    diagram.commit();
}

void EpkCtrl::onAddFunction()
{
    onAddItem( Function::TID );
}

void EpkCtrl::onAddEvent()
{
    onAddItem( Event::TID );
}

void EpkCtrl::onAddConnector()
{
    onAddItem( Connector::TID );
}

void EpkCtrl::onAddNote()
{
    ENABLED_IF( !d_mdl->isReadOnly() ); // Notes dürfen überall erzeugt werden!

    QList<Udb::Obj> items = d_mdl->getMultiSelection(true,false,false);
    Udb::Obj diag = d_mdl->getDiagram();
    DiagItem item = DiagItem::createKind( diag, DiagItem::Note, d_mdl->getStart( true ) );
    if( items.size() == 1 && items.first().getValue( DiagItem::AttrKind ).getUInt8() == DiagItem::Plain )
        item.setPinnedTo( items.first() );
    item.commit();
    d_mdl->selectObject(item);
}

void EpkCtrl::onAddFrame()
{
    ENABLED_IF( !d_mdl->isReadOnly() ); // Frames dürfen auch auf Diagram::TID erzeugt werden!

    Udb::Obj diag = d_mdl->getDiagram();
    DiagItem item = DiagItem::createKind( diag, DiagItem::Frame, d_mdl->getStart( true ) );
    item.commit();
    d_mdl->selectObject(item);
}

void EpkCtrl::onSelectAll()
{
    ENABLED_IF( true );
    QPainterPath selectionArea;
    selectionArea.addRect( d_mdl->sceneRect() );
    d_mdl->setSelectionArea( selectionArea );
}

void EpkCtrl::onSelectRightward()
{
    ENABLED_IF( true );
    QRectF r = d_mdl->sceneRect();
    QPointF p = d_mdl->getStart();
    r.setLeft( p.x() );
    QPainterPath pp;
    pp.addRect( r );
    d_mdl->setSelectionArea( pp, Qt::ContainsItemShape );
}

void EpkCtrl::onSelectUpward()
{
    ENABLED_IF( true );
    QRectF r = d_mdl->sceneRect();
    QPointF p = d_mdl->getStart();
    r.setBottom( p.y() );
    QPainterPath pp;
    pp.addRect( r );
    d_mdl->setSelectionArea( pp, Qt::ContainsItemShape );
}

void EpkCtrl::onSelectLeftward()
{
    ENABLED_IF( true );
    QRectF r = d_mdl->sceneRect();
    QPointF p = d_mdl->getStart();
    r.setRight( p.x() );
    QPainterPath pp;
    pp.addRect( r );
    d_mdl->setSelectionArea( pp, Qt::ContainsItemShape );
}

void EpkCtrl::onSelectDownward()
{
    ENABLED_IF( true );
    QRectF r = d_mdl->sceneRect();
    QPointF p = d_mdl->getStart();
    r.setTop( p.y() );
    QPainterPath pp;
    pp.addRect( r );
    d_mdl->setSelectionArea( pp, Qt::ContainsItemShape );
}

void EpkCtrl::onRemoveItems()
{
    ENABLED_IF( !d_mdl->isReadOnly() &&
        !d_mdl->selectedItems().isEmpty() );
    const int res = QMessageBox::warning( getView(), tr("Remove Items from Diagram"),
        tr("Do you really want to remove the %1 selected items? This cannot be undone." ).
        arg( d_mdl->selectedItems().size() ),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
    if( res == QMessageBox::No )
        return;
    d_mdl->removeSelectedItems();
}

void EpkCtrl::onInsertHandle()
{
    ENABLED_IF( !d_mdl->isReadOnly() &&
        d_mdl->selectedItems().size() == 1 &&
        d_mdl->selectedItems().first()->type() == EpkNode::_Flow );

    d_mdl->insertHandle();
}

void EpkCtrl::onExportToClipboard()
{
    ENABLED_IF( true );
    d_mdl->exportPng( QString() );
}

void EpkCtrl::onExportToFile()
{
    ENABLED_IF( true );
    QString filter;
	QString path = QFileDialog::getSaveFileName( getView(), tr( "Export Diagram - FlowLine" ),
        d_mdl->getDiagram().getString( Root::AttrText ),
        tr("*.png;;*.pdf;;*.html;;*.flnx;;*.svg"), &filter, QFileDialog::DontUseNativeDialog );
    if( path.isEmpty() )
        return;

    if( filter == "*.png" )
    {
        if( !path.endsWith( ".png" ) )
            path += ".png";
        d_mdl->exportPng( path );
    }else if( filter == "*.pdf" )
    {
        if( !path.endsWith( ".pdf" ) )
            path += ".pdf";
        d_mdl->exportPdf( path );
    }else if( filter == "*.html" )
    {
        if( !path.endsWith( ".html" ) )
            path += ".html";
        d_mdl->exportHtml( path );
    }else if( filter == "*.svg" )
    {
        if( !path.endsWith( ".svg" ) )
            path += ".svg";
        d_mdl->exportSvg( path );
    }else if( filter == "*.flnx" )
    {
        if( !path.endsWith( ".flnx" ) )
            path += ".flnx";
        QFile f( path );
        if( !f.open( QIODevice::WriteOnly ) )
        {
			QMessageBox::critical( getView(), tr("Export Process - FlowLine"),
                tr("Cannot open file for writing!") );
            return;
        }
        Stream::DataWriter out( &f );
        Epk::EpkStream::exportProc( out, d_mdl->getDiagram() );
        d_mdl->getDiagram().getTxn()->commit();  // Wegen UUID
    }
}

void EpkCtrl::onLink()
{
    ENABLED_IF( d_mdl->canStartLink() );
    d_mdl->startLink();
}

void EpkCtrl::onCopy()
{
    ENABLED_IF( !d_mdl->selectedItems().isEmpty() );

    QList<Udb::Obj> sel = d_mdl->getMultiSelection();

    QMimeData* mime = new QMimeData();
    writeItems( mime, sel );
    QApplication::clipboard()->setMimeData( mime );
}

void EpkCtrl::onCut()
{
    ENABLED_IF( !d_mdl->isReadOnly() && !d_mdl->selectedItems().isEmpty() );

    const int res = QMessageBox::warning( getView(), tr("Cut Items from Diagram"),
        tr("Do you really want to remove the %1 selected items? This cannot be undone." ).
        arg( d_mdl->selectedItems().size() ),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
    if( res == QMessageBox::No )
        return;
    QList<Udb::Obj> sel = d_mdl->getMultiSelection();

    QMimeData* mime = new QMimeData();
    writeItems( mime, sel );
    QApplication::clipboard()->setMimeData( mime );
    d_mdl->removeSelectedItems();
}

void EpkCtrl::onPaste()
{
    ENABLED_IF( QApplication::clipboard()->mimeData()->hasFormat( Udb::Obj::s_mimeObjectRefs ) ||
                QApplication::clipboard()->mimeData()->hasFormat( s_mimeEpkItems ) );

    if( QApplication::clipboard()->mimeData()->hasFormat( s_mimeEpkItems ) )
    {
        QList<Udb::Obj> objs = readItems( QApplication::clipboard()->mimeData() );
        adjustTo( objs, d_mdl->getStart( true ) );
        d_mdl->getDiagram().getTxn()->commit();
    }else if( QApplication::clipboard()->mimeData()->hasFormat( Udb::Obj::s_mimeObjectRefs ) )
    {
        pasteItemRefs( QApplication::clipboard()->mimeData(), d_mdl->getStart( true ) );
    }
}

void EpkCtrl::onShowId()
{
    CHECKED_IF( true, d_mdl->isShowId() );
    d_mdl->setShowId( !d_mdl->isShowId() );
    getView()->viewport()->update();
}

void EpkCtrl::onMarkAlias()
{
    CHECKED_IF( true, d_mdl->isMarkAlias() );
    d_mdl->setMarkAlias( !d_mdl->isMarkAlias() );
    getView()->viewport()->update();
}

void EpkCtrl::onDeleteItems()
{
    ENABLED_IF( !d_mdl->isReadOnly() && !d_mdl->selectedItems().isEmpty() );
    const int res = QMessageBox::warning( getView(), tr("Delete Items from Database"),
        tr("Do you really want to permanently delete the %1 selected items? This cannot be undone." ).
        arg( d_mdl->selectedItems().size() ),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
    if( res == QMessageBox::No )
        return;
    QList<Udb::Obj> sel = d_mdl->getMultiSelection();
    foreach( Udb::Obj o, sel )
    {
        Udb::Obj orig = o.getValueAsObj( DiagItem::AttrOrigObject );
        Procs::erase( orig );
    }
    d_mdl->getDiagram().getTxn()->commit();
}

void EpkCtrl::onLayout()
{
    ENABLED_IF( !d_mdl->isReadOnly() && !d_mdl->getDiagram().isNull() );
    //Udb::Obj diagram = d_mdl->getDiagram();

    if( !s_layouter.prepareEngine( getView() ) )
        return;

    QMessageBox msg( QMessageBox::Warning, tr("Layout Diagram - FlowLine"),
                     tr("Do you really want to relayout the diagram? This cannot be undone." ),
                     QMessageBox::NoButton);
    msg.setDefaultButton( msg.addButton( QMessageBox::Yes ) );
    msg.addButton( QMessageBox::Cancel );
    const int res = msg.exec();
    if( res == QMessageBox::Cancel )
        return;

    QApplication::setOverrideCursor( Qt::WaitCursor );
    QList<Udb::Obj> sel = d_mdl->getMultiSelection();
    doLayout();
    d_mdl->selectObjects( sel );
    QApplication::restoreOverrideCursor();
    //d_mdl->fitSceneRect(true); // führt zu Crash wenn man anschliessend ein Objekt bewegt
}

void EpkCtrl::doLayout()
{
    // Es ist besser, wenn während dem Layout das Diagramm nicht dargestellt wird.
    // Aus irgendwelchen Gründen werden die neuen Positionen ansonsten nicht angezeigt.
    Function diagram = d_mdl->getDiagram();
    d_mdl->setDiagram( Udb::Obj() );
    if( !s_layouter.layoutDiagram( diagram, false, diagram.getDirection() == Function::TopToBottom ) )
    {
        diagram.getTxn()->rollback();
        d_mdl->setDiagram( diagram );
		QMessageBox::critical( getView(), tr("Layout Diagram Error - FlowLine" ),
                              s_layouter.getErrors().join( "; " ) );
        return;
    } // else
    diagram.commit();
    d_mdl->setDiagram( diagram );
}

void EpkCtrl::onSetConnType()
{
    Connector o = getSingleSelection();
    CHECKED_IF( !d_mdl->isReadOnly() && o.getType() == Connector::TID,
                Gui2::UiFunction::me()->data().toInt() == o.getConnType() );

    if( Gui2::UiFunction::me()->data().toInt() == o.getConnType() )
        return;
    o.setConnType( Gui2::UiFunction::me()->data().toInt() );
    o.commit();
}

void EpkCtrl::onToggleFuncEvent()
{
    Udb::Obj doc = getSingleSelection();
    const quint32 type = doc.getType();
    ENABLED_IF( type == Function::TID || type == Event::TID );
    if( type == Function::TID )
        Procs::retypeObject( doc, Event::TID );
    else
        Procs::retypeObject( doc, Function::TID );
    doc.commit();
}

void EpkCtrl::onExtendDiagram()
{
    ENABLED_IF(true);

    QDialog dlg( getView() );
	dlg.setWindowTitle( tr("Add Linked Tasks/Milestones - FlowLine") );
    QVBoxLayout vbox( &dlg );
    vbox.addWidget( new QLabel(tr("Please set the following parameters to extend the diagram:"), &dlg ) );
    QHBoxLayout hbox;
    vbox.addLayout( &hbox );
    QSpinBox spin( &dlg );
    spin.setRange(1,255);
    spin.setValue(1);
    hbox.addWidget( &spin );
    hbox.addWidget( new QLabel( tr("How many levels to extend"), &dlg ) );
    QCheckBox succ( tr("Extend successors"), &dlg );
    succ.setChecked( true );
    vbox.addWidget( &succ );
    QCheckBox pred( tr("Extend predecessors"), &dlg );
    pred.setChecked( true );
    vbox.addWidget( &pred );
    QCheckBox layout( tr("Layout diagram"), &dlg );
    layout.setChecked( true );
    vbox.addWidget( &layout );
    QDialogButtonBox bb(QDialogButtonBox::Ok
        | QDialogButtonBox::Cancel, Qt::Horizontal, &dlg );
    vbox.addWidget( &bb );
    connect( &bb, SIGNAL(accepted()), &dlg, SLOT(accept()));
    connect( &bb, SIGNAL(rejected()), &dlg, SLOT(reject()));
    if( dlg.exec() == QDialog::Rejected )
        return;

    if( layout.isChecked() && !s_layouter.prepareEngine( getView() ) )
        return;

    QList<Udb::Obj> sel = d_mdl->getMultiSelection(); // DiagItem

    QList<Udb::Obj> objs;
    foreach( Udb::Obj item, sel )
    {
        Udb::Obj orig = item.getValueAsObj( DiagItem::AttrOrigObject );
        if( Procs::isValidAggregate( Function::TID, orig.getType() ) )
            objs.append( orig );
    }
    Udb::Obj diagram = d_mdl->getDiagram();
    if( objs.isEmpty() )
        objs = Procs::findAllItemOrigObjs( diagram, true, false );
    QApplication::setOverrideCursor( Qt::WaitCursor );
    objs = Procs::findExtendedSchedObjs( objs, spin.value(), succ.isChecked(), pred.isChecked()  );
    objs = Procs::addItemsToDiagram( diagram, objs, QPointF(0,0) );
    diagram.commit();
	// hier gleich auch die Links einfügen zu den Elementen, die schon im Diagramm sind.
    Procs::addItemLinksToDiagram( diagram, objs );
    diagram.commit();
    if( layout.isChecked() )
        doLayout();
    d_mdl->selectObjects( objs );
    QApplication::restoreOverrideCursor();
}


void EpkCtrl::onShowShortestPath()
{
    QList<Udb::Obj> sel = d_mdl->getMultiSelection( true, false, false );
    ENABLED_IF( sel.size() == 2 );

    Udb::Obj start = sel.first().getValueAsObj( DiagItem::AttrOrigObject );
    Udb::Obj goal = sel.last().getValueAsObj( DiagItem::AttrOrigObject );

	QMessageBox msg( QMessageBox::Warning, tr("Add Connecting Functions/Events - FlowLine"),
                     tr("Do you also want to relayout the diagram?" ),
                     QMessageBox::NoButton);
    msg.setDefaultButton( msg.addButton( QMessageBox::Yes ) );
    msg.addButton( QMessageBox::No );
    msg.addButton( QMessageBox::Cancel );
    const int res = msg.exec();
    if( res == QMessageBox::Cancel )
        return;

    if( res == QMessageBox::Yes && !s_layouter.prepareEngine( getView() ) )
        return;


    QApplication::setOverrideCursor( Qt::WaitCursor );
    QList<Udb::Obj> objs = Procs::findShortestPath( start, goal );
    if( objs.isEmpty() )
    {
        QApplication::restoreOverrideCursor();
        return;
    }
    Udb::Obj diagram = d_mdl->getDiagram();
    objs = Procs::addItemsToDiagram( diagram, objs, QPointF(0,0) );
    diagram.commit();
    // hier gleich auch die Links einfügen zu dem Zeug, das schon im Diagramm ist.
    Procs::addItemLinksToDiagram( diagram, objs );
    diagram.commit();
    if( res == QMessageBox::Yes )
        doLayout();
    d_mdl->selectObjects( objs );
    QApplication::restoreOverrideCursor();

}

void EpkCtrl::onRemoveAllAliasses()
{
    ENABLED_IF( true );

    QList<Udb::Obj> aliasses = Procs::findAllAliasses( d_mdl->getDiagram() );
    const int res = QMessageBox::warning( getView(), tr("Remove Aliasses from Diagram"),
        tr("Do you really want to remove the %1 aliasses? This cannot be undone." ).
        arg( aliasses.size() ),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
    if( res == QMessageBox::No )
        return;
    foreach( Udb::Obj o, aliasses )
    {
        Q_ASSERT( o.getType() == DiagItem::TID );
		Procs::erase( o );
    }
    d_mdl->getDiagram().getTxn()->commit();
}

void EpkCtrl::onSetDirection()
{
    Function diag = d_mdl->getDiagram();
    const quint32 dir = Gui2::UiFunction::me()->data().toUInt();
    CHECKED_IF( !diag.isNull(), !diag.isNull() && diag.getDirection() == dir );

    diag.setDirection( dir );
    diag.commit();
}

void EpkCtrl::onEditAttrs()
{
    Udb::Obj doc = getSingleSelection();
    ENABLED_IF( doc.getType() == Function::TID || doc.getType() == Event::TID );

    ObjAttrDlg dlg( getView() );
    dlg.edit( doc );
}

static inline bool _xor( bool a, bool b ) { return ( a || b ) && !( a && b ); }

void EpkCtrl::onPinItems()
{
    QList<Udb::Obj> items = d_mdl->getMultiSelection(true,false,false);
	QList<Udb::Obj> notes, frames, plains;

	foreach( DiagItem item, items )
	{
		switch( item.getKind() )
		{
		case DiagItem::Note:
			notes.append( item );
			break;
		case DiagItem::Frame:
			frames.append( item );
			break;
		case DiagItem::Plain:
			plains.append( item );
			break;
		}
	}
	const bool pinNotes = ( notes.size() > 0 && _xor( frames.size() == 1, plains.size() == 1 ) );
	const bool pinPlains = ( plains.size() > 0 && frames.size() == 1 && notes.size() == 0 );

	ENABLED_IF( !d_mdl->isReadOnly() && ( pinNotes || pinPlains ) );

	if( pinNotes )
	{
		DiagItem target;
		if( !frames.isEmpty() )
			target = frames.first();
		else
			target = plains.first();
		foreach( DiagItem item, notes )
			item.setPinnedTo( target );
		target.commit();
	}else if( pinPlains )
	{
		foreach( DiagItem item, plains )
			item.setPinnedTo( frames.first() );
		frames.first().commit();
	}
}

void EpkCtrl::onUnpinItems()
{
	QList<Udb::Obj> items = d_mdl->getMultiSelection(true,false,false);
	QList<Udb::Obj> notes, plains;

	foreach( DiagItem item, items )
	{
		switch( item.getKind() )
		{
		case DiagItem::Note:
			notes.append( item );
			break;
		case DiagItem::Plain:
			plains.append( item );
			break;
		}
	}
	const bool unpinNotes = ( notes.size() > 0 && plains.size() == 0 );
	const bool unpinPlains = ( plains.size() > 0 && notes.size() == 0 );

	// Hier sind also alle items Notes oder genau eines davon Plain
	ENABLED_IF( !d_mdl->isReadOnly() && ( unpinNotes || unpinPlains ) );

	if( unpinNotes )
	{
		foreach( DiagItem item, notes )
			item.setPinnedTo( Udb::Obj() );
		notes.first().commit();
	}else if( unpinPlains )
	{
		foreach( DiagItem item, plains )
			item.setPinnedTo( Udb::Obj() );
		plains.first().commit();
	}
}

void EpkCtrl::onSelectHiddenSchedObjs()
{
    ENABLED_IF( !d_mdl->getDiagram().isNull() );

    Udb::Obj diagram = d_mdl->getDiagram();
    QSet<Udb::Obj> hiddens = Procs::findHiddenSchedObjs( diagram );

    QDialog dlg( getView() );
	dlg.setWindowTitle( tr("Add Hidden Tasks/Milestones - FlowLine") );
    QVBoxLayout vbox( &dlg );
    vbox.addWidget( new QLabel( tr("Select one or more Tasks/Milestones:"), &dlg ) );
    QListWidget list( &dlg );
    list.setAlternatingRowColors( true );
    list.setSelectionMode( QAbstractItemView::ExtendedSelection );
    vbox.addWidget( &list );
    QDialogButtonBox bb(QDialogButtonBox::Ok
        | QDialogButtonBox::Cancel, Qt::Horizontal, &dlg );
    vbox.addWidget( &bb );
    connect(&bb, SIGNAL(accepted()), &dlg, SLOT(accept()));
    connect(&bb, SIGNAL(rejected()), &dlg, SLOT(reject()));
    connect(&list, SIGNAL( itemDoubleClicked ( QListWidgetItem * ) ), &dlg, SLOT( accept() ) );

    foreach( Udb::Obj o, hiddens )
    {
        QListWidgetItem* lwi = new QListWidgetItem( &list );
        lwi->setText( Procs::formatObjectTitle( o ) );
        lwi->setData( Qt::UserRole, o.getOid() );
        lwi->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
    }
    list.sortItems();

    QPointF p = d_mdl->getStart( true );
    if( dlg.exec() == QDialog::Accepted )
    {
        QList<Udb::Obj> done;
        foreach( QListWidgetItem* lwi, list.selectedItems() )
        {
            Udb::Obj hidden = diagram.getObject( lwi->data( Qt::UserRole ).toULongLong() );
            Q_ASSERT( !hidden.isNull() );
            done.append( hidden );
            DiagItem::create( diagram, hidden, p );
            p += QPointF( DiagItem::s_rasterX, DiagItem::s_rasterY );
        }
        diagram.commit();
        // hier gleich auch die Links einfügen zu dem Zeug, das schon im Diagramm ist.
        Procs::addItemLinksToDiagram( diagram, done );
        diagram.commit();
    }
}

void EpkCtrl::onSelectHiddenLinks()
{
    ENABLED_IF( !d_mdl->getDiagram().isNull() );

    Udb::Obj diagram = d_mdl->getDiagram();
    QList<Udb::Obj> startset;
    foreach( Udb::Obj o, d_mdl->getMultiSelection() )
    {
        if( o.getType() == DiagItem::TID )
        {
            Udb::Obj orig = o.getValueAsObj( DiagItem::AttrOrigObject );
            if( Procs::isValidAggregate( Function::TID, orig.getType() ) )
                startset.append( orig );
        }
    }
    QList<Udb::Obj> hiddenLinks = Procs::findHiddenLinks( diagram, startset );

    QDialog dlg( getView() );
	dlg.setWindowTitle( tr("Select hidden links - FlowLine") );
    QVBoxLayout vbox( &dlg );
    vbox.addWidget( new QLabel( tr("Select one or more Links:"), &dlg ) );
    QListWidget list( &dlg );
    list.setAlternatingRowColors( true );
    list.setSelectionMode( QAbstractItemView::ExtendedSelection );
    vbox.addWidget( &list );
    QDialogButtonBox bb(QDialogButtonBox::Ok
        | QDialogButtonBox::Cancel, Qt::Horizontal, &dlg );
    vbox.addWidget( &bb );
    connect(&bb, SIGNAL(accepted()), &dlg, SLOT(accept()));
    connect(&bb, SIGNAL(rejected()), &dlg, SLOT(reject()));
    connect(&list, SIGNAL( itemDoubleClicked ( QListWidgetItem * ) ), &dlg, SLOT( accept() ) );

    foreach( Udb::Obj link, hiddenLinks )
    {
        QListWidgetItem* lwi = new QListWidgetItem( &list );
        lwi->setText( QString( "%1 %2 %3" ).
                      arg( Procs::formatObjectId( link.getValueAsObj( ConFlow::AttrPred ) ) ).
                      arg( QChar( 0x2192 ) ).
                      arg( Procs::formatObjectId( link.getValueAsObj( ConFlow::AttrSucc ) ) ) );
        lwi->setToolTip( QString( "%1\n%2\n%3" ).
                         arg( Procs::formatObjectTitle( link.getValueAsObj( ConFlow::AttrPred ) ) ).
                         arg( QChar( 0x2192 ) ).
                         arg( Procs::formatObjectTitle( link.getValueAsObj( ConFlow::AttrSucc ) ) ) );
        lwi->setData( Qt::UserRole, link.getOid() );
        lwi->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
    }
    list.sortItems();
    if( dlg.exec() == QDialog::Accepted )
    {
        foreach( QListWidgetItem* lwi, list.selectedItems() )
        {
            Udb::Obj link = diagram.getObject( lwi->data( Qt::UserRole ).toULongLong() );
            Q_ASSERT( !link.isNull() );
            DiagItem::create( diagram, link );
        }
        diagram.commit();
    }
}

void EpkCtrl::onDblClick(const QPoint & pos)
{
    if( !d_mdl->isReadOnly() )
    {
        EpkNode* i = dynamic_cast<EpkNode*>( d_mdl->itemAt( getView()->mapToScene( pos ) ) );
        if( i && i->hasItemText() ) // Auch Aliasse änderbar
        {
            _FlowChartViewTextEdit* edit = new _FlowChartViewTextEdit( 0, i );
            edit->resize( DiagItem::s_boxWidth * 1.5, DiagItem::s_boxHeight * 1.5 );
            edit->move( getView()->viewport()->mapToGlobal( pos ) -
                QPoint( DiagItem::s_boxWidth * 0.5, DiagItem::s_boxHeight * 0.5 ) );
            edit->show();
            edit->setFocus();
        }
    }
}

void EpkCtrl::onCreateLink(const Udb::Obj &pred, const Udb::Obj &succ, const QPolygonF &path)
{
    DiagItem pdmItem = DiagItem::createLink( d_mdl->getDiagram(), pred, succ );
    pdmItem.setNodeList( path );
    pdmItem.commit();
}

void EpkCtrl::onCreateSuccLink(const Udb::Obj &pred, int type, const QPointF &pos, const QPolygonF &path)
{
    int kind = 0;
    switch( type )
    {
    case EpkItemMdl::_Function:
        type = Function::TID;
        break;
    case EpkItemMdl::_Event:
        type = Event::TID;
        break;
    case EpkItemMdl::_AndConn:
        type = Connector::TID;
        kind = Connector::And;
        break;
    case EpkItemMdl::_OrConn:
        type = Connector::TID;
        kind = Connector::Or;
        break;
    case EpkItemMdl::_XorConn:
        type = Connector::TID;
        kind = Connector::Xor;
        break;
    case EpkItemMdl::_StartConn:
        type = Connector::TID;
        kind = Connector::Start;
        break;
    case EpkItemMdl::_FinishConn:
        type = Connector::TID;
        kind = Connector::Finish;
        break;
    default:
        qWarning() << "EpkCtrl::onCreateSuccLink: unknown type" << type;
        return;
    }

    Udb::Obj diag = d_mdl->getDiagram();
    Udb::Obj succ = Procs::createObject( type, diag );
    if( type == Connector::TID )
        succ.setValue( Connector::AttrConnType, Stream::DataCell().setUInt8(kind) );
    DiagItem obj = DiagItem::create( diag, succ, pos );
    DiagItem pdmItem = DiagItem::createLink( diag, pred, succ );
    pdmItem.setNodeList( path );
    pdmItem.commit();
    d_mdl->selectObject(obj);
}

void EpkCtrl::onSelectionChanged()
{
    emit signalSelectionChanged();
}

void EpkCtrl::onDrop(QByteArray data, QPointF where)
{
    // Dieser Umweg ist nötig, damit onDrop asynchron aufgerufen werden kann.
    QMimeData mime;
    mime.setData( Udb::Obj::s_mimeObjectRefs, data );
    pasteItemRefs( &mime, where );
}

void EpkCtrl::adjustTo( const QList<Udb::Obj>& pdmItems, const QPointF& to )
{
    QPainterPath bound;
    int i;
    for( i = 0; i < pdmItems.size(); i++ )
    {
        DiagItem pdmItem = pdmItems[i];
        if( !pdmItem.hasNodeList() )
            bound.addRect( pdmItem.getBoundingRect() );
    }
    QPointF off = to - bound.boundingRect().topLeft();
    for( i = 0; i < pdmItems.size(); i++ )
    {
        DiagItem pdmItem = pdmItems[i];
        if( pdmItem.hasNodeList() )
        {
            // Das ist ein Link
            QPolygonF p = pdmItem.getNodeList();
            for( int j = 0; j < p.size(); j++ )
                p[j] = EpkItemMdl::rastered( p[j] + off );
            pdmItem.setNodeList( p );
        }else
        {
            // Das ist ein Task/Milestone
            pdmItem.setPos( EpkItemMdl::rastered( pdmItem.getPos() + off ) );
        }
    }
}

void EpkCtrl::writeItems(QMimeData *data, const QList<Udb::Obj> & items)
{
    if( items.isEmpty() )
        return;
    Q_ASSERT( !items.first().isNull() );
    Stream::DataWriter out1;
    QList<Udb::Obj> origs = DiagItem::writeItems( items, out1 );
    data->setData( QLatin1String( s_mimeEpkItems ), out1.getStream() );
    Udb::Obj::writeObjectRefs( data, origs );
	Udb::Obj::writeObjectUrls( data, origs, Udb::ContentObject::AttrIdent, Udb::ContentObject::AttrText );
}

QList<Udb::Obj> EpkCtrl::readItems(const QMimeData *data )
{
    if( !data->hasFormat( QLatin1String( s_mimeEpkItems ) ) )
        return QList<Udb::Obj>();
    Stream::DataReader r( data->data(QLatin1String( s_mimeEpkItems )) );
    return DiagItem::readItems( d_mdl->getDiagram(), r );
}

const Udb::Obj &EpkCtrl::getDiagram() const
{
    return d_mdl->getDiagram();
}

bool EpkCtrl::focusOn(const Udb::Obj & o)
{
    if( o.isNull() || !getView()->isIdle() )
        return d_mdl->contains( o.getOid() );
    QGraphicsItem* i = d_mdl->selectObject( o );
    if( i )
    {
        getView()->ensureVisible( i );
        return true;
    }else
        return false;
}

class _AttrTextEdit : public QTextEdit
{
public:
    _AttrTextEdit(){}
    void keyPressEvent ( QKeyEvent * e )
    {
        if( e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return )
            e->ignore();
        else
            QTextEdit::keyPressEvent( e );
    }
    void focusInEvent(QFocusEvent *e)
    {
        QTextEdit::focusInEvent(e);
        selectAll();
    }
};

bool ObjAttrDlg::edit(Udb::Obj & obj)
{
    if( obj.isNull() )
        return false;
    Udb::ContentObject co = obj;
    setWindowTitle( tr("Edit '%1' %2 - FlowLine").arg( Procs::prettyTypeName(obj.getType()) ).
                    arg( Procs::formatObjectId(obj) ) );
    QVBoxLayout vbox(this);
    QFormLayout form;
    vbox.addLayout(&form);
    QLineEdit id;
    id.setText(co.getAltIdent());
    _AttrTextEdit text;
    text.setAcceptRichText(false);
    text.setFixedHeight(DiagItem::s_boxHeight);
    text.setText(co.getText());
    form.addRow(tr("Custom ID:"), &id );
    form.addRow(tr("Text:"), &text );
    form.addRow(tr("Internal ID:"), new QLabel(co.getIdent(),this));
    form.addRow(tr("Created On:"), new QLabel(Procs::prettyDateTime(co.getCreatedOn()),this));
    form.addRow(tr("Modified On:"), new QLabel(Procs::prettyDateTime(co.getModifiedOn()),this));
    vbox.addStretch();
    QDialogButtonBox bb(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this );
    vbox.addWidget(&bb);
    connect(&bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(&bb, SIGNAL(rejected()), this, SLOT(reject()));
    setMinimumWidth(300);

    if( exec() == QDialog::Accepted )
    {
        bool changed = false;
        changed |= id.text() != co.getAltIdent();
        changed |= text.toPlainText() != co.getText();
        if( changed )
        {
            co.setAltIdent( id.text() );
            co.setText( text.toPlainText().simplified() );
            co.setModifiedOn();
            co.commit();
        }
        return true;
    }else
        return false;
}
