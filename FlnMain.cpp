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

#include <QtApp/QtSingleApplication>
#include "FlnMainWindow.h"
#include "FlowLine2App.h"
#include <QIcon>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QtDebug>
#include <Udb/DatabaseException.h>
using namespace Fln;

int main(int argc, char *argv[])
{
    QtSingleApplication app( FlowLine2App::s_appName, argc, argv);

    QIcon icon;
    icon.addFile( ":/FlowLine2/Images/epk16.png" );
    icon.addFile( ":/FlowLine2/Images/epk32.png" );
    icon.addFile( ":/FlowLine2/Images/epk48.png" );
    icon.addFile( ":/FlowLine2/Images/epk64.png" );
    icon.addFile( ":/FlowLine2/Images/epk128.png" );
    app.setWindowIcon( icon );

#ifndef _DEBUG
    try
    {
#endif
        FlowLine2App ctx;
        QObject::connect( &app, SIGNAL( messageReceived(const QString&)), &ctx, SLOT( onOpen(const QString&) ) );

        QString path;

        QSettings set;
        if( set.contains( "App/Font" ) )
        {
            QFont f1 = QApplication::font();
            QFont f2 = set.value( "App/Font" ).value<QFont>();
            f1.setFamily( f2.family() );
            f1.setPointSize( f2.pointSize() );
            QApplication::setFont( f1 );
            ctx.setAppFont( f1 );
        }else
            ctx.setAppFont( QApplication::font() );

        QStringList args = QCoreApplication::arguments();
		QString oidArg;
		for( int i = 1; i < args.size(); i++ ) // arg 0 enthlt Anwendungspfad
        {
            if( !args[ i ].startsWith( '-' ) )
			{
				path = args[ i ];
			}else
			{
				if( args[ i ].startsWith( "-oid:") )
					oidArg = args[ i ];
			}
		}

        if( path.isEmpty() )
            path = QFileDialog::getSaveFileName( 0, FlowLine2App::tr("Create/Open Repository - FlowLine"),
                QString(), QString( "*%1" ).arg( QLatin1String( FlowLine2App::s_extension ) ),
                0, QFileDialog::DontConfirmOverwrite | QFileDialog::DontUseNativeDialog );
        if( path.isEmpty() )
            return 0;

        if( !path.toLower().endsWith( QLatin1String( FlowLine2App::s_extension ) ) )
            path += QLatin1String( FlowLine2App::s_extension );

		if( path.contains(QChar(' ')) && !path.startsWith(QChar('"') ) )
			path = QChar('"') + path + QChar('"');

		if( !oidArg.isEmpty() )
			path += QChar(' ') + oidArg;
#ifndef _DEBUG
        if( app.sendMessage( path ) )
                 return 0; // Eine andere Instanz ist bereits offen
#endif
        if( !ctx.open( path ) )
            return -1;

        const int res = app.exec();
        //ctx.getTxn()->getDb()->dump();
        return res;
#ifndef _DEBUG
    }catch( Udb::DatabaseException& e )
    {
        QMessageBox::critical( 0, FlowLine2App::tr("FlowLine Error"),
            QString("Database Error: [%1] %2").arg( e.getCodeString() ).arg( e.getMsg() ), QMessageBox::Abort );
        return -1;
    }catch( std::exception& e )
    {
        QMessageBox::critical( 0, FlowLine2App::tr("FlowLine Error"),
            QString("Generic Error: %1").arg( e.what() ), QMessageBox::Abort );
        return -1;
    }catch( ... )
    {
        QMessageBox::critical( 0, FlowLine2App::tr("FlowLine Error"),
            QString("Generic Error: unexpected internal exception"), QMessageBox::Abort );
        return -1;
    }
#endif
    return 0;
}
