# Welcome to FlowLine2

FlowLine2 is a modelling tool supporting Functional Analysis and Business Process Modelling using Event-driven process chain (EPC) syntax with BPMN extensions (instead of Functional Flow Block Diagrams). It also integrates an outliner with cross-link capabilities, Lua scripting and a full text database with built-in search engine. 

![alt text](http://software.rochus-keller.info/flowlinescreenshot2.png "FlowLine2 Screenshot")

Even if the tool was already of use in projects it is still considered work in progress by its author. Especially the System Tree and allocation concept need more work. 

Here a selection of relevant literature the tool is based on:

* Grady, J. (2006): System Requirements Analysis, 1th edition; Elsevier
* Grady, J. (2010): System Management, 2nd edition; CRC Press
* Staud, J. (2006): Geschäftsprozessanalyse, 3rd edition; Springer
* OMG Specification 2009-01-03 (2008): Business Process Model and Notation (BPMN), Version 1.2; Object Management Group

## Download and Installation

You can either compile FlowLine2 yourself or download
the pre-compiled version from here: 

http://software.rochus-keller.info/FlowLine2_win32.zip

http://software.rochus-keller.info/FlowLine2_linux_x86.tar.gz

This is a compressed single-file executable which was built using the source code from here. You can also build the executable yourself if you want (see below for instructions). Since it is a single executable, it can just be downloaded and unpacked. No installation is necessary. You therefore need no special privileges to run FlowLine2 on your machine. 

Here is a demo file with some instructions on how to use FlowLine2: http://software.rochus-keller.info/FlowLine2Demo.fln2

## How to Build FlowLine2

### Preconditions
FlowLine2 was originally developed using Qt4.x. The single-file executables are static builds based on Qt 4.4.3. But it compiles also well with the Qt 4.8 series; the current version is not yet compatible with Qt 5.x, but migration should be straight forward. 

You can download the Qt 4.4.3 source tree from here: http://download.qt.io/archive/qt/4.4/qt-all-opensource-src-4.4.3.tar.gz

The source tree also includes documentation and build instructions.

If you intend to do static builds on Windows without dependency on C++ runtime libs and manifest complications, follow the recommendations in this post: http://www.archivum.info/qt-interest@trolltech.com/2007-02/00039/Fed-up-with-Windows-runtime-DLLs-and-manifest-files-Here's-a-solution.html

Here is the summary on how to do implement Qt Win32 static builds:
1. in Qt/mkspecs/win32-msvc2005/qmake.conf replace MD with MT and MDd with MTd
2. in Qt/mkspecs/features clear the content of the two embed_manifest_*.prf files (but don't delete the files)
3. run configure -release -static -platform win32-msvc2005

To use Qt with FlowLine2 you have to make the following modification: QTreeView::indexRowSizeHint has to be virtual; the correspondig line in qtreeview.h should look like:
    virtual int indexRowSizeHint(const QModelIndex &index) const;

### Build Steps
Follow these steps if you inted to build FlowLine2 yourself (don't forget to meet the preconditions before you start):

1. Create a directory; let's call it BUILD_DIR
2. Download the FlowLine2 source code from https://github.com/rochus-keller/FlowLine2/archive/master.zip and unpack it to the BUILD_DIR; rename the subdirectory to "FlowLine2".
3. Download the Stream source code from https://github.com/rochus-keller/Stream/archive/github.zip and unpack it to the BUILD_DIR; rename "Stream-github" to "Stream".
4. Download the Udb source code from https://github.com/rochus-keller/Udb and unpack it to the BUILD_DIR; rename "Udb-github" to "Udb".
5. Create the subdirectory "Sqlite3" in BUILD_DIR; download the Sqlite source from http://software.rochus-keller.info/Sqlite3.tar.gz and unpack it to the subdirectory.
6. Download the Txt source code from https://github.com/rochus-keller/Txt and unpack it to the BUILD_DIR; rename "Txt-github" to "Txt".
7. Download the Oln2 source code from https://github.com/rochus-keller/Oln2 and unpack it to the BUILD_DIR; rename "Oln2-github" to "Oln2".
8. Download the NAF source code from https://github.com/rochus-keller/NAF/archive/master.zip and unpack it to the BUILD_DIR; rename "NAF-Master" to "NAF". We only need the Gui2, Script, Script2 and Qtl2 subdirectory so you can delete all other stuff in the NAF directory.
9. Create the subdirectory "Lua" in BUILD_DIR; download the modified Lua source from http://cara.nmr-software.org/download/Lua_5.1.5_CARA_modified.tar.gz and unpack it to the subdirectory.
10. Download the CrossLine source code from https://github.com/rochus-keller/CrossLine/archive/master.zip and unpack it to the BUILD_DIR; rename the subdirectory to "CrossLine"; we only need the files DocSelector.* and DocTabWidget.*.
11. Download the WorkTree source code from https://github.com/rochus-keller/WorkTree/archive/master.zip and unpack it to the BUILD_DIR; rename the subdirectory to "WorkTree"; FlowLine2 needs a subset of WorkTree classes.
12. Download the QtSingleApplication source files from https://github.com/qtproject/qt-solutions/tree/master/qtsingleapplication/src and copy them to BUILD_DIR/QtApp directory together with this file: http://software.rochus-keller.info/QtApp.pri
13. Goto the BUILD_DIR/FlowLine2 subdirectory and execute `QTDIR/bin/qmake FlowLine2.pro` (see the Qt documentation concerning QTDIR).
14. Run make; after a couple of minutes you will find the executable in the tmp subdirectory.

Alternatively you can open FlowLine2.pro using QtCreator and build it there.

## Support
If you need support or would like to post issues or feature requests please use the Github issue list at https://github.com/rochus-keller/FlowLine2/issues or send an email to the author.



