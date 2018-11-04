#ifndef EPKHELPER_H
#define EPKHELPER_H

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

#include <QObject>
#include <Udb/Obj.h>

namespace Epk
{
    class Procs : public QObject
    {
    public:
        static QString prettyTypeName( quint32 type );
        static QString prettyAttrName( quint32 attr );
        static QString formatObjectId(const Udb::Obj & o, bool useOid = false);
        static QString formatObjectTitle(const Udb::Obj & o, bool showId = true);
        static QString formatValue(Udb::Atom attr, const Udb::Obj & obj, bool useIcons);
        static QString formatConnType(quint8 type, bool addTypeName = false );
        static QString formatDirection( quint8 );
        static QString prettyDate( const QDate& d, bool includeDay = true, bool includeWeek = true );
        static QString prettyDateTime( const QDateTime& t );
        static bool canHaveText(quint32 type);
        static bool canConvert( const Udb::Obj&, quint32 toType );
        static bool isValidAggregate( quint32 parent, quint32 child );
        static Udb::Obj createObject( quint32 type, Udb::Obj parent, const Udb::Obj &before = Udb::Obj() );
        static quint32 getNextId( Udb::Transaction *, quint32 type );
        static QString getNextIdString( Udb::Transaction *, quint32 type );
        static void retypeObject( Udb::Obj& o, quint32 type ); // Prüft nicht, ob zulässig!
        static void moveTo( Udb::Obj& o, Udb::Obj& newParent, const Udb::Obj& before ); // Prüft nicht, ob zulässig!
        static void erase( Udb::Obj& o );
        static bool isDiagram( quint32 type );
        static bool isDiagNode( quint32 type );
        static QSet<Udb::OID> findAllItemOrigOids( const Udb::Obj& diagram );
        static QList<Udb::Obj> addItemsToDiagram( Udb::Obj diagram, const QList<Udb::Obj>& items, QPointF where );
        static QList<Udb::Obj> addItemLinksToDiagram( Udb::Obj diagram, const QList<Udb::Obj>& items );
        static QList<Udb::Obj> findHiddenLinks( const Udb::Obj& diagram, QList<Udb::Obj> startset );
        static QSet<Udb::Obj> findHiddenSchedObjs( const Udb::Obj& diagram );
        static QList<Udb::Obj> findAllItemOrigObjs( const Udb::Obj& diagram, bool schedObjs, bool links );
        static QList<Udb::Obj> findExtendedSchedObjs( QList<Udb::Obj> startset, quint8 levels, bool toSucc, bool toPred );
        static QList<Udb::Obj> findSuccessors( const Udb::Obj& item );
        static QList<Udb::Obj> findPredecessors( const Udb::Obj& item );
        static QList<Udb::Obj> findShortestPath( const Udb::Obj& start, const Udb::Obj& goal );
        static QList<Udb::Obj> findAllAliasses( const Udb::Obj& diagram ); // returns DiagItems
        static Udb::Obj findItemInDiagram( const Udb::Obj& diagram, const Udb::Obj& orig );
    private:
        explicit Procs(){}
    };
}

#endif // EPKHELPER_H
