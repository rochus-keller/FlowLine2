#include "EpkView.h"
#include <QMouseEvent>
#include <QStyleOptionRubberBand>
#include <QPainter>
#include <QScrollBar>
#include <QApplication>
#include <QtDebug>
#include "EpkCtrl.h"
#include "EpkItemMdl.h"
using namespace Epk;

EpkView::EpkView( QWidget* p ):QGraphicsView(p),d_mode( Idle )
{
    setRubberBandSelectionMode( Qt::ContainsItemBoundingRect );
    //setDragMode( QGraphicsView::RubberBandDrag ); nicht brauchbar, da unabhängig von Modifier
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
}

void EpkView::mousePressEvent ( QMouseEvent * e )
{
    if( e->button() == Qt::LeftButton )
    {
        if( scene() && scene()->itemAt( mapToScene( e->pos() ) ) == 0 )
        {
            // Click ins Leere
            d_rubberRect = QRect( e->pos(), QSize( 1, 1 ) );
            if( e->modifiers() == Qt::NoModifier || e->modifiers() == Qt::ShiftModifier )
            {
                // Start Rubberbanding
                d_mode = (e->modifiers() == Qt::NoModifier)?Selecting:Extending;
                if( d_mode == Selecting )
                {
                    scene()->clearSelection();
                    QGraphicsView::mousePressEvent( e );
                }
            }else if( e->modifiers() == Qt::ControlModifier )
            {
                // Start Scrolling
                d_mode = Scrolling;
                QApplication::setOverrideCursor( Qt::ClosedHandCursor);
            }else
            {
                d_mode = Clicking;
                QGraphicsView::mousePressEvent( e );
            }
        }else
        {
            // Click auf ein Objekt
            d_mode = Clicking;
            QGraphicsView::mousePressEvent( e );
            if( e->button() == Qt::LeftButton && e->modifiers() == Qt::NoModifier )
                emit signalClick( e->pos() );
        }
    }else
        QGraphicsView::mousePressEvent( e );
}

void EpkView::mouseMoveEvent ( QMouseEvent * e )
{
    if( d_mode == Selecting || d_mode == Extending )
    {
        viewport()->update( d_rubberRect.normalized() );
        d_rubberRect.setBottomRight( e->pos() );
        viewport()->update( d_rubberRect.normalized() );
    }else if( d_mode == Scrolling )
    {
        QScrollBar* hBar = horizontalScrollBar();
        QScrollBar* vBar = verticalScrollBar();
        QPoint delta = e->pos() - d_rubberRect.topLeft();
        hBar->setValue(hBar->value() - delta.x());
        vBar->setValue(vBar->value() - delta.y());
        d_rubberRect.setTopLeft( e->pos() );
    }else
        QGraphicsView::mouseMoveEvent( e );
}

void EpkView::scrollBy( int dx, int dy )
{
    QScrollBar* hBar = horizontalScrollBar();
    QScrollBar* vBar = verticalScrollBar();
    hBar->setValue(hBar->value() - dx );
    vBar->setValue(vBar->value() - dy );
}

EpkCtrl *EpkView::getCtrl() const
{
    foreach( QObject* o, children() )
        if( EpkCtrl* c = dynamic_cast<EpkCtrl*>( o ) )
            return c;
    return 0;
}

EpkItemMdl *EpkView::getMdl() const
{
    return dynamic_cast<EpkItemMdl*>( scene() );
}

void EpkView::mouseReleaseEvent ( QMouseEvent * e )
{
    if( d_mode == Selecting || d_mode == Extending )
    {
        QPainterPath selectionArea;
        QRect r = d_rubberRect.normalized();
        selectionArea.addPolygon( mapToScene( r ) );
        selectionArea.closeSubpath();
        if( scene() )
            scene()->setSelectionArea( selectionArea, Qt::ContainsItemShape );
        viewport()->update( r );
    }else if( d_mode == Scrolling )
    {
        QApplication::restoreOverrideCursor();
    }else
    {
        QGraphicsView::mouseReleaseEvent( e );
        if( !viewport()->rect().contains( e->pos() ) )
            ensureVisible( QRectF( mapToScene( e->pos() ), QSize() ) ); // RISK
    }
    d_mode = Idle;
}

void EpkView::paintEvent ( QPaintEvent * e )
{
    Q_ASSERT( scene() );
    const bool big = scene()->items().size() > 1000;
    setRenderHint( QPainter::Antialiasing, !big );
    setRenderHint( QPainter::TextAntialiasing, !big );

    QGraphicsView::paintEvent( e );
    if( d_mode == Selecting || d_mode == Extending )
    {
        QPainter p( viewport() );
        QStyleOptionRubberBand option;
        option.initFrom(viewport());
        option.rect = d_rubberRect.normalized();
        option.shape = QRubberBand::Rectangle;

        QStyleHintReturnMask mask;
        if (viewport()->style()->styleHint(QStyle::SH_RubberBand_Mask, &option, viewport(), &mask)) {
            // painter clipping for masked rubberbands
            p.setClipRegion(mask.region, Qt::IntersectClip);
        }

        viewport()->style()->drawControl(QStyle::CE_RubberBand, &option, &p, viewport());
    }
}

void EpkView::mouseDoubleClickEvent ( QMouseEvent * ev )
{
    if( ev->button() == Qt::LeftButton && ev->modifiers() == Qt::NoModifier )
        emit signalDblClick( ev->pos() );
    else
        QGraphicsView::mouseDoubleClickEvent( ev );
}
