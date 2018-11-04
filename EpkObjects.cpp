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

#include "EpkObjects.h"
#include "EpkProcs.h"
#include <Oln2/OutlineItem.h>
#include <Udb/Database.h>
#include <Udb/Transaction.h>
#include <Udb/Idx.h>
#include <QSet>
using namespace Epk;
using namespace Stream;

int DiagItem::TID = 300;
int Function::TID = 301;
int Event::TID = 302;
int Connector::TID = 303;
int ConFlow::TID = 304;
int FuncDomain::TID = 305;
int Diagram::TID = 306;
int Allocation::TID = 307;
int SystemElement::TID = 308;

const float DiagItem::s_penWidth = 1.0;
const float DiagItem::s_selPenWidth = 3.0;
const float DiagItem::s_radius = 5.0;
const float DiagItem::s_boxWidth = 100.0;
const float DiagItem::s_boxHeight = 60.0; // 72, 18, 12
const float DiagItem::s_boxInset = 12; // Ergebnis sollte ganzzahliger Teiler sein von s_boxHeight fuer bestes Aussehen
const float DiagItem::s_circleDiameter = DiagItem::s_boxHeight / 2.0;
const float DiagItem::s_textMargin = 2.5;
const float DiagItem::s_rasterX = 0.125 * DiagItem::s_boxWidth;
const float DiagItem::s_rasterY = 0.125 * DiagItem::s_boxHeight;

const QUuid FuncDomain::s_uuid = "{e254e4e5-6099-4dee-8b48-d09a77262725}";
const char* Root::s_rootUuid = "{E6153DDB-3A00-49fe-985F-30486E13C0A9}";

void DiagItem::setPos(const QPointF & p)
{
    setValue( AttrPosX, DataCell().setFloat( p.x() ) );
    setValue( AttrPosY, DataCell().setFloat( p.y() ) );
    setModifiedOn();
}

QPointF DiagItem::getPos() const
{
    return QPointF( getValue( AttrPosX ).getFloat(), getValue( AttrPosY ).getFloat() );
}

void DiagItem::setNodeList(const QPolygonF & p)
{
    Stream::DataWriter w;
    for( int i = 0; i < p.size(); i++ )
    {
        w.startFrame();
        w.writeSlot( Stream::DataCell().setFloat( p[i].x() ) );
        w.writeSlot( Stream::DataCell().setFloat( p[i].y() ) );
        w.endFrame();
    }
    setValue( AttrNodeList, w.getBml() );
    setModifiedOn();
}

QPolygonF DiagItem::getNodeList() const
{
    QPolygonF res;
    Stream::DataReader r( getValue( AttrNodeList ) );
    Stream::DataReader::Token t = r.nextToken();
    while( t == Stream::DataReader::BeginFrame )
    {
        t = r.nextToken();
        QPointF p;
        if( t == Stream::DataReader::Slot )
            p.setX( r.getValue().getFloat() );
        else
            return QPolygonF();
        t = r.nextToken();
        if( t == Stream::DataReader::Slot )
            p.setY( r.getValue().getFloat() );
        else
            return QPolygonF();
        t = r.nextToken();
        if( t != Stream::DataReader::EndFrame )
            return QPolygonF();
        t = r.nextToken();
        res.append( p );
    }
    return res;
}

bool DiagItem::hasNodeList() const
{
    return hasValue( AttrNodeList );
}

void DiagItem::setOrigObject(const Udb::Obj & o)
{
    setValueAsObj( AttrOrigObject, o );
    setModifiedOn();
}

Udb::Obj DiagItem::getOrigObject() const
{
    return getValueAsObj( AttrOrigObject );
}

void DiagItem::setPinnedTo(const Udb::Obj & o)
{
    setValueAsObj( AttrPinnedTo, o );
    setModifiedOn();
}

Udb::Obj DiagItem::getPinnedTo() const
{
	return getValueAsObj( AttrPinnedTo );
}

QList<DiagItem> DiagItem::getPinneds() const
{
	QList<DiagItem> res;
	if( isNull() )
		return res;
	Udb::Idx idx( getTxn(), Index::PinnedTo );
	if( idx.seek( *this ) ) do
	{
		DiagItem o = getObject( idx.getOid() );
		if( o.getType() == DiagItem::TID )
			res.append( o );
	}while( idx.nextKey() );
	return res;
}

DiagItem::Kind DiagItem::getKind() const
{
    return (Kind)getValue(AttrKind).getUInt8();
}

void DiagItem::setSize(const QSizeF & s)
{
    setValue( AttrWidth, DataCell().setFloat( s.width() ) );
    setValue( AttrHeight, DataCell().setFloat( s.height() ) );
    setModifiedOn();
}

QSizeF DiagItem::getSize() const
{
	// TODO: hier ev. auch die Standardgrssen fuer Function, Event und Connector zurueckgeben
    return QSizeF( getValue( AttrWidth ).getFloat(), getValue( AttrHeight ).getFloat() );
}

QRectF DiagItem::getBoundingRect() const
{
    QPointF pos = getPos();
    const qreal bwh = s_boxWidth / 2.0;
    const qreal bhh = s_boxHeight / 2.0;
    // const qreal pwhh = s_circleDiameter / 2.0;
    QRectF res;
    if( !hasNodeList() )
    {
        res = QRectF( - bwh, - bhh, s_boxWidth, s_boxHeight );
        //qDebug() << res;
        res.moveTo( pos );
        //qDebug() << "move to" << pos << res;
        return res;
    }else
    {
        QPolygonF p = getNodeList();
        return p.boundingRect();
    }
}

DiagItem DiagItem::create(Udb::Obj diagram, Udb::Obj orig, const QPointF &p)
{
    Q_ASSERT( !diagram.isNull() );
    DiagItem item = diagram.createAggregate( TID );
    item.setCreatedOn();
    item.setOrigObject( orig );
    if( !p.isNull() )
        item.setPos( p );
    return item;
}

DiagItem DiagItem::createKind(Udb::Obj diagram, Kind k, const QPointF &p)
{
    Q_ASSERT( !diagram.isNull() );
    DiagItem item = diagram.createAggregate( TID );
    item.setCreatedOn();
    item.setValue(AttrKind, DataCell().setUInt8(k) );
    if( k == Note )
        item.setValue( AttrWidth, DataCell().setFloat( s_boxWidth ) );
    else if( k == Frame )
        item.setSize( QSizeF( s_boxWidth, s_boxHeight ) );
    item.setOrigObject(item);
    // RISK: Wir simulieren ein richtiges OrigObject, da EpkItemMdl leere DiagItem löscht.
    if( !p.isNull() )
        item.setPos( p );
    return item;
}

DiagItem DiagItem::createLink(Udb::Obj diagram, Udb::Obj pred, const Udb::Obj &succ)
{
    Q_ASSERT( !diagram.isNull() );
    Q_ASSERT( !pred.isNull() );
    Q_ASSERT( !succ.isNull() );

    Udb:Obj link = Procs::createObject( ConFlow::TID, pred );
    link.setValue( ConFlow::AttrPred, pred );
    link.setValue( ConFlow::AttrSucc, succ );
    return create( diagram, link );
}

QList<Udb::Obj> DiagItem::writeItems(const QList<Udb::Obj> &items, DataWriter & out1)
{
    out1.writeSlot( Stream::DataCell().setUuid( items.first().getDb()->getDbUuid() ) );
    // Wir müssen das Format auf die lokale DB beschränken, da es Referenzen auf Objekte gibt
	// Gehe zweimal durch, dass sicher die Funktionen/Events zuerst kopiert werden
    QList<Udb::Obj> origs;
    foreach( Udb::Obj o, items )
    {
        if( o.getValueAsObj(AttrOrigObject).getType() != ConFlow::TID )
        {
            out1.startFrame( Stream::NameTag( "item" ) );
            out1.writeSlot( o.getValue( AttrPosX ), Stream::NameTag("x") );
            out1.writeSlot( o.getValue( AttrPosY ), Stream::NameTag("y") );
            out1.writeSlot( o.getValue( AttrWidth ), Stream::NameTag("w") );
            out1.writeSlot( o.getValue( AttrHeight ), Stream::NameTag("h") );
            out1.writeSlot( o.getValue( AttrKind ), Stream::NameTag("k") );
            out1.writeSlot( o.getValue( AttrText ), Stream::NameTag("txt") );
            if( o.getValue( AttrKind ).getUInt8() == 0 )
            {
                // Nur bei den Plain DiagItems wird Orig gespeichert; bei den anderen zeigt Orig auf sich selbst.
                Udb::Obj orig = o.getValueAsObj( AttrOrigObject );
                out1.writeSlot( orig, Stream::NameTag("obj") );
                origs.append( orig );
            }
            out1.endFrame();
        }
    }
    foreach( Udb::Obj o, items )
    {
        if( o.getValueAsObj(AttrOrigObject).getType() == ConFlow::TID )
        {
            out1.startFrame( Stream::NameTag( "link" ) );
            out1.writeSlot( o.getValue( AttrNodeList ), Stream::NameTag("path") );
            Udb::Obj orig = o.getValueAsObj( AttrOrigObject );
            out1.writeSlot( orig, Stream::NameTag("obj") );
            out1.endFrame();
            origs.append( orig );
        }
    }
    return origs;
}

QList<Udb::Obj> DiagItem::readItems(Udb::Obj diagram, DataReader & r)
{
    Q_ASSERT( !diagram.isNull() );
    QList<Udb::Obj> res;
    Stream::DataReader::Token t = r.nextToken();
    if( t != Stream::DataReader::Slot || r.getValue().getUuid() != diagram.getDb()->getDbUuid() )
        return res; // Objekte leben in anderer Datenbank; kein Paste möglich.
    QSet<Udb::OID> existingItems = Procs::findAllItemOrigOids( diagram );
    t = r.nextToken();
    QList<Udb::Obj> done;
    while( t == Stream::DataReader::BeginFrame && r.getName().getTag().equals( "item" ) )
    {
        Stream::DataCell x;
        Stream::DataCell y;
        Stream::DataCell w;
        Stream::DataCell h;
        Stream::DataCell txt;
        DiagItem::Kind k;
        Stream::DataCell obj;
        t = r.nextToken();
        while( t == Stream::DataReader::Slot )
        {
            if( r.getName().getTag().equals("x") )
                x = r.getValue();
            else if( r.getName().getTag().equals("y") )
                y = r.getValue();
            else if( r.getName().getTag().equals("w") )
                w = r.getValue();
            else if( r.getName().getTag().equals("h") )
                h = r.getValue();
            else if( r.getName().getTag().equals("txt") )
                txt = r.getValue();
            else if( r.getName().getTag().equals("k") )
                k = (DiagItem::Kind)r.getValue().getUInt8();
            else if( r.getName().getTag().equals("obj") )
                obj = r.getValue();
            t = r.nextToken();
        }
        Q_ASSERT( t == Stream::DataReader::EndFrame );
        Udb::Obj orig = diagram.getObject( obj.getOid() );
        if( k != Plain || ( !orig.isNull() && !existingItems.contains( obj.getOid() ) ) )
        {
            // Lege nur Diagrammelemente für Objekte an, die im Diagramm noch nicht vorhanden sind,
            // oder aber für Notes
            DiagItem current = create( diagram, orig );
            current.setValue( AttrPosX, x );
            current.setValue( AttrPosY, y );
            if( k != Plain )
            {
                current.setValue( AttrWidth, w );
                current.setValue( AttrHeight, h );
                current.setValue( AttrText, txt );
                current.setValue( AttrKind, DataCell().setUInt8(k) );
                current.setValueAsObj( AttrOrigObject, current );
            }else
            {
                existingItems.insert( orig.getOid() );
                done.append( orig );
            }
            res.append( current );
        }
        t = r.nextToken();
    }
    while( t == Stream::DataReader::BeginFrame && r.getName().getTag().equals( "link" ) )
    {
        Stream::DataCell path;
        Stream::DataCell obj;
        t = r.nextToken();
        while( t == Stream::DataReader::Slot )
        {
            if( r.getName().getTag().equals("path") )
                path = r.getValue();
            else if( r.getName().getTag().equals("obj") )
                obj = r.getValue();
            t = r.nextToken();
        }
        Q_ASSERT( t == Stream::DataReader::EndFrame );
        Udb::Obj link = diagram.getObject( obj.getOid() );
        if( !link.isNull() && !existingItems.contains( link.getOid() ) &&
                existingItems.contains( link.getValue( ConFlow::AttrPred ).getOid() ) &&
                existingItems.contains( link.getValue( ConFlow::AttrSucc ).getOid() ) )
        {
            // Lege nur Links für Objekte an, die im Diagramm vorhanden sind
            Udb::Obj current = create( diagram, link );
            current.setValue( AttrNodeList, path );
            existingItems.insert( link.getOid() );
            res.append( current );
        }
        t = r.nextToken();
    }
    QList<Udb::Obj> links = Procs::findHiddenLinks( diagram, done );
    foreach( Udb::Obj link, links )
        res.append( create( diagram, link ) );
    return res;
}

void Function::setElemCount(quint32 c)
{
    setValue( AttrElemCount, DataCell().setUInt32( c ) );
    // gilt nicht als modified
}

quint32 Function::getElemCount() const
{
    return getValue( AttrElemCount ).getUInt32();
}

void Function::setStart(const Udb::Obj & o)
{
    setValueAsObj( AttrStart, o );
    // gilt nicht als modified
}

Udb::Obj Function::getStart() const
{
    return getValueAsObj( AttrStart );
}

void Function::setFinish(const Udb::Obj & o)
{
    setValueAsObj( AttrFinish, o );
    // gilt nicht als modified
}

Udb::Obj Function::getFinish() const
{
    return getValueAsObj( AttrFinish );
}

void Function::setDirection(int d)
{
    setValue( AttrDirection, Stream::DataCell().setUInt8(d) );
    setModifiedOn();
}

Function::Direction Function::getDirection() const
{
    return (Direction)getValue(AttrDirection).getUInt8();
}

void Connector::setConnType(int t)
{
    if( getType() != Connector::TID )
        return;
    Udb::Obj oldStart = getParent().getValueAsObj( Function::AttrStart );
    if( equals( oldStart ) )
        getParent().clearValue( Function::AttrStart );
    Udb::Obj oldFinish = getParent().getValueAsObj( Function::AttrFinish );
    if( equals( oldFinish ) )
        getParent().clearValue( Function::AttrFinish );
    setValue( AttrConnType, Stream::DataCell().setUInt8( t ) );
    switch( t )
    {
    case Unspecified:
        clearValue( AttrConnType );
        break;
    case Start:
        getParent().setValueAsObj( Function::AttrStart, *this );
        if( !oldStart.isNull() )
            oldStart.clearValue( AttrConnType );
        break;
    case Finish:
        getParent().setValueAsObj( Function::AttrFinish, *this );
        if( !oldFinish.isNull() )
            oldFinish.clearValue( AttrConnType );
        break;
    }
    setModifiedOn();
}

Connector::Type Connector::getConnType() const
{
    return (Type)getValue( AttrConnType ).getUInt8();
}

void ConFlow::setPred(const Udb::Obj & o)
{
    setValueAsObj( AttrPred, o );
    setModifiedOn();
}

Udb::Obj ConFlow::getPred() const
{
    return getValueAsObj( AttrPred );
}

void ConFlow::setSucc(const Udb::Obj & o)
{
    setValueAsObj( AttrSucc, o );
    setModifiedOn();
}

Udb::Obj ConFlow::getSucc() const
{
    return getValueAsObj( AttrSucc );
}

QString Root::formatObjectTitle() const
{
    return Procs::formatObjectTitle( *this );
}

const char* Index::Ident = "Ident";
const char* Index::AltIdent = "AltIdent";
const char* Index::OrigObject = "OrigObject";
const char* Index::Pred = "Pred";
const char* Index::Succ = "Succ";
const char* Index::PinnedTo = "PinnedTo";
const char* Index::Func = "Func";
const char* Index::Elem = "Elem";

void Index::init(Udb::Database &db)
{
	Oln::OutlineItem::AliasIndex = "Alias";
    Udb::Database::Lock lock( &db);

    db.presetAtom( "StartOfDynAttr", StartOfDynAttr );

    if( db.findIndex( Index::Ident ) == 0 )
    {
        Udb::IndexMeta def( Udb::IndexMeta::Value );
        def.d_items.append( Udb::IndexMeta::Item( Udb::ContentObject::AttrIdent ) );
        db.createIndex( Index::Ident, def );
    }
    if( db.findIndex( Index::AltIdent ) == 0 )
    {
        Udb::IndexMeta def( Udb::IndexMeta::Value );
        def.d_items.append( Udb::IndexMeta::Item( Udb::ContentObject::AttrAltIdent ) );
        db.createIndex( Index::AltIdent, def );
    }
	if( db.findIndex( Oln::OutlineItem::AliasIndex ) == 0 )
    {
        Udb::IndexMeta def( Udb::IndexMeta::Value );
        def.d_items.append( Udb::IndexMeta::Item( Oln::OutlineItem::AttrAlias ) );
		db.createIndex( Oln::OutlineItem::AliasIndex, def );
    }
    if( db.findIndex( Index::OrigObject ) == 0 )
    {
        Udb::IndexMeta def( Udb::IndexMeta::Value );
        def.d_items.append( Udb::IndexMeta::Item( DiagItem::AttrOrigObject ) );
        db.createIndex( Index::OrigObject, def );
    }
    if( db.findIndex( Index::Pred ) == 0 )
    {
        Udb::IndexMeta def( Udb::IndexMeta::Value );
        def.d_items.append( Udb::IndexMeta::Item( ConFlow::AttrPred ) );
        db.createIndex( Index::Pred, def );
    }
    if( db.findIndex( Index::Succ ) == 0 )
    {
        Udb::IndexMeta def( Udb::IndexMeta::Value );
        def.d_items.append( Udb::IndexMeta::Item( ConFlow::AttrSucc ) );
        db.createIndex( Index::Succ, def );
    }
    if( db.findIndex( Index::PinnedTo ) == 0 )
    {
        Udb::IndexMeta def( Udb::IndexMeta::Value );
        def.d_items.append( Udb::IndexMeta::Item( DiagItem::AttrPinnedTo ) );
        db.createIndex( Index::PinnedTo, def );
    }
    if( db.findIndex( Index::Func ) == 0 )
    {
        Udb::IndexMeta def( Udb::IndexMeta::Value );
        def.d_items.append( Udb::IndexMeta::Item( Allocation::AttrFunc ) );
        db.createIndex( Index::Func, def );
    }
    if( db.findIndex( Index::Elem ) == 0 )
    {
        Udb::IndexMeta def( Udb::IndexMeta::Value );
        def.d_items.append( Udb::IndexMeta::Item( Allocation::AttrElem ) );
        db.createIndex( Index::Elem, def );
	}
}

Udb::Obj Index::getRoot(Udb::Transaction * txn)
{
	Q_ASSERT( txn != 0 );
	return txn->getOrCreateObject( Root::s_rootUuid );
}

FuncDomain FuncDomain::getOrCreateRoot(Udb::Transaction * txn)
{
    Q_ASSERT( txn != 0 );
    return txn->getOrCreateObject( s_uuid, TID );
}

const QUuid SystemElement::s_uuid = "{fd68a9e9-8a1b-4d6c-858e-c4e8eb586b00}";
SystemElement SystemElement::getOrCreateRoot(Udb::Transaction * txn)
{
    Q_ASSERT( txn != 0 );
    return txn->getOrCreateObject( s_uuid, TID );
}

void Allocation::setFunc(const Udb::Obj & o)
{
    setValueAsObj( AttrFunc, o );
    setModifiedOn();
}

Udb::Obj Allocation::getFunc() const
{
    return getValueAsObj(AttrFunc);
}

void Allocation::setElem(const Udb::Obj & o)
{
    setValueAsObj( AttrElem, o );
    setModifiedOn();
}

Udb::Obj Allocation::getElem() const
{
    return getValueAsObj(AttrElem);
}
