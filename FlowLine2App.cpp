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

#include "FlowLine2App.h"
#include "FlnMainWindow.h"
#include "EpkObjects.h"
#include "FuncsImp.h"
#include <Oln2/LuaBinding.h>
#include "EpkLuaBinding.h"
#include <QApplication>
#include <QPlastiqueStyle>
#include <QMessageBox>
#include <QDesktopServices>
#include <Udb/Database.h>
#include <Udb/DatabaseException.h>
#include <Oln2/OutlineUdbMdl.h>
#include <Oln2/OutlineItem.h>
#include <Qtl2/Objects.h>
#include <Qtl2/Variant.h>
#include <Script2/QtObject.h>
#include <Script/Engine2.h>
using namespace Fln;

const char* FlowLine2App::s_company = "Dr. Rochus Keller";
const char* FlowLine2App::s_domain = "me@rochus-keller.info";
const char* FlowLine2App::s_appName = "FlowLine2";
const char* FlowLine2App::s_version = "2.3";
const char* FlowLine2App::s_date = "2018-11-04";
const char* FlowLine2App::s_extension = ".fln2";

static FlowLine2App* s_inst = 0;

static int type(lua_State * L)
{
	luaL_checkany(L, 1);
	if( lua_type(L,1) == LUA_TUSERDATA )
	{
		Lua::ValueBindingBase::pushTypeName( L, 1 );
		if( !lua_isnil( L, -1 ) )
			return 1;
		lua_pop( L, 1 );
	}
	lua_pushstring(L, luaL_typename(L, 1) );
	return 1;
}

FlowLine2App::FlowLine2App(QObject *parent) :
    QObject(parent)
{
    s_inst = this;

    FuncsImp::init();

    Oln::OutlineUdbMdl::registerPixmap( Oln::OutlineItem::TID, QString( ":/FlowLine2/Images/outline_item.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( Oln::Outline::TID, QString( ":/FlowLine2/Images/outline.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( Epk::Connector::TID, QString( ":/FlowLine2/Images/connector.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( Epk::Event::TID, QString( ":/FlowLine2/Images/event.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( Epk::Function::TID, QString( ":/FlowLine2/Images/function.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( Epk::FuncDomain::TID, QString( ":/FlowLine2/Images/funcdomain.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( Epk::Diagram::TID, QString( ":/FlowLine2/Images/epk-document.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( Epk::SystemElement::TID, QString( ":/FlowLine2/Images/syselement.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( Epk::Allocation::TID, QString( ":/FlowLine2/Images/allocation.png" ) );
	Oln::OutlineUdbMdl::registerPixmap( Udb::Folder::TID, QString( ":/FlowLine2/Images/folder-open.png" ) );
	Oln::OutlineUdbMdl::registerPixmap( Udb::ScriptSource::TID, QString( ":/FlowLine2/Images/lua-script.png" ) );
//	Oln::OutlineUdbMdl::registerPixmap( TypeEpkFrame, QString( ":/FlowLine2/Images/frame.png" ) );
//	Oln::OutlineUdbMdl::registerPixmap( TypeEpkNote, QString( ":/FlowLine2/Images/note.png" ) );

    qApp->setOrganizationName( s_company );
    qApp->setOrganizationDomain( s_domain );
    qApp->setApplicationName( s_appName );

    qApp->setStyle( new QPlastiqueStyle() );

    d_styles = new Txt::Styles( this );
    Txt::Styles::setInst( d_styles );

	QDesktopServices::setUrlHandler( tr("xoid"), this, "onHandleXoid" ); // hier ohne SLOT und ohne Args ntig!

	Lua::Engine2::setInst( new Lua::Engine2() );
	Lua::Engine2::getInst()->addStdLibs();
	Lua::Engine2::getInst()->addLibrary( Lua::Engine2::PACKAGE );
	Lua::Engine2::getInst()->addLibrary( Lua::Engine2::OS );
	Lua::Engine2::getInst()->addLibrary( Lua::Engine2::IO );
	Lua::QtObjectBase::install( Lua::Engine2::getInst()->getCtx() );
	Qtl::Variant::install( Lua::Engine2::getInst()->getCtx() );
	Qtl::Objects::install( Lua::Engine2::getInst()->getCtx() );
	Udb::LuaBinding::install( Lua::Engine2::getInst()->getCtx() );
	Oln::LuaBinding::install( Lua::Engine2::getInst()->getCtx() );
	Epk::LuaBinding::install( Lua::Engine2::getInst()->getCtx() );
	lua_pushcfunction( Lua::Engine2::getInst()->getCtx(), type );
	lua_setfield( Lua::Engine2::getInst()->getCtx(), LUA_GLOBALSINDEX, "type" );
}

FlowLine2App::~FlowLine2App()
{
    foreach( MainWindow* w, d_docs )
		w->close(); // da selbst qApp->quit() zuerst alle Fenster schliesst, duerfte das nie vorkommen.
    s_inst = 0;
}

QString parseFilePath(const QString & cmdLine )
{
	if( cmdLine.isEmpty() )
		return QString();
	if( cmdLine[0] == QChar('"') )
	{
		// "path"
		const int pos = cmdLine.indexOf( QChar('"'), 1 );
		if( pos != -1 )
			return cmdLine.mid( 1, pos - 1 );
	}else
	{
		// c:\abc.hedb -oid:
		const int pos = cmdLine.indexOf( QChar(' '), 1 );
		if( pos != -1 )
			return cmdLine.left( pos );
	}
	return cmdLine;
}

quint64 parseOid(const QString & cmdLine)
{
	if( cmdLine.isEmpty() )
		return 0;
	int pos = -1;
	if( cmdLine[0] == QChar('"') )
		// "path"
	   pos = cmdLine.indexOf( QChar('"'), 1 );
	else
		pos = cmdLine.indexOf( QChar(' '), 1 );
	if( pos != -1 )
	{
		// -oid:12345 -xx
		const int pos2 = cmdLine.indexOf( QString("-oid:"), pos + 1 );
		if( pos2 != -1 )
		{
			const int pos3 = cmdLine.indexOf( QChar(' '), pos2 + 5 );
			if( pos3 != -1 )
				return cmdLine.mid( pos2 + 5, pos3 - ( pos2 + 5 ) ).toULongLong();
			else
				return cmdLine.mid( pos2 + 5 ).toULongLong();
		}
	}
	return 0;
}

class _SplashDialog : public QDialog
{
public:
	_SplashDialog( QWidget* p ):QDialog(p){ setFixedSize(10,10); exec(); }
	void paintEvent ( QPaintEvent * event )
	{
		QDialog::paintEvent( event );
		QMetaObject::invokeMethod(this,"accept", Qt::QueuedConnection );
	}
};

bool FlowLine2App::open(const QString & cmdLine)
{
	const QString path = parseFilePath( cmdLine );
	const quint64 oid = parseOid( cmdLine );

	foreach( MainWindow* w, d_docs )
    {
        if( w->getTxn()->getDb()->getFilePath() == path ) // TODO: Case sensitive paths?
        {
			w->showOid( oid );
			w->activateWindow();
            w->raise();
#ifndef _WIN32
			_SplashDialog dlg(w);
#endif
			return true;
        }
    }
    Udb::Transaction* txn = 0;
    try
    {
        Udb::Database* db = new Udb::Database( this );
        db->open( path );
        db->setCacheSize( 10000 ); // RISK
        txn = new Udb::Transaction( db, this );
        Epk::Index::init( *db );
        txn->commit();
		txn->setIndividualNotify(false); // RISK
		Oln::OutlineItem::doBackRef();
		txn->addCallback( Oln::OutlineItem::itemErasedCallback );
		db->registerDatabase();
	}catch( Udb::DatabaseException& e )
    {
        QMessageBox::critical( 0, tr("Create/Open Repository"),
            tr("Error <%1>: %2").arg( e.getCodeString() ).arg( e.getMsg() ) );

        // Gib false zurck, wenn ansonsten kein Doc offen ist, damit Anwendung enden kann.
        return d_docs.isEmpty();
    }
    Q_ASSERT( txn != 0 );
	Epk::LuaBinding::setRepository( Lua::Engine2::getInst()->getCtx(), txn );
    MainWindow* w = new MainWindow( txn );
    connect( w, SIGNAL(closing()), this, SLOT(onClose()) );
	w->showOid( oid );
	if( d_docs.isEmpty() )
    {
        setAppFont( w->font() );
    }
	w->activateWindow();
	w->raise();
#ifndef _WIN32
		_SplashDialog dlg(w);
#endif
	d_docs.append( w );
    return true;
}

void FlowLine2App::setAppFont(const QFont &f)
{
    d_styles->setFontStyle( f.family(), f.pointSize() );
}

void FlowLine2App::onOpen(const QString & path)
{
    if( !open( path ) )
    {
        if( d_docs.isEmpty() )
            qApp->quit();
    }
}

void FlowLine2App::onClose()
{
    MainWindow* w = static_cast<MainWindow*>( sender() );
    if( d_docs.removeAll( w ) > 0 )
    {
	   // w->deleteLater(); // Wird mit Qt::WA_DeleteOnClose von MainWindow selber geloescht
    }
}

FlowLine2App *FlowLine2App::inst()
{
    return s_inst;
}

void FlowLine2App::onHandleXoid(const QUrl & url)
{
	const QString oid = url.userInfo();
	const QString uid = url.host();
	Udb::Database::runDatabaseApp( uid, QStringList() << tr("-oid:%1").arg(oid) );
}
