#ifndef EPKCTRL_H
#define EPKCTRL_H

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

#include <QDialog>
#include <QtCore/QSet>
#include <Udb/Obj.h>
#include <Gui2/AutoMenu.h>

namespace Epk
{
    class EpkView;
    class EpkItemMdl;

    class EpkCtrl : public QObject
    {
        Q_OBJECT
    public:
        static const char* s_mimeEpkItems;

        EpkCtrl( EpkView*, EpkItemMdl* );
        EpkView* getView() const;
        static EpkCtrl* create( QWidget* parent, const Udb::Obj& doc );
        void addCommands( Gui2::AutoMenu * pop );
        Udb::Obj getSingleSelection() const; // Gibt Task/Milestone/Link zurck, nicht PdmItem
        static void writeItems(QMimeData *data, const QList<Udb::Obj>& );
        QList<Udb::Obj> readItems(const QMimeData *data ); // gibt DiagItem zurck!
        const Udb::Obj& getDiagram() const;
        bool focusOn( const Udb::Obj& );
    signals:
        void signalSelectionChanged();
    public slots:
        void onAddFunction();
        void onAddEvent();
        void onAddConnector();
        void onAddNote();
        void onAddFrame();
        void onSelectAll();
        void onRemoveItems();
        void onDeleteItems();
        void onInsertHandle();
        void onExportToFile();
        void onExportToClipboard();
        void onSelectRightward();
        void onSelectUpward();
        void onSelectLeftward();
        void onSelectDownward();
        void onLink();
        void onCopy();
        void onPaste();
        void onCut();
        void onShowId();
        void onSelectHiddenSchedObjs();
        void onSelectHiddenLinks();
        void onMarkAlias();
        void onLayout();
        void onSetConnType();
        void onToggleFuncEvent();
        void onExtendDiagram();
        void onShowShortestPath();
        void onRemoveAllAliasses();
        void onSetDirection();
        void onEditAttrs();
		void onPinItems();
		void onUnpinItems();
	protected slots:
        void onDblClick( const QPoint& );
        void onCreateLink( const Udb::Obj& pred, const Udb::Obj& succ, const QPolygonF& path );
        void onCreateSuccLink( const Udb::Obj& pred, int type, const QPointF& pos, const QPolygonF& path );
        void onSelectionChanged();
        void onDrop( QByteArray, QPointF );
    protected:
        void onAddItem( quint32 type, int kind = 0 );
        void pasteItemRefs(const QMimeData *data, const QPointF &where );
        void doLayout();
        static void adjustTo( const QList<Udb::Obj>&, const QPointF& to ); // erwartet PdmItems
    private:
        EpkItemMdl* d_mdl;
    };

    class ObjAttrDlg : public QDialog
    {
    public:
        ObjAttrDlg( QWidget* p):QDialog(p) {}
        bool edit( Udb::Obj& );
    };
}

#endif // EPKCTRL_H
