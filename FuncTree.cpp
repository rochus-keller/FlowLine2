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

#include "FuncTree.h"
#include "EpkObjects.h"
#include "EpkProcs.h"
#include "EpkStream.h"
#include "EpkCtrl.h"
#include <Oln2/OutlineStream.h>
#include <QtGui/QTreeView>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QApplication>
#include <QClipboard>
using namespace Fln;

const char* FuncTreeCtrl::s_mimeFuncTree = "application/flowline/functree-data";

FuncTreeCtrl::FuncTreeCtrl(QTreeView *tree, Wt::GenericMdl *mdl):
    GenericCtrl(tree, mdl)
{
}

FuncTreeCtrl *FuncTreeCtrl::create(QWidget *parent, const Udb::Obj &root)
{
    QTreeView* tree = new QTreeView( parent );
    tree->setHeaderHidden( true );
    tree->setSelectionMode( QAbstractItemView::ExtendedSelection );
    tree->setEditTriggers( QAbstractItemView::SelectedClicked );
    tree->setDragDropMode( QAbstractItemView::DragDrop );
    tree->setDragEnabled( true );
    tree->setDragDropOverwriteMode( false );
    tree->setAlternatingRowColors(true);
    tree->setIndentation( 15 );
    tree->setExpandsOnDoubleClick( false );

    FuncTreeMdl* mdl = new FuncTreeMdl( tree );
    tree->setModel( mdl );

    FuncTreeCtrl* ctrl = new FuncTreeCtrl(tree, mdl);
    ctrl->setRoot( root );

    connect( mdl, SIGNAL(signalNameEdited(QModelIndex,QVariant)),
             ctrl, SLOT(onNameEdited(QModelIndex,QVariant) ) );
    connect( mdl, SIGNAL(signalMoveTo(QList<Udb::Obj>,QModelIndex,int)),
             ctrl, SLOT(onMoveTo(QList<Udb::Obj>,QModelIndex,int)) );
    connect( tree->selectionModel(), SIGNAL( selectionChanged ( const QItemSelection &, const QItemSelection & ) ),
        ctrl, SLOT( onSelectionChanged() ) );
    connect( tree, SIGNAL(clicked(QModelIndex)), ctrl, SLOT( onSelectionChanged() ) );
    connect( tree, SIGNAL(doubleClicked(QModelIndex)), ctrl, SLOT(onDoubleClicked(QModelIndex)) );

    return ctrl;
}

void FuncTreeCtrl::addCommands(Gui2::AutoMenu * pop)
{
    pop->addAction( tr("Expand All"), getTree(), SLOT( expandAll() ) );
    pop->addAction( tr("Expand Selected"), this, SLOT( onExpandSelected() ) );
    pop->addAction( tr("Collapse All"), getTree(), SLOT( collapseAll() ) );
    pop->addSeparator();
    pop->addCommand( tr("Move up"), this, SLOT(onMoveUp()), tr("SHIFT+Up"), true );
    pop->addCommand( tr("Move down"), this, SLOT(onMoveDown()), tr("SHIFT+Down"), true );
    pop->addSeparator();
    pop->addCommand( tr("New Function"), this, SLOT(onAddFunction()), tr("CTRL+R"), true );
    pop->addCommand( tr("New Domain"), this, SLOT(onAddDomain()), tr("CTRL+SHIFT+R"), true );
    pop->addCommand( tr("New Sibling"), this, SLOT(onAddNext()), tr("CTRL+N"), true );
    pop->addCommand( tr("Import..."), this, SLOT(onImport()) );
    pop->addSeparator();
    pop->addCommand( tr("Copy"), this, SLOT( onCopy() ), tr("CTRL+C"), true );
    pop->addCommand( tr("Paste"), this, SLOT( onPaste() ), tr("CTRL+V"), true );
    pop->addSeparator();
    pop->addCommand( tr("Edit Name"), this, SLOT( onEditName() ) );
    pop->addCommand( tr("Edit Attributes..."), this, SLOT(onEditAttrs()), tr("CTRL+Return"), true );
    pop->addCommand( tr("Toggle Function/Domain"), this, SLOT(onToggleType()), tr("CTRL+T"), true );
    pop->addCommand( tr("Delete Items..."), this, SLOT( onDeleteItems() ), tr("CTRL+D"), true );
}

void FuncTreeCtrl::onAddFunction()
{
    add( Epk::Function::TID );
}

void FuncTreeCtrl::onAddDomain()
{
    add( Epk::FuncDomain::TID );
}

void FuncTreeCtrl::onAddNext()
{
    Udb::Obj doc = getSelectedObject();
    ENABLED_IF( !doc.isNull() );

    Udb::Obj o = Epk::Procs::createObject( doc.getType(), doc.getParent(), doc.getNext() );
    o.commit();
    focusOn( o, true );
}

void FuncTreeCtrl::onToggleType()
{
    Udb::Obj doc = getSelectedObject();
    const Udb::Atom type = doc.getType();
    ENABLED_IF( ( type == Epk::Function::TID && Epk::Procs::canConvert( doc, Epk::FuncDomain::TID ) ) ||
                ( type == Epk::FuncDomain::TID && Epk::Procs::canConvert( doc, Epk::Function::TID ) ) )
    if( type == Epk::Function::TID )
        Epk::Procs::retypeObject( doc, Epk::FuncDomain::TID );
    else
        Epk::Procs::retypeObject( doc, Epk::Function::TID );
    doc.commit();
}

void FuncTreeCtrl::onImport()
{
    ENABLED_IF( true );

    const QString path = QFileDialog::getOpenFileName( getTree(), tr("Import Function  - FlowLine"),
                                                       QString(), tr("*.flnx") );
    if( path.isEmpty() )
        return;

    QFile f( path );
    if( !f.open( QIODevice::ReadOnly ) )
    {
        QMessageBox::critical( getTree(), tr("Import Function - FlowLine"),
            tr("Cannot open file for reading!") );
        return;
    }
    Udb::Obj doc = getSelectedObject();
    if( doc.isNull() )
        doc = Epk::FuncDomain::getOrCreateRoot(getMdl()->getRoot().getTxn());
    QApplication::setOverrideCursor(Qt::WaitCursor);
    Stream::DataReader in( &f );
    Epk::EpkStream stream;
    Udb::Obj o = stream.importProc( in, doc );
    if( o.isNull() )
    {
        doc.getTxn()->rollback();
        QApplication::restoreOverrideCursor();
        QMessageBox::critical( getTree(), tr("Import Process - FlowLine"), stream.getError() );
        return;
    }
    o.commit();
    QApplication::restoreOverrideCursor();
}

FuncTreeMdl::FuncTreeMdl(QObject *parent):GenericMdl(parent)
{
}

bool FuncTreeMdl::isSupportedType(quint32 type)
{
    return type == Epk::Function::TID || type == Epk::FuncDomain::TID;
}

void FuncTreeCtrl::writeTo(const Udb::Obj &o, Stream::DataWriter &out) const
{
    const Udb::Atom type = o.getType();
    if( type == Epk::Function::TID )
        out.startFrame( Stream::NameTag( "func" ) );
    else if( type == Epk::Event::TID )
        out.startFrame( Stream::NameTag( "evnt" ) );
//    else if( type == Epk::Connector::TID )
//        out.startFrame( Stream::NameTag( "conn" ) );
    //        // TODO: im Moment zeigen gepastete auf die ursprnglichen Objekte, was keinen Sinn macht
    else if( type == Epk::FuncDomain::TID )
        out.startFrame( Stream::NameTag( "fdom" ) );
    else
        return;

    Stream::DataCell v;
    Udb::Obj ref;
    v = o.getValue( Epk::Root::AttrText );
    if( v.hasValue() )
        out.writeSlot( v, Stream::NameTag( "text" ) );
    v = o.getValue( Epk::Function::AttrElemCount );
    if( v.hasValue() )
        out.writeSlot( v, Stream::NameTag( "ecnt" ) );
    v = o.getValue( Epk::Connector::AttrConnType );
    if( v.hasValue() )
        out.writeSlot( v, Stream::NameTag( "cotp" ) );
    ref = o.getValueAsObj( Epk::ConFlow::AttrPred );
    if( !ref.isNull() )
        out.writeSlot( Stream::DataCell().setUuid( ref.getUuid() ), Stream::NameTag( "pred" ) );
    ref = o.getValueAsObj( Epk::ConFlow::AttrSucc );
    if( !ref.isNull() )
        out.writeSlot( Stream::DataCell().setUuid( ref.getUuid() ), Stream::NameTag( "succ" ) );
    v = o.getValue( Epk::Function::AttrShowIds );
    if( v.hasValue() )
        out.writeSlot( v, Stream::NameTag( "shid" ) );
    v = o.getValue( Epk::Function::AttrMarkAlias );
    if( v.hasValue() )
        out.writeSlot( v, Stream::NameTag( "mara" ) );
    ref = o.getValueAsObj( Epk::Function::AttrStart );
    if( !ref.isNull() )
        out.writeSlot( Stream::DataCell().setUuid( ref.getUuid() ), Stream::NameTag( "star" ) );
    ref = o.getValueAsObj( Epk::Function::AttrFinish );
    if( !ref.isNull() )
        out.writeSlot( Stream::DataCell().setUuid( ref.getUuid() ), Stream::NameTag( "fini" ) );
    v = o.getValue( Epk::Function::AttrDirection );
    if( v.hasValue() )
        out.writeSlot( v, Stream::NameTag( "dir" ) );

    Udb::Obj sub = o.getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == Oln::OutlineItem::TID )
			Oln::OutlineStream::writeTo( out, sub );
        else
            writeTo( sub, out );
    }while( sub.next() );
    out.endFrame();
}

void FuncTreeCtrl::writeMime(QMimeData *data, const QList<Udb::Obj> & objs)
{
    // Schreibe Values
    Stream::DataWriter out2;
    out2.startFrame( Stream::NameTag( "ftr" ) );
    foreach( Udb::Obj o, objs)
    {
        writeTo( o, out2 );
    }
    out2.endFrame();
    data->setData( QLatin1String( s_mimeFuncTree ), out2.getStream() );
    getMdl()->getRoot().getTxn()->commit(); // RISK: wegen getUuid in OutlineStream
}

Udb::Obj FuncTreeCtrl::readFrom(Stream::DataReader &in, Udb::Obj &parent)
{
    Q_ASSERT( in.getCurrentToken() == Stream::DataReader::BeginFrame );
    quint32 type = 0;
    Stream::NameTag tag = in.getName().getTag();
    if( tag.equals( "func" ) )
        type = Epk::Function::TID;
    else if( tag.equals( "evnt" ) )
        type = Epk::Event::TID;
    else if( tag.equals( "conn" ) )
        type = Epk::Connector::TID;
    else if( tag.equals( "fdom" ) )
        type = Epk::FuncDomain::TID;
    else if( tag.equals( Oln::OutlineStream::s_olnFrameTag ) )
    {
        Oln::OutlineStream str;
		Udb::Obj o = str.readFrom( in, parent.getTxn(), parent.getOid(), false );
        o.aggregateTo( parent );
        return o;
    }
    if( Epk::Procs::isValidAggregate( parent.getType(), type ) )
    {
        Udb::Obj obj = Epk::Procs::createObject( type, parent );
        Stream::DataReader::Token t = in.nextToken();
        Udb::Transaction* txn = parent.getTxn();
        while( t == Stream::DataReader::Slot || t == Stream::DataReader::BeginFrame )
        {
            switch( t )
            {
            case Stream::DataReader::Slot:
                {
                    const Stream::NameTag name = in.getName().getTag();
                    if( name.equals( "text" ) )
                        obj.setValue( Epk::Root::AttrText, in.readValue() );
                    else if( name.equals( "ecnt" ) )
                        obj.setValue( Epk::Function::AttrElemCount, in.readValue() );
                    else if( name.equals( "cotp" ) )
                        obj.setValue( Epk::Connector::AttrConnType, in.readValue() );
                    else if( name.equals( "pred" ) )
                        obj.setValueAsObj( Epk::ConFlow::AttrPred, txn->getObject(in.readValue()) );
                    else if( name.equals( "succ" ) )
                        obj.setValueAsObj( Epk::ConFlow::AttrSucc, txn->getObject(in.readValue()) );
                    else if( name.equals( "shid" ) )
                        obj.setValue( Epk::Function::AttrShowIds, in.readValue() );
                    else if( name.equals( "mara" ) )
                        obj.setValue( Epk::Function::AttrMarkAlias, in.readValue() );
                    else if( name.equals( "star" ) )
                        obj.setValueAsObj( Epk::Function::AttrStart, txn->getObject(in.readValue()) );
                    else if( name.equals( "fini" ) )
                        obj.setValueAsObj( Epk::Function::AttrFinish, txn->getObject(in.readValue()) );
                    else if( name.equals( "dir" ) )
                        obj.setValue( Epk::Function::AttrDirection, in.readValue() );
                }
                break;
            case Stream::DataReader::BeginFrame:
                readFrom( in, obj );
                break;
            }
            t =in.nextToken();
        }
        Q_ASSERT( t == Stream::DataReader::EndFrame );
        return obj;
    }else
    {
		// Lese bis naechstes EndFrame auf gleicher Ebene
        in.skipToEndFrame();
        Q_ASSERT( in.getCurrentToken() == Stream::DataReader::EndFrame );
        return Udb::Obj();
    }
}

QList<Udb::Obj> FuncTreeCtrl::readMime(const QMimeData *data, Udb::Obj &parent)
{
    QList<Udb::Obj> res;
    Stream::DataReader in( data->data(QLatin1String( s_mimeFuncTree )) );

    Stream::DataReader::Token t = in.nextToken();
    if( t == Stream::DataReader::BeginFrame && in.getName().getTag().equals( "ftr" ) )
    {
        Stream::DataReader::Token t = in.nextToken();
        while( t == Stream::DataReader::BeginFrame )
        {
            Udb::Obj o = readFrom( in, parent );
            if( !o.isNull() )
                res.append( o );
            t = in.nextToken();
        }
        getMdl()->getRoot().getTxn()->commit();
    }
    return res;
}

void FuncTreeCtrl::onCopy()
{
    QModelIndexList sr = getTree()->selectionModel()->selectedRows();
    ENABLED_IF( !sr.isEmpty() );

    QList<Udb::Obj> objs;
    QMimeData* mime = new QMimeData();
    foreach( QModelIndex i, sr )
    {
        Udb::Obj o = getMdl()->getObject( i );
        Q_ASSERT( !o.isNull() );
        objs.append( o );
    }
    writeMime( mime, objs );
    Udb::Obj::writeObjectRefs( mime, objs );
	FuncTreeMdl::writeObjectUrls( mime, objs );
    QApplication::clipboard()->setMimeData( mime );
}

void FuncTreeCtrl::onPaste()
{
    Udb::Obj doc = getSelectedObject( true );
    ENABLED_IF( !doc.isNull() && QApplication::clipboard()->mimeData()->hasFormat( s_mimeFuncTree ) );

    readMime( QApplication::clipboard()->mimeData(), doc );
}

void FuncTreeCtrl::onEditAttrs()
{
    Udb::Obj doc = getSelectedObject();
    ENABLED_IF( doc.getType() == Epk::Function::TID || doc.getType() == Epk::FuncDomain::TID );

    Epk::ObjAttrDlg dlg( getTree() );
    dlg.edit( doc );
}




