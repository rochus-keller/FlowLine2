#ifndef SBDOBJECTS_H
#define SBDOBJECTS_H

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

namespace Sbd
{
    enum { MinAttr = 350, MaxAttr = 354, StartOfDynAttr = 0x100000 };

    class SystemElement : public Udb::ContentObject
    {
    public:
        static int TID;
        SystemElement( const Udb::Obj& o ):Udb::ContentObject(o){}
    };

    class SysRoot : public SystemElement
    {
    public:
        static int TID;
        static const QUuid s_uuid;
        SysRoot( const Udb::Obj& o ):SystemElement(o){}
        static SysRoot getOrCreateRoot( Udb::Transaction* );
    };

    class Interface : public Udb::ContentObject
    {
    public:
        static int TID;
        enum Attrs
        {
            AttrSrc = 350,        // OID, Source Terminal, SystemElement
            AttrDest = 351,         // OID, Destination Terminal, SystemElement
            AttrDirected = 352   // bool
        };
        Interface( const Udb::Obj& o ):Udb::ContentObject(o){}
    };

    struct Index
    {
        static const char* Src; // AttrSrc
        static const char* Dest; // AttrDest
        static void init( Udb::Database &db );
    };
}

#endif // SBDOBJECTS_H
