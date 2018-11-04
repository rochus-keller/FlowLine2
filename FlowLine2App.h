#ifndef FLOWLINE2APP_H
#define FLOWLINE2APP_H

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
#include <QUuid>
#include <Txt/Styles.h>

namespace Fln
{
    class MainWindow;

    class FlowLine2App : public QObject
    {
        Q_OBJECT
    public:
        static const char* s_company;
        static const char* s_domain;
        static const char* s_appName;
        static const char* s_version;
        static const char* s_date;
        static const char* s_extension;

        explicit FlowLine2App(QObject *parent = 0);
        ~FlowLine2App();
        static FlowLine2App* inst();
		bool open(const QString& cmdLine);
        const QList<MainWindow*>& getDocs() const { return d_docs; }
        void setAppFont( const QFont& f );
    public slots:
        void onOpen(const QString&);
        void onClose();
		void onHandleXoid(const QUrl & url);
    private:
        QList<MainWindow*> d_docs;
        Txt::Styles* d_styles;
    };
}

#endif // FLOWLINE2APP_H
