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

#include "FuncsImp.h"
#include <WorkTree/Funcs.h>
#include <WorkTree/TextViewCtrl.h>
#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include "EpkProcs.h"
#include "EpkObjects.h"
using namespace Fln;

static Udb::Obj _getAlias(const Udb::Obj& o)
{
    return o.getValueAsObj( Oln::OutlineItem::AttrAlias );
}

static void _setText(Udb::Obj &o, const QString &str)
{
    o.setString( Epk::Root::AttrText, str );
    o.setTimeStamp( Epk::Root::AttrModifiedOn );
}

static QString _prettyName( quint32 atom, bool isType, Udb::Transaction* txn )
{
	if( isType )
		return Epk::Procs::prettyTypeName( atom );
	else if( atom < Epk::StartOfDynAttr )
        return Epk::Procs::prettyAttrName( atom );
    else
    {
        Q_ASSERT( txn != 0 );
        return QString::fromLatin1( txn->getDb()->getAtomString( atom ) );
    }
}

void FuncsImp::init()
{
    Wt::Funcs::getText = Epk::Procs::formatObjectTitle;
    Wt::Funcs::setText = _setText;
    Wt::Funcs::isValidAggregate = Epk::Procs::isValidAggregate;
    Wt::Funcs::formatValue = Epk::Procs::formatValue;
    Wt::Funcs::getName = _prettyName;
    Wt::Funcs::getAlias = _getAlias;
    Wt::Funcs::createObject = Epk::Procs::createObject;
    Wt::Funcs::moveTo = Epk::Procs::moveTo;
    Wt::Funcs::erase = Epk::Procs::erase;
    Wt::TextViewCtrl::s_pin = ":/FlowLine2/Images/pin.png";
}
