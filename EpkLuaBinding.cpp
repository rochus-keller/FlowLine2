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

#include "EpkLuaBinding.h"
#include <Udb/LuaBinding.h>
#include "EpkObjects.h"
#include "EpkProcs.h"
#include <Script2/QtValue.h>
#include <Script/Engine2.h>
#include <Udb/Idx.h>
#include <Udb/Transaction.h>
using namespace Epk;
using namespace Udb;

struct _Node
{
	static int getElemCount(lua_State *L)
	{
		Node* obj = CoBin<Node>::check( L, 1 );
		lua_pushinteger( L, obj->getValue( Function::AttrElemCount ).getUInt32() );
		return 1;
	}
	static int getSubFunctions(lua_State *L)
	{
		Node* f = CoBin<Node>::check( L, 1 );
		QList<Obj> items;
		Obj sub = f->getFirstObj();
		if( !sub.isNull() ) do
		{
			const Atom t = sub.getType();
			if( t == Function::TID )
				items.append(sub);
		}while( sub.next() );
		lua_createtable(L, items.size(), 0 );
		const int table = lua_gettop(L);
		for( int i = 0; i < items.size(); i++ )
		{
			Udb::LuaBinding::pushObject( L, items[i] );
			lua_rawseti( L, table, i + 1 );
		}
		return 1;
	}
	static int getElements(lua_State *L)
	{
		Node* f = CoBin<Node>::check( L, 1 );
		QList<Obj> items;
		Obj sub = f->getFirstObj();
		if( !sub.isNull() ) do
		{
			const Atom t = sub.getType();
			if( t == Function::TID || t == Event::TID || t == Connector::TID ) // Flows gehören zu Pred
				items.append(sub);
		}while( sub.next() );
		lua_createtable(L, items.size(), 0 );
		const int table = lua_gettop(L);
		for( int i = 0; i < items.size(); i++ )
		{
			Udb::LuaBinding::pushObject( L, items[i] );
			lua_rawseti( L, table, i + 1 );
		}
		return 1;
	}
	static int getOutbounds(lua_State *L)
	{
		Node* f = CoBin<Node>::check( L, 1 );
		QList<Udb::Obj> links;
		Udb::Idx idx( f->getTxn(), Epk::Index::Pred );
		if( idx.seek( Stream::DataCell().setOid( f->getOid() ) ) ) do
		{
			Udb::Obj link = f->getObject( idx.getOid() );
			if( link.getType() == ConFlow::TID )
				links.append( link );
		}while( idx.nextKey() );
		lua_createtable(L, links.size(), 0 );
		const int table = lua_gettop(L);
		for( int i = 0; i < links.size(); i++ )
		{
			Udb::LuaBinding::pushObject( L, links[i] );
			lua_rawseti( L, table, i + 1 );
		}
		return 1;
	}
	static int getInbounds(lua_State *L)
	{
		Node* f = CoBin<Node>::check( L, 1 );
		QList<Udb::Obj> links;
		Udb::Idx idx( f->getTxn(), Epk::Index::Succ );
		if( idx.seek( Stream::DataCell().setOid( f->getOid() ) ) ) do
		{
			Udb::Obj link = f->getObject( idx.getOid() );
			if( link.getType() == ConFlow::TID )
				links.append( link );
		}while( idx.nextKey() );
		lua_createtable(L, links.size(), 0 );
		const int table = lua_gettop(L);
		for( int i = 0; i < links.size(); i++ )
		{
			Udb::LuaBinding::pushObject( L, links[i] );
			lua_rawseti( L, table, i + 1 );
		}
		return 1;
	}
	static int linkTo(lua_State *L)
	{
		Node* from = CoBin<Node>::check( L, 1 );
		Node* to = CoBin<Node>::check( L, 2 );
		Obj link = Procs::createObject( ConFlow::TID, *from );
		link.setValue( ConFlow::AttrPred, *from );
		link.setValue( ConFlow::AttrSucc, *to );
		Udb::LuaBinding::pushObject( L, link );
		return 1;
	}
	static int createFunction(lua_State *L)
	{
		Node* n = CoBin<Node>::check( L, 1 );
		if( !Procs::isValidAggregate( n->getType(), Function::TID ) )
			luaL_error( L, "cannot create Function with parent %s", Procs::prettyTypeName( n->getType() ).toAscii().data() );
		Udb::Obj o = Procs::createObject( Function::TID, *n );
		Udb::LuaBinding::pushObject( L, o );
		return 1;
	}
	static int createEvent(lua_State *L)
	{
		Node* n = CoBin<Node>::check( L, 1 );
		if( !Procs::isValidAggregate( n->getType(), Event::TID ) )
			luaL_error( L, "cannot create Event with parent %s", Procs::prettyTypeName( n->getType() ).toAscii().data() );
		Udb::Obj o = Procs::createObject( Event::TID, *n );
		Udb::LuaBinding::pushObject( L, o );
		return 1;
	}
	static int createConnector(lua_State *L)
	{
		Node* n = CoBin<Node>::check( L, 1 );
		if( !Procs::isValidAggregate( n->getType(), Connector::TID ) )
			luaL_error( L, "cannot create Connector with parent %s", Procs::prettyTypeName( n->getType() ).toAscii().data() );
		Udb::Obj o = Procs::createObject( Connector::TID, *n );
		Udb::LuaBinding::pushObject( L, o );
		return 1;
	}
};
static const luaL_reg _Node_reg[] =
{
	{ "getElemCount", _Node::getElemCount },
	{ "getSubFunctions", _Node::getSubFunctions },
	{ "getElements", _Node::getElements },
	{ "getOutbounds", _Node::getOutbounds },
	{ "getInbounds", _Node::getInbounds },
	{ "linkTo", _Node::linkTo },
	{ "createFunction", _Node::createFunction },
	{ "createEvent", _Node::createEvent },
	{ "createConnector", _Node::createConnector },
	{ 0, 0 }
};

struct _Function
{
	static int getStartEvent(lua_State *L)
	{
		Function* obj = CoBin<Function>::check( L, 1 );
		Udb::LuaBinding::pushObject( L, obj->getStart() );
		return 1;
	}
	static int setStartEvent(lua_State *L)
	{
		Function* obj = CoBin<Function>::check( L, 1 );
		if( Event* e = CoBin<Event>::cast( L, 2 ) )
			obj->setStart( *e );
		else
			obj->setStart( Obj() );
		return 0;
	}
	static int getFinishEvent(lua_State *L)
	{
		Function* obj = CoBin<Function>::check( L, 1 );
		Udb::LuaBinding::pushObject( L, obj->getFinish() );
		return 1;
	}
	static int setFinishEvent(lua_State *L)
	{
		Function* obj = CoBin<Function>::check( L, 1 );
		if( Event* e = CoBin<Event>::cast( L, 2 ) )
			obj->setFinish( *e );
		else
			obj->setFinish( Obj() );
		return 0;
	}
	static int setTopToBottom(lua_State *L)
	{
		Function* obj = CoBin<Function>::check( L, 1 );
		obj->setDirection( (lua_toboolean(L, 2) )? Function::TopToBottom : Function::LeftToRight );
		return 0;
	}
	static int allocateTo(lua_State *L)
	{
		Function* obj = CoBin<Function>::check( L, 1 );
		SystemElement* sys = CoBin<SystemElement>::check( L, 2 );
		Epk::Allocation a = Epk::Procs::createObject( Epk::Allocation::TID, *obj );
		a.setFunc( *obj );
		a.setElem( *sys );
		Udb::LuaBinding::pushObject( L, a );
		return 1;
	}
	static int getAllocs(lua_State *L)
	{
		Function* obj = CoBin<Function>::check( L, 1 );
		QList<Udb::Obj> allocs;
		Udb::Idx idx( obj->getTxn(), Epk::Index::Func );
		if( idx.seek( Stream::DataCell().setOid( obj->getOid() ) ) ) do
		{
			Udb::Obj a = obj->getObject( idx.getOid() );
			if( a.getType() == Allocation::TID )
				allocs.append( a );
		}while( idx.nextKey() );
		lua_createtable(L, allocs.size(), 0 );
		const int table = lua_gettop(L);
		for( int i = 0; i < allocs.size(); i++ )
		{
			Udb::LuaBinding::pushObject( L, allocs[i] );
			lua_rawseti( L, table, i + 1 );
		}
		return 1;
	}
};
static const luaL_reg _Function_reg[] =
{
	{ "getStartEvent", _Function::getStartEvent },
	{ "setStartEvent", _Function::setStartEvent },
	{ "getFinishEvent", _Function::getFinishEvent },
	{ "setFinishEvent", _Function::setFinishEvent },
	{ "setTopToBottom", _Function::setTopToBottom },
	{ "allocateTo", _Function::allocateTo },
	{ "getAllocations", _Function::getAllocs },
	{ 0, 0 }
};

struct _Connector
{
	static int getConnType(lua_State *L)
	{
		Connector* obj = CoBin<Connector>::check( L, 1 );
		lua_pushinteger( L, obj->getConnType() );
		return 1;
	}
	static int setConnType(lua_State *L)
	{
		Connector* obj = CoBin<Connector>::check( L, 1 );
		int t = Connector::Unspecified;
		t = luaL_checkinteger( L, 2 );
		if( t < Connector::Unspecified || t > Connector::Finish )
			luaL_argerror( L, 2, "expecting Connector.Unspecified, And, Or, Xor, Start or Finish" );
		obj->setConnType( t );
		return 0;
	}
};

static const luaL_reg _Connector_reg[] =
{
	{ "getConnType", _Connector::getConnType },
	{ "setConnType", _Connector::setConnType },
	{ 0, 0 }
};

struct _ConFlow
{
	static int getPred(lua_State *L)
	{
		ConFlow* obj = CoBin<ConFlow>::check( L, 1 );
		Udb::LuaBinding::pushObject( L, obj->getPred() );
		return 1;
	}
	static int getSucc(lua_State *L)
	{
		ConFlow* obj = CoBin<ConFlow>::check( L, 1 );
		Udb::LuaBinding::pushObject( L, obj->getSucc() );
		return 1;
	}
};

static const luaL_reg _ConFlow_reg[] =
{
	{ "getPred", _ConFlow::getPred },
	{ "getSucc", _ConFlow::getSucc },
	{ 0, 0 }
};

struct _Repository
{
	Transaction* d_txn;
	_Repository():d_txn(0) {}

	static int getRootFunction(lua_State *L)
	{
		_Repository* obj = Lua::ValueBinding<_Repository>::check( L, 1 );
		Udb::LuaBinding::pushObject( L, FuncDomain::getOrCreateRoot( obj->d_txn ) );
		return 1;
	}
	static int getRootFolder(lua_State *L)
	{
		_Repository* obj = Lua::ValueBinding<_Repository>::check( L, 1 );
		Udb::LuaBinding::pushObject( L, Udb::RootFolder::getOrCreate( obj->d_txn ) );
		return 1;
	}
	static int getRootElement(lua_State *L)
	{
		_Repository* obj = Lua::ValueBinding<_Repository>::check( L, 1 );
		Udb::LuaBinding::pushObject( L, Epk::SystemElement::getOrCreateRoot( obj->d_txn ) );
		return 1;
	}
	static int commit(lua_State *L)
	{
		_Repository* obj = Lua::ValueBinding<_Repository>::check( L, 1 );
		obj->d_txn->commit();
		return 0;
	}
	static int rollback(lua_State *L)
	{
		_Repository* obj = Lua::ValueBinding<_Repository>::check( L, 1 );
		obj->d_txn->rollback();
		return 0;
	}
};

static const luaL_reg _Repository_reg[] =
{
	{ "getRootFolder", _Repository::getRootFolder },
	{ "getRootFunction", _Repository::getRootFunction },
	{ "getRootElement", _Repository::getRootElement },
	{ "commit", _Repository::commit },
	{ "rollback", _Repository::rollback },
	{ 0, 0 }
};

static int _getPrettyTitle(lua_State *L)
{
	Udb::ContentObject* obj = CoBin<Udb::ContentObject>::check( L, 1 );
	*Lua::QtValue<QString>::create(L) = Procs::formatObjectTitle( *obj );
	return 1;
}

static int _erase(lua_State *L)
{
	Udb::ContentObject* obj = CoBin<Udb::ContentObject>::check( L, 1 );
	Procs::erase( *obj );
	// *obj = Udb::Obj(); // Nein, es koennte ja danach Rollback stattfinden
	return 1;
}

static int _moveTo(lua_State *L)
{
	Udb::ContentObject* obj = CoBin<Udb::ContentObject>::check( L, 1 );
	Udb::ContentObject* parent = CoBin<Udb::ContentObject>::check( L, 2 );
	Udb::ContentObject* before = CoBin<Udb::ContentObject>::cast( L, 3 );
	if( before && !before->getParent().equals( *parent ) )
		luaL_argerror( L, 3, "'before' must be a child of 'parent'" );
	if( !Procs::isValidAggregate( parent->getType(), obj->getType() ) )
		luaL_argerror( L, 2, "invalid 'parent' for object" );
	if( before )
		Procs::moveTo( *obj, *parent, *before );
	else
		Procs::moveTo( *obj, *parent, Udb::Obj() );
	return 0;
}

static const luaL_reg _ContentObject_reg[] =
{
	{ "getPrettyTitle", _getPrettyTitle },
	{ "delete", _erase },
	{ "moveTo", _moveTo },
	{ 0, 0 }
};

static void _pushObject(lua_State * L, const Obj& o )
{
	const Udb::Atom t = o.getType();
	if( o.isNull() )
		lua_pushnil(L);
	else if( t == Folder::TID )
		CoBin<Folder>::create( L, o );
	else if( t == TextDocument::TID )
		CoBin<TextDocument>::create( L, o );
	else if( t == ScriptSource::TID )
		CoBin<ScriptSource>::create( L, o );
	else if( t == Oln::OutlineItem::TID )
		CoBin<Oln::OutlineItem>::create( L, o );
	else if( t == Oln::Outline::TID )
		CoBin<Oln::Outline>::create( L, o );
	else if( t == Function::TID )
		CoBin<Function>::create( L, o );
	else if( t == Event::TID )
		CoBin<Event>::create( L, o );
	else if( t == Diagram::TID )
		CoBin<Diagram>::create( L, o );
	else if( t == FuncDomain::TID )
		CoBin<FuncDomain>::create( L, o );
	else if( t == Connector::TID )
		CoBin<Connector>::create( L, o );
	else if( t == ConFlow::TID )
		CoBin<ConFlow>::create( L, o );
	else if( t == SystemElement::TID )
		CoBin<SystemElement>::create( L, o );
	else if( t == Allocation::TID )
		CoBin<Allocation>::create( L, o );
	else
	{
		lua_pushnil(L);
		// luaL_error hier nicht sinnvoll, da auch aus normalem Code heraus aufgerufen und Lua exit aufruft
		qWarning( "cannot create binding object for type ID %d %s", t, Procs::prettyTypeName(t).toUtf8().data() );
	}
}

static int _getFolderItems(lua_State *L)
{
	Udb::Folder* f = CoBin<Udb::Folder>::check( L, 1 );
	QList<Obj> items;
	Obj sub = f->getFirstObj();
	if( !sub.isNull() ) do
	{
		const Atom t = sub.getType();
		if( t == Folder::TID || t == ScriptSource::TID ||
			t == Oln::Outline::TID ||
			t == Diagram::TID )
			items.append(sub);
	}while( sub.next() );
	lua_createtable(L, items.size(), 0 );
	const int table = lua_gettop(L);
	for( int i = 0; i < items.size(); i++ )
	{
		Udb::LuaBinding::pushObject( L, items[i] );
		lua_rawseti( L, table, i + 1 );
	}
	return 1;
}

static int _createElement(lua_State *L)
{
	SystemElement* obj = CoBin<SystemElement>::check( L, 1 );
	SystemElement sys = Epk::Procs::createObject( SystemElement::TID, *obj );
	Udb::LuaBinding::pushObject( L, sys );
	return 1;
}

static int _getAllocs(lua_State *L)
{
	SystemElement* obj = CoBin<SystemElement>::check( L, 1 );
	QList<Udb::Obj> allocs;
	Udb::Idx idx( obj->getTxn(), Epk::Index::Elem );
	if( idx.seek( Stream::DataCell().setOid( obj->getOid() ) ) ) do
	{
		Udb::Obj a = obj->getObject( idx.getOid() );
		if( a.getType() == Allocation::TID )
			allocs.append( a );
	}while( idx.nextKey() );
	lua_createtable(L, allocs.size(), 0 );
	const int table = lua_gettop(L);
	for( int i = 0; i < allocs.size(); i++ )
	{
		Udb::LuaBinding::pushObject( L, allocs[i] );
		lua_rawseti( L, table, i + 1 );
	}
	return 1;
}

static const luaL_reg _SystemElement_reg[] =
{
	{ "createElement", _createElement },
	{ "getAllocations", _getAllocs },
	{ 0, 0 }
};

static bool _isWritable( quint32 atom, const Obj& )
{
	return  atom >= StartOfDynAttr;
}

void Epk::LuaBinding::install(lua_State *L)
{
	Lua::StackTester test(L, 0 );
	Udb::LuaBinding::pushObject = _pushObject;
	Udb::LuaBinding::isWritable = _isWritable;
	CoBin<Udb::ContentObject>::addMethods( L, _ContentObject_reg );
	CoBin<Udb::Folder>::addMethod( L, "getFolderItems", _getFolderItems );
	CoBin<Node,ContentObject>::install( L, "EpkNode", _Node_reg );
	CoBin<Function,Node>::install( L, "Function", _Function_reg );
	CoBin<Event,Node>::install( L, "Event" );
	CoBin<Diagram,Function>::install( L, "EpkDiagram" );
	CoBin<FuncDomain,Function>::install( L, "FuncDomain" );
	CoBin<Connector,Node>::install( L, "Connector", _Connector_reg );
	lua_getfield( L, LUA_GLOBALSINDEX, "Connector" );
	const int table = lua_gettop(L);
	lua_pushinteger( L, Connector::Unspecified );
	lua_setfield( L, table, "Unspecified" );
	lua_pushinteger( L, Connector::And );
	lua_setfield( L, table, "And" );
	lua_pushinteger( L, Connector::Or );
	lua_setfield( L, table, "Or" );
	lua_pushinteger( L, Connector::Xor );
	lua_setfield( L, table, "Xor" );
	lua_pushinteger( L, Connector::Start );
	lua_setfield( L, table, "Start" );
	lua_pushinteger( L, Connector::Finish );
	lua_setfield( L, table, "Finish" );
	lua_pop(L, 1); // table
	CoBin<ConFlow,ContentObject>::install( L, "ControlFlow", _ConFlow_reg );
	CoBin<SystemElement,ContentObject>::install( L, "SystemElement", _SystemElement_reg );
	CoBin<Allocation,ContentObject>::install( L, "Allocation" );
	Lua::ValueBinding<_Repository>::install( L, "Repository", _Repository_reg );
}

void Epk::LuaBinding::setRepository(lua_State *L, Udb::Transaction* txn)
{
	Lua::StackTester test(L, 0 );
	lua_getfield( L, LUA_GLOBALSINDEX, "Repository" );
	const int table = lua_gettop(L);
	Lua::ValueBinding<_Repository>::create(L)->d_txn = txn;
	lua_pushvalue( L, -1 );
	lua_setfield( L, table, "instance" );
	lua_setfield( L, LUA_GLOBALSINDEX, "fln" );
	lua_pop(L, 1); // table
}

void Epk::LuaBinding::setCurrentObject(lua_State *L, const Obj & o)
{
	Lua::StackTester test(L, 0 );
	lua_getfield( L, LUA_GLOBALSINDEX, "Repository" );
	const int table = lua_gettop(L);
	Udb::LuaBinding::pushObject( L, o );
	lua_setfield( L, table, "current" );
	lua_pop(L, 1); // table
}

void Epk::LuaBinding::setCurrentObject(const Obj & o)
{
	setCurrentObject( Lua::Engine2::getInst()->getCtx(), o );
}

