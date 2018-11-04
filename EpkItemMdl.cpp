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

#include "EpkItemMdl.h"
#include "EpkProcs.h"
#include "EpkObjects.h"
#include "EpkItems.h"
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QDesktopWidget>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QPrinter>
#include <QSvgGenerator>
#include <QTextDocument>
#include <QTextStream>
#include <QFile>
#include <QMenu>
#include <QtDebug>
#include <Udb/Database.h>
#include <Udb/Idx.h>
#include <Oln2/OutlineMdl.h>
#include <Oln2/OutlineToHtml.h>
#include <math.h>
using namespace Epk;

const float EpkItemMdl::s_cellWidth = DiagItem::s_boxWidth * 1.25;
const float EpkItemMdl::s_cellHeight = DiagItem::s_boxHeight * 1.25;
const char* EpkItemMdl::s_mimeEvent = "application/flowline/event-ref";

EpkItemMdl::EpkItemMdl( QObject* p ):
    QGraphicsScene(p),d_mode(Idle),d_tempLine(0),d_tempBox(0),d_lastHitItem(0),
	d_readOnly(false),d_toEnlarge(false),d_strictSyntax(false),d_commitLock(false)
{
    QDesktopWidget dw;
    setSceneRect( dw.screenGeometry() );
	// setItemIndexMethod( QGraphicsScene::NoIndex ); // braucht es das?
}

void EpkItemMdl::fetchAttributes( EpkNode* i, const Udb::Obj& orig ) const
{
    if( i == 0 || orig.isNull() )
        return;
    Q_ASSERT( orig.getType() != DiagItem::TID || orig.getValue(DiagItem::AttrKind).getUInt8() != 0 );
    // Hier wird das Original erwartet
    i->setText( orig.getValue( Root::AttrText ).toString() );
    i->setId( Procs::formatObjectId( orig ) );
    i->setToolTip( Procs::formatObjectTitle( orig ) );
    switch( i->type() )
    {
    case EpkNode::_Function:
        i->setProcess( orig.getValue( Function::AttrElemCount ).getUInt32() > 0 );
        i->setAlias( !d_doc.equals( orig.getParent() ) );
        break;
    case EpkNode::_Event:
        i->setAlias( !d_doc.equals( orig.getParent() ) );
        break;
    case EpkNode::_Connector:
        i->setCode( orig.getValue( Connector::AttrConnType ).getUInt8() );
        i->setAlias( !d_doc.equals( orig.getParent() ) );
        break;
    case EpkNode::_Note:
        i->setWidth( orig.getValue( DiagItem::AttrWidth ).getFloat() );
        Udb::Obj(orig).setValue( DiagItem::AttrHeight, Stream::DataCell().setFloat( i->getSize().height() ) );
        break;
    case EpkNode::_Frame:
        i->setSize( QSizeF( orig.getValue( DiagItem::AttrWidth ).getFloat(),
                            orig.getValue( DiagItem::AttrHeight ).getFloat() ) );
        break;
    }
}

void EpkItemMdl::fetchItemFromDb( const Udb::Obj& obj, bool links, bool vertices )
{
    DiagItem diagItem = obj;
    Q_ASSERT( diagItem.getType() == DiagItem::TID );
    const Udb::Obj orig = diagItem.getValueAsObj( DiagItem::AttrOrigObject );
    if( orig.isNull( true ) )
    {
        d_orphans.append( obj );
        return; // Orphan
    }
    const quint32 type = orig.getType();
    const quint8 kind = diagItem.getKind();
    if( ( type == Function::TID || type == Event::TID || type == Connector::TID ||
          kind != DiagItem::Plain ) && vertices )
    {
        if( d_cache.contains( orig.getOid() ) )
        {
            // Das kann vorkommen, wenn setDiagram vor commit aufgerufen wird.
            qDebug() << "fetchItemFromDb Task already in diagram:" <<
                        Procs::prettyTypeName( orig.getType() ) << Procs::formatObjectTitle(orig);
            return; // Ein Object kann nur genau einmal auf einem Diagramm vorhanden sein
        }
        EpkNode::Type t = EpkNode::_Function;
        if( type == Event::TID )
            t = EpkNode::_Event;
        else if( type == Connector::TID )
            t = EpkNode::_Connector;
        else if( type == DiagItem::TID )
        {
            if( kind == DiagItem::Note )
                t = EpkNode::_Note;
            else if( kind == DiagItem::Frame )
                t = EpkNode::_Frame;
            else
                return;
        }
        EpkNode* i = new EpkNode( diagItem.getOid(), orig.getOid(), t );
        i->setPos( diagItem.getPos() );
        addItem( i ); // muss vor fetch stehen, da sonst scene nicht verfgbar
        fetchAttributes( i, orig );
        d_cache[diagItem.getOid()] = i;
        d_cache[orig.getOid()] = i;
    }
    if( type == ConFlow::TID && links )
    {
        if( d_cache.contains( orig.getOid() ) )
        {
            // Das kann vorkommen, wenn setDiagram vor commit aufgerufen wird.
            qDebug() << "fetchItemFromDb Link already in diagram:" << Procs::formatObjectTitle(orig);
            return; // Ein Object kann nur genau einmal auf einem Diagramm vorhanden sein
        }
        EpkNode* start = dynamic_cast<EpkNode*>(
                             d_cache.value( orig.getValue( ConFlow::AttrPred ).getOid() ) );
        EpkNode* end = dynamic_cast<EpkNode*>(
                           d_cache.value( orig.getValue( ConFlow::AttrSucc ).getOid() ) );
        if( start != 0 && end != 0 )
        {
            QPolygonF nl = diagItem.getNodeList();
            for( int j = 0; j < nl.size(); j++ )
            {
                EpkNode* n = new EpkNode(0,0,EpkNode::_Handle);
                n->setPos( nl[j] );
                addItem( n );
                LineSegment* s = addSegment( start, n );
                s->setToolTip( Procs::formatObjectTitle( orig ) );
                start = n;
            }
            LineSegment* lastSegment = addSegment( start, end, diagItem );
            lastSegment->setToolTip( Procs::formatObjectTitle( orig ) );
        }else
            d_orphans.append( obj ); // Der Link existiert zwar, aber nicht auf diesem Diagramm
    }
    if( links )
		installPin( diagItem );
}

void EpkItemMdl::setShowId( bool on )
{
    if( !d_doc.isNull() && !d_readOnly )
    {
        d_doc.setValue( Function::AttrShowIds, Stream::DataCell().setBool( on ) );
        d_doc.commit();
    }
}

bool EpkItemMdl::isShowId() const
{
    if( d_doc.isNull() )
        return false;
    Stream::DataCell v = d_doc.getValue( Function::AttrShowIds );
    if( v.isNull() )
        return true; // RISK
    else
        return v.getBool();
}

bool EpkItemMdl::isMarkAlias() const
{
    if( d_doc.isNull() )
        return false;
    Stream::DataCell v = d_doc.getValue( Function::AttrMarkAlias );
    if( v.isNull() )
        return d_doc.getType() != Diagram::TID; // RISK
    else
        return v.getBool();
}

void EpkItemMdl::setMarkAlias(bool on)
{
    if( !d_doc.isNull() && !d_readOnly )
    {
        d_doc.setValue( Function::AttrMarkAlias, Stream::DataCell().setBool( on ) );
        d_doc.commit();
    }
}

void EpkItemMdl::setDiagram( const Udb::Obj& doc )
{
    if( d_doc.equals( doc ) )
        return;
    if( !d_doc.isNull() )
        d_doc.getDb()->removeObserver( this, SLOT( onDbUpdate( Udb::UpdateInfo ) ) );
    clear();
    d_cache.clear();
    d_doc = doc;
    if( !d_doc.isNull() )
    {
        // Erzeuge zuerst die EpkItems ohne Handle und Segments
        Udb::Obj pdmItem = d_doc.getFirstObj();
        if( !pdmItem.isNull() ) do
        {
            if( pdmItem.getType() == DiagItem::TID )
                fetchItemFromDb( pdmItem, false, true );
        }while( pdmItem.next() );

        // Erzeuge nun die Handle und Segments
        pdmItem = d_doc.getFirstObj();
        if( !pdmItem.isNull() ) do
        {
            if( pdmItem.getType() == DiagItem::TID )
                fetchItemFromDb( pdmItem, true, false );
        }while( pdmItem.next() );

        if( !d_orphans.isEmpty() )
        {
            foreach( Udb::Obj o, d_orphans )
				Procs::erase( o );
            d_orphans.clear();
            d_doc.commit();
        }
        d_doc.getDb()->addObserver( this, SLOT( onDbUpdate( Udb::UpdateInfo ) ), false ); // neu synchron

        fitSceneRect();
        // qDebug() << "#Items" << items().size();
    }
}

void EpkItemMdl::keyPressEvent ( QKeyEvent * e )
{
    if( e->key() == Qt::Key_Escape && e->modifiers() == Qt::NoModifier && d_mode == AddingLink )
    {
        Q_ASSERT( d_tempLine != 0 );
        //removeItem( d_tempLine );
        delete d_tempLine;
        d_tempLine = 0;
        if( d_lastHitItem )
            deleteLinkOrHandle( d_lastHitItem );
        d_lastHitItem = 0;
        d_startItem = 0;
        d_mode = Idle;
    }
}

void EpkItemMdl::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if( event->mimeData()->hasFormat(Udb::Obj::s_mimeObjectRefs ) )
    {
		// Dieser Umweg ist noetig, damit onDrop asynchron aufgerufen werden kann.
        // Bei synchronem Aufruf gibt es eine Exception in QMutex::lock bzw. QMetaObject::activate
        // Auf Windows blockiert QDrag::exec laut Dokumentation die Event Queue, was die Ursache sein
		// koennte, zumal ja signals auch synchron irgendwie via Events versendet werden.
        emit signalDrop( event->mimeData()->data( Udb::Obj::s_mimeObjectRefs ), event->scenePos() );
        event->acceptProposedAction();
    }else
        event->ignore();
}

void EpkItemMdl::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if( event->mimeData()->hasFormat(Udb::Obj::s_mimeObjectRefs ) )
        event->acceptProposedAction();
    else
        event->ignore();
}

void EpkItemMdl::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    // NOP
}

void EpkItemMdl::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    // NOP
}

void EpkItemMdl::drawItems(QPainter *painter, int numItems, QGraphicsItem *items[], const QStyleOptionGraphicsItem options[], QWidget *widget)
{
    if( d_toEnlarge )
    {
        d_toEnlarge = false;
        enlargeSceneRect();
        //return;
    }
    QGraphicsScene::drawItems( painter, numItems, items, options, widget );
}

static inline bool _canAddHandle( const QList<QGraphicsItem *>& l )
{
    if( l.isEmpty() )
        return true;
    // Wenn sich darunter nur Handles befinden, knnen wir weitere draufsetzen
    for( int i = 0; i < l.size(); i++ )
    {
        switch( l[i]->type() )
        {
        case EpkNode::_Handle:
        case EpkNode::_Flow:
            break;
        default:
            return false;
        }
    }
    return true;
}

void EpkItemMdl::mousePressEvent(QGraphicsSceneMouseEvent * e)
{
    // Migrated
    d_startPos = e->scenePos();
    d_lastPos = d_startPos;
    if (e->button() != Qt::LeftButton)
        return;

    if( d_mode == AddingLink )
    {
        Q_ASSERT( d_tempLine != 0 );
        Q_ASSERT( d_lastHitItem != 0 );
        Q_ASSERT( d_startItem != 0 );
        QList<QGraphicsItem *> endItems = items( d_tempLine->line().p2() );
        if( endItems.count() && endItems.first() == d_tempLine )
            endItems.removeFirst();

        EpkNode* to = 0;
        if( !endItems.isEmpty() )
            to = dynamic_cast<EpkNode*>( endItems.first() );
        if( e->modifiers() == Qt::NoModifier && ( _canAddHandle( endItems ) || to == 0 ) )
        {
            // Erzeuge Punkt
            EpkNode* n = addHandle();
            addSegment( d_lastHitItem, n );
            d_lastHitItem = n;
            d_tempLine->setLine( QLineF( n->scenePos(), d_startPos ) );
        }else
        {
            CreateType type = _None;
            if( e->modifiers() == Qt::ControlModifier )
            {
                // Zeige ein Men mit den Elementen, die hier erzeugt werden knnen.
                QMenu pop;
                pop.addAction( QIcon( ":/FlowLine2/Images/function.png" ),
                               Procs::prettyTypeName( Function::TID ) )->setData( _Function );
                pop.addAction( QIcon( ":/FlowLine2/Images/event.png" ),
                               Procs::prettyTypeName( Event::TID ) )->setData( _Event );
                pop.addAction( QIcon( ":/FlowLine2/Images/connector.png" ),
                               Procs::formatConnType( Connector::And, true ) )->setData( _AndConn );
                pop.addAction( QIcon( ":/FlowLine2/Images/connector.png" ),
                               Procs::formatConnType( Connector::Or, true ) )->setData( _OrConn );
                pop.addAction( QIcon( ":/FlowLine2/Images/connector.png" ),
                               Procs::formatConnType( Connector::Xor, true ) )->setData( _XorConn );
                pop.addAction( QIcon( ":/FlowLine2/Images/connector.png" ),
                               Procs::formatConnType( Connector::Start, true ) )->setData( _StartConn );
                pop.addAction( QIcon( ":/FlowLine2/Images/connector.png" ),
                               Procs::formatConnType( Connector::Finish, true ) )->setData( _FinishConn );
                if( _canAddHandle( endItems ) )
                    pop.addAction( QIcon( ":/FlowLine2/Images/handle.png" ), tr("Handle") )->setData( _Handle );
                QAction* res = pop.exec( e->screenPos() );
                if( res )
                    type = (CreateType)res->data().toInt();
                if( type == _Handle )
                {
                    EpkNode* n = addHandle();
                    addSegment( d_lastHitItem, n );
                    d_lastHitItem = n;
                    d_tempLine->setLine( QLineF( n->scenePos(), d_startPos ) );
                    return;
                }
            }
            if( ( to != 0 || type != _None ) && d_startItem != to && canEndLink( to ) )
            {
                Q_ASSERT( !d_doc.isNull() );
                Udb::Obj predObj = d_doc.getObject( d_startItem->getOrigOid() );
                Udb::Obj succObj;
                QPolygonF path;
                if( to )
                {
                    succObj = d_doc.getObject( to->getOrigOid() );
                    LineSegment* f = addSegment( d_lastHitItem, to );
                    path = getNodeList( f );
                    deleteLinkOrHandle( f );
                }else
                {
                    path = getNodeList( d_lastHitItem );
                    deleteLinkOrHandle( d_lastHitItem );
                }
                Q_ASSERT( d_tempLine != 0 );
                removeItem( d_tempLine );
                delete d_tempLine;
                d_tempLine = 0;
                d_lastHitItem = 0;
                d_startItem = 0;
                d_mode = Idle;
                if( type == _None )
                    emit signalCreateLink( predObj, succObj, path );
                else
                    emit signalCreateSuccLink( predObj, type, rastered(d_startPos), path );
            }
        }
    }else
    {
        Q_ASSERT( d_mode == Idle );
        QGraphicsItem* i = itemAt( d_startPos );
        if( i == 0 )
        {
            QGraphicsScene::mousePressEvent(e);
        }else
        {
            if( e->modifiers() == Qt::ShiftModifier )
            {
                // Es geht um Selektionen
                if( i->isSelected() )
                    i->setSelected( false );
                else
                    i->setSelected( true );
                if( !d_readOnly )
                    d_mode = PrepareMove;
            }else
            {
                if( !i->isSelected() )
                {
                    clearSelection();
                    i->setSelected( true );
                }
                if( !d_readOnly )
                {
                    d_startItem = dynamic_cast<EpkNode*>( i );
                    d_lastHitItem = d_startItem;
                    if( d_startItem && e->modifiers() == Qt::ControlModifier && canStartLink( d_startItem ) )
                    {
                        // Beginne zeichnen eines ControlFlows
                        d_mode = AddingLink;
                        d_tempLine = new QGraphicsLineItem( QLineF(d_startItem->scenePos(), e->scenePos() ) );
                        d_tempLine->setZValue( 1000 );
                        d_tempLine->setPen(QPen(Qt::gray, 1));
                        addItem(d_tempLine);
                    }else if( d_startItem && canScale( d_startItem ) && e->modifiers() == Qt::ControlModifier )
                        d_mode = Scaling; // Figuren skalieren
                    else if( e->modifiers() == Qt::NoModifier )
                        d_mode = PrepareMove;
                }
				EpkNode* node = dynamic_cast<EpkNode*>( i );
				if( node && selectedItems().size() == 1 && e->modifiers() == Qt::NoModifier && node->pinnedTo() )
				{
					// Zeichne temporr einen Rahmen um das Objekt, an welches node gepinnt ist
					d_tempBox = new QGraphicsPathItem( node->pinnedTo()->shape() );
					d_tempBox->setPos( node->pinnedTo()->pos() );
					d_tempBox->setZValue( node->pinnedTo()->zValue() + 1 );
					d_tempBox->setPen(QPen(Qt::blue, 2, Qt::DotLine));
					addItem(d_tempBox);
				}
			}
        }
    }
}

bool EpkItemMdl::canScale( EpkNode* i ) const
{
    return i->type() == EpkNode::_Note || i->type() == EpkNode::_Frame;
}

void EpkItemMdl::mouseMoveEvent(QGraphicsSceneMouseEvent * e)
{
    // migrated
    if( d_mode == AddingLink )
    {
        if( d_tempLine != 0 )
            d_tempLine->setLine( QLineF( d_tempLine->line().p1(), e->scenePos() ) );
    }else if( d_mode == Moving )
    {
        Q_ASSERT( d_tempBox != 0 );
        d_lastPos = e->scenePos();
        d_tempBox->setPos( rastered( d_lastPos - d_startPos ) );
    }else if( d_mode == Scaling )
    {
        Q_ASSERT( d_lastHitItem != 0 );
        QPointF p = e->scenePos() - e->lastScenePos();
        d_lastHitItem->adjustSize( p.x(), p.y() );
    }else if( d_mode == PrepareMove )
    {
        QPoint pos = QPointF( e->scenePos() - d_startPos ).toPoint();
        if( pos.manhattanLength() > 5 )
        {
            QPainterPath path;
            QList<QGraphicsItem*> l = selectedItems();
            for( int i = 0; i < l.size(); i++ )
            {
                if( dynamic_cast<EpkNode*>( l[i] ) )
                {
                    QPainterPath s = l[i]->shape();
                    QPointF off = l[i]->scenePos();
                    for( int j = 0; j < s.elementCount(); j++ )
                    {
                        QPointF pos = s.elementAt( j );
                        s.setElementPositionAt( j, pos.x() + off.x(), pos.y() + off.y() );
                    }
                    path.addPath( s );
                }
            }
            d_lastPos = e->scenePos();
			if( d_tempLine )
				delete d_tempLine;
			d_tempLine = 0;
			if( d_tempBox )
				delete d_tempBox;
            d_tempBox = new QGraphicsPathItem( path );
            d_tempBox->setPos( rastered( d_lastPos - d_startPos ) );
            d_tempBox->setZValue( 1000 );
            d_tempBox->setPen(QPen(Qt::gray, 1));
            addItem(d_tempBox);
            d_mode = Moving;
        }
    }else
    {
        QGraphicsScene::mouseMoveEvent( e );
    }
}

void EpkItemMdl::enlargeSceneRect()
{
    // migrated
    QRectF sr = sceneRect();
    const QRectF br = itemsBoundingRect();
    QDesktopWidget dw;
    const QRect screen = dw.screenGeometry();

    if( br.top() < sr.top() )
        sr.adjust( 0, - screen.height(), 0, 0 );
    if( br.bottom() > sr.bottom() )
        sr.adjust( 0, 0, 0, screen.height() );
    if( br.left() < sr.left() )
        sr.adjust( - screen.width(), 0, 0, 0 );
    if( br.right() > sr.right() )
        sr.adjust( 0, 0, screen.width(), 0 );

    setSceneRect( sr );
}

void EpkItemMdl::fitSceneRect(bool forceFit)
{
    QRectF r = itemsBoundingRect().adjusted(
        -DiagItem::s_boxWidth * 0.5, -DiagItem::s_boxHeight * 0.5,
        DiagItem::s_boxWidth * 0.5, DiagItem::s_boxHeight * 0.5 );
    if( !forceFit )
    {
        qreal diff = sceneRect().width() - r.width();
        if( diff > 0.0 )
            r.adjust( -diff * 0.5, 0, diff * 0.5, 0 );
        diff = sceneRect().height() - r.height();
        if( r.height() < sceneRect().height() )
            r.adjust( 0, -diff * 0.5, 0, diff * 0.5 );
    }
    setSceneRect( r );
}

void EpkItemMdl::mouseReleaseEvent(QGraphicsSceneMouseEvent * e)
{
    // migrated
    if( d_mode == AddingLink )
    {
        // NOP
    }else if( d_mode == Moving )
    {
        Q_ASSERT( d_tempBox != 0 );
        QPointF off = d_lastPos - d_startPos;
        QList<QGraphicsItem*> l = selectedItems();
		d_commitLock = true;
        for( int i = 0; i < l.size(); i++ )
        {
            if( EpkNode* ei = dynamic_cast<EpkNode*>( l[i] ) )
            {
                rasteredMoveBy( ei, off.x(), off.y() );
            }
        }
        if( !d_doc.isNull() )
            d_doc.commit();
		d_commitLock = false;
        enlargeSceneRect();
        d_mode = Idle;
    }else if( d_mode == Scaling )
    {
        if( EpkNode* ni = dynamic_cast<EpkNode*>( d_lastHitItem ) )
        {
            if( ni->type() == EpkNode::_Note || ni->type() == EpkNode::_Frame )
            {
                DiagItem o = d_doc.getObject( ni->getOrigOid() );
                o.setSize( ni->getSize() );
                o.commit();
            }
        }
        d_lastHitItem = 0;
        d_startItem = 0;
        d_mode = Idle;
    }else
    {
        d_mode = Idle;
        QGraphicsScene::mouseReleaseEvent( e );
    }
	if( d_tempBox )
		delete d_tempBox;
	d_tempBox = 0;
}

QPointF EpkItemMdl::toCellPos( const QPointF& pos ) const
{
    return QPointF( s_cellWidth * ::floor( pos.x() / s_cellWidth ),
            s_cellHeight * ::floor( pos.y() / s_cellHeight ) );
}

QPolygonF EpkItemMdl::getNodeList( QGraphicsItem* i ) const
{
    if( i == 0 )
        return QPolygonF();
    else if( i->type() == EpkNode::_Handle )
    {
        return getNodeList( static_cast<EpkNode*>( i )->getFirstInSegment() );
        // Die Points werden immer ausgehend von einem ControlFlow ermittelt. Wenn das Objekt also
        // ein Handle ist, wird der Flow verwendet, der in den Handle reinmndet. Da ein Handle nie alleine
		// stehen kann und immer genau ein Flow einmndet, duerfte getInFlow() nie null zurckgeben.
    }else if( i->type() == EpkNode::_Flow )
    {
        LineSegment* f = static_cast<LineSegment*>( i );
        if( f->getStartItem()->type() == EpkNode::_Handle )
            // f ist noch nicht das erste Segment im Flow.
            return getNodeList( f->getStartItem() );
        else
        {
            // f ist nun das erste Segment im Flow. Gehe nun bis zum Ende und sammle die Handle-Punkte ein.
            QPolygonF res;
            while( f && f->getEndItem()->type() == EpkNode::_Handle )
            {
                EpkNode* n = f->getEndItem();
                res.append( n->scenePos() );
                f = n->getFirstOutSegment();
            }
            return res;
        }
    }//else
    return QPolygonF();
}

EpkNode* EpkItemMdl::addHandle()
{
    if( d_readOnly )
        return 0;
    // Handle hat keine Oid. Stattdessen liegt die Oid auf dem letzten Link-Segment vor dem Target.
    EpkNode* i = new EpkNode(0,0,EpkNode::_Handle);
    i->setPos( rastered( d_startPos ) );
    addItem( i );
    return i;
}

LineSegment* EpkItemMdl::addSegment( EpkNode* from, EpkNode* to, const Udb::Obj &pdmItem )
{
    // migrated
    const quint32 origID = ( !pdmItem.isNull() )?pdmItem.getValue( DiagItem::AttrOrigObject ).getOid():0;
    LineSegment* segment = new LineSegment( pdmItem.getOid(), origID );
    from->addLine(segment, true);
    to->addLine(segment, false);
    addItem(segment);
    segment->updatePosition();
    if( !pdmItem.isNull() )
    {
        d_cache[ segment->getItemOid() ] = segment;
        d_cache[ origID ] = segment;
    }
    return segment;
}

QPointF EpkItemMdl::rastered( const QPointF& p )
{
    QPointF res = p;
    res.setX( ::floor( res.x() / DiagItem::s_rasterX + 0.5 ) * DiagItem::s_rasterX );
    res.setY( ::floor( res.y() / DiagItem::s_rasterY + 0.5 ) * DiagItem::s_rasterY );
    return res;
}

void EpkItemMdl::removeFromCache(QGraphicsItem * i)
{
    // Nur hier wird aus dem Cache gelscht und nur der Destructor von Item ruft die Methode auf
    if( LineSegment* link = dynamic_cast<LineSegment*>( i ) )
    {
        d_cache.remove( link->getOrigOid() );
        d_cache.remove( link->getItemOid() );
    }else if( EpkNode* item = dynamic_cast<EpkNode*>( i ) )
    {
        d_cache.remove( item->getOrigOid() );
        d_cache.remove( item->getItemOid() );
	}
}

void EpkItemMdl::movePinned(const QList<EpkNode *> &l, const QPointF &diff)
{
	foreach( EpkNode* p, l )
	{
		DiagItem o = d_doc.getObject( p->getItemOid() );
		QPointF newPos = p->pos() + diff;
		o.setPos( newPos );
		p->setPos( newPos );
		Q_ASSERT( p->type() != EpkNode::_Handle );
		for( int i = 0; i < p->getLinks().size(); i++ )
		{
			if( p->getLinks()[i]->getStartItem() == p )
			{
				// Node hat ausgehende Links. Verschiebe alle Handles
				LineSegment* seg = p->getLinks()[i];
				LineSegment* last = seg->getLastSegment();
				if( seg != last )
				{
					// Der ausgehende Link enthlt Handles
					while( seg != last )
					{
						EpkNode* h = seg->getEndItem();
						Q_ASSERT( h->type() == EpkNode::_Handle );
						h->setPos( h->pos() + diff );
						seg = h->getFirstOutSegment();
					}
					DiagItem s = d_doc.getObject( last->getItemOid() );
					QPolygonF nl = s.getNodeList();
					nl.translate(diff);
					s.setNodeList( nl );
				}
			}
		}
	}
	if( !d_commitLock )
		d_doc.commit();
}

void EpkItemMdl::rasteredMoveBy( EpkNode* i, qreal dx, qreal dy )
{
    // migrated
    QPointF pos = i->pos() + QPointF( dx, dy );
    i->setPos( rastered( pos ) );
    if( !d_doc.isNull() )
    {
        if( i->type() == EpkNode::_Handle )
        {
            LineSegment* f = i->getLastSegment();
            Q_ASSERT( f != 0 && f->getItemOid() != 0 );
            DiagItem o = d_doc.getObject( f->getItemOid() );
            o.setNodeList( getNodeList( f ) );
        }else
        {
            Q_ASSERT( i->getItemOid() != 0 );
            DiagItem o = d_doc.getObject( i->getItemOid() );
            o.setPos( i->scenePos() ); // Gespeichert werden immer absolute Koordinaten, egal ob Pinned oder nicht
        }
    }
}

bool EpkItemMdl::canStartLink( EpkNode* i ) const
{
    if( i->type() == EpkNode::_Function  || i->type() == EpkNode::_Event )
    {
        if( !d_strictSyntax )
            return true;
        if( i->getLinks().isEmpty() )
            return true;
        if( i->getLinks().size() >= 2 )
            return false;
        if( i->getLinks().first()->getStartItem() == i )
            return false; // Es geht bereits ein Arrow davon weg
        return true;
    }else if( i->type() == EpkNode::_Connector  )
        return true;
    else
        return false;
}

bool EpkItemMdl::canEndLink( EpkNode* i ) const
{
    if( i == 0 )
        return true;
    if( i->type() == EpkNode::_Function  || i->type() == EpkNode::_Event )
    {
        if( !d_strictSyntax )
            return true;
        if( i->getLinks().isEmpty() )
            return true;
        if( i->getLinks().size() >= 2 )
            return false;
        if( i->getLinks().first()->getEndItem() == i )
            return false; // Es geht bereits ein Arrow davon weg
        return true;
    }else if( i->type() == EpkNode::_Connector  )
        return true;
    else
        return false;
}

void EpkItemMdl::deleteAllLinkSegments( LineSegment* segment )
{
    // Arbeitet sich vom ersten bis zum letzten durch und lscht auch alle
    // Handles, die unterwegs auftauchen
    EpkNode* item = segment->getEndItem();
    if( item && item->type() == EpkNode::_Handle )
    {
        foreach( LineSegment* nextSegment, item->getLinks() )
        {
            if( nextSegment != segment )
            {
                deleteAllLinkSegments( nextSegment );
                break;
            }
        }
        delete item;
    }
    delete segment;
}

void EpkItemMdl::deleteLinkOrHandle( QGraphicsItem* linkOrHandle )
{
    if( linkOrHandle->type() == EpkNode::_Flow )
    {
        LineSegment* link = static_cast<LineSegment*>( linkOrHandle )->getFirstSegment();
        deleteAllLinkSegments( link );
    }else if( linkOrHandle->type() == EpkNode::_Handle )
    {
        EpkNode* handle = static_cast<EpkNode*>( linkOrHandle );
        deleteLinkOrHandle( handle->getFirstSegment() );
    }
}

void EpkItemMdl::removeHandle( EpkNode *handle )
{
    // Lsche die Kante davor und verbinde die nachfolgende mit dem Handle davor.
    LineSegment* prev = handle->getFirstInSegment();
    LineSegment* next = handle->getFirstOutSegment();
	LineSegment* last = handle->getLastSegment(); // Hier muesste der OID sein
    EpkNode* start = prev->getStartItem();
    start->removeLine( prev );
    start->addLine( next, true );
    next->updatePosition();
    delete prev;
    delete handle;
    Q_ASSERT( !d_doc.isNull() );
    if( last->getItemOid() )
    {
        DiagItem o = d_doc.getObject( last->getItemOid() );
        o.setNodeList( getNodeList( start ) );
    }
}

bool EpkItemMdl::insertHandle()
{
    // migrated
    if( d_readOnly )
        return false;
    if( selectedItems().isEmpty() || selectedItems().first()->type() != EpkNode::_Flow )
        return false;
    LineSegment* f = static_cast<LineSegment*>( selectedItems().first() );
    EpkNode* start = f->getStartItem();
    start->removeLine( f );
    EpkNode* n = addHandle();
    n->addLine( f, true );
    f->updatePosition();
    addSegment( start, n );
	LineSegment* last = n->getLastSegment(); // Hier muesste der OID sein
    if( !d_doc.isNull() && last->getItemOid() )
    {
        DiagItem o = d_doc.getObject( last->getItemOid() );
        o.setNodeList( getNodeList( start ) );
    }
    clearSelection();
    n->setSelected(true);
    n->update();
    return true;
}

void EpkItemMdl::removeNode( QGraphicsItem* i )
{
    Q_ASSERT( i->type() == EpkNode::_Event || i->type() == EpkNode::_Function ||
              i->type() == EpkNode::_Connector ||
              i->type() == EpkNode::_Note || i->type() == EpkNode::_Frame );
    EpkNode* item = static_cast<EpkNode*>( i );
    // Lsche zuerst alle Links
    while( !item->getLinks().isEmpty() )
    {
        deleteLinkOrHandle( item->getLinks().first() );
    }
    delete item;
}

void EpkItemMdl::setItemText( QGraphicsItem* i, const QString& str )
{
	// migriert
	if( d_readOnly )
		return;
	if( EpkNode* ei = dynamic_cast<EpkNode*>( i ) )
	{
		ei->update();
		Q_ASSERT( !d_doc.isNull() );
		Q_ASSERT( ei->getItemOid() != 0 );
		Q_ASSERT( ei->getOrigOid() != 0 );
		Udb::Obj orig = d_doc.getObject( ei->getOrigOid() );
		if( !orig.isNull(true) )
		{
			// DB WRITE
			orig.setString( Root::AttrText, str );
			orig.setTimeStamp( Root::AttrModifiedOn );
			orig.commit();
		}
	}
}

void EpkItemMdl::installPin( const DiagItem& diagItem )
{
	const Udb::Obj to = diagItem.getPinnedTo();
	EpkNode* diagNode = dynamic_cast<EpkNode*>( d_cache.value( diagItem.getOid() ) );
	if( !to.isNull() )
	{
		if( diagNode && ( diagNode->pinnedTo() == 0 || diagNode->pinnedTo()->getItemOid() != to.getOid() ) )
		{
			EpkNode* toNode = dynamic_cast<EpkNode*>( d_cache.value( to.getOid() ) );
			if( toNode )
				diagNode->setPinnedTo( toNode );
			else
				qDebug() << "EpkItemMdl::installPin: cannot find pinning target";
		}
	}else
	{
		if( diagNode && diagNode->pinnedTo() )
		{
			diagNode->setPinnedTo( 0 );
		} // else bereits vollzogen
	}
}

void EpkItemMdl::onDbUpdate( Udb::UpdateInfo info )
{
    Q_ASSERT( !d_doc.isNull() );
    switch( info.d_kind )
    {
    case Udb::UpdateInfo::TypeChanged:
        if( info.d_name == Function::TID || info.d_name == Event::TID )
        {
            QGraphicsItem* i = d_cache.value( info.d_id );
            if( i!= 0 )
            {
                switch( i->type() )
                {
                case EpkNode::_Function:
                case EpkNode::_Event:
                case EpkNode::_Connector:
                case EpkNode::_Note:
                    {
                        EpkNode* item = static_cast<EpkNode*>( i );
                        if( info.d_name == Function::TID )
                            item->setType( EpkNode::_Function );
                        else if( info.d_name == Event::TID )
                            item->setType( EpkNode::_Event );
                        else if( !Procs::isDiagNode( info.d_name ) )
                        {
                            removeNode( item );
                            return;
                        }
                        Udb::Obj o = d_doc.getObject( item->getOrigOid() );
                        fetchAttributes( item, o );
                    }
                    break;
                }
            }
        }
        break;
    case Udb::UpdateInfo::ValueChanged:
        if( info.d_name == Root::AttrText || info.d_name == Root::AttrIdent
                || info.d_name == Root::AttrAltIdent )
        {
            QGraphicsItem* gi = d_cache.value( info.d_id );
            EpkNode* pi = dynamic_cast<EpkNode*>( gi );
            LineSegment* ls = dynamic_cast<LineSegment*>( gi );
            if( pi && pi->getOrigOid() == info.d_id )
            {
                Udb::Obj o = d_doc.getObject( info.d_id );
                fetchAttributes( pi, o );
                pi->update();
            }else if( ls && ls->getOrigOid() == info.d_id )
            {
                Udb::Obj o = d_doc.getObject( info.d_id );
                ls->setToolTip( Procs::formatObjectTitle( o ) );
                ls->update();
            }
        }else if( info.d_name == Function::AttrElemCount )
        {
            EpkNode* pi = dynamic_cast<EpkNode*>( d_cache.value( info.d_id ) );
            if( pi )
            {
                Udb::Obj o = d_doc.getObject( info.d_id );
                pi->setProcess( o.getValue( Function::AttrElemCount ).getUInt32() > 0 );
                pi->update();
            }
        }else if( info.d_name == Connector::AttrConnType )
        {
            EpkNode* pi = dynamic_cast<EpkNode*>( d_cache.value( info.d_id ) );
            if( pi && pi->type() == EpkNode::_Connector )
            {
                Udb::Obj o = d_doc.getObject( info.d_id );
                pi->setCode( o.getValue( Connector::AttrConnType ).getUInt8() );
                pi->update();
            }
        }else if( info.d_name == DiagItem::AttrPinnedTo )
        {
			installPin( d_doc.getObject( info.d_id ) );
		}
		// Braucht es nicht, da Positionen von Elementen und Nodes immer nur via GUI geaendert werden
//		else if( info.d_name == DiagItem::AttrPosX ) // tritt immer zusammen mit AttrPosY auf,
//		{
//			EpkNode* pi = dynamic_cast<EpkNode*>( d_cache.value( info.d_id ) );
//			if( pi )
//			{
//				DiagItem i = d_doc.getObject( info.d_id );
//				pi->setPos( i.getPos() );
//			}
//		}else if( info.d_name == DiagItem::AttrNodeList )
//		{
//			LineSegment* pi = dynamic_cast<LineSegment*>( d_cache.value( info.d_id ) );
//			if( pi && pi->getStartItem()->type() == EpkNode::_Handle )
//			{
//				EpkNode* node = pi->getStartItem();
//				DiagItem item = d_doc.getObject( info.d_id );
//				const QPolygonF nl = item.getNodeList();
//				int n = nl.size() - 1;
//				while( n >= 0 && node != 0 )
//				{
//					node->setPos( nl[n] );
//					n--;
//				}
//			}
//		}
        break;
    case Udb::UpdateInfo::ObjectErased:
        {
            QGraphicsItem* i = d_cache.value( info.d_id );
            if( i!= 0 )
            {
                // TODO: wird hier Item aus Cache entfernt?
                switch( i->type() )
                {
                case EpkNode::_Flow:
                case EpkNode::_Handle:
					// Wenn Orig zuerst geloescht wird ist das nicht tragisch, da Item beim naechsten
					// oeffnen als Orphan erkannt und geloescht wird.
                    deleteLinkOrHandle( i );
                    break;
                case EpkNode::_Function:
                case EpkNode::_Event:
                case EpkNode::_Connector:
                case EpkNode::_Note:
                case EpkNode::_Frame:
					// Wenn Orig zuerst geloescht wird ist das nicht tragisch, da Item beim naechsten
					// oeffnen als Orphan erkannt und geloescht wird.
                    removeNode( i );
                    break;
                }
            }
        }
        break;
    case Udb::UpdateInfo::Aggregated:
        if( info.d_parent == d_doc.getOid() )
        {
            DiagItem pdmItem = d_doc.getObject( info.d_id );
            if( pdmItem.getType() == DiagItem::TID )
            {
                fetchItemFromDb( pdmItem, true, true );
                d_toEnlarge = true;
            }else
            {
                Q_ASSERT( pdmItem.getType() != DiagItem::TID );
                EpkNode* pi = dynamic_cast<EpkNode*>( d_cache.value( info.d_id ) );
                if( pi && pi->getOrigOid() == info.d_id )
                {
                    pi->setAlias( false );
                    pi->update();
                }
            }
        }
        break;
    case Udb::UpdateInfo::Deaggregated:
        if( info.d_parent == d_doc.getOid() )
        {
            EpkNode* pi = dynamic_cast<EpkNode*>( d_cache.value( info.d_id ) );
            if( pi && pi->getOrigOid() == info.d_id )
            {
                pi->setAlias( true );
                pi->update();
            }
        }
        break;
    }
}

Udb::Obj EpkItemMdl::getSingleSelection() const
{
    // migriert
    if( selectedItems().size() != 1 || d_doc.isNull() )
        return Udb::Obj();
    QList<Udb::Obj> res = getMultiSelection();
    if( !res.isEmpty() )
        return res.first();
    else
        return Udb::Obj();
}

QList<Udb::Obj> EpkItemMdl::getMultiSelection(bool elems, bool link,
                                              bool handle) const
{
    QList<Udb::Obj> res;
    QList<QGraphicsItem*> sel = selectedItems();
    foreach( QGraphicsItem* i, sel )
    {
        switch( i->type() )
        {
        case EpkNode::_Function:
        case EpkNode::_Event:
        case EpkNode::_Connector:
        case EpkNode::_Note:
        case EpkNode::_Frame:
            if( elems )
                res.append( d_doc.getObject( static_cast<EpkNode*>( i )->getItemOid() ) );
            break;
        case EpkNode::_Flow:
            if( link )
            {
                LineSegment* f = static_cast<LineSegment*>( i );
                f = f->getLastSegment();
                Udb::Obj link = d_doc.getObject( f->getItemOid() );
                if( !res.contains( link ) )
                    res.append( link );
            }
            break;
        case EpkNode::_Handle:
            if( handle )
            {
                EpkNode* n = static_cast<EpkNode*>( i );
                LineSegment* f = n->getLastSegment();
                Udb::Obj link = d_doc.getObject( f->getItemOid() );
                if( !res.contains( link ) )
                    res.append( link );
            }
            break;
        default:
            Q_ASSERT( false );
        }
    }
    return res;
}

QGraphicsItem* EpkItemMdl::selectObject( const Udb::Obj& o, bool clearSel )
{
    // migriert
    QGraphicsItem* i = d_cache.value( o.getOid() );
    if( i != 0 )
    {
        if( clearSel )
            clearSelection();
        i->setSelected( true );
        if( LineSegment* ls = dynamic_cast<LineSegment*>(i) )
        {
            foreach( LineSegment* l, ls->getChain() )
                l->setSelected( true );
        }
    }
    return i;
}

void EpkItemMdl::selectObjects(const QList<Udb::Obj> &obs, bool clearSel)
{
    bool first = clearSel;
    foreach( Udb::Obj o, obs )
    {
        selectObject( o, first );
        first = false;
    }
}

void EpkItemMdl::removeSelectedItems()
{
    // NOTE: Diese Methode ist eine Ausnahme, da ich keine Lust habe, Internas der PdmItems nach PdmCtrl
	// zu exportieren. Daher wird hier geloescht.
    QList<QGraphicsItem*> sel = selectedItems();
    QList<Udb::Obj> toRemoveItems;
    QList<EpkNode*> toRemoveHandles;
    foreach( QGraphicsItem * i, sel )
    {
        switch( i->type() )
        {
        case EpkNode::_Flow:
            {
                LineSegment* s = static_cast<LineSegment*>( i );
                s = s->getLastSegment();
                Udb::Obj item = d_doc.getObject( s->getItemOid() );
                if( !toRemoveItems.contains( item ) )
                    toRemoveItems.append( item );
                // Links sind auf mehr als ein Segment abgebildet
            }
            break;
        case EpkNode::_Handle:
            toRemoveHandles.append( static_cast<EpkNode*>( i ) );
            break;
        case EpkNode::_Function:
        case EpkNode::_Event:
        case EpkNode::_Connector:
        case EpkNode::_Note:
        case EpkNode::_Frame:
            {
                EpkNode* p = static_cast<EpkNode*>( i );
                Udb::Obj item = d_doc.getObject( p->getItemOid() );
                Q_ASSERT( !toRemoveItems.contains( item ) ); // Darf nicht vorkommen
                toRemoveItems.append( item );
				// Sorge dafuer, dass auch die zu- und wegfhuerenden Links ordentlich geloescht werden
                foreach( LineSegment* s, p->getLinks() )
                {
                    s = s->getLastSegment();
                    item = d_doc.getObject( s->getItemOid() );
                    if( !toRemoveItems.contains( item ) )
                        toRemoveItems.append( item );
                }
            }
            break;
        default:
            Q_ASSERT( false ); // RISK
        }
    }
    foreach( EpkNode* h, toRemoveHandles )
    {
        Udb::Obj o = d_doc.getObject( h->getLastSegment()->getItemOid() );
		if( !toRemoveItems.contains( o ) ) // Das Handle nur entfernen, wenn nicht gleich der Link geloescht wird
            removeHandle( h );
    }
    foreach( Udb::Obj item, toRemoveItems )
    {
		Procs::erase(item);
    }
    d_doc.commit();
}

bool EpkItemMdl::canStartLink() const
{
    if( d_readOnly )
        return false;
    if( d_mode == Idle )
    {
        QGraphicsItem* i = itemAt( d_startPos );
        if( i != 0 )
        {
            EpkNode* ei = dynamic_cast<EpkNode*>( i );
            if( ei && canStartLink( ei ) )
                return true;
        }
    }
    return false;
}

void EpkItemMdl::startLink()
{
    if( d_readOnly )
        return;
    if( d_mode == Idle )
    {
        QGraphicsItem* i = itemAt( d_startPos );
        if( i != 0 )
        {
            if( !i->isSelected() )
            {
                clearSelection();
                i->setSelected( true );
            }
            d_startItem = dynamic_cast<EpkNode*>( i );
            d_lastHitItem = d_startItem;
            if( d_startItem && canStartLink( d_startItem ) )
            {
                // Wie in mousePressEvent
                d_mode = AddingLink;
                d_tempLine = new QGraphicsLineItem( QLineF(d_startItem->scenePos(), d_startPos ) );
                d_tempLine->setZValue( 1000 );
                d_tempLine->setPen(QPen(Qt::gray, 1));
                addItem(d_tempLine);
            }
        }
    }
}

QPointF EpkItemMdl::getStart(bool rastered) const
{
    if( rastered )
        return this->rastered( d_startPos );
    else
        return d_startPos;
}


void EpkItemMdl::exportPng( const QString& path )
{
    clearSelection();
    QBrush back = backgroundBrush();
    setBackgroundBrush( Qt::white );
    QRectF b = itemsBoundingRect().adjusted( -DiagItem::s_boxWidth * 0.5, -DiagItem::s_boxHeight * 0.5,
        DiagItem::s_boxWidth * 0.5, DiagItem::s_boxHeight * 0.5 );
    QImage img( b.size().toSize(), QImage::Format_RGB32 );
    QPainter painter( &img );
    painter.setRenderHints( QPainter::Antialiasing | QPainter::TextAntialiasing );
    render( &painter, QRectF( QPointF(0.0,0.0), b.size() ), b );
    if( !path.isEmpty() )
        img.save( path, "PNG" );
    else
        QApplication::clipboard()->setImage( img );
    setBackgroundBrush( back );
}

static inline bool _hasOutline( const Udb::Obj& o, bool followAlias = false )
{
    if( o.isNull() )
        return false;
    Udb::Obj ali;
    if( followAlias )
        ali = o.getValueAsObj( Oln::OutlineItem::AttrAlias );
    if( ali.isNull() )
        ali = o;
    Udb::Obj s = ali.getFirstObj();
    if( !s.isNull() ) do
    {
        if( s.getType() == Oln::OutlineItem::TID )
            return true;
    }while( s.next() );
    return false;
}

void EpkItemMdl::exportPdf( const QString& path, bool withDetails )
{
    clearSelection();
    QBrush back = backgroundBrush();
    setBackgroundBrush( Qt::white );
    QRectF b = itemsBoundingRect().adjusted( -DiagItem::s_boxWidth * 0.5, -DiagItem::s_boxHeight * 0.5,
        DiagItem::s_boxWidth * 0.5, DiagItem::s_boxHeight * 0.5 );
    QPrinter prn(QPrinter::PrinterResolution);
    prn.setPaperSize(QPrinter::A4);
    if( b.width() > b.height() )
        prn.setOrientation( QPrinter::Landscape );
    prn.setOutputFormat( QPrinter::PdfFormat );
    prn.setOutputFileName( path );
    QPainter painter( &prn );
    render( &painter, QRectF(), b );
    setBackgroundBrush( back );
}

static QString _coded( QString str )
{
    // Ansonten Qt::escape, aber quot wird dort nicht escaped
    str.replace( QChar('"'), "&quot;" );
    return str;
}

void EpkItemMdl::exportHtml( const QString& path, bool withPng )
{
    const qreal off = 10.0;

    if( d_doc.isNull() )
        return;
    QImage img;
    QSet< quint32 > imagemap;
    QRectF bound;
    if( withPng )
    {
        clearSelection();
        QHash<quint32,QGraphicsItem*>::const_iterator i;
        for( i = d_cache.begin(); i != d_cache.end(); ++i )
        {
            if( _hasOutline( d_doc.getObject( i.key() ), true ) )
            {
                i.value()->setSelected( true );
                imagemap.insert( i.key() );
            }
        }
        QBrush back = backgroundBrush();
        setBackgroundBrush( Qt::white );
        bound = itemsBoundingRect();
        bound.adjust( -off, -off, off, off );
        img = QImage( bound.size().toSize(), QImage::Format_RGB32 );
        QPainter painter( &img );
        painter.fillRect( img.rect(), Qt::white );
        painter.setRenderHints( QPainter::Antialiasing | QPainter::TextAntialiasing );
        render( &painter, QRectF( QPointF(0,0), bound.size() ), bound );
        setBackgroundBrush( back );
    }

    QMap<quint64,Udb::Obj> order;
    order.insert( d_doc.getOid(), d_doc ); // RISK
    Udb::Obj sub = d_doc.getFirstObj();
    if( !sub.isNull() ) do
    {
        const quint32 type = sub.getType();
        if( type == Event::TID || type == Function::TID )
            order[sub.getOid()] = sub;
    }while( sub.next() );
    QFile f( path );
    f.open( QIODevice::WriteOnly );
    QTextStream out( &f );
	out.setCodec( "UTF-8" );
    out << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n";
	out << "<html><META http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n";
    QString title = Qt::escape( Procs::formatObjectTitle(d_doc, isShowId()) );
	out << QString( "<head><title>%1</title>" ).arg( title );
	Oln::OutlineToHtml::writeCss(out);
	out << "</head><body>\n";
    if( withPng )
    {
        // width=100% funktioniert nicht mit map
        out << "<img usemap=\"#map1\" src=\"data:image/png;base64,";
        Stream::DataCell v;
        v.setImage( img );
        out << v.getArr().toBase64();
        out << "\"";
        out << " >\n";
        out << "<map name=\"map1\">\n";
        QHash<quint32,QGraphicsItem*>::const_iterator i;
        for( i = d_cache.begin(); i != d_cache.end(); ++i )
        {
            if( i.value()->type() == EpkNode::_Function ||
                    i.value()->type() == EpkNode::_Event ||
                    i.value()->type() == EpkNode::_Connector )
            {
                QRectF b = i.value()->sceneBoundingRect();
                b.moveTo( b.x() - bound.x(), b.y() - bound.y());
                QRect bb = b.toRect();
                out << "<area ";
                Udb::Obj o = d_doc.getObject( i.key() );
                if( o.hasValue( Oln::OutlineItem::AttrAlias ) )
                    o = o.getValueAsObj( Oln::OutlineItem::AttrAlias );
                Q_ASSERT( !o.isNull() );
                if( imagemap.contains( i.key() ) )
                    out << "href=\"#" << o.getOid() << "\"";
                out << " shape=\"rect\" coords=\"";
                out << QString("%1,%2,%3,%4").arg( bb.left() ).arg( bb.top() ).arg( bb.right() ).arg( bb.bottom() );
                out << "\" title=\"";
                out << _coded( o.getString( Root::AttrText ) );
                out << "\" />\n";
            }
        }
        out << "</map>\n";
        out << "</img>";
    }
    QMap<quint64,Udb::Obj>::const_iterator i;
    for( i = order.begin(); i != order.end(); ++i )
    {
        if( _hasOutline( i.value() ) )
        {
            title = Qt::escape( Procs::formatObjectTitle( i.value(), isShowId() ) );
			out << "<a name=\"" << i.value().getOid() << "\"><h3>" << title << "</h3></a>\n" << endl;
			Oln::OutlineToHtml writer;
            writer.writeTo( out, i.value(), QString(), true );
        }
    }
	out << "</body></html>";
}

void EpkItemMdl::exportSvg(const QString &path )
{
    clearSelection();
    QBrush back = backgroundBrush();
    setBackgroundBrush( Qt::white );
    QRectF b = itemsBoundingRect().adjusted( -DiagItem::s_boxWidth * 0.5, -DiagItem::s_boxHeight * 0.5,
        DiagItem::s_boxWidth * 0.5, DiagItem::s_boxHeight * 0.5 );
    QSvgGenerator prn;
    prn.setFileName( path );
    QPainter painter( &prn );
    render( &painter, QRectF(), b );
    setBackgroundBrush( back );
}

