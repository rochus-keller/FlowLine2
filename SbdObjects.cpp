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

#include "SbdObjects.h"
#include <Udb/Database.h>
#include <Udb/Transaction.h>
using namespace Sbd;

const QUuid SysRoot::s_uuid = "{fd68a9e9-8a1b-4d6c-858e-c4e8eb586b00}";

int SysRoot::TID = 350;
int SystemElement::TID = 351;
int Interface::TID = 352;

const char* Index::Src = "Src";
const char* Index::Dest = "Dest";

void Index::init(Udb::Database &db)
{
    Udb::Database::Lock lock( &db);

    db.presetAtom( "StartOfDynAttr", StartOfDynAttr );

    if( db.findIndex( Index::Src ) == 0 )
    {
        Udb::IndexMeta def( Udb::IndexMeta::Value );
        def.d_items.append( Udb::IndexMeta::Item( Interface::AttrSrc ) );
        db.createIndex( Index::Src, def );
    }
    if( db.findIndex( Index::Dest ) == 0 )
    {
        Udb::IndexMeta def( Udb::IndexMeta::Value );
        def.d_items.append( Udb::IndexMeta::Item( Interface::AttrDest ) );
        db.createIndex( Index::Dest, def );
    }
}

SysRoot SysRoot::getOrCreateRoot(Udb::Transaction * txn)
{
    Q_ASSERT( txn != 0 );
    return txn->getOrCreateObject( s_uuid, TID );
}
