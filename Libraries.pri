#/*
#* Copyright 2010-2018 Rochus Keller <mailto:me@rochus-keller.info>
#*
#* This file is part of the FlowLine2 application.
#*
#* The following is the license that applies to this copy of the
#* application. For a license to use the application under conditions
#* other than those described here, please email to me@rochus-keller.info.
#*
#* GNU General Public License Usage
#* This file may be used under the terms of the GNU General Public
#* License (GPL) versions 2.0 or 3.0 as published by the Free Software
#* Foundation and appearing in the file LICENSE.GPL included in
#* the packaging of this file. Please review the following information
#* to ensure GNU General Public Licensing requirements will be met:
#* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
#* http://www.gnu.org/copyleft/gpl.html.
#*/

SOURCES += \
	../CrossLine/DocSelector.cpp \
	../CrossLine/DocTabWidget.cpp \
	../NAF/Gui/ListView.cpp \


HEADERS  += \
	../CrossLine/DocSelector.h \
	../CrossLine/DocTabWidget.h \
	../NAF/Gui/ListView.h \


include(../Lua/Lua.pri)
include(../Sqlite3/Sqlite3.pri)
include(../../Libraries/QtApp/QtApp.pri)
include(../Udb/Udb.pri)
include(../Stream/Stream.pri)
include(../Oln2/Oln2.pri)
include(../Txt/Txt.pri)
include(../NAF/Gui2/Gui2.pri)
include(../NAF/Qtl2/Qtl2.pri)
include(../NAF/Script/Script.pri)
include(../NAF/Script2/Script2.pri)
