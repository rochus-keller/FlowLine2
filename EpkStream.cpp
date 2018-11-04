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

#include "EpkStream.h"
#include <QSet>
#include <QtDebug>
#include <Oln2/OutlineStream.h>
#include "EpkObjects.h"
#include "EpkProcs.h"
using namespace Epk;
using namespace Stream;

void EpkStream::exportProc( Stream::DataWriter& out, const Udb::Obj& proc )
{
    if( proc.isNull() || proc.getType() != Function::TID )
		return;
    out.writeSlot( DataCell().setAscii( "FlowLineStream" ) );
    out.writeSlot( DataCell().setAscii( "0.1" ) );
    out.writeSlot( DataCell().setDateTime( QDateTime::currentDateTime() ) );
    writeObjTo( out, proc, Udb::Obj() );
}

Udb::Obj EpkStream::importProc( Stream::DataReader& in, Udb::Obj& parent )
{
	d_error.clear();
	if( in.nextToken() != DataReader::Slot || in.getValue().getArr() != "FlowLineStream" )
	{
		d_error = "invalid data stream";
		return Udb::Obj();
	}
	if( in.nextToken() != DataReader::Slot || in.getValue().getArr() != "0.1" )
	{
		d_error = "invalid stream version";
		return Udb::Obj();
	}
	if( in.nextToken() != DataReader::Slot || !in.getValue().isDateTime() )
	{
		d_error = "invalid protocol";
		return Udb::Obj();
	}
	if( in.nextToken() != DataReader::BeginFrame || !in.getName().getTag().equals( "proc" ) )
	{
		d_error = "invalid protocol";
		return Udb::Obj();
	}
    return readItem( in, parent );
}

static inline DataCell _floatToDouble( const DataCell& in )
{
    return DataCell().setDouble( in.getFloat() );
}

void EpkStream::writeAtts( Stream::DataWriter& out, const Udb::Obj& orig, const Udb::Obj &diagItem )
{
	DataCell v;
    out.writeSlot( orig, NameTag( "oid" ) ); // Darauf nimmt dann ControlFlow Bezug

    v = orig.getValue( Root::AttrText );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "text" ) );
    v = orig.getValue( Root::AttrIdent );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "id" ) );
    v = orig.getValue( Connector::AttrConnType );
    if( v.hasValue() )
        out.writeSlot( v, NameTag( "ctyp" ), true );
    v = orig.getValue( Oln::OutlineItem::AttrAlias );
    if( v.hasValue() )
        out.writeSlot( DataCell().setUuid( orig.getValueAsObj(
            Oln::OutlineItem::AttrAlias ).getUuid() ), NameTag( "ali" ), true );

    if( !diagItem.isNull() )
    {
        v = diagItem.getValue( DiagItem::AttrPosX );
        if( v.hasValue() )
            out.writeSlot( _floatToDouble(v), NameTag( "posx" ), true );
        v = diagItem.getValue( DiagItem::AttrPosY );
        if( v.hasValue() )
            out.writeSlot( _floatToDouble(v), NameTag( "posy" ), true );
        v = diagItem.getValue( DiagItem::AttrWidth );
        if( v.hasValue() )
            out.writeSlot( _floatToDouble(v), NameTag( "w" ), true );
        v = diagItem.getValue( DiagItem::AttrHeight );
        if( v.hasValue() )
            out.writeSlot( _floatToDouble(v), NameTag( "h" ), true );
    }
}

static DataCell _setNodeList( const QPolygonF& p )
{
    Stream::DataWriter w;
    for( int i = 0; i < p.size(); i++ )
    {
        w.startFrame();
        w.writeSlot( DataCell().setDouble( p[i].x() ) );
        w.writeSlot( DataCell().setDouble( p[i].y() ) );
        w.endFrame();
    }
    return w.getBml();
}

void EpkStream::writeObjTo( Stream::DataWriter& out, const Udb::Obj& orig, const Udb::Obj &diagItem )
{
    const quint32 type = orig.getType();
    /*if( type == Atoms::typeEpkNote )
		out.startFrame( NameTag( "note" ) );
	else if( type == Atoms::typeEpkFrame )
		out.startFrame( NameTag( "fram" ) );
    else */
    if( type == Function::TID )
    {
        if( orig.getValue( Function::AttrElemCount ).getUInt32() > 0 )
            out.startFrame( NameTag( "proc" ) );
        else
            out.startFrame( NameTag( "func" ) );
    }else if( type == Event::TID )
		out.startFrame( NameTag( "evt" ) );
    else if( type == Connector::TID )
		out.startFrame( NameTag( "conn" ) );
	else
		return;
    writeAtts( out, orig, diagItem );

	Oln::OutlineStream olnout;
    Udb::Obj sub = orig.getFirstObj();
	if( !sub.isNull() ) do
	{
        if( sub.getType() == DiagItem::TID )
        {
            Udb::Obj subOrig = sub.getValueAsObj(DiagItem::AttrOrigObject);
			if( subOrig.getType() == Oln::OutlineItem::TID )
				olnout.writeTo( out, subOrig );
            else
                writeObjTo( out, subOrig, sub );
        }
	}while( sub.next() );
    sub = orig.getFirstObj();
	if( !sub.isNull() ) do
	{
        if( sub.getType() == DiagItem::TID )
        {
            DiagItem di = sub;
            Udb::Obj subOrig = di.getOrigObject();
            if( subOrig.getType() == ConFlow::TID )
            {
                out.startFrame( NameTag( "flow" ) );
                out.writeSlot( subOrig.getValue( ConFlow::AttrPred ), NameTag( "from" ) );
                out.writeSlot( subOrig.getValue( ConFlow::AttrSucc ), NameTag( "to" ) );
                out.writeSlot( _setNodeList(di.getNodeList()), NameTag( "nlst" ) );
                out.endFrame();
            }
        }
	}while( sub.next() );

    out.endFrame();
}

static inline Udb::Obj _create( const NameTag& tag, Udb::Obj& parent )
{
    DiagItem::Kind k = DiagItem::Plain;
	quint32 type = 0;
    if( tag.equals( "proc" ) )
        type = Function::TID;
    else if( tag.equals( "note" ) )
    {
        type = DiagItem::TID;
        k = DiagItem::Note;
    }else if( tag.equals( "fram" ) )
    {
        type = DiagItem::TID;
        k = DiagItem::Frame;
    }else if( tag.equals( "func" ) )
        type = Function::TID;
    else if( tag.equals( "evt" ) )
        type = Event::TID;
    else if( tag.equals( "conn" ) )
        type = Connector::TID;
	else
		return Udb::Obj();
    if( type )
    {
        if( k == DiagItem::Plain )
            return Procs::createObject( type, parent );
        else
            return DiagItem::createKind( parent, k );
    }else
        return Udb::Obj();
}

static quint8 _flnConnTypeTo2( const DataCell& in )
{
    switch( in.getUInt8() )
    {
    case 0: // ConnType_And
        return Connector::And;
    case 1: // ConnType_Or
        return Connector::Or;
    case 2: // ConnType_Xor
        return Connector::Xor;
    case 10: // ConnType_Start
        return Connector::Start;
    case 11: // ConnType_End
        return Connector::Finish;
    default:
        return Connector::Unspecified;
    }
}

static float _flnPosTo2( const DataCell& in )
{
    return in.getDouble();
}

Udb::Obj EpkStream::readItem( Stream::DataReader& in, Udb::Obj& parent, quint64* remoteOid, int level )
{
    // proc, func etc. wurde hier bereits gelesen
	Q_ASSERT( !parent.isNull() );
	Q_ASSERT( in.getCurrentToken() == Stream::DataReader::BeginFrame && !in.getName().getTag().equals("flow") );
    //qDebug() << QString(level*4,QChar('*')) << "Frame" << in.getName().toString(); // TEST
    Udb::Obj orig = _create( in.getName().getTag(), parent );
    if( orig.isNull() )
	{
        qWarning() << QString( "ignoring '%1'" ).arg( in.getName().toString() );
        in.skipToEndFrame();
        return Udb::Obj();
	}
    Q_ASSERT( !orig.isNull() );
    // orig.getType() ist Function, Event oder Connector, oder Aber DiagItem bei Notes und Frames
    DiagItem di;
    if( remoteOid != 0 ) // nur bei Top-Level-Call der Fall
    {
        if( orig.getType() != DiagItem::TID )
            di = DiagItem::create( parent, orig );
        else
            di = orig;
    }

	QHash<quint64,Udb::Obj> subs;
	Oln::OutlineStream olns;
    Stream::DataReader::Token t = in.nextToken( true ); // Schaue eine Stelle voraus
	// t kann sein: Slot, EndFrame, Start von Embedded oder Outline
	while( Stream::DataReader::isUseful( t ) )
	{
		switch( t )
		{
		case Stream::DataReader::Slot:
			{
				in.nextToken(); // bestätige nextToken
				const NameTag n = in.getName().getTag();
               // qDebug() << QString(level*4,QChar(' ')) << "Slot" << n.toString() << in.getValue().toPrettyString(); // TEST
				if( n.equals( "text" ) )
                    orig.setValue( Root::AttrText, in.getValue() );
				else if( n.equals( "id" ) )
				{
                    if( !orig.hasValue( Root::AttrIdent ) )
                        orig.setValue( Root::AttrIdent, in.getValue() );
                    else
                        orig.setValue( Root::AttrAltIdent, in.getValue() );
                }else if( n.equals( "oid" ) && remoteOid )
                    *remoteOid = in.getValue().getOid();
                else if( n.equals( "posx" ) && !di.isNull() )
                    di.setValue( DiagItem::AttrPosX, DataCell().setFloat(_flnPosTo2(in.getValue())) );
                else if( n.equals( "posy" ) && !di.isNull() )
                    di.setValue( DiagItem::AttrPosY, DataCell().setFloat(_flnPosTo2(in.getValue())) );
                else if( n.equals( "w" ) && !di.isNull() )
                    di.setValue( DiagItem::AttrWidth, DataCell().setFloat(_flnPosTo2(in.getValue())) );
                else if( n.equals( "h" ) && !di.isNull() )
                    di.setValue( DiagItem::AttrHeight, DataCell().setFloat(_flnPosTo2(in.getValue())) );
				else if( n.equals( "ctyp" ) )
                    orig.setValue( Connector::AttrConnType, DataCell().setUInt8( _flnConnTypeTo2( in.getValue() ) ) );
				else if( n.equals( "ali" ) )
				{
					Udb::Obj a = parent.getTxn()->getObject( in.getValue() );
					// TODO: was wenn a Teil des Streams ist und noch nicht instanziiert?
                    orig.setValue( Oln::OutlineItem::AttrAlias, a );
				}
			}
			break;
		case Stream::DataReader::EndFrame:
			in.nextToken(); // bestätige nextToken
            return orig;
		case Stream::DataReader::BeginFrame:
			{
				const NameTag n = in.getName().getTag();
				if( n.equals( "oln" ) )
				{
					// olns.readFrom konsumiert final das Token
                   // qDebug() << QString(level*4,QChar('*')) << "Frame" << n.toString(); // TEST
					Udb::Obj oln = olns.readFrom( in, orig.getTxn(), orig.getOid() );
					if( oln.isNull() )
					{
						d_error = QString( "invalid embedded outline" );
						return Udb::Obj();
					}
                    oln.aggregateTo( orig );
				}else if( n.equals( "flow" ) )
				{
					in.nextToken(); // bestätige nextToken
                    readFlow( in, orig, subs, level + 1 );
                }else if( orig.getType() == Function::TID )
				{
					// Lese die Objekte ausser Flow, die in einem Process enthalten sein können
					in.nextToken(); // bestätige nextToken
                    quint64 remoteOid = 0;
                    Udb::Obj sub = readItem( in, orig, &remoteOid, level + 1 );
                    if( !sub.isNull() && remoteOid != 0 )
                    {
                        subs[ remoteOid ] = sub;
                       // qDebug() << "Remote" << remoteOid << "Local" << sub.getOid();
                    }
                    // else ignore; readItem hat bereits ganzes Frame konsumiert
				}else
				{
                    d_error = QString( "invalid frame '%1' in oid=%2" ).arg( n.toString() ).arg( *remoteOid );
					return Udb::Obj();
				}
			}
			break;
		} // switch
		t = in.nextToken(true);
	} // while( isUseful() )
	d_error = QString( "protocol error" );
	return Udb::Obj();
}

static QPolygonF _flnNodesTo2( const DataCell& in )
{
    QPolygonF res;
    Stream::DataReader r( in );
    Stream::DataReader::Token t = r.nextToken();
    while( t == Stream::DataReader::BeginFrame )
    {
        t = r.nextToken();
        QPointF p;
        if( t == Stream::DataReader::Slot )
            p.setX( r.getValue().getDouble() );
        else
            return QPolygonF();
        t = r.nextToken();
        if( t == Stream::DataReader::Slot )
            p.setY( r.getValue().getDouble() );
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

Udb::Obj EpkStream::readFlow( Stream::DataReader& in, Udb::Obj& parent,
                              const QHash<quint64,Udb::Obj>& objs, int level )
{
    Q_ASSERT( in.getCurrentToken() == Stream::DataReader::BeginFrame && in.getName().getTag().equals("flow") );
    Q_ASSERT( !parent.isNull() );
    // Voraussetzung: 'in' sitzt auf einem BeginFrame, das nicht ein Flow ist.
    // qDebug() << QString(level*4,QChar('*')) << "Frame" << in.getName().toString(); // TEST
    Stream::DataReader::Token t = in.nextToken();
    Udb::Obj from;
    Udb::Obj to;
    QPolygonF nodes;
    while( t == Stream::DataReader::Slot )
    {
        const NameTag n = in.getName().getTag();
       //qDebug() << QString(level*4,QChar(' ')) << "Slot" << n.toString() << in.getValue().toPrettyString(); // TEST
        if( n.equals( "from" ) )
            from = objs.value( in.getValue().getOid() );
        else if( n.equals( "to" ) )
            to = objs.value( in.getValue().getOid() );
        else if( n.equals( "nlst" ) )
            nodes = _flnNodesTo2(in.getValue());
        t = in.nextToken();
    }
    if( t != Stream::DataReader::EndFrame )
    {
        d_error = QString( "invalid format" );
        return Udb::Obj();
    }
    //qDebug() << QString(level*4,QChar(' ')) << "Diag" << parent.getOid() << "from" << from.getOid() << "to" << to.getOid(); // TEST
    if( !from.isNull() && !to.isNull() )
    {
        DiagItem di = DiagItem::createLink( parent, from, to );
        di.setNodeList( nodes );
       // qDebug() << QString(level*4,QChar(' ')) << "Fln2 Nodes" << nodes;
        return di.getOrigObject();
    }
    return Udb::Obj();
}
