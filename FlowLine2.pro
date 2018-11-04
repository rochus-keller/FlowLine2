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

QT += core gui network svg

TARGET = FlowLine2
TEMPLATE = app

INCLUDEPATH += ../NAF ./.. ../../Libraries

win32 {
	INCLUDEPATH += $$[QT_INSTALL_PREFIX]/include/Qt
	RC_FILE = FlowLine2.rc
	DEFINES -= UNICODE
	LIBS += -lQtCLucene
 }else {
	DESTDIR = ./tmp
	OBJECTS_DIR = ./tmp
	CONFIG(debug, debug|release) {
		DESTDIR = ./tmp-dbg
		OBJECTS_DIR = ./tmp-dbg
		DEFINES += _DEBUG
	}
	RCC_DIR = ./tmp
	UI_DIR = ./tmp
	MOC_DIR = ./moc
	QMAKE_CXXFLAGS += -Wno-reorder -Wno-unused-parameter
	INCLUDEPATH += /home/me/Programme/Qt-4.4.3/include/Qt
	LIBS += -lQtCLucene
	DEFINES += LUA_USE_LINUX
 }


SOURCES += FlnMain.cpp\
	FlowLine2App.cpp \
    EpkObjects.cpp \
    ../WorkTree/GenericMdl.cpp \
    ../WorkTree/GenericCtrl.cpp \
    ../WorkTree/TextViewCtrl.cpp \
    ../WorkTree/Funcs.cpp \
    ../WorkTree/AttrViewCtrl.cpp \
    ../WorkTree/ObjectTitleFrame.cpp \
    FuncTree.cpp \
    EpkProcs.cpp \
    FuncsImp.cpp \
    FlnMainWindow.cpp \
    ../WorkTree/SceneOverview.cpp \
    EpkItems.cpp \
    EpkItemMdl.cpp \
    EpkCtrl.cpp \
    EpkView.cpp \
    EpkLayouter.cpp \
    EpkLinkViewCtrl.cpp \
    EpkStream.cpp \
    FlnFolderCtrl.cpp \
    ../WorkTree/SearchView.cpp \
    ../WorkTree/Indexer.cpp \
    SbdObjects.cpp \
    SysTree.cpp \
    AllocViewCtrl.cpp \
	EpkLuaBinding.cpp \
    ../WorkTree/RefByViewCtrl.cpp

HEADERS  += \
	FlowLine2App.h \
    EpkObjects.h \
    ../WorkTree/GenericMdl.h \
    ../WorkTree/GenericCtrl.h \
    ../WorkTree/TextViewCtrl.h \
    ../WorkTree/Funcs.h \
    ../WorkTree/AttrViewCtrl.h \
    ../WorkTree/ObjectTitleFrame.h \
    FuncTree.h \
    EpkProcs.h \
    FuncsImp.h \
    FlnMainWindow.h \
    ../WorkTree/SceneOverview.h \
    EpkItems.h \
    EpkItemMdl.h \
    EpkCtrl.h \
    EpkView.h \
    EpkLayouter.h \
    EpkLinkViewCtrl.h \
    EpkStream.h \
    FlnFolderCtrl.h \
    ../WorkTree/SearchView.h \
    ../WorkTree/Indexer.h \
    SbdObjects.h \
    SysTree.h \
    AllocViewCtrl.h \
	EpkLuaBinding.h \
    ../WorkTree/RefByViewCtrl.h


include(Libraries.pri)

RESOURCES += \
	FlowLine2.qrc
