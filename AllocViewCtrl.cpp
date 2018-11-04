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

#include "AllocViewCtrl.h"
#include "EpkObjects.h"
#include "EpkProcs.h"
#include <Udb/Database.h>
#include <Udb/Transaction.h>
#include <Udb/Idx.h>
#include <Oln2/OutlineUdbMdl.h>
#include <WorkTree/ObjectTitleFrame.h>
#include <QVBoxLayout>
#include <QListWidget>
#include <QMimeData>
using namespace Fln;

class _AllocViewList : public QListWidget
{
public:
    _AllocViewList( QWidget* p, Wt::ObjectTitleFrame *title ):QListWidget(p),d_title(title) {}
    Wt::ObjectTitleFrame* d_title;
    QStringList mimeTypes () const
    {
        return QStringList() << QLatin1String( Udb::Obj::s_mimeObjectRefs );
    }
    bool dropMimeData ( int index, const QMimeData * data, Qt::DropAction action )
    {
        if( d_title->getObj().isNull() )
            return false;
        clearSelection();
        if( data->hasFormat( Udb::Obj::s_mimeObjectRefs ) )
        {
            QList<Udb::Obj> objs = Udb::Obj::readObjectRefs( data, d_title->getObj().getTxn() );
            foreach( Udb::Obj sys, objs )
            {
                if( sys.getType() == Epk::SystemElement::TID )
                {
                    Epk::Allocation a = Epk::Procs::createObject( Epk::Allocation::TID, d_title->getObj() );
                    a.setFunc( d_title->getObj() );
                    a.setElem( sys );
                    QListWidgetItem* lwi = new QListWidgetItem( this );
                    lwi->setSelected( true );
                    formatItem( lwi, a );
                }
            }
            sortItems();
            d_title->getObj().getTxn()->commit();
            return true;
        }else
            return false;
    }
    static void formatItem(QListWidgetItem *lwi, const Epk::Allocation &a)
    {
        lwi->setData( Qt::UserRole, a.getOid() );
        lwi->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
        lwi->setText( Epk::Procs::formatObjectTitle( a.getElem() ) );
        lwi->setToolTip( lwi->text() );
        lwi->setIcon( Oln::OutlineUdbMdl::getPixmap( a.getType() ) );
    }
};

AllocViewCtrl::AllocViewCtrl(QWidget *parent) :
    QObject(parent)
{
}

AllocViewCtrl *AllocViewCtrl::create(QWidget *parent, Udb::Transaction *txn)
{
    QWidget* pane = new QWidget( parent );
    QVBoxLayout* vbox = new QVBoxLayout( pane );
    vbox->setMargin( 2 );
    vbox->setSpacing( 2 );

    AllocViewCtrl* ctrl = new AllocViewCtrl( pane );

    ctrl->d_title = new Wt::ObjectTitleFrame( pane );
    vbox->addWidget( ctrl->d_title );

    ctrl->d_list = new _AllocViewList( pane, ctrl->d_title );
    ctrl->d_list->setAlternatingRowColors( true );
    ctrl->d_list->setSelectionMode( QAbstractItemView::ExtendedSelection );
    ctrl->d_list->setDragEnabled( true );
    ctrl->d_list->setDragDropMode( QAbstractItemView::DropOnly );

    vbox->addWidget( ctrl->d_list );

    connect( ctrl->d_list, SIGNAL(itemPressed(QListWidgetItem*)), ctrl,SLOT(onClicked(QListWidgetItem*)) );
    connect( ctrl->d_title, SIGNAL(signalClicked()), ctrl, SLOT(onTitleClick()) );

    txn->getDb()->addObserver( ctrl, SLOT( onDbUpdate( Udb::UpdateInfo ) ) );

    return ctrl;
}

void AllocViewCtrl::setObj(const Udb::Obj & o)
{
    if( o.getType() != Epk::Function::TID )
        return;

    d_title->setObj( o );
    d_list->clear();
    if( o.isNull() )
        return;

    Udb::Idx idx( o.getTxn(), Epk::Index::Func );
    if( idx.seek( Stream::DataCell().setOid( o.getOid() ) ) ) do
    {
        Udb::Obj sys = o.getObject( idx.getOid() );
        QListWidgetItem* lwi = new QListWidgetItem( d_list );
        _AllocViewList::formatItem( lwi, sys );
    }while( idx.nextKey() );

    d_list->sortItems();
}

QWidget *AllocViewCtrl::getWidget() const
{
    return static_cast<QWidget*>( parent() );
}

void AllocViewCtrl::addCommands(Gui2::AutoMenu * pop)
{
//    pop->addCommand( tr("Copy"),  this, SLOT( onCopy() ), tr("CTRL+C"), true );
//    pop->addCommand( tr("Paste"),  this, SLOT( onPaste() ), tr("CTRL+V"), true );
//    pop->addSeparator();
    pop->addCommand( tr("Remove Reference"),  this, SLOT( onRemove() ), tr("CTRL+D"), true );
}

void AllocViewCtrl::focusOn(const Udb::Obj &)
{
}

void AllocViewCtrl::clear()
{
    d_title->setObj( Udb::Obj() );
    d_list->clear();
}

void AllocViewCtrl::onDbUpdate(Udb::UpdateInfo info)
{
    switch( info.d_kind )
    {
    case Udb::UpdateInfo::ObjectErased:
        if( info.d_id == d_title->getObj().getOid() )
            clear();
        else if( info.d_name == Epk::SystemElement::TID )
        {
            for( int i = 0; i < d_list->count(); i++ )
                if( d_list->item(i)->data(Qt::UserRole).toULongLong() == info.d_id )
                {
                    delete d_list->item(i);
                    return;
                }
        }
        break;
    }
}

void AllocViewCtrl::onTitleClick()
{
    if( !d_title->getObj().isNull() )
        emit signalSelect( d_title->getObj() );
}

void AllocViewCtrl::onRemove()
{
    QList<QListWidgetItem *> sel = d_list->selectedItems();
    ENABLED_IF( !sel.isEmpty() && !d_title->getObj().isNull() );

    // TODO
//    foreach( QListWidgetItem* i, sel )
//    {
//        Udb::Obj imp = d_title->getObj().getObject( i->data(Qt::UserRole).toULongLong() );
//        imp.clearValue( AttrWbsRef );
//        delete i;
//    }
//    d_title->getObj().getTxn()->commit();
}

void AllocViewCtrl::onClicked(QListWidgetItem * item)
{
    if( item == 0 || d_title->getObj().isNull() )
        return;
    emit signalSelect( d_title->getObj().getObject( item->data(Qt::UserRole).toULongLong() ) );
}

