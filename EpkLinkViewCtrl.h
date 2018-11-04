#ifndef EPKLINKVIEWCTRL_H
#define EPKLINKVIEWCTRL_H

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
#include <Udb/UpdateInfo.h>
#include <Gui2/AutoMenu.h>

class QListWidget;
class QListWidgetItem;

namespace Wt
{
    class ObjectTitleFrame;
}
namespace Epk
{
    class EpkView;

    class EpkLinkViewCtrl : public QObject
    {
        Q_OBJECT
    public:
        explicit EpkLinkViewCtrl(QWidget *parent = 0);

        static EpkLinkViewCtrl* create(QWidget* parent,Udb::Transaction *txn );
        void setObj( const Udb::Obj& );
        void clear();
        QWidget* getWidget() const;
        void addCommands( Gui2::AutoMenu* );
        void showLink( const Udb::Obj& );
        void setObserved( EpkView* );
    signals:
        void signalSelect( const Udb::Obj&, bool open );
    protected slots:
        void onDbUpdate( Udb::UpdateInfo info );
        void onClicked(QListWidgetItem*);
        void onDblClick(QListWidgetItem*);
        void onShowLink();
        void onShowPeer();
        void onDelete();
        void onTitleClick();
        void onCopy();
    private:
        void addLink( const Udb::Obj&, bool inbound );
        void formatItem( QListWidgetItem*, const Udb::Obj&, bool inbound );
        void markObservedLinks();
        Wt::ObjectTitleFrame* d_title;
        QListWidget* d_list;
        EpkView* d_view;
    };
}

#endif // EPKLINKVIEWCTRL_H
