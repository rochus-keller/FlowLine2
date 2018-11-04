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

#include "SysTree.h"
#include "EpkObjects.h"
#include "EpkCtrl.h"
#include <QtGui/QTreeView>
using namespace Fln;

SysTreeCtrl *SysTreeCtrl::create(QWidget *parent, const Udb::Obj &root)
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

    SysTreeMdl* mdl = new SysTreeMdl( tree );
    tree->setModel( mdl );

    SysTreeCtrl* ctrl = new SysTreeCtrl(tree, mdl);
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

void SysTreeCtrl::addCommands(Gui2::AutoMenu * pop)
{
    pop->addAction( tr("Expand All"), getTree(), SLOT( expandAll() ) );
    pop->addAction( tr("Expand Selected"), this, SLOT( onExpandSelected() ) );
    pop->addAction( tr("Collapse All"), getTree(), SLOT( collapseAll() ) );
    pop->addSeparator();
    pop->addCommand( tr("Move up"), this, SLOT(onMoveUp()), tr("SHIFT+Up"), true );
    pop->addCommand( tr("Move down"), this, SLOT(onMoveDown()), tr("SHIFT+Down"), true );
    pop->addSeparator();
    pop->addCommand( tr("New System Element"), this, SLOT(onAddElement()), tr("CTRL+R"), true );
    pop->addCommand( tr("New Sibling"), this, SLOT(onAddNext()), tr("CTRL+N"), true );
    pop->addSeparator();
    pop->addCommand( tr("Edit Name"), this, SLOT( onEditName() ) );
    pop->addCommand( tr("Edit Attributes..."), this, SLOT(onEditAttrs()), tr("CTRL+Return"), true );
    pop->addCommand( tr("Delete Items..."), this, SLOT( onDeleteItems() ), tr("CTRL+D"), true );
}

void SysTreeCtrl::onAddElement()
{
    add( Epk::SystemElement::TID );
}

void SysTreeCtrl::onEditAttrs()
{
    Udb::Obj doc = getSelectedObject();
    ENABLED_IF( doc.getType() == Epk::Function::TID || doc.getType() == Epk::FuncDomain::TID );

    Epk::ObjAttrDlg dlg( getTree() );
    dlg.edit( doc );
}

bool Fln::SysTreeMdl::isSupportedType(quint32 type)
{
    return type == Epk::SystemElement::TID;
}
