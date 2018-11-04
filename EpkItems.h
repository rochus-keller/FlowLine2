#ifndef EPKITEMS_H
#define EPKITEMS_H

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

#include <QAbstractGraphicsShapeItem>

namespace Epk
{
    class LineSegment;

    class EpkNode : public QGraphicsItem
    {
        // Repräsentiert Functions, Events und Connectors; Überbegriff von Node und Line ist Item
    public:
        enum Type { _Function = UserType + 1, _Event, _Connector, _Flow, _Handle, _Note, _Frame };
        EpkNode( quint32 item, quint32 orig, int type );
        ~EpkNode();

        QString getText() const { return d_text; }
        QString getId() const { return d_id; }
        quint32 getItemOid() const { return d_itemOid; }
        quint32 getOrigOid() const { return d_origOid; }
        bool isAlias() const { return d_alias; }
        void setAlias( bool on ) { d_alias = on; }
        void setProcess( bool on ) { d_isProcess = on; }
        bool isProcess() const { return d_isProcess; }
        bool hasItemText() const { return type() == _Function || type() == _Event || type() == _Note || type() == _Frame; }
        void setType( int type );
        void setWidth( qreal w ); // für Note
        void updateNoteHeight();
        void setSize( const QSizeF& ); // für Frame
        const QSizeF& getSize() const { return d_size; }
        EpkNode* parentNode() const { return dynamic_cast<EpkNode*>(parentItem() ); }

        // Interface für Model
        void addLine(LineSegment*, bool start);
        void removeLine(LineSegment*);
        void setText( const QString& t );
        void setId( const QString& t ) { d_id = t; }
        void setCode( quint8 c ) { d_code = c; }
        const QList<LineSegment*>& getLinks() const { return d_links; }
		void setPinnedTo( EpkNode* );
		EpkNode* pinnedTo() const { return d_pinnedTo; }

        // Folgendes für LineHandles
        LineSegment* getFirstInSegment() const;
        LineSegment* getFirstOutSegment() const;
        LineSegment* getFirstSegment() const;
        LineSegment* getLastSegment() const;

        QPolygonF toPolygon() const;

        // overrides
        virtual QRectF boundingRect () const;
        virtual QPainterPath shape () const;
        virtual void paint ( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0 );
        virtual void adjustSize( qreal dx, qreal dy );
        virtual int type () const { return d_type; }
    protected:
        void paintEvent( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0 );
        void paintFunction( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0 );
        void paintHandle( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0 );
        void paintConnector( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0 );
        void paintNote( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0 );
        void paintFrame( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0 );
        static QRectF handleShapeRect();
        // overrides
        QVariant itemChange(GraphicsItemChange change, const QVariant &value);
    private:
        friend class LineSegment;
        QList<LineSegment*> d_links;
		EpkNode* d_pinnedTo;
		QList<EpkNode*> d_pinneds;
        quint32 d_itemOid; // DiagItem
        quint32 d_origOid; // OrigObject
        QString d_text;
        QString d_id;  // von OrigObject
        int d_type;
        // Text stammt ebenfalls von OrigObject
        bool d_alias; // true: OrigObject.getParent() != PdmItem.getParent()
        bool d_isProcess;
        quint8 d_code; // Types
        QSizeF d_size; // für Note (width) und Frame (width, height)
    };

    class LineSegment : public QGraphicsLineItem
    {
    public:
        LineSegment( quint32 item, quint32 orig);
        ~LineSegment();

        EpkNode* getStartItem() const { return d_start; }
        EpkNode* getUltimateStartItem() const;
        LineSegment* getFirstSegment() const;
        EpkNode* getEndItem() const { return d_end; }
        EpkNode* getUltimateEndItem() const;
        LineSegment* getLastSegment() const;
        QList<LineSegment*> getChain() const;
        quint32 getItemOid() const { return d_itemOid; }
        quint32 getOrigOid() const { return d_origOid; }
        void selectAllSegments();

        // Overrides
        QRectF boundingRect() const;
        QPainterPath shape() const;
        virtual int type () const { return EpkNode::_Flow; }
    public slots:
        void updatePosition();

    protected:
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget = 0);

    private:
        //friend class PdmItemMdl;
        friend class EpkNode; // Wegen Destructor
        EpkNode* d_start;
        EpkNode* d_end;
        QPolygonF d_arrowHead;
        quint32 d_itemOid; // PdmItem
        quint32 d_origOid; // OrigObject
    };
}

#endif // EPKITEMS_H
