#ifndef FLN_MAINWINDOW_H
#define FLN_MAINWINDOW_H

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

#include <QMainWindow>
#include <Udb/Transaction.h>
#include <Gui2/AutoMenu.h>

namespace Oln
{
    class DocTabWidget;
}
namespace Wt
{
    class AttrViewCtrl;
    class TextViewCtrl;
    class SceneOverview;
    class SearchView;
	class RefByViewCtrl;
}
namespace Epk
{
    class EpkLinkViewCtrl;
}
namespace Fln
{
    class FuncTreeCtrl;
    class AttrViewCtrl;
    class TextViewCtrl;
    class FuncTreeCtrl;
    class FolderCtrl;
    class SysTreeCtrl;
    class AllocViewCtrl;

    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        MainWindow(Udb::Transaction* txn);
        ~MainWindow();
        Udb::Transaction* getTxn() const { return d_txn; }
		void showOid(quint64 oid);
    signals:
        void closing();
    protected slots:
        void onFullScreen();
		void onSetAppFont();
        void onAbout();
        void onTabChanged( int i );
        void onFollowObject( const Udb::Obj&);
        void onFuncSelected( const Udb::Obj&);
        void onFuncDblClicked(const Udb::Obj&);
        void onSysSelected( const Udb::Obj&);
        void onOpenEpkDiagram();
        void onGoBack();
        void onGoForward();
        void onShowSubTask();
        void onShowSuperTask();
        void onFollowAlias();
        void onPdmSelection();
        void onLinkSelected(const Udb::Obj&,bool);
        void onOpenDocument();
        void onFldrSelected(const Udb::Obj&);
        void onFldrDblClicked(const Udb::Obj&);
        void onFollowOID(quint64);
        void onUrlActivated(const QUrl& );
        void onSearch();
        void onSearchSelected(const Udb::Obj&);
        void onAllocSelected(const Udb::Obj&);
		void handleExecute();
		void saveEditor();
		void onSetScriptFont();
		void onItemActivated(const Udb::Obj&);
		void onItemActivated(quint64);
		void onRebuildIndices();
		void onAutoStart();
	protected:
        void setCaption();
        void addTopCommands( Gui2::AutoMenu* );
        void setupAttrView();
        void setupTextView();
        void setupFunctions();
        void setupOverview();
        void setupLinkView();
        void setupAllocView();
        void setupFolders();
        void setupSysTree();
        void setupSearchView();
		void setupTerminal();
		void setupItemRefView();
		void pushBack(const Udb::Obj & o); // TODO: Db-Callback und gelöschte Objekte aus History entfernen
        void showInEpkDiagram( const Udb::Obj& select, bool checkAllOpenDiagrams );
        void openEpkDiagram( const Udb::Obj& diagram, const Udb::Obj& select = Udb::Obj() );
        void openOutline( const Udb::Obj& doc, const Udb::Obj& select = Udb::Obj() );
		void openScript( const Udb::Obj& doc );
        static void toFullScreen(QMainWindow * w);
        // Overrides
        void closeEvent ( QCloseEvent * event );
    private:
        Wt::AttrViewCtrl* d_atv;
        Wt::TextViewCtrl* d_tv;
        FuncTreeCtrl* d_ft;
        Epk::EpkLinkViewCtrl* d_lv;
        FolderCtrl* d_fldr;
        SysTreeCtrl* d_sys;
		Wt::RefByViewCtrl* d_rbv;
		AllocViewCtrl* d_alloc;
        Wt::SceneOverview* d_ov;
        Wt::SearchView* d_sv;
        Udb::Transaction* d_txn;
		Oln::DocTabWidget* d_tab;
        QList<Udb::OID> d_backHisto; // d_backHisto.last() ist aktuell angezeigtes Objekt
        QList<Udb::OID> d_forwardHisto;
        bool d_pushBackLock;
        bool d_fullScreen;
        bool d_selectLock;
    };
}

#endif // FLN_MAINWINDOW_H
