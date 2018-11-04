#ifndef EPKITEMMDL_H
#define EPKITEMMDL_H

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

#include <QGraphicsScene>
#include <QHash>
#include <Udb/Obj.h>

namespace Epk
{
    class EpkNode;
	class DiagItem;
    class LineSegment;

    class EpkItemMdl : public QGraphicsScene
    {
        Q_OBJECT
    public:
        enum Mode { Idle, AddingLink, PrepareMove, Moving, Scaling };
        enum CreateType { _None, _Function, _Event, _AndConn, _OrConn, _XorConn, _StartConn, _FinishConn, _Handle };

        static const float s_cellWidth;
        static const float s_cellHeight;
        static const char* s_mimeEvent;

        EpkItemMdl( QObject* p );
        void setDiagram( const Udb::Obj& );
        const Udb::Obj& getDiagram() const { return d_doc; }
        Udb::Obj getSingleSelection() const; // Nur wenn eines selektiert; gibt PdmItem zurück
        QList<Udb::Obj> getMultiSelection(bool elems = true,
            bool link = true, bool handle = true) const; // Gibt alle selektiereten PdmItem (!) zurück
        QGraphicsItem* selectObject( const Udb::Obj&, bool clearSel = true ); // funktioniert mit PdmItem und OrigObject
        void selectObjects( const QList<Udb::Obj>& obs, bool clearSel = true ); // dito
        void removeSelectedItems(); // Wirkt nur auf PdmItems
        bool isShowId() const;
        void setShowId( bool on );
        bool isMarkAlias() const;
        void setMarkAlias( bool on );
        void setReadOnly( bool on ) { d_readOnly = on; }
        bool isReadOnly() const { return d_doc.isNull() || d_readOnly; }
        bool contains( quint32 oid ) const { return d_cache.contains( oid ); }
        void enlargeSceneRect();
        void fitSceneRect(bool forceFit = false);

        QPointF getStart(bool rastered = false) const;
        QPointF toCellPos( const QPointF& ) const;

        bool canStartLink() const;
        void startLink();
        void setItemText( QGraphicsItem* i, const QString& str );
        bool insertHandle();
        void setChartFont( const QFont& f ) { d_chartFont = f; update(); }
        const QFont& getChartFont() const { return d_chartFont; }

        void exportPng( const QString& path );
        void exportPdf( const QString& path, bool withDetails = false );
        void exportHtml( const QString& path, bool withPng = true );
        void exportSvg( const QString& path );

        static QPointF rastered( const QPointF& );
        void removeFromCache( QGraphicsItem* ); // Implementationsdetail
		void movePinned( const QList<EpkNode*>&, const QPointF& diff );
    signals:
        void signalCreateSuccLink( const Udb::Obj& pred, int type, const QPointF& pos, const QPolygonF& path );
        void signalCreateLink( const Udb::Obj& pred, const Udb::Obj& succ, const QPolygonF& path );
        void signalDrop( QByteArray, QPointF );
    protected:
        void rasteredMoveBy( EpkNode*, qreal dx, qreal dy );
        bool canStartLink( EpkNode* ) const;
        bool canEndLink( EpkNode* ) const;
        bool canScale( EpkNode* ) const;
        LineSegment* addSegment( EpkNode* from, EpkNode* to, const Udb::Obj& pdmItem = Udb::Obj() );
        EpkNode *addHandle();
        void deleteLinkOrHandle( QGraphicsItem* );
        void removeHandle( EpkNode * );
        void removeNode( QGraphicsItem* );
        void deleteAllLinkSegments( LineSegment* segment );
        QPolygonF getNodeList( QGraphicsItem* ) const;
        void fetchItemFromDb( const Udb::Obj&, bool links, bool vertices );
        void fetchAttributes( EpkNode*, const Udb::Obj& ) const;
		void installPin( const DiagItem& diagItem );
        // overrides
        void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent);
        void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent);
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent);
        void keyPressEvent ( QKeyEvent * keyEvent );
        void dropEvent ( QGraphicsSceneDragDropEvent * event );
        void dragEnterEvent ( QGraphicsSceneDragDropEvent * event );
        void dragLeaveEvent ( QGraphicsSceneDragDropEvent * event );
        void dragMoveEvent ( QGraphicsSceneDragDropEvent * event );
        void drawItems(QPainter *painter, int numItems, QGraphicsItem *items[],
                       const QStyleOptionGraphicsItem options[], QWidget *widget);
    protected slots:
        void onDbUpdate( Udb::UpdateInfo );
    private:
        QPointF d_startPos;
        QPointF d_lastPos;
        Mode d_mode;
        EpkNode* d_lastHitItem;
        EpkNode* d_startItem;
        QGraphicsLineItem* d_tempLine;
        QGraphicsPathItem* d_tempBox;
        Udb::Obj d_doc;
        QHash<quint32,QGraphicsItem*> d_cache; // oid->Item, sowohl PdmItem als auch OrigObject!
        QList<Udb::Obj> d_orphans;
        QFont d_chartFont;
        bool d_readOnly;
        bool d_toEnlarge;
        bool d_strictSyntax;
		bool d_commitLock; // Um rekursive Commits zu verhindern
    };
}

#endif // EPKITEMMDL_H
