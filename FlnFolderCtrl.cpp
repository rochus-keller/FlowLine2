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

#include "FlnFolderCtrl.h"
#include <QtGui/QTreeView>
#include "EpkObjects.h"
using namespace Fln;

FolderCtrl::FolderCtrl(QTreeView *tree, Wt::GenericMdl *mdl) :
    GenericCtrl(tree, mdl)
{
}

FolderCtrl *FolderCtrl::create(QWidget *parent, const Udb::Obj &root)
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

    FolderMdl* mdl = new FolderMdl( tree );
    tree->setModel( mdl );

    FolderCtrl* ctrl = new FolderCtrl(tree, mdl);
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

void FolderCtrl::addCommands(Gui2::AutoMenu * pop)
{
    pop->addAction( tr("Expand All"), getTree(), SLOT( expandAll() ) );
	pop->addAction( tr("Expand Selected"), this, SLOT( onExpandSelected() ) );
	pop->addAction( tr("Collapse All"), getTree(), SLOT( collapseAll() ) );
    pop->addSeparator();
    pop->addCommand( tr("Move up"), this, SLOT(onMoveUp()), tr("SHIFT+Up"), true );
    pop->addCommand( tr("Move down"), this, SLOT(onMoveDown()), tr("SHIFT+Down"), true );
	pop->addSeparator();
    pop->addCommand( tr("New Item (same type)"), this, SLOT(onAddNext()), tr("CTRL+N"), true );
	pop->addCommand( tr("New Folder"), this, SLOT(onAddFolder()) );
    pop->addCommand( tr("New EPK Diagram"), this, SLOT(onAddEpkDiagram()) );
    pop->addCommand( tr("New Outline"), this, SLOT(onAddOutline()) );
	pop->addCommand( tr("New Lua Script"), this, SLOT(onAddScript()) );
	pop->addSeparator();
    pop->addCommand( tr("Copy"), this, SLOT( onCopy() ), tr("CTRL+C"), true );
    pop->addSeparator();
	pop->addCommand( tr("Edit Name"), this, SLOT( onEditName() ) );
    pop->addCommand( tr("Delete Items"), this, SLOT( onDeleteItems() ), tr("CTRL+D"), true );
}

void FolderCtrl::onAddFolder()
{
    add( Udb::Folder::TID );
}

void FolderCtrl::onAddEpkDiagram()
{
    add( Epk::Diagram::TID );
}

void FolderCtrl::onAddOutline()
{
    add( Oln::Outline::TID );
}

void FolderCtrl::onAddScript()
{
	add( Udb::ScriptSource::TID );
}

FolderMdl::FolderMdl(QObject *parent):GenericMdl(parent)
{
}

bool FolderMdl::isSupportedType(quint32 type)
{
	return type == Udb::Folder::TID || type == Udb::ScriptSource::TID ||
			type == Epk::Diagram::TID || type == Oln::Outline::TID;
}
