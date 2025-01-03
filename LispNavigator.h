#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/*
* Copyright 2024 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the Interlisp project.
*
* The following is the license that applies to this copy of the
* file. For a license to use the file under conditions
* other than those described here, please email to me@rochus-keller.ch.
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
// Adopted from the Lisa Pascal Navigator

#include <QMainWindow>
#include "LispReader.h"

class QTreeWidget;
class QLabel;
class QTreeWidgetItem;
class CodeEditor;
class QPlainTextEdit;
class QListWidget;
class QListWidgetItem;

class Navigator : public QMainWindow
{
    Q_OBJECT

public:
    Navigator(QWidget *parent = 0);
    ~Navigator();

    void load(const QString& path);
    void logMessage(const QString&);

protected slots:
    void fileDoubleClicked(QTreeWidgetItem*,int);
    void handleGoBack();
    void handleGoForward();
    void onXrefDblClicked();
    void onCursor();
    void onUpdateLocation(int line, int col);
    void onSearchAtom();
    void onSelectAtom();
    void onAtomDblClicked(QListWidgetItem*);
    void onRunParser();
    void onOpen();
    void onPropertiesDblClicked(QTreeWidgetItem*,int);

protected:
    struct Location
    {
        // Qt-Koordinaten
        quint32 d_line;
        quint16 d_col;
        quint16 d_yoff;
        QString d_file;
        bool operator==( const Location& rhs ) { return d_line == rhs.d_line && d_col == rhs.d_col &&
                    d_file == rhs.d_file; }
        Location(const QString& f, quint32 l, quint16 c, quint16 y ):d_file(f),d_line(l),d_col(c),d_yoff(y){}
    };
    void pushLocation( const Location& );
    void showFile(const QString& file);
    void showFile(const QString& file, const Lisp::RowCol& pos);
    void openGenerated(const QString& file);
    void showPosition(const Lisp::RowCol& pos);
    void showFile(const Location& file);
    void createSourceTree();
    void createXref();
    void createLog();
    void createAtomList();
    void createProperties();
    void closeEvent(QCloseEvent* event);
    void fillXrefForAtom(const char* atom, const Lisp::RowCol& rc);
    void fillProperties(const char* atom);
    void fillAtomList();
    void syncSelectedAtom(const char* atom, const Lisp::RowCol& rc);
    QPair<Lisp::Reader::List*,int> findSymbolBySourcePos(const QString& file, quint32 line, quint16 col);
    QPair<Lisp::Reader::List*,int> findSymbolBySourcePos(Lisp::Reader::List*, quint32 line, quint16 col);

private:
    QTreeWidget* tree;
    QLabel* title;
    QLabel* d_xrefTitle;
    QTreeWidget* d_xref;
    QTreeWidget* properties;
    QLabel* propTitle;
    QPlainTextEdit* d_msgLog;
    QStringList sourceFiles;
    QListWidget* atomList;
    class Viewer;
    Viewer* viewer;
    QString root;
    QMap<QString,Lisp::Reader::Object> asts;
    QHash<const char*,QHash<QString,Lisp::Reader::Refs> > xref;
    Lisp::Reader::Atoms atoms;
    QList<Location> d_backHisto; // d_backHisto.last() ist aktuell angezeigtes Objekt
    QList<Location> d_forwardHisto;
    bool d_pushBackLock, d_lock3;
};

#endif // MAINWINDOW_H
