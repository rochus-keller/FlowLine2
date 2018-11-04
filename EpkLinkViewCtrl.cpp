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

#include "EpkLinkViewCtrl.h"
#include <QtGui/QVBoxLayout>
#include <WorkTree/ObjectTitleFrame.h>
#include <Udb/Database.h>
#include <Udb/Transaction.h>
#include <QtGui/QListWidget>
#include <QtGui/QMessageBox>
#include <QtGui/QApplication>
#include <QtGui/QClipboard>
#include <QtCore/QMimeData>
#include <Udb/Idx.h>
#include "EpkObjects.h"
#include "EpkProcs.h"
#include "EpkItemMdl.h"
#include "EpkView.h"
using namespace Epk;

const int LinkRole = Qt::UserRole;
const int PeerRole = Qt::UserRole + 1;

class _EpkLinkList : public QListWidget
{
public:
    _EpkLinkList( QWidget* p, Udb::Transaction *txn ):QListWidget(p),d_txn(txn) {}
    Udb::Transaction* d_txn;
    QMimeData * mimeData ( const QList<QListWidgetItem *> items ) const
    {
        QList<Udb::Obj> objs;
        foreach( QListWidgetItem* i, items )
        {
            objs.append( d_txn->getObject( i->data(PeerRole).toULongLong() ) );
            objs.append( d_txn->getObject( i->data(LinkRole).toULongLong() ) );
        }
        QMimeData* data = new QMimeData();
        Udb::Obj::writeObjectRefs( data, objs );
        return data;
    }
    QStringList mimeTypes () const
    {
        return QStringList() << QLatin1String( Udb::Obj::s_mimeObjectRefs );
    }
};

EpkLinkViewCtrl::EpkLinkViewCtrl(QWidget *parent) :
    QObject(parent),d_view(0)
{
}

EpkLinkViewCtrl *EpkLinkViewCtrl::create(QWidget *parent,Udb::Transaction *txn )
{
    Q_ASSERT( txn != 0 );
    QWidget* pane = new QWidget( parent );
    QVBoxLayout* vbox = new QVBoxLayout( pane );
	vbox->setMargin( 2 );
	vbox->setSpacing( 2 );

    EpkLinkViewCtrl* ctrl = new EpkLinkViewCtrl( pane );

    ctrl->d_title = new Wt::ObjectTitleFrame( pane );
    vbox->addWidget( ctrl->d_title );

    ctrl->d_list = new _EpkLinkList( pane, txn );
    ctrl->d_list->setAlternatingRowColors( true );
    ctrl->d_list->setSelectionMode( QAbstractItemView::ExtendedSelection );
    ctrl->d_list->setDragEnabled( true );
    ctrl->d_list->setDragDropMode( QAbstractItemView::DragOnly );

	vbox->addWidget( ctrl->d_list );

    connect( ctrl->d_list, SIGNAL(itemPressed(QListWidgetItem*)), ctrl,SLOT(onClicked(QListWidgetItem*)) );
    connect( ctrl->d_list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), ctrl,SLOT(onDblClick(QListWidgetItem*)));
    connect( ctrl->d_title, SIGNAL(signalClicked()), ctrl, SLOT(onTitleClick()) );

    txn->getDb()->addObserver( ctrl, SLOT( onDbUpdate( Udb::UpdateInfo ) ) );

    return ctrl;
}

void EpkLinkViewCtrl::setObj(const Udb::Obj & o)
{
    if( o.getType() == ConFlow::TID )
        return;

    d_title->setObj( o );
    d_list->clear();
    if( o.isNull() )
        return;

    Udb::Idx succIdx( o.getTxn(), Index::Succ );
    if( succIdx.seek( Stream::DataCell().setOid( o.getOid() ) ) ) do
    {
        addLink( o.getObject( succIdx.getOid() ), true );
    }while( succIdx.nextKey() );
    Udb::Idx predIdx( o.getTxn(), Index::Pred );
    if( predIdx.seek( Stream::DataCell().setOid( o.getOid() ) ) ) do
    {
        addLink( o.getObject( predIdx.getOid() ), false );
    }while( predIdx.nextKey() );

    d_list->sortItems();
    markObservedLinks();
}

void EpkLinkViewCtrl::clear()
{
    d_title->setObj( Udb::Obj() );
    d_list->clear();
}

QWidget *EpkLinkViewCtrl::getWidget() const
{
    return static_cast<QWidget*>( parent() );
}

void EpkLinkViewCtrl::addCommands(Gui2::AutoMenu * pop)
{
    pop->addCommand( tr("Show Link"), this, SLOT( onShowLink() ) );
    pop->addCommand( tr("Show Function/Event"),  this, SLOT( onShowPeer() ) );
    pop->addCommand( tr("Copy"),  this, SLOT( onCopy() ), tr("CTRL+C"), true );
    pop->addSeparator();
    pop->addCommand( tr("Delete..."),  this, SLOT( onDelete() ), tr("CTRL+D"), true );
}

void EpkLinkViewCtrl::showLink(const Udb::Obj & link)
{
    for( int i = 0; i < d_list->count(); i++ )
    {
        if( d_list->item(i)->data(LinkRole).toULongLong() == link.getOid() )
        {
            d_list->setCurrentItem( d_list->item(i), QItemSelectionModel::SelectCurrent );
            d_list->scrollToItem( d_list->item(i) );
            return;
        }
    }
}

void EpkLinkViewCtrl::setObserved(EpkView * v)
{
    d_view = v;
    markObservedLinks();
}

void EpkLinkViewCtrl::onDbUpdate(Udb::UpdateInfo info)
{
    switch( info.d_kind )
	{
    case Udb::UpdateInfo::ObjectErased:
        if( info.d_id == d_title->getObj().getOid() )
            clear();
        else if( info.d_name == ConFlow::TID )
        {
            for( int i = 0; i < d_list->count(); i++ )
                if( d_list->item(i)->data(LinkRole).toULongLong() == info.d_id )
                {
                    delete d_list->item(i);
                    return;
                }
        }
        break;
    }
}

void EpkLinkViewCtrl::onClicked(QListWidgetItem* item)
{
    if( item == 0 || d_title->getObj().isNull() )
        return;
    emit signalSelect( d_title->getObj().getObject( item->data(LinkRole).toULongLong() ), false );
}

void EpkLinkViewCtrl::onDblClick(QListWidgetItem* item)
{
    if( item == 0 || d_title->getObj().isNull() )
        return;
    Udb::Obj link = d_title->getObj().getObject( item->data(LinkRole).toULongLong() );
    Udb::Obj peer = d_title->getObj().getObject( item->data(PeerRole).toULongLong() );
    emit signalSelect( peer, true );
}

void EpkLinkViewCtrl::onShowLink()
{
    QList<QListWidgetItem *> sel = d_list->selectedItems();
    ENABLED_IF( sel.size() == 1 && !d_title->getObj().isNull() );
    emit signalSelect( d_title->getObj().getObject(
                               sel.first()->data(LinkRole).toULongLong() ), true );
}

void EpkLinkViewCtrl::onShowPeer()
{
    QList<QListWidgetItem *> sel = d_list->selectedItems();
    ENABLED_IF( sel.size() == 1 && !d_title->getObj().isNull() );
    onDblClick( sel.first() );
}

void EpkLinkViewCtrl::onDelete()
{
    QList<QListWidgetItem *> sel = d_list->selectedItems();
    ENABLED_IF( !sel.isEmpty() && !d_title->getObj().isNull() );

    const int res = QMessageBox::warning( getWidget(), tr("Delete Link from Database"),
                                          tr("Do you really want to permanently delete %1 links? This cannot be undone." ).
                                          arg( sel.size() ), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
    if( res == QMessageBox::No )
		return;

    foreach( QListWidgetItem* i, sel )
    {
        Udb::Obj link = d_title->getObj().getObject( i->data(LinkRole).toULongLong() );
        Procs::erase( link );
    }
    d_title->getObj().getTxn()->commit();
}

void EpkLinkViewCtrl::onTitleClick()
{
    if( !d_title->getObj().isNull() )
        emit signalSelect( d_title->getObj(), false );
}

void EpkLinkViewCtrl::onCopy()
{
    QList<QListWidgetItem *> sel = d_list->selectedItems();
    ENABLED_IF( !sel.isEmpty() && !d_title->getObj().isNull() );

    QList<Udb::Obj> objs;
    foreach( QListWidgetItem* i, sel )
    {
        objs.append( d_title->getObj().getObject( i->data(PeerRole).toULongLong() ) );
        objs.append( d_title->getObj().getObject( i->data(LinkRole).toULongLong() ) );
    }
    QMimeData* data = new QMimeData();
    Udb::Obj::writeObjectRefs( data, objs );
	Udb::Obj::writeObjectUrls( data, objs, Udb::ContentObject::AttrIdent, Udb::ContentObject::AttrText );
    QApplication::clipboard()->setMimeData( data );
}

static inline QString _formatLink( const Udb::Obj& link, const Udb::Obj& peer )
{
    return Procs::formatObjectTitle( peer );
}

void EpkLinkViewCtrl::addLink(const Udb::Obj & link, bool inbound )
{
    if( link.isNull() )
        return;
    QListWidgetItem* lwi = new QListWidgetItem( d_list );
    formatItem( lwi, link, inbound );
}

void EpkLinkViewCtrl::formatItem(QListWidgetItem * lwi, const Udb::Obj & link, bool inbound)
{
    Udb::Obj peer;
    if( inbound )
    {
        peer = link.getValueAsObj( ConFlow::AttrPred );
        lwi->setText( _formatLink( link, peer ) );
        lwi->setToolTip( QString( "Inbound Link/Predecessor: %1" ).arg( lwi->text() ) );
        lwi->setIcon( QIcon( ":/FlowLine2/Images/pred.png" ) );
    }else
    {
        peer = link.getValueAsObj( ConFlow::AttrSucc );
        lwi->setText( _formatLink( link, peer ) );
        lwi->setToolTip( QString( "Outbound Link/Successor: %1" ).arg( lwi->text() ) );
        lwi->setIcon( QIcon( ":/FlowLine2/Images/succ.png" ) );
    }
    lwi->setData( LinkRole, link.getOid() );
    lwi->setData( PeerRole, peer.getOid() );
    lwi->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled );
}

void EpkLinkViewCtrl::markObservedLinks()
{
    if( d_view == 0 )
        for( int i = 0; i < d_list->count(); i++ )
            d_list->item(i)->setFont( d_list->font() );
    else
    {
        EpkItemMdl* mdl = d_view->getMdl();
        QFont bold = d_list->font();
        bold.setBold( true );
        for( int i = 0; i < d_list->count(); i++ )
        {
            if( mdl->contains( d_list->item(i)->data(LinkRole).toULongLong() ) &&
                    mdl->contains( d_list->item(i)->data(PeerRole).toULongLong() ) )
                d_list->item(i)->setFont( bold );
            else
                d_list->item(i)->setFont( d_list->font() );
        }
    }
}
