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

#include "EpkProcs.h"
#include "EpkObjects.h"
#include <Oln2/OutlineItem.h>
#include <Oln2/OutlineUdbMdl.h>
#include <Oln2/LinkSupport.h>
#include <Txt/TextOutHtml.h>
#include <Udb/Idx.h>
#include <QtDebug>
using namespace Epk;

QString Procs::prettyTypeName(quint32 type)
{
    if( type == Function::TID )
        return tr("Function");
    else if( type == DiagItem::TID )
        return tr("EPK Diagram Element");
	else if( type == Diagram::TID )
		return tr("EPK Diagram");
	else if( type == Event::TID )
        return tr("Event");
    else if( type == Connector::TID )
        return tr("Connector");
    else if( type == ConFlow::TID )
        return tr("Control Flow");
    else if( type == Oln::OutlineItem::TID )
        return tr("Outline Item");
    else if( type == Oln::Outline::TID )
        return tr("Outline");
    else if( type == FuncDomain::TID )
        return tr("Function Domain");
    else if( type == Udb::Folder::TID )
        return tr("Folder");
    else if( type == Udb::RootFolder::TID )
        return tr("Folder Tree");
    else if( type == Udb::TextDocument::TID )
        return tr("Text Document");
    else if( type == Allocation::TID )
        return tr("Allocation");
    else if( type == SystemElement::TID )
        return tr("System Element");
	else if( type == Udb::ScriptSource::TID )
		return tr("Lua Script");
	else
        return QString("<unknown type>");
}

QString Procs::prettyAttrName(quint32 attr)
{
	if( attr == Udb::ContentObject::AttrCreatedOn )
		return tr("Created On");
	else if( attr == Udb::ContentObject::AttrModifiedOn )
		return tr("Modified On");
	else if( attr == Udb::ContentObject::AttrText )
		return tr("Header/Text");
	else if( attr == Udb::ContentObject::AttrIdent )
		return tr("Internal ID");
	else if( attr == Udb::ContentObject::AttrAltIdent )
		return tr("Custom ID");
	else if( attr == Udb::TextDocument::AttrBody )
		return tr("Body");

    switch( attr )
    {
    case Connector::AttrConnType:
        return tr("Connector Type");
    case ConFlow::AttrPred:
        return tr("Predecessor");
    case ConFlow::AttrSucc:
        return tr("Successor");
    case Function::AttrElemCount:
        return tr("Process");
    case Function::AttrStart:
        return tr("Start Event");
    case Function::AttrFinish:
        return tr("Finish Event");
    case DiagItem::AttrOrigObject:
        return tr("Original Object" );
    case DiagItem::AttrPosX:
        return tr("X Position");
    case DiagItem::AttrPosY:
        return tr("Y Position");
    case DiagItem::AttrNodeList:
        return tr("Point List" );
    case DiagItem::AttrWidth:
        return tr("Width");
    case DiagItem::AttrHeight:
        return tr("Height");
    case Function::AttrDirection:
        return tr("Direction");
    case Allocation::AttrFunc:
        return tr("Function");
    case Allocation::AttrElem:
        return tr("System Element");
	}
    return QString();
}

QString Procs::formatObjectTitle(const Udb::Obj & o, bool showId)
{
    if( o.isNull() )
        return tr("<null>");
    Udb::Obj resolvedObj = o.getValueAsObj( Oln::OutlineItem::AttrAlias );
    if( resolvedObj.isNull() )
        resolvedObj = o;
    QString id = resolvedObj.getString( Udb::ContentObject::AttrAltIdent, true );
    if( id.isEmpty() )
        id = resolvedObj.getString( Udb::ContentObject::AttrIdent, true );
    QString name = resolvedObj.getString( Udb::ContentObject::AttrText, true );
    if( name.isEmpty() )
    {
        if( resolvedObj.getType() == Connector::TID )
            name = formatConnType( resolvedObj.getValue( Connector::AttrConnType ).getUInt8(), true );
        else
            name = prettyTypeName( resolvedObj.getType() );
    }

    if( id.isEmpty() || !showId )
#ifdef _DEBUG_TEST
        return QString("%1%2 %3").arg( QLatin1String("#" ) ).
            arg( o.getOid(), 3, 10, QLatin1Char('0' ) ).
            arg( name );
#else
        return name;
#endif
    else
        return QString("%1 %3").arg( id ).arg( name );
}

QString Procs::formatObjectId(const Udb::Obj & o, bool useOid)
{
    if( o.isNull() )
        return tr("<null>");
    QString id = o.getString( Udb::ContentObject::AttrAltIdent );
    if( id.isEmpty() )
        id = o.getString( Udb::ContentObject::AttrIdent );
    if( id.isEmpty() && useOid )
        id = QString("#%1").arg( o.getOid() );
    return id;
}

bool Procs::canHaveText(quint32 type)
{
    if( type == ConFlow::TID || type == DiagItem::TID || type == Connector::TID )
        return false;
    else
        return true;
}

bool Procs::canConvert(const Udb::Obj & obj, quint32 toType)
{
    const Udb::Atom fromType = obj.getType();
    bool allowed = false;
    if( fromType == Function::TID )
    {
        if( toType == Event::TID )
            allowed = true;
        else if( toType == FuncDomain::TID )
        {
            Udb::Idx idx( obj.getTxn(), Index::OrigObject );
            Udb::Idx predIdx( obj.getTxn(), Index::Pred );
            Udb::Idx succIdx( obj.getTxn(), Index::Succ );
            allowed = !idx.seek( obj ) && !predIdx.seek(obj) && !succIdx.seek(obj);
            // Nicht mglich, solange die Funktion in einem Diagramm bentzt oder verlinkt ist.
        }
    }else if( fromType == Event::TID )
        allowed = toType == Function::TID;
    else if( fromType == FuncDomain::TID )
        allowed = toType == Function::TID;
    if( allowed )
        return isValidAggregate( obj.getParent().getType(), toType );
    else
        return false;
}

bool Procs::isValidAggregate(quint32 parent, quint32 child)
{
	if( child == Oln::OutlineItem::TID )
		return true;

    if( parent == Function::TID || parent == Diagram::TID )
        return child == Function::TID || child == Event::TID || child == Connector::TID || child == ConFlow::TID;
    else if( parent == FuncDomain::TID )
        return child == FuncDomain::TID || isValidAggregate( Function::TID, child );
    else if( parent == Udb::RootFolder::TID || parent == Udb::Folder::TID )
		return child == Udb::Folder::TID || child == Oln::Outline::TID ||
				child == Diagram::TID || child == Udb::ScriptSource::TID;
    else if( parent == SystemElement::TID )
        return child == SystemElement::TID;
    // TODO
    return false;
}

QString Procs::formatValue(Udb::Atom attr, const Udb::Obj & obj, bool useIcons)
{
    if( obj.isNull() )
        return tr("<null>");
    const Stream::DataCell v = obj.getValue( attr );

    if( attr == Function::AttrElemCount )
        return (v.getUInt32() > 0)?"true":"false";
    else if( attr == Connector::AttrConnType )
        return formatConnType( v.getUInt8() );
    else if( attr == Function::AttrDirection )
        return formatDirection( v.getUInt8() );

    // TODO: Auflsung von weiteren EnumDefs

    switch( v.getType() )
    {
    case Stream::DataCell::TypeDateTime:
        return prettyDateTime( v.getDateTime() );
    case Stream::DataCell::TypeDate:
        return prettyDate( v.getDate(), false );
    case Stream::DataCell::TypeTime:
        return v.getTime().toString( "hh:mm" );
    case Stream::DataCell::TypeBml:
        {
            Stream::DataReader r( v.getBml() );
            return r.extractString(true);
        }
    case Stream::DataCell::TypeUrl:
        {
            QString host = v.getUrl().host();
            if( host.isEmpty() )
                host = v.getUrl().toString();
            return QString( "<a href=\"%1\">%2</a>" ).arg( QString::fromAscii( v.getArr() ) ).arg( host );
        }
    case Stream::DataCell::TypeHtml:
        return Txt::TextOutHtml::htmlToPlainText( v.getStr() );
    case Stream::DataCell::TypeOid:
        {
            Udb::Obj o = obj.getObject( v.getOid() );
            if( !o.isNull() )
            {
                QString img;
                if( useIcons )
                    img = Oln::OutlineUdbMdl::getPixmapPath( o.getType() );
                if( !img.isEmpty() )
                    img = QString( "<img src=\"%1\" align=\"middle\"/>&nbsp;" ).arg( img );
                return Txt::TextOutHtml::linkToHtml( Oln::Link( o ).writeTo(),
                                                         img + formatObjectTitle( o ) );
            }
        }
    case Stream::DataCell::TypeTimeSlot:
        {
            Stream::TimeSlot t = v.getTimeSlot();
            if( t.d_duration == 0 )
                return t.getStartTime().toString( "hh:mm" );
            else
                return t.getStartTime().toString( "hh:mm - " ) + t.getEndTime().toString( "hh:mm" );
        }
    case Stream::DataCell::TypeNull:
    case Stream::DataCell::TypeInvalid:
        return QString();
    case Stream::DataCell::TypeAtom:
        return prettyTypeName( v.getAtom() );
    default:
        return v.toPrettyString();
    }
    return QString();
}

QString Procs::formatConnType(quint8 type, bool addTypeName)
{
    QString typeName;
    if( addTypeName )
        typeName = QChar(' ') + prettyTypeName( Connector::TID );
    switch( type )
    {
    case Connector::Unspecified:
        return tr("Unspecified") + typeName;
    case Connector::And:
        return tr("AND") + typeName;
    case Connector::Or:
        return tr("OR") + typeName;
    case Connector::Xor:
        return tr("XOR") + typeName;
    case Connector::Start:
        return tr("Start") + typeName;
    case Connector::Finish:
        return tr("Finish") + typeName;
    default:
        return tr("<unknown>") + typeName;
    }
}

QString Procs::formatDirection(quint8 dir)
{
    switch( dir )
    {
    case Function::TopToBottom:
        return tr("Top to Bottom");
    case Function::LeftToRight:
        return tr("Left to Right");
    default:
        return tr("<unknown>");
    }
}

QString Procs::prettyDate( const QDate& d, bool includeDay, bool includeWeek )
{
    const char* s_dateFormat = "d.M.yy"; // Qt::DefaultLocaleShortDate
    QString res;
    if( includeDay )
        res = d.toString( "ddd, " ) + d.toString( s_dateFormat );
    else
        res = d.toString( s_dateFormat );
    if( includeWeek )
        res += QString( ", W%1" ).arg( d.weekNumber(), 2, 10, QChar( '0' ) );
    return res;
}

QString Procs::prettyDateTime( const QDateTime& t )
{
    return t.date().toString( Qt::DefaultLocaleShortDate ) + t.time().toString( " hh:mm" );
}

Udb::Obj Procs::createObject(quint32 type, Udb::Obj parent, const Udb::Obj& before )
{
    Q_ASSERT( !parent.isNull() );
    Udb::Obj o = parent.createAggregate( type, before );
    if( canHaveText( type ) )
        o.setString( Root::AttrText, prettyTypeName( type ) );
	o.setTimeStamp( Root::AttrCreatedOn );
    const QString id = getNextIdString( o.getTxn(), type );
    if( !id.isEmpty() )
        o.setString( Root::AttrIdent, id );
    if( isDiagNode( o.getType() ) )
    {
        Q_ASSERT( !parent.isNull() );
        parent.incCounter( Function::AttrElemCount );
    }
    return o;
}

quint32 Procs::getNextId(Udb::Transaction * txn, quint32 type)
{
    Udb::Obj root = FuncDomain::getOrCreateRoot( txn );
    Q_ASSERT( !root.isNull() );
    return root.incCounter( type );
}

QString Procs::getNextIdString(Udb::Transaction * txn, quint32 type)
{
    QString prefix;
    if( type == Function::TID )
        prefix = QLatin1String("F");
    else if( type == Event::TID )
        prefix = QLatin1String("E");
    else if( type == FuncDomain::TID )
        prefix = QLatin1String("FD");
    else if( type == Allocation::TID )
        prefix = QLatin1String("R");
    else if( type == SystemElement::TID )
        prefix == QLatin1String("A");

    if( !prefix.isEmpty() )
        return QString("%1%2").arg( prefix ).
                     arg( getNextId( txn, type ), 3, 10, QLatin1Char('0' ) );
    else
        return QString();
}

void Procs::retypeObject(Udb::Obj &o, quint32 type)
{
    Q_ASSERT( !o.isNull() );
    if( o.getType() == type )
        return;
    o.setCell( Udb::Obj::KeyList() << Stream::DataCell().setAtom( o.getType() ), o.getValue( Root::AttrIdent ) );
    QString id = o.getCell( Udb::Obj::KeyList() << Stream::DataCell().setAtom( type ) ).toString();
    if( id.isEmpty() )
        id = getNextIdString( o.getTxn(), type );
    o.setString( Root::AttrIdent, id );
    o.setTimeStamp( Root::AttrModifiedOn );
    o.setType( type );
    if( isDiagNode( o.getType() ) )
    {
        Q_ASSERT( !o.getParent().isNull() );
        o.getParent().incCounter( Function::AttrElemCount );
    }
}

void Procs::moveTo(Udb::Obj &o, Udb::Obj &newParent, const Udb::Obj &before)
{
    Q_ASSERT( !o.getParent().isNull() );
    Q_ASSERT( !newParent.isNull() );
    if( isDiagNode( o.getType() ) )
    {
        // Cleanup old parent
        o.getParent().decCounter( Function::AttrElemCount );
        if( o.getParent().getValueAsObj( Function::AttrStart ).equals( o ) )
        {
            o.getParent().clearValue( Function::AttrStart );
            o.clearValue( Connector::Unspecified );
        }
        if( o.getParent().getValueAsObj( Function::AttrFinish ).equals( o ) )
        {
            o.getParent().clearValue( Function::AttrFinish );
            o.clearValue( Connector::Unspecified );
        }
        newParent.incCounter( Function::AttrElemCount );
    }
    // Install new parent
    o.aggregateTo( newParent, before );
}

static void _erasePinnedDiagItems( const Udb::Obj& diagItem )
{
	Udb::Idx idx2( diagItem.getTxn(), Index::PinnedTo );
	if( idx2.seek( diagItem ) ) do
	{
		DiagItem item2 = diagItem.getObject( idx2.getOid() );
		if( item2.getKind() == DiagItem::Plain )
			// diagItem ist ein Frame, an das Plains gepinnt sind; die Plains werden nicht gelscht
			item2.setPinnedTo( Udb::Obj() );
		else
			// Notes und Frames werden mitgelscht
			Procs::erase(item2);
	}while( idx2.nextKey() );
}

static void _eraseDiagItems( const Udb::Obj& orig )
{
    if( orig.isNull() )
        return;
    Udb::Idx idx( orig.getTxn(), Index::OrigObject );
    if( idx.seek( orig ) ) do
    {
        Udb::Obj item = orig.getObject( idx.getOid() );
        if( item.getType() == DiagItem::TID )
        {
			Procs::erase(item);
		}
    }while( idx.nextKey() );
}

static void _recursiveEraseLinks( Udb::Obj &orig )
{
    // Lsche auch die Links, wo das Object Successor ist (die brigen sind ja
    // Children des Predecessors)
    if( orig.isNull() )
        return;
    Udb::Idx idx( orig.getTxn(), Index::Succ );
    if( idx.seek( Stream::DataCell().setOid( orig.getOid() ) ) ) do
    {
        Udb::Obj link = orig.getObject( idx.getOid() );
		if( link.getType() == ConFlow::TID )
        {
            _eraseDiagItems( link );
            link.erase();
        }
    }while( idx.nextKey() );
    Udb::Obj sub = orig.getFirstObj();
    if( !sub.isNull() )do
    {
		// Mit orig werden auch saemtliche Children geloescht. Bevor dies passiert, muessen wir also
		// saemtliche Children behandeln, die Orig von irgendwelchen DiagItem sind.
        if( Procs::isDiagNode( sub.getType() ) || sub.getType() == ConFlow::TID )
            _eraseDiagItems( sub );
        _recursiveEraseLinks( sub );
    }while( sub.next() );
}

void Procs::erase(Udb::Obj &o)
{
    if( o.isNull( false, true ) )
        return;
    if( isDiagNode( o.getType() ) )
    {
        Q_ASSERT( !o.getParent().isNull() );
		// Task loeschen mit Loeschen der nicht mehr benoetigten Links
        _recursiveEraseLinks( o );
        o.getParent().decCounter( Function::AttrElemCount );
        if( o.getParent().getValueAsObj( Function::AttrStart ).equals( o ) )
            o.getParent().clearValue( Function::AttrStart );
        if( o.getParent().getValueAsObj( Function::AttrFinish ).equals( o ) )
            o.getParent().clearValue( Function::AttrFinish );
        _eraseDiagItems( o );
    }else if( o.getType() == ConFlow::TID )
        _eraseDiagItems( o );
    else if( o.getType() == DiagItem::TID )
    {
		// Die Notes und Frames hier loeschen
		// Entferne auch die DiagItems, welche an das item gepinnt sind
		_erasePinnedDiagItems( o );
	}
    o.erase();
	// Weil this und die untergeordneten eh geloescht werden ist, auch Function::AttrElemCount nicht mehr relevant
}

bool Procs::isDiagram(quint32 type)
{
    return type == Function::TID || type == FuncDomain::TID || type == Diagram::TID;
}

bool Procs::isDiagNode(quint32 type)
{
    return type == Function::TID || type == Event::TID || type == Connector::TID;
}

QList<Udb::Obj> Procs::addItemsToDiagram(Udb::Obj diagram, const QList<Udb::Obj> &items, QPointF where)
{
    QSet<Udb::OID> existingItems = findAllItemOrigOids( diagram );
    QList<Udb::Obj> done;
    foreach( Udb::Obj o, items )
    {
        if( isValidAggregate( Function::TID, o.getType() ) &&
                !existingItems.contains( o.getOid() ) )
        {
            DiagItem::create( diagram, o, where );
            where += QPointF( DiagItem::s_rasterX, DiagItem::s_rasterY ); // TODO: unschn
            done.append( o );
            existingItems.insert( o.getOid() );
        }else if( o.getType() == ConFlow::TID && !existingItems.contains( o.getOid() ) )
        {
            DiagItem::create( diagram, o );
            existingItems.insert( o.getOid() );
        }
    }
    return done;
}

QSet<Udb::OID> Procs::findAllItemOrigOids( const Udb::Obj& diagram )
{
    QSet<Udb::OID> test;
    Udb::Obj sub = diagram.getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == DiagItem::TID )
        {
#ifdef _DEBUG
            Udb::Obj orig = sub.getValueAsObj( DiagItem::AttrOrigObject );
            if( test.contains( orig.getOid() ) )
            {
                qWarning() << "WARNING: Diagram" << formatObjectTitle( diagram ) <<
                              "already contains PdmItem for" << prettyTypeName( orig.getType() ) <<
                              formatObjectTitle( orig );
            }
            test.insert( orig.getOid() );
#else
			test.insert( sub.getValue( DiagItem::AttrOrigObject ).getOid() );
#endif
        }
    }while( sub.next() );
    return test;
}

QList<Udb::Obj> Procs::addItemLinksToDiagram(Udb::Obj diagram, const QList<Udb::Obj> &items)
{
    QList<Udb::Obj> links = findHiddenLinks( diagram, items );
    foreach( Udb::Obj link, links )
        DiagItem::create( diagram, link );
    return links;
}

QList<Udb::Obj> Procs::findHiddenLinks(const Udb::Obj &diagram, QList<Udb::Obj> startset)
{
    if( startset.isEmpty() )
    {
        Udb::Obj sub = diagram.getFirstObj();
        if( !sub.isNull() ) do
        {
            if( sub.getType() == DiagItem::TID )
            {
                Udb::Obj orig = sub.getValueAsObj( DiagItem::AttrOrigObject );
                if( isValidAggregate( Function::TID, orig.getType() ) )
                    startset.append( orig );
            }
        }while( sub.next() );
    }
    QList<Udb::Obj> res;
    QSet<Udb::OID> existingItems = findAllItemOrigOids( diagram );
    foreach( Udb::Obj o, startset )
    {
        Udb::Idx predIdx( o.getTxn(), Index::Pred );
        if( predIdx.seek( Stream::DataCell().setOid( o.getOid() ) ) ) do
        {
            Udb::Obj link = o.getObject( predIdx.getOid() );
            if( !link.isNull() && !existingItems.contains( link.getOid() ) &&
                    existingItems.contains( link.getValue( ConFlow::AttrSucc ).getOid() ) )
            {
                // Obj in Startset ist Pred von einem Link, dessen Succ im Diagramm ist, und
                // der Link selber ist im Diagramm noch nicht enthalten.
                res.append( link );
                existingItems.insert( link.getOid() ); // vorher war "&& !res.contains( link )" in Bedingung
            }
        }while( predIdx.nextKey() );
        Udb::Idx succIdx( o.getTxn(), Index::Succ );
        if( succIdx.seek( Stream::DataCell().setOid( o.getOid() ) ) ) do
        {
            Udb::Obj link = o.getObject( succIdx.getOid() );
            if( !link.isNull() && !existingItems.contains( link.getOid() ) &&
                    existingItems.contains( link.getValue( ConFlow::AttrPred ).getOid() ) )
            {
                // Obj in Startset ist Succ von einem Link, dessen Pred im Diagramm ist, und
                // der Link selber ist im Diagramm noch nicht enthalten.
                res.append( link );
                existingItems.insert( link.getOid() );
            }
        }while( succIdx.nextKey() );
    }
    return res;
}

QList<Udb::Obj> Procs::findAllItemOrigObjs(const Udb::Obj &diagram, bool schedObjs, bool links)
{
    QList<Udb::Obj> res;
    Udb::Obj sub = diagram.getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == DiagItem::TID )
        {
            Udb::Obj o = sub.getValueAsObj( DiagItem::AttrOrigObject );
            if( schedObjs && isValidAggregate( Function::TID, o.getType() ) )
                res.append( o );
            else if( links && o.getType() == ConFlow::TID )
                res.append( o );
        }
    }while( sub.next() );
    return res;
}

QList<Udb::Obj> Procs::findExtendedSchedObjs(QList<Udb::Obj> startset, quint8 levels, bool toSucc, bool toPred )
{
    // Diese Methode garantiert nicht, dass die Ergebnisse nicht schon im Diagramm sind
    // RISK: diese Funktion ist bei grossen Diagrammen zu langsam!
    QList<Udb::Obj> res;
    QSet<Udb::OID> visited;
    while( levels > 0 )
    {
        QList<Udb::Obj> objs;
        foreach( Udb::Obj o, startset )
        {
            if( !visited.contains( o.getOid() ) )
            {
                visited.insert( o.getOid() );
                if( toSucc )
                {
                    QList<Udb::Obj> tmp = findSuccessors( o );
					// NOTE: tmp waere unnoetig; ich habe mit Qt4.4 verifiziert, dass foreach die Funktion
                    // nur einmal aufruft.
                    foreach( Udb::Obj s, tmp )
                    {
                        objs.append( s );
                    }
                }
                if( toPred )
                {
                    QList<Udb::Obj> tmp = findPredecessors( o );
                    foreach( Udb::Obj p, tmp )
                    {
                        objs.append( p );
                    }
                }
            }
        }
        res += objs;
        startset = objs;
        levels--;
    }
    return res;
}

QList<Udb::Obj> Procs::findSuccessors(const Udb::Obj &item)
{
    // Diese Funktion garantiert nicht, dass Items nicht schon im Diagramm sind!
    QList<Udb::Obj> successors;
    Udb::Idx predIdx( item.getTxn(), Index::Pred );
    if( predIdx.seek( Stream::DataCell().setOid( item.getOid() ) ) ) do
    {
        Udb::Obj link = item.getObject( predIdx.getOid() );
        if( !link.isNull() )
        {
            Udb::Obj succ = link.getValueAsObj( ConFlow::AttrSucc );
            if( !succ.isNull() )
                successors.append( succ );
        }
    }while( predIdx.nextKey() );
    return successors;
}

QList<Udb::Obj> Procs::findPredecessors( const Udb::Obj &item)
{
    // Diese Funktion garantiert nicht, dass Items nicht schon im Diagramm sind!
    QList<Udb::Obj> predecessors;
    Udb::Idx succIdx( item.getTxn(), Index::Succ );
    if( succIdx.seek( Stream::DataCell().setOid( item.getOid() ) ) ) do
    {
        Udb::Obj link = item.getObject( succIdx.getOid() );
        if( !link.isNull() )
        {
            Udb::Obj pred = link.getValueAsObj( ConFlow::AttrPred );
            if( !pred.isNull() )
                predecessors.append( pred );
        }
    }while( succIdx.nextKey() );
    return predecessors;
}

typedef QPair<Udb::Obj,int> Pred;
typedef QHash<Udb::OID,Pred> Visited; // current -> ( Predecessor, path length )
typedef QMultiHash<Udb::OID,Udb::OID> PredSucc;

static void _search( const Udb::Obj& cur, int val, const Udb::Obj& goal,
                     const PredSucc& predSucc, Visited& visited )
{
    val += 1;
    PredSucc::const_iterator i = predSucc.find( cur.getOid() );
    while( i != predSucc.end() && i.key() == cur.getOid() )
    {
        Udb::Obj next = cur.getObject( i.value() );
        Pred& p = visited[ i.value() ];
        if( p.first.isNull() )
        {
            // Not yet visited
            p.first = cur;
            p.second = val;
            if( !next.equals( goal ) )
                _search( next, val, goal, predSucc, visited );
        }else if( p.second > val )
        {
            // already visited, but shorter path found
            p.first = cur;
            p.second = val;
        }else
            // already visited
        ++i;
    }
}

static QList<Udb::Obj> _unroll( const Udb::Obj &start, const Udb::Obj &goal, const Visited& visited  )
{
    QList<Udb::Obj> path;
    Visited::const_iterator i = visited.find( goal.getOid() );
    while( i != visited.end() )
    {
        path.prepend( start.getObject(i.key()) );
        if( i.value().first.equals( start ) )
        {
            path.prepend( start );
            return path;
        }
        i = visited.find( i.value().first.getOid() );
    }
    return QList<Udb::Obj>();
}

QList<Udb::Obj> Procs::findShortestPath(const Udb::Obj &start, const Udb::Obj &goal )
{
    // Diese Methode garantiert nicht, dass die Ergebnisse noch nicht im Diagramm sind!

    Q_ASSERT( !start.isNull() && !goal.isNull() );
    // Es gibt dafr keinen schlauen bzw. etablierten Algorithmus ausser rudimentre Suche
    Visited visited;
    PredSucc predSucc;
    Udb::Idx predIdx( start.getTxn(), Index::Pred );
    if( predIdx.first() ) do
    {
        Udb::Obj o = start.getObject( predIdx.getOid() );
        Q_ASSERT( !o.isNull() );
        predSucc.insert( o.getValue( ConFlow::AttrPred ).getOid(), o.getValue( ConFlow::AttrSucc ).getOid() );
    }while( predIdx.next() );
    _search( start, 0, goal, predSucc, visited );
    if( !visited.contains( goal.getOid() ) )
    {
        // Wir haben keinen Pfad gefunden, also umgekehrte Suche
        visited.clear();
        _search( goal, 0, start, predSucc, visited );
        return _unroll( goal, start, visited );
    }else
        return _unroll( start, goal, visited );
}

QList<Udb::Obj> Procs::findAllAliasses(const Udb::Obj &diagram)
{
    QList<Udb::Obj> res;
    Udb::Obj sub = diagram.getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == DiagItem::TID )
        {
            Udb::Obj o = sub.getValueAsObj( DiagItem::AttrOrigObject );
            if( isValidAggregate( Function::TID, o.getType() ) && !o.getParent().equals( diagram ) )
                res.append( sub );
        }
    }while( sub.next() );
    return res;
}

Udb::Obj Procs::findItemInDiagram(const Udb::Obj &diagram, const Udb::Obj &orig)
{
    if( diagram.isNull() || orig.isNull() )
        return DiagItem();
    DiagItem sub = diagram.getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == DiagItem::TID && sub.getOrigObject().equals( orig ) )
            return sub;
    }while( sub.next() );
    return DiagItem();
}

QSet<Udb::Obj> Procs::findHiddenSchedObjs(const Udb::Obj &diagram)
{
    Udb::Obj sub = diagram.getFirstObj();
    QSet<Udb::Obj> hiddens;
    if( !sub.isNull() ) do
    {
        if( isValidAggregate( Function::TID, sub.getType() ) )
            hiddens.insert( sub );
    }while( sub.next() );
    sub = diagram.getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == DiagItem::TID )
            hiddens.remove( sub.getValueAsObj( DiagItem::AttrOrigObject ) );
    }while( sub.next() );
    return hiddens;
}

