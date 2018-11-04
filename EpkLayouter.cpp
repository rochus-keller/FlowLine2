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

#include "EpkLayouter.h"
#include <QHash>
#include "EpkItems.h"
#include "EpkProcs.h"
#include "EpkObjects.h"
#include <Udb/Transaction.h>
#include <graphviz/gvc.h>
#include <graphviz/cgraph.h>
#include <QtDebug>
#include <QApplication>
#include <QSettings>
#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>
#include <QtGui/QDesktopServices>
#include <QProcess>
using namespace Epk;

typedef GVC_t *(*GvContext)(void);
typedef const char * (*GvcVersion)(GVC_t*);
typedef int (*GvFreeContext)(GVC_t *gvc);
typedef int (*GvLayout)(GVC_t *gvc, graph_t *g, const char *engine);
typedef int (*GvFreeLayout)(GVC_t *gvc, graph_t *g);
typedef int (*GvRenderFilename)(GVC_t *gvc, graph_t *g, const char *format, const char *filename);
typedef void (*Attach_attrs)(graph_t *g);
typedef int (*Agclose)(Agraph_t * g);
typedef Agraph_t* (*Agread)(void *chan, Agdisc_t * disc);
typedef Agraph_t* (*Agopen)(const char *name, Agdesc_t desc, Agdisc_t * disc);
typedef char * (*Aglasterr)(void);
typedef Agraph_t* (*Agmemread)(const char *cp);
typedef Agnode_t* (*Agidnode)(Agraph_t * g, unsigned long id, int createflag);
typedef Agedge_t* (*Agidedge)(Agraph_t * g, Agnode_t * t, Agnode_t * h,
                           unsigned long id, int createflag);
// Agidnode und Agidedge funktionieren nicht
typedef Agnode_t* (*Agnode)(Agraph_t * g, const char *name, int createflag);
typedef Agedge_t* (*Agedge)(Agraph_t * g, Agnode_t * t, Agnode_t * h,
                         const char *name, int createflag);
typedef const char* (*Agget)(void *obj, const char *name);
typedef int (*Agset)(void *obj, const char *name, const char *value);
typedef Agsym_t* (*Agattr)(Agraph_t * g, int kind, const char *name, const char *value);
typedef const char* (*Agnameof)(void *);
typedef int (*Agsafeset)(void* obj, const char* name, const char* value, const char* def);
typedef Agraph_t* (*Agsubg)(Agraph_t * g, const char *name, int cflag);

namespace Epk
{
    struct _Gvc
    {
        GvContext gvContext;
        GvcVersion gvcVersion;
        GvFreeContext gvFreeContext;
        GvLayout gvLayout;
        GvFreeLayout gvFreeLayout;
        GvRenderFilename gvRenderFilename;
        Attach_attrs attach_attrs;
        Agclose agclose;
        Agread agread;
        Agopen agopen;
        Aglasterr aglasterr;
        Agmemread agmemread;
        Agidnode agidnode;
        Agidedge agidedge;
        Agnode agnode;
        Agedge agedge;
        Agget agget;
        Agset agset;
        Agsafeset agsafeset;
        Agattr agattr;
        Agnameof agnameof;
		Agsubg agsubg;
        _Gvc():gvContext(0),gvcVersion(0),gvFreeContext(0),gvLayout(0),gvFreeLayout(0),
            agclose(0),gvRenderFilename(0),agread(0),agopen(0),aglasterr(0),
            agmemread(0),agidnode(0),agidedge(0),agnode(0),agedge(0),agget(0),
			agset(0),agattr(0),agnameof(0),attach_attrs(0),agsafeset(0),agsubg(0){}
        bool allResolved() const { return gvContext && gvcVersion && gvFreeContext &&
                    gvLayout && gvFreeLayout && gvRenderFilename && agclose &&
                    agread && agopen && aglasterr && agmemread && agidnode &&
                    agidedge && agnode && agedge && agget && agset && agattr &&
					agnameof && attach_attrs && agsafeset && agsubg; }
    };
}

// aus https://github.com/ellson/graphviz/blob/master/lib/cgraph/graph.c
Agdesc_t Agdirected = { 1, 0, 0, 1 };
Agdesc_t Agstrictdirected = { 1, 1, 0, 1 };
Agdesc_t Agundirected = { 0, 0, 0, 1 };
Agdesc_t Agstrictundirected = { 0, 1, 0, 1 };

EpkLayouter::EpkLayouter(QObject *parent) :
    QObject(parent), d_ctx(0)
{
}

EpkLayouter::~EpkLayouter()
{
    if( d_ctx )
        delete d_ctx;
}

bool EpkLayouter::renderPdf(const QString &dotFile, const QString &pdfFile)
{
    d_errors.clear();
    if( !loadFunctions() )
        return false;

    GVC_t* gvc = d_ctx->gvContext();
    Q_ASSERT( gvc != 0 );

    QFile in( dotFile );
    if( !in.open( QIODevice::ReadOnly ) )
    {
        d_errors.append( tr("Cannot open file for reading: %1").arg( dotFile ) );
        d_ctx->gvFreeContext( gvc );
        return false;
    }
    Agraph_t* G = d_ctx->agmemread( in.readAll().data() ); // das funktioniert!

    if( G == 0 )
    {
        d_errors.append( QString::fromAscii( d_ctx->aglasterr() ) );
        d_ctx->gvFreeContext( gvc );
        return false;
    }

    d_ctx->gvLayout( gvc, G, "dot");
    d_ctx->gvRenderFilename( gvc, G, "pdf", pdfFile.toAscii().data() );

    d_ctx->agclose( G );

    d_ctx->gvFreeContext( gvc );
    return true;
}

bool EpkLayouter::layoutDiagram(const Udb::Obj & diagram, bool ortho, bool topToBottom )
{
    d_errors.clear();
    if( !loadFunctions() )
        return false;

    GVC_t* gvc = d_ctx->gvContext();
    Q_ASSERT( gvc != 0 );

    Agraph_t* G = d_ctx->agopen( Procs::formatObjectTitle( diagram ).toUtf8().data(), Agdirected, 0 );
    Q_ASSERT( G != 0 );

    d_ctx->agattr( G, AGRAPH, "splines", (ortho)?"ortho":"polyline" ); // line, polyline, ortho
    d_ctx->agattr( G, AGRAPH, "rankdir", (topToBottom)?"TB":"LR" );
    d_ctx->agattr( G, AGRAPH, "dpi", "72" );
    d_ctx->agattr( G, AGRAPH, "nodesep", QByteArray::number( DiagItem::s_boxHeight / 72.0 * 0.5 ) );
    d_ctx->agattr( G, AGRAPH, "ranksep",
                   QByteArray::number( ( (topToBottom)?DiagItem::s_boxHeight:DiagItem::s_boxWidth ) / 72.0 * 0.5 ) );
    d_ctx->agattr( G, AGNODE, "fixedsize", "true" );
    d_ctx->agattr( G, AGNODE, "shape", "box" );
    d_ctx->agattr( G, AGNODE, "height", QByteArray::number( DiagItem::s_boxHeight / 72.0 ) ); // inches
    d_ctx->agattr( G, AGNODE, "width", QByteArray::number( DiagItem::s_boxWidth / 72.0 ) ); // inches
    d_ctx->agattr( G, AGEDGE, "label", "" );


    QHash<Udb::OID,Agnode_t*> nodeCache; // orig->node
    QList<Agedge_t*> edgeList;
	QHash<Udb::OID,Agraph_t*> frameCache; // item->graph

	DiagItem item = diagram.getFirstObj();
	if( !item.isNull() ) do
	{
		if( item.getType() == DiagItem::TID && item.getKind() == DiagItem::Frame )
		{
			QList<DiagItem> pinneds = item.getPinneds();
			bool onlyNotes = true;
			for( int i = 0; i < pinneds.size(); i++ )
			{
				if( pinneds[i].getKind() != DiagItem::Note )
				{
					onlyNotes = false;
					break;
				}
			}
			Agraph_t * g = 0;
			if( !onlyNotes )
			{
				g = d_ctx->agsubg( G, QString("cluster%1").arg(item.getOid()).toAscii().data(), 1 );
				// For clusters, this specifies the space between the nodes in the cluster and the cluster
				// bounding box. By default, this is 8 points.
				d_ctx->agattr( g, AGRAPH, "margin", QByteArray::number( (item.hasValue(Root::AttrText))?32:16 ) );
			}
			frameCache[ item.getOid() ] =  g;
		}
	}while( item.next() );

	item = diagram.getFirstObj();
    if( !item.isNull() ) do
    {
		if( item.getType() == DiagItem::TID && item.getKind() != DiagItem::Frame )
        {
            Udb::Obj orig = item.getOrigObject();
			if( orig.getType() != ConFlow::TID  )
            {
				Agraph_t * g = G;
				DiagItem pinned = item.getPinnedTo();
				if( !pinned.isNull() )
				{
					switch( pinned.getKind() )
					{
					case DiagItem::Frame:
						g = frameCache.value(pinned.getOid());
						break;
					case DiagItem::Plain:
						pinned = pinned.getPinnedTo();
						if( !pinned.isNull() && pinned.getKind() == DiagItem::Frame )
							g = frameCache.value(pinned.getOid());
						break;
					}
				}
				if( g == 0 || ( item.getKind() == DiagItem::Note && pinned.isNull() ) )
					continue; // Wir lassen Note-Frames und ungepinnte Notes in Ruhe

				Agnode_t* n = d_ctx->agnode( g, QByteArray::number( item.getOid() ), 1 ); // ID wird von DiagItem verwendet
                Q_ASSERT( n != 0 );

                if( orig.getType() == Connector::TID )
                {
                    d_ctx->agsafeset( n, "height", QByteArray::number( DiagItem::s_boxHeight * 0.5 / 72.0 ),"" );
                    d_ctx->agsafeset( n, "width", QByteArray::number( DiagItem::s_boxHeight * 0.5 / 72.0 ),"" );
				}else if( item.getKind() == DiagItem::Note )
                {
                    d_ctx->agsafeset( n, "height", QByteArray::number( item.getSize().height() / 72.0 ),"" );
                    d_ctx->agsafeset( n, "width", QByteArray::number( item.getSize().width() / 72.0 ),"" );
                }
				nodeCache[ orig.getOid() ] = n; // bei Note zeigt Orig auf Item
            }
        }
    }while( item.next() );
    item = diagram.getFirstObj();
    if( !item.isNull() ) do
    {
        if( item.getType() == DiagItem::TID )
        {
            Udb::Obj link = item.getOrigObject();
            if( link.getType() == ConFlow::TID )
            {
                Agnode_t* pred = nodeCache.value( link.getValue( ConFlow::AttrPred ).getOid() );
                Agnode_t* succ = nodeCache.value( link.getValue( ConFlow::AttrSucc ).getOid() );
				// Wenn man Nodes löscht, können verwaiste PdmItems übrigbleiben bis zum nächsten öffnen.
                if( pred && succ )
                {
                    Agedge_t* e = d_ctx->agedge( G, pred, succ, QByteArray::number( item.getOid() ), 1 );
                    Q_ASSERT( e != 0 );
                    edgeList.append( e );
                }
            }
            DiagItem pin = item.getPinnedTo();
			if( item.getKind() == DiagItem::Note && !pin.isNull() )
            {
                Agnode_t* succ = nodeCache.value( pin.getOrigObject().getOid() );
                Agnode_t* pred = nodeCache.value( item.getOrigObject().getOid() );
                if( pred && succ )
                {
                    Agedge_t* e = d_ctx->agedge( G, pred, succ, QByteArray::number( item.getOid() ) + "-" +
                                                 QByteArray::number( pin.getOid() ), 1 );
                    Q_ASSERT( e != 0 );
                    // Nein: edgeList.append( e );
                }
            }
        }
    }while( item.next() );

    d_ctx->gvLayout( gvc, G, "dot");
//    d_ctx->gvRenderFilename( gvc, G, "pdf", "out.pdf" ); // TEST
    d_ctx->attach_attrs( G ); // ansonsten steht nichts in pos

//  Aus dotguide 2010, App F:
//    Coordinate values increase up and to the right. Positions
//    are represented by two integers separated by a comma, representing the X and Y
//    coordinates of the location specified in points (1/72 of an inch). A position refers
//    to the center of its associated object. Lengths are given in inches.

	QHash<Udb::OID,Agraph_t*>::const_iterator k;
	for( k = frameCache.begin(); k != frameCache.end(); ++k )
	{
		// qDebug() << d_ctx->agget( k.value(), "bb" ) << d_ctx->agget( k.value(), "lp" );
		if( k.value() == 0 )
			continue;
		const QStringList bb = QString::fromAscii( d_ctx->agget( k.value(), "bb" ) ).split( QChar(','), QString::SkipEmptyParts );
		if( bb.isEmpty() )
			continue;
		Q_ASSERT( bb.size() == 4 );
		DiagItem item = diagram.getObject( k.key() );
		// llx,lly,urx,ury gives the coordinates, in points, of the lower-left corner (llx,lly)
		// and the upper-right corner (urx,ury).
		const QPointF lowerLeft( bb[0].toFloat(), -bb[1].toFloat() );
		const QPointF upperRight( bb[2].toFloat(), -bb[3].toFloat() );
		// Bei Note und Frame ist Pos links oben
//		p.setX( p.x() - s.width() / 2.0 );
//		p.setY( p.y() - s.height() / 2.0 );
		item.setPos( QPointF( lowerLeft.x(), upperRight.y() ) );
		item.setSize( QSizeF( upperRight.x() - lowerLeft.x(), lowerLeft.y() - upperRight.y() ) );
	}

    QHash<Udb::OID,Agnode_t*>::const_iterator j;
	for( j = nodeCache.begin(); j != nodeCache.end(); ++j )
	{
        Agnode_t* n = j.value();
        DiagItem item = diagram.getObject( QByteArray( d_ctx->agnameof( n ) ).toULongLong() );
        Q_ASSERT( !item.isNull() );
        QStringList pos = QString::fromAscii( d_ctx->agget( n, "pos" ) ).split( QChar(',') );
        Q_ASSERT( pos.size() == 2 );
        QPointF p( pos.first().toFloat(), -pos.last().toFloat() );
        if( item.getKind() != DiagItem::Plain )
        {
            // Bei Note und Frame ist Pos links oben
            const QSizeF s = item.getSize();
            p.setX( p.x() - s.width() / 2.0 );
            p.setY( p.y() - s.height() / 2.0 );
        }
        item.setPos( p );
    }
    foreach( Agedge_t* e, edgeList )
    {
        DiagItem item = diagram.getObject( QByteArray( d_ctx->agnameof( e ) ).toULongLong() );
        Q_ASSERT( !item.isNull() );
        QStringList points = QString::fromAscii( d_ctx->agget( e, "pos" ) ).split( QChar(' ') );
        Q_ASSERT( points.first().startsWith( QChar('e') ) );
        points.pop_front();
        QPolygonF poly;
        for( int i = (ortho)?0:3; i < ( points.size() - 1 ); i += 3 )
        {
            QStringList pos = points[i].split( QChar(',') );
            Q_ASSERT( pos.size() == 2 );
            poly.append( QPointF( pos.first().toFloat(), -pos.last().toFloat() ) );
        }
        item.setNodeList( poly );

//        Every edge is assigned a pos attribute, which consists of a list of 3n + 1
//        locations. These are B-spline control points: points p0; p1; p2; p3 are the first Bezier
//        spline, p3; p4; p5; p6 are the second, etc.
//        In the pos attribute, the list of control points might be preceded by a start
//        point ps and/or an end point pe. These have the usual position representation with a
//        "s," or "e," prefix, respectively. A start point is present if there is an arrow at p0.
//        In this case, the arrow is from p0 to ps , where ps is actually on the nodes boundary.
    }

    d_ctx->agclose( G );
    d_ctx->gvFreeContext( gvc );
    return true;
}

bool EpkLayouter::loadLibs()
{
    d_errors.clear();
    if( d_ctx )
        return true;

#ifdef _WIN32
    d_libGvc.setFileName( "gvc");
    d_libCgraph.setFileName( "cgraph" );
#else
    d_libGvc.setLoadHints( QLibrary::ResolveAllSymbolsHint );
    d_libCgraph.setLoadHints( QLibrary::ResolveAllSymbolsHint );
    d_libGvc.setFileName( "libgvc.so.6");
    d_libCgraph.setFileName( "libcgraph.so.6" );
#endif
    if( d_libGvc.load() && d_libCgraph.load() )
    {
        return true;
    }else
    {
        d_errors.append( tr("Could not load Graphviz libraries; make sure Graphviz is on the library path.") );
        qDebug() << "EpkLayouter::loadLibs libgvc:" << d_libGvc.errorString();
        d_errors.append( d_libGvc.errorString() );
        qDebug() << "EpkLayouter::loadLibs libcgraph:" << d_libCgraph.errorString();
        d_errors.append( d_libCgraph.errorString() );
        return false;
    }
}

bool EpkLayouter::loadFunctions()
{
    d_errors.clear();
    if( d_ctx )
        return true;
    if( !d_libGvc.isLoaded() || !d_libCgraph.isLoaded() )
    {
        d_errors.append( tr("Graphviz libraries not loaded.") );
        return false;
    }

    _Gvc ctx;
    // libGv
    ctx.gvContext = (GvContext) d_libGvc.resolve( "gvContext" );
    ctx.gvcVersion = (GvcVersion) d_libGvc.resolve( "gvcVersion" );
    ctx.gvFreeContext = (GvFreeContext) d_libGvc.resolve( "gvFreeContext" );
    ctx.gvLayout = (GvLayout) d_libGvc.resolve( "gvLayout" );
    ctx.gvFreeLayout = (GvFreeLayout) d_libGvc.resolve( "gvFreeLayout" );
    ctx.gvRenderFilename = (GvRenderFilename) d_libGvc.resolve( "gvRenderFilename" );
    ctx.attach_attrs = (Attach_attrs) d_libGvc.resolve( "attach_attrs" );

    // libCgraph
    ctx.agclose = (Agclose) d_libCgraph.resolve( "agclose" );
    ctx.agread = (Agread) d_libCgraph.resolve( "agread" );
    ctx.agopen = (Agopen) d_libCgraph.resolve( "agopen" );
    ctx.aglasterr = (Aglasterr) d_libCgraph.resolve( "aglasterr" );
    ctx.agmemread = (Agmemread) d_libCgraph.resolve( "agmemread" );
    ctx.agidnode = (Agidnode) d_libCgraph.resolve( "agidnode" );
    ctx.agidedge = (Agidedge) d_libCgraph.resolve( "agidedge" );
    ctx.agnode = (Agnode) d_libCgraph.resolve( "agnode" );
    ctx.agedge = (Agedge) d_libCgraph.resolve( "agedge" );
    ctx.agget = (Agget) d_libCgraph.resolve( "agget" );
    ctx.agset = (Agset) d_libCgraph.resolve( "agset" );
    ctx.agsafeset = (Agsafeset)d_libCgraph.resolve( "agsafeset" );
    ctx.agattr = (Agattr) d_libCgraph.resolve( "agattr" );
    ctx.agnameof = (Agnameof) d_libCgraph.resolve( "agnameof" );
	ctx.agsubg = (Agsubg) d_libCgraph.resolve( "agsubg" );

    if( !ctx.allResolved() )
    {
        d_errors.append( tr("Could not resolve all needed functions.") );
        return false;
    }
    d_ctx = new _Gvc( ctx );
    return true;
}

bool EpkLayouter::prepareEngine(QWidget * parent)
{
    QSettings set;
    const bool canSetLibPath = addLibraryPath( set.value( "GraphvizBinPath" ).toString() );
    while( !loadLibs() )
    {
        if( canSetLibPath )
        {
            if( QMessageBox::warning( parent, tr("Layout Diagram - WorkTree" ),
                                  tr("<html>Could not find the <a href=\"http://graphviz.org\">"
                                     "Graphviz library</a>. Do you want to locate it?</html>" ),
                                  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel )
                                    == QMessageBox::Cancel )
                return false;
        }else
        {
            QMessageBox::warning( parent, tr("Layout Diagram - WorkTree" ),
                                      tr("<html>Could not find the <a href=\"http://graphviz.org\">"
                                         "Graphviz library</a>. To use this function "
                                         "you have to add the Graphviz 'bin' "
                                         "directory to the PATH variable of your system.</html>" ) );
            return false;
        }
        QString path = QFileDialog::getExistingDirectory( parent,
                              tr("Looking for Graphviz bin directory - WorkTree"),
                              QDesktopServices::storageLocation(
                                  QDesktopServices::ApplicationsLocation ),
                              QFileDialog::DontUseNativeDialog | QFileDialog::ShowDirsOnly );
        if( path.isEmpty() )
            return false;
        set.setValue( "GraphvizBinPath", path );
        addLibraryPath( path );
    }
    return true;
}

bool EpkLayouter::addLibraryPath(const QString & path)
{
#ifdef _WIN32
    // Dieser Code crashed beim Rcksprung aus der Funktion, wenn __stdcall nicht definiert
    QLibrary kernel32( "Kernel32.dll" );
    // aus winbase.h, leider im Compiler nicht verfgbar
    typedef int (__stdcall *SetDllDirectory)( const char* lpPathName );
    SetDllDirectory setDllDirectory = (SetDllDirectory)kernel32.resolve( "SetDllDirectoryA" );
    if( setDllDirectory )
    {
        if( path.isEmpty() )
            return true;
        setDllDirectory( path.toLatin1().data() );
        return true;
    }else
        return false; // QApplication::addLibraryPath hat keine Wirkung auf Windows
#else
    // qDebug() << QProcess::systemEnvironment();
    QApplication::addLibraryPath( path );
    return true;
#endif
}
