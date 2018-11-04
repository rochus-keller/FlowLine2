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

#include "EpkItems.h"
#include "EpkItemMdl.h"
#include "EpkObjects.h"
#include <QFontMetricsF>
#include <QPainter>
#include <QApplication>
#include <math.h>
#include <QtDebug>
using namespace Epk;

static inline EpkItemMdl* _mdl( const QGraphicsItem* i )
{
    EpkItemMdl* mdl = dynamic_cast<EpkItemMdl*>( i->scene() );
    Q_ASSERT( mdl );
    return mdl;
}

static inline bool _showId( const QGraphicsItem* i )
{
    EpkItemMdl* m = _mdl( i );
    return m && m->isShowId();
}

static inline bool _markAlias( const QGraphicsItem* i )
{
    EpkItemMdl* m = _mdl( i );
    return m && m->isMarkAlias();
}

EpkNode::EpkNode(quint32 item, quint32 orig, int type):
    d_itemOid(item),d_origOid(orig),d_alias(false),d_isProcess(false),d_type(type),
	d_code(0),d_size( DiagItem::s_boxWidth, DiagItem::s_boxHeight ),d_pinnedTo(0)
{
    setFlags(ItemIsSelectable );
    if( type == _Frame )
        setZValue(-2000.0);
}

EpkNode::~EpkNode()
{
    // Sorge dafür, dass Querverweise gelöscht werden, da ansonten Scene::clear crasht
    foreach( LineSegment* s, d_links )
    {
        if( s->d_start == this )
            s->d_start = 0;
        if( s->d_end == this )
            s->d_end = 0;
    }
    if( d_itemOid || d_origOid )
    {
        EpkItemMdl* mdl = dynamic_cast<EpkItemMdl*>( scene() );
        if( mdl )
            mdl->removeFromCache( this );
    }
}

void EpkNode::setType(int type)
{
    prepareGeometryChange();
    d_type = type;
}

void EpkNode::setWidth(qreal w)
{
    update();
    prepareGeometryChange();
    const qreal minw = DiagItem::s_boxWidth * 0.25;
    if( w < minw )
        w = minw;
    d_size.setWidth( w );
    updateNoteHeight();
    update();
}

void EpkNode::updateNoteHeight()
{
    QFontMetricsF fm( _mdl(this)->getChartFont() );
    const QString text = (!d_text.isEmpty())?d_text:QString("Empty");
    const QRectF textBound = fm.boundingRect( QRectF( 0, 0, d_size.width() - 2.0 * DiagItem::s_textMargin,
           DiagItem::s_boxHeight ), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text );
    const qreal h = textBound.height() + 2 * DiagItem::s_textMargin;
    if( d_size.height() != h )
        prepareGeometryChange();
    d_size.setHeight( h );
}

void EpkNode::setSize(const QSizeF & s)
{
    update();
    prepareGeometryChange();
    d_size = s;
    const qreal minw = DiagItem::s_boxWidth * 0.25;
    const qreal minh = DiagItem::s_boxHeight * 0.25;
    if( d_size.width() < minw )
        d_size.setWidth( minw );
    if( d_size.height() < minh )
        d_size.setHeight( minh );
    update();
}

LineSegment* EpkNode::getFirstInSegment() const
{
    for( int i = 0; i < d_links.size(); i++ )
        if( d_links[i]->getEndItem() == this )
            return d_links[i];
    return 0;
}

LineSegment* EpkNode::getFirstOutSegment() const
{
    for( int i = 0; i < d_links.size(); i++ )
        if( d_links[i]->getStartItem() == this )
            return d_links[i];
    return 0;
}

QRectF EpkNode::boundingRect() const
{
    const qreal pw = DiagItem::s_penWidth / 2.0;
    const QRectF r = toPolygon().boundingRect().adjusted( -pw, -pw, pw, pw );
    if( type() == _Note )
    {
		if( d_pinnedTo ) // isSelected() &&
        {
            // Sorge dafür, dass auch die Linie von der Note zum Pin-Target richtig gezeichnet wird
			return r.united( QRectF( QPointF(0,0), mapFromItem( d_pinnedTo, QPointF(0,0) ) ) );
        }else
            return r;
    }else if( _showId( this ) )
    {
        QFontMetricsF f( scene()->font() );
        return r.adjusted( 0, - f.height(), 0, 0 );
    }else
        return r;
}

QPolygonF EpkNode::toPolygon() const
{
    // Bei allen Formen ausser Note/Frame ist der Nullpunkt in der Mitte der Form
    switch( d_type )
    {
    case _Event:
        {
			QPolygonF p;
            p << QPointF( 0, DiagItem::s_boxHeight * 0.5 ) << // links mitte
                 QPointF( DiagItem::s_boxInset, 0 ) << // links oben
                 QPointF( DiagItem::s_boxWidth - DiagItem::s_boxInset, 0 ) << // rechts oben
                 QPointF( DiagItem::s_boxWidth, DiagItem::s_boxHeight * 0.5 ) << // rechts mitte
                 QPointF( DiagItem::s_boxWidth - DiagItem::s_boxInset, DiagItem::s_boxHeight ) << // rechts unten
                 QPointF( DiagItem::s_boxInset, DiagItem::s_boxHeight ) << // links unten
                 QPointF( 0, DiagItem::s_boxHeight * 0.5 ); // links mitte
            p.translate( -DiagItem::s_boxWidth * 0.5, -DiagItem::s_boxHeight * 0.5 );
            return p;
        }
    case _Handle:
        return handleShapeRect();
    case _Connector:
        {
			const qreal rad = DiagItem::s_circleDiameter / 2.0;
			const qreal len = 0.83 * rad;
			const qreal len2 = len * 0.5;
			QPolygonF p;
			p << QPointF( -rad, -len2) << QPointF( -len2, -rad) << QPointF( len2, -rad) << QPointF( rad, -len2) <<
				 QPointF( rad, len2) << QPointF( len2, rad) << QPointF( -len2, rad) << QPointF( -rad, len2) <<
				 QPointF( -rad, -len2); // A polygon is said to be closed if its start point and end point are equal.
			return p;
        }
    case _Note:
    case _Frame:
        {
            // Bei Frame und Note ist Nullpunkt links oben
			//const qreal pwh = DiagItem::s_penWidth / 2.0;
            return QRectF( 0, 0, d_size.width(), d_size.height() );
        }
    default:
        return QRectF( - DiagItem::s_boxWidth / 2.0, - DiagItem::s_boxHeight / 2.0,
                           DiagItem::s_boxWidth, DiagItem::s_boxHeight );
    }
}

void EpkNode::adjustSize(qreal dx, qreal dy)
{
    Q_UNUSED(dy);
    if( type() == _Note )
        setWidth( d_size.width() + dx );
    else if( type() == _Frame )
        setSize( QSizeF( d_size.width() + dx, d_size.height() + dy ) );
}

QPainterPath EpkNode::shape () const
{
    QPainterPath p;
    if( type() == _Connector )
        p.addEllipse( -DiagItem::s_boxHeight * 0.25, -DiagItem::s_boxHeight * 0.25,
                      DiagItem::s_boxHeight * 0.5, DiagItem::s_boxHeight * 0.5 );
    else
        p.addPolygon( toPolygon() );
    return p;
}

void EpkNode::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    switch( d_type )
    {
    case _Function:
        paintFunction( painter, option, widget );
        break;
    case _Event:
        paintEvent( painter, option, widget );
        break;
    case _Handle:
        paintHandle( painter, option, widget );
        break;
    case _Connector:
        paintConnector( painter, option, widget );
        break;
    case _Note:
        paintNote( painter, option, widget );
        break;
    case _Frame:
        paintFrame( painter, option, widget );
        break;
    default:
        qWarning( "Don't know how to paint DiagItem type=%d", d_type );
    }
}

void EpkNode::removeLine(LineSegment *arrow)
{
    if( arrow == 0 )
        return;
    if( arrow->d_start == this )
        arrow->d_start = 0;
    else if( arrow->d_end == this )
        arrow->d_end = 0;
    else
        Q_ASSERT( false );
    d_links.removeAll( arrow );
}

void EpkNode::setText(const QString &t)
{
    d_text = t;
    if( type() == _Note )
        updateNoteHeight();
}

void EpkNode::addLine(LineSegment *arrow, bool start)
{
    if( start )
    {
        if( arrow->d_start )
            arrow->d_start->removeLine( arrow );
        Q_ASSERT( arrow->d_start == 0 );
        arrow->d_start = this;
    }else
    {
        if( arrow->d_end )
            arrow->d_end->removeLine( arrow );
        Q_ASSERT( arrow->d_end == 0 );
        arrow->d_end = this;
    }
    d_links.append(arrow);
}

void EpkNode::setPinnedTo(EpkNode * node)
{
	if( d_pinnedTo )
	{
		d_pinnedTo->d_pinneds.removeAll(this);
		d_pinnedTo = 0;
	}
	d_pinnedTo = node;
	if( d_pinnedTo )
		d_pinnedTo->d_pinneds.append(this);
}

QVariant EpkNode::itemChange(GraphicsItemChange change, const QVariant &value)
{
	switch( change )
	{
	case QGraphicsItem::ItemPositionHasChanged:
		foreach( LineSegment *arrow, d_links )
			arrow->updatePosition();
		break;
	case QGraphicsItem::ItemPositionChange:
		{
			const QPointF diff = value.value<QPointF>() - pos();
			// qDebug() << "Top: " << this << " old: " << oldPos << " new: " << newPos << " diff: " << diff;
			if( scene() == 0 )
				break; // dann _mdl nicht vorhanden
			_mdl(this)->movePinned( d_pinneds, diff );
		}
		break;
	}
    return value;
}

LineSegment* EpkNode::getFirstSegment() const
{
    LineSegment* prev = getFirstInSegment();
    if( prev )
        return prev->getFirstSegment();
    else
        return 0;
}

LineSegment* EpkNode::getLastSegment() const
{
    LineSegment* next = getFirstOutSegment();
    if( next )
        return next->getLastSegment();
    else
        return 0;
}


static const QColor s_lightBrown( 221, 208, 155 );
static const QColor s_darkBrown( 140, 120, 33 );
static const QColor s_lightBlue( 202, 215, 236 );
static const QColor s_darkBlue( 119, 152, 206 );


void EpkNode::paintFunction( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget )
{
    QColor borderClr( 71, 179, 0 );
    QColor fillClr( 150, 255, 0 ); // TaskType_Discrete
    QColor textClr = Qt::black;

    if( d_isProcess )
    {
        fillClr = s_lightBrown;
        borderClr = s_darkBrown;
    }

    if( isAlias() && _markAlias( this ) ) // TODO
    {
        borderClr = Qt::lightGray;
        fillClr = borderClr.lighter(130);
        textClr = Qt::gray;
    }

    painter->setPen( QPen( borderClr, isSelected()?DiagItem::s_selPenWidth:DiagItem::s_penWidth ) );
    painter->setBrush( fillClr );
    QRectF r( -DiagItem::s_boxWidth * 0.5, -DiagItem::s_boxHeight * 0.5, DiagItem::s_boxWidth, DiagItem::s_boxHeight );
    painter->drawRoundedRect( r, DiagItem::s_radius, DiagItem::s_radius );
    if( d_isProcess )
    {
        painter->drawRoundedRect( r.adjusted( DiagItem::s_textMargin, DiagItem::s_textMargin,
                                              -DiagItem::s_textMargin, -DiagItem::s_textMargin ),
            DiagItem::s_radius - DiagItem::s_textMargin, DiagItem::s_radius - DiagItem::s_textMargin );
    }
    painter->setPen( textClr );
    painter->setFont( _mdl( this )->getChartFont() );
    r.adjust( DiagItem::s_textMargin, DiagItem::s_textMargin, -DiagItem::s_textMargin, -DiagItem::s_textMargin );
    int flags = Qt::TextWordWrap;
    QString text = getText();
    QFontMetricsF fm( painter->font(), painter->device() );
    const QRectF br = fm.boundingRect( r, Qt::TextWordWrap, text );
    if( br.height() > r.height() )
        flags |= Qt::AlignTop;
    else
        flags |= Qt::AlignVCenter;
    if( br.width() > r.width() )
        flags |= Qt::AlignLeft;
    else
        flags |= Qt::AlignHCenter;
    painter->drawText( r, flags, text );
    if( br.height() - fm.descent() > r.height() )
    {
        // Punkte nach überschüssigem Text
        painter->setPen( QPen( textClr, 2.0 * DiagItem::s_penWidth, Qt::DotLine, Qt::RoundCap ) );
        painter->drawLine( r.bottomRight() - QPointF( DiagItem::s_boxInset, 0 ), r.bottomRight() );
    }
    if( _showId( this ) )
    {
        painter->setPen( Qt::blue );
        QFont f = painter->font();
        f.setBold( true );
        painter->setFont( f );
        QFontMetricsF fm( f );
        QRectF br = fm.boundingRect( getId() );
        br.moveBottomLeft( QPointF( -DiagItem::s_boxWidth * 0.5,
                                    -DiagItem::s_boxHeight * 0.5 -DiagItem::s_textMargin + 3.0 ) );
        painter->fillRect( br.adjusted( 0, 2, 0, -2 ), Qt::white );
        painter->drawText( br, getId() );
    }
}

void EpkNode::paintEvent( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget )
{
    //painter->setRenderHint( QPainter::Antialiasing );
    QColor borderClr = QColor( 179, 98, 5 );
    QColor fillClr = QColor( 255, 178, 7 );
    QColor textClr = Qt::black;
    if( isAlias() && _markAlias( this ) )
    {
        borderClr = Qt::lightGray;
        // fillClr = fillClr.lighter(130);
        fillClr = borderClr.lighter(130);
        textClr = Qt::gray;
    }

    painter->setPen( QPen( borderClr, isSelected()?DiagItem::s_selPenWidth:DiagItem::s_penWidth ) );
    painter->setBrush( fillClr );

    QPolygonF p = toPolygon();
    painter->drawPolygon( p );

    QRectF textRect = p.boundingRect().adjusted( DiagItem::s_boxInset, DiagItem::s_textMargin,
                                                       -DiagItem::s_boxInset, -DiagItem::s_textMargin );
    painter->setPen( textClr );
    painter->setFont( _mdl( this )->getChartFont() );

    int textFlags = Qt::TextWordWrap;
    QFontMetricsF fm( painter->font(), painter->device() );
    QString text = getText();
    const QRectF textBounding = fm.boundingRect( textRect, Qt::TextWordWrap, text );
    if( textBounding.height() > textRect.height() )
        textFlags |= Qt::AlignTop;
    else
        textFlags |= Qt::AlignVCenter;
    if( textBounding.width() > textRect.width() )
        textFlags |= Qt::AlignLeft;
    else
        textFlags |= Qt::AlignHCenter;
    painter->drawText( textRect, textFlags, text );
    if( textBounding.height() - fm.descent() > textRect.height() )
    {
        // Punkte hinter Text
        painter->setPen( QPen( textClr, 2.0 * DiagItem::s_penWidth, Qt::DotLine, Qt::RoundCap ) );
        painter->drawLine( textRect.bottomRight() - QPointF( DiagItem::s_boxInset, 0 ), textRect.bottomRight() );
    }
    if( _showId( this ) )
    {
        painter->setPen( Qt::blue );
        QFont f = painter->font();
        f.setBold( true );
        painter->setFont( f );
        QFontMetricsF fm( f );
        QRectF br = fm.boundingRect( getId() );
        br.moveBottomLeft( QPointF( -DiagItem::s_boxWidth * 0.5 + DiagItem::s_boxInset,
                                    -DiagItem::s_boxHeight * 0.5 -DiagItem::s_textMargin + 3.0 ) );
        painter->fillRect( br.adjusted( 0, 2, 0, -2 ), Qt::white );
        painter->drawText( br, getId() );
    }
}

void EpkNode::paintHandle( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget )
{
    if( isSelected() )
    {
        painter->setPen( QPen( Qt::black, DiagItem::s_penWidth ) );
        painter->setBrush( Qt::black );
        painter->drawRect( handleShapeRect() );
    }
}

void EpkNode::paintConnector(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QColor line( 103, 103, 103 );
    QColor fill( 208, 208, 208 );
    if( d_code == Connector::Start )
    {
        line = Qt::black;
        fill = QColor( 196, 246, 121 );
    }else if( d_code == Connector::Finish )
    {
        line = Qt::black;
        fill = QColor( 246, 121, 121 );
    }
    painter->setPen( QPen( line, isSelected()?DiagItem::s_selPenWidth:DiagItem::s_penWidth ) );
    painter->setBrush( fill );
    const qreal h = DiagItem::s_boxHeight * 0.5;
    const qreal h2 = DiagItem::s_boxHeight * 0.25;
    QRectF r( -h2, -h2, h, h );
    painter->drawEllipse( r );
    QString text;
    switch( d_code )
    {
    case Connector::And:
        text = QObject::tr("AND");
        break;
    case Connector::Or:
        text = QObject::tr("OR");
        break;
    case Connector::Xor:
        text = QObject::tr("XOR");
        break;
    }
    QFont f = QApplication::font();
    f.setPointSizeF( DiagItem::s_circleDiameter * 0.3 );
    painter->setFont( f );
    painter->setPen( Qt::black );
    painter->drawText( r.adjusted( 0, 0, 1, 0 ), Qt::AlignCenter, text );
}

void EpkNode::paintNote(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if( isSelected() )
        painter->setPen( QPen( Qt::darkGray, DiagItem::s_selPenWidth ) );
    else
        painter->setPen( Qt::NoPen );
    painter->setBrush( QColor( 240, 240, 240 ) );

    const QRectF rect( QPointF(0,0),d_size );

//	if( isSelected() && d_pinnedTo )
//		painter->drawLine( rect.center(), mapFromItem( d_pinnedTo, QPointF(0,0) ) );
       // Ein Pinned Note zeigt auf seinen Parent wenn selektiert

    painter->drawRect( rect );
    painter->setPen( Qt::black );
    painter->setFont( _mdl(this)->getChartFont() );
    painter->drawText( rect.adjusted( DiagItem::s_textMargin, DiagItem::s_textMargin, 0, 0 ),
                       Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, d_text );
}

void EpkNode::paintFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if( isSelected() )
        painter->setPen( QPen( Qt::darkGray, DiagItem::s_selPenWidth ) );
    else
        painter->setPen( QPen( Qt::gray, DiagItem::s_penWidth ) );
    painter->setBrush( QColor( 250, 250, 250 ) );

    QRectF r( 0, 0, d_size.width(), d_size.height() );
    painter->drawRoundedRect( r, DiagItem::s_radius, DiagItem::s_radius );
    painter->setPen( Qt::black );
    QFont f = _mdl( this )->getChartFont();
    f.setBold(true);
    painter->setFont( f );
    painter->drawText( r.adjusted( 2.0 * DiagItem::s_textMargin, DiagItem::s_textMargin,
                                   -DiagItem::s_textMargin, -DiagItem::s_textMargin ),
        Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, d_text );
}

QRectF EpkNode::handleShapeRect()
{
    qreal r = DiagItem::s_radius * 0.75; // * ()?1.0:0.5;
    return QRectF( -r, -r, 2.0 * r, 2.0 * r );
}

static const qreal Pi = 3.14;

LineSegment::LineSegment( quint32 item, quint32 orig)
    :d_itemOid(item),d_origOid(orig),d_start(0), d_end(0)
{
    setZValue(-1000.0);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setPen(QPen(Qt::black, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    // Gezeichnet wird nur mit einem Punkt, aber dank den 5 kann man besser selektieren.
}

LineSegment::~LineSegment()
{
    if( d_start )
        d_start->removeLine( this );
    Q_ASSERT( d_start == 0 );
    if( d_end )
        d_end->removeLine( this );
    Q_ASSERT( d_end == 0 );
    if( d_itemOid || d_origOid )
    {
        EpkItemMdl* mdl = dynamic_cast<EpkItemMdl*>( scene() );
        if( mdl )
            mdl->removeFromCache( this );
    }
}

QRectF LineSegment::boundingRect() const
{
    return shape().boundingRect();
}

QPainterPath LineSegment::shape() const
{
    QPainterPath path = QGraphicsLineItem::shape();
    path.addPolygon(d_arrowHead);
    return path;
}

void LineSegment::updatePosition()
{
	QLineF line( mapFromItem(d_start, 0, 0), mapFromItem(d_end, 0, 0) );
    setLine(line);
}

void LineSegment::paint(QPainter *painter, const QStyleOptionGraphicsItem *,QWidget *)
{
    if (d_start->collidesWithItem(d_end))
        return;

    QPen myPen = pen();
    if( isSelected() )
        myPen.setWidth( DiagItem::s_selPenWidth );
    else
        myPen.setWidth( DiagItem::s_penWidth );
    const qreal arrowSize = 10;
    if( d_start->isAlias() && _markAlias( d_start ) )
    {
        myPen.setColor(Qt::gray);
        painter->setBrush(Qt::gray);
    }else
    {
        myPen.setColor(Qt::black);
        painter->setBrush(Qt::black);
    }
    painter->setPen(myPen);

	const QLineF centerLine(d_start->pos(), d_end->pos());
	const QPolygonF endPolygon = d_end->toPolygon();
	QPointF p1 = endPolygon.first() + d_end->pos();
	QPointF intersectPoint;
    for (int i = 1; i < endPolygon.count(); ++i)
    {
		const QPointF p2 = endPolygon.at(i) + d_end->pos();
		const QLineF polyLine = QLineF(p1, p2);
		QLineF::IntersectType intersectType = polyLine.intersect(centerLine, &intersectPoint);
        if (intersectType == QLineF::BoundedIntersection)
            break;
        p1 = p2;
    }

	setLine(QLineF(intersectPoint, d_start->pos()));

    double angle = ::acos(line().dx() / line().length());
    if (line().dy() >= 0)
        angle = (Pi * 2) - angle;

	const QPointF arrowP1 = line().p1() + QPointF(sin(angle + Pi / 3.0) * arrowSize,
		cos(angle + Pi / 3.0) * arrowSize);
	const QPointF arrowP2 = line().p1() + QPointF(sin(angle + Pi - Pi / 3.0) * arrowSize,
		cos(angle + Pi - Pi / 3.0) * arrowSize);

    d_arrowHead.clear();
    d_arrowHead << line().p1() << arrowP1 << arrowP2;
	painter->drawLine(QLineF(d_end->pos(), d_start->pos()));
    if( d_end->type() != EpkNode::_Handle )
        painter->drawPolygon(d_arrowHead);
}

EpkNode* LineSegment::getUltimateStartItem() const
{
    LineSegment* f = getFirstSegment();
    Q_ASSERT( f != 0 );
    return f->getStartItem();
}

EpkNode* LineSegment::getUltimateEndItem() const
{
    LineSegment* f = getLastSegment();
    Q_ASSERT( f != 0 );
    return f->getEndItem();
}

LineSegment* LineSegment::getLastSegment() const
{
    if( getEndItem() && getEndItem()->type() == EpkNode::_Handle )
    {
        return getEndItem()->getLastSegment();
    }else
        return const_cast<LineSegment*>( this );
}

QList<LineSegment*> LineSegment::getChain() const
{
    QList<LineSegment*> res;
    res.append( const_cast<LineSegment*>( this ) );
    const LineSegment* cur = this;
    while( cur->getStartItem() && cur->getStartItem()->type() == EpkNode::_Handle )
    {
        cur = cur->getStartItem()->getFirstInSegment();
        res.append( const_cast<LineSegment*>( cur ) );
    }
    return res;
}

LineSegment* LineSegment::getFirstSegment() const
{
    if( getStartItem() && getStartItem()->type() == EpkNode::_Handle )
    {
        return getStartItem()->getFirstSegment();
    }else
        return const_cast<LineSegment*>( this );
}

void LineSegment::selectAllSegments()
{
    LineSegment* cf = getLastSegment();
    while( cf )
    {
        cf->setSelected( true );
        if( cf->getStartItem()->type() == EpkNode::_Handle )
        {
            EpkNode* handle = cf->getStartItem();
            handle->setSelected( true );
            cf = handle->getFirstInSegment();
        }else
            cf = 0;
    }
}




