#ifndef FUNCTREE_H
#define FUNCTREE_H

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

#include <WorkTree/GenericCtrl.h>
#include <WorkTree/GenericMdl.h>

namespace Fln
{
    class FuncTreeCtrl : public Wt::GenericCtrl
    {
        Q_OBJECT
    public:
        static const char* s_mimeFuncTree;
        explicit FuncTreeCtrl(QTreeView *tree, Wt::GenericMdl* mdl );
        static FuncTreeCtrl* create(QWidget* parent, const Udb::Obj& root );
        void addCommands( Gui2::AutoMenu* );
        void writeMime( QMimeData *data, const QList<Udb::Obj>& );
        QList<Udb::Obj> readMime(const QMimeData *data, Udb::Obj& parent );
    public slots:
        void onAddFunction();
        void onAddDomain();
        void onAddNext();
        void onToggleType();
        void onImport();
        void onCopy();
        void onPaste();
        void onEditAttrs();
    protected:
        void writeTo(const Udb::Obj & o, Stream::DataWriter &out) const;
        Udb::Obj readFrom(Stream::DataReader & in, Udb::Obj &parent );
    };

    class FuncTreeMdl : public Wt::GenericMdl
    {
    public:
        explicit FuncTreeMdl(QObject *parent = 0);
    protected:
        virtual bool isSupportedType( quint32 );
    };
}

#endif // FUNCTREE_H
