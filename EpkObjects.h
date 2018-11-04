#ifndef EPKOBJECTS_H
#define EPKOBJECTS_H

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

#include <Udb/ContentObject.h>
#include <Oln2/OutlineItem.h>

namespace Epk
{
	enum { MinAttr = 301, MaxAttr = 321, StartOfDynAttr = 0x100000 };

    class Root : public Udb::ContentObject
    {
    public:
        Root( const Udb::Obj& o ):Udb::ContentObject(o){}
		Root() {}
		enum Attrs
		{
			AttrAutoOpen = 321   // OID: optionale Referenz auf ein Outline, das beim Start geöffnet wird
		};
        QString formatObjectTitle() const;
		static const char* s_rootUuid;
	};

    class DiagItem : public Root
    {
        // Repräsentiert die graphischen Elemente in einem Diagramm (nicht die Datenobjekte selber, die
        // stattdessen als OrigObject referenziert werden). Kommen vor in EpkDiagram, Function und FunctionTree.
    public:
        static const float s_penWidth;
        static const float s_selPenWidth;
        static const float s_radius;
        static const float s_boxWidth;
        static const float s_boxHeight;
        static const float s_boxInset;
        static const float s_circleDiameter;
        static const float s_textMargin;
        static const float s_rasterX;
        static const float s_rasterY;

        static int TID;            // Für alle Diagrammelemente: Nodes und Lines
        enum Kind
        {
            Plain = 0,
            Note = 1,
            Frame = 2
        };
        enum Attrs
        {
            AttrPosX = 301,         // float
            AttrPosY = 302,         // float
            AttrNodeList = 303,     // frame ( xFloat, yFloat )* end
            AttrOrigObject = 304,   // OID; zeigt auf Function, Event oder Link, die durch das DiagramObj dargestellt werden
            AttrWidth = 305,	    // float, optional, für Notes und Frames
            AttrHeight = 306,       // float, optional, für Notes und Frames
            AttrKind = 317,         // quint8, Kind
            AttrPinnedTo = 318      // OID; optional; zeigt auf DiagItem, an welches dieses angeheftet ist
        };
        DiagItem( const Udb::Obj& o = Udb::Obj() ):Root(o){}
        void setPos( const QPointF& );
        QPointF getPos() const;
        void setNodeList( const QPolygonF& );
        QPolygonF getNodeList() const;
        bool hasNodeList() const;
        void setOrigObject( const Udb::Obj& );
        Udb::Obj getOrigObject() const;
        void setPinnedTo( const Udb::Obj& );
        Udb::Obj getPinnedTo() const;
		QList<DiagItem> getPinneds() const;
        Kind getKind() const;
        void setSize( const QSizeF& );
        QSizeF getSize() const;
        QRectF getBoundingRect() const;
        static DiagItem create(Udb::Obj diagram, Udb::Obj orig, const QPointF & p = QPointF());
        static DiagItem createKind(Udb::Obj diagram, Kind k, const QPointF & p = QPointF());
        static DiagItem createLink( Udb::Obj diagram, Udb::Obj pred, const Udb::Obj& succ );
        static QList<Udb::Obj> writeItems( const QList<Udb::Obj> & items, Stream::DataWriter& );
        static QList<Udb::Obj> readItems(Udb::Obj diagram, Stream::DataReader& );
    };

	class Node : public Root
	{
	public:
		Node( const Udb::Obj& o ):Root(o){}
		Node() {}
	};

	class Function : public Node
    {
    public:
        static int TID;
        enum Direction
        {
            TopToBottom = 0,
            LeftToRight = 1
        };

        enum Attrs
        {
            AttrElemCount = 307,     // uint32: Anzahl direkt enthaltener Functions, Events und Connectors
            AttrStart = 308,         // OID: Referenz auf Start-Event
            AttrFinish = 309,        // OID: Referenz auf Finish-Event
            AttrShowIds = 314,       // bool
            AttrMarkAlias = 315,     // bool
            AttrDirection = 316     // quint8, Direction
        };

		Function( const Udb::Obj& o ):Node(o){}
		Function(){}

        void setElemCount(quint32);
        quint32 getElemCount() const;
        void setStart( const Udb::Obj& );
        Udb::Obj getStart() const;
        void setFinish( const Udb::Obj& );
        Udb::Obj getFinish() const;
        void setDirection( int );
        Direction getDirection() const;
    };

	class Diagram : public Function
	{
	public:
		static int TID;
		Diagram( const Udb::Obj& o ):Function(o){}
		Diagram() {}
	};

	class FuncDomain : public Function
    {
    public:
        static int TID;
        static const QUuid s_uuid;
        FuncDomain( const Udb::Obj& o ):Function(o){}
		FuncDomain(){}
        static FuncDomain getOrCreateRoot( Udb::Transaction* );
    };

	class Event : public Node
    {
    public:
        static int TID;
		Event( const Udb::Obj& o ):Node(o){}
		Event() {}
    };

	class Connector : public Node
    {
    public:
        static int TID;
        enum Type
        {
            Unspecified = 0,
            And = 1,
            Or = 2,
            Xor = 3,
            Start = 4,
            Finish = 5
        };
        enum Attrs
        {
            AttrConnType = 311      // uint8, Type
        };

		Connector( const Udb::Obj& o ):Node(o){}
		Connector(){}

        void setConnType( int );
        Type getConnType() const;
    };

    class ConFlow : public Root // Control Flow
    {
    public:
        // Child of Pred
        static int TID;
        enum Attrs
        {
            AttrPred = 312,        // OID, Predecessor Function, Event or Connector
            AttrSucc = 313         // OID, Successor Function, Event or Connector
        };

        ConFlow( const Udb::Obj& o ):Root(o){}
		ConFlow(){}

        void setPred( const Udb::Obj& );
        Udb::Obj getPred() const;
        void setSucc( const Udb::Obj& );
        Udb::Obj getSucc() const;
    };

    class Allocation : public Root
    {
    public:
        // Child of Function
        static int TID;
        enum Attrs
        {
            AttrFunc = 319,        // OID, Function
            AttrElem = 320         // OID, SystemElement oder was immer
        };
        Allocation( const Udb::Obj& o ):Root(o){}
		Allocation(){}
        void setFunc( const Udb::Obj& );
        Udb::Obj getFunc() const;
        void setElem( const Udb::Obj& );
        Udb::Obj getElem() const;
    };

    class SystemElement : public Udb::ContentObject
    { // Nur hier für Convenience; wenn Epk zusammen mit Sdb verwendet wird, soll dessen Klasse verwendet werden
    public:
        static int TID;
        static const QUuid s_uuid;
        SystemElement( const Udb::Obj& o ):Udb::ContentObject(o){}
		SystemElement(){}
        static SystemElement getOrCreateRoot( Udb::Transaction* );
    };

    struct Index
    {
        static const char* Ident; // AttrIdent
        static const char* AltIdent; // AttrAltIdent
        static const char* OrigObject; // AttrOrigObject
        static const char* Pred; // AttrPred
        static const char* Succ; // AttrSucc
        static const char* PinnedTo; // AttrPinnedTo
        static const char* Func; // AttrFunc
        static const char* Elem; // AttrElem
        static void init( Udb::Database &db );
		static Udb::Obj getRoot( Udb::Transaction * );
	};
}

#endif // EPKOBJECTS_H
