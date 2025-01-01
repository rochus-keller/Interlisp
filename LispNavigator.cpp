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

#include "LispNavigator.h"
#include "LispReader.h"
#include "LispLexer.h"
#include "LispBuiltins.h"
#include "LispHighlighter.h"
#include <GuiTools/CodeEditor.h>
#include <GuiTools/AutoMenu.h>
#include <GuiTools/AutoShortcut.h>
#include <QDir>
#include <QDockWidget>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtDebug>
#include <QApplication>
#include <QScrollBar>
#include <QtDebug>
#include <QSettings>
#include <QDesktopWidget>
#include <QShortcut>
#include <QInputDialog>
#include <QListWidget>
#include <QElapsedTimer>
#include <QFileDialog>

static Navigator* s_this = 0;
static void report(QtMsgType type, const QString& message )
{
    if( s_this )
    {
        switch(type)
        {
        case QtDebugMsg:
            s_this->logMessage(QString("INF: ") + message);
            break;
        case QtWarningMsg:
            s_this->logMessage(QString("WRN: ") + message);
            break;
        case QtCriticalMsg:
        case QtFatalMsg:
            s_this->logMessage(QString("ERR: ") + message);
            break;
        }
    }
}

static QtMessageHandler s_oldHandler = 0;
void messageHander(QtMsgType type, const QMessageLogContext& ctx, const QString& message)
{
    if( s_oldHandler )
        s_oldHandler(type, ctx, message );
    report(type,message);
}

class Navigator::Viewer : public CodeEditor
{
public:
    Viewer(Navigator* p):CodeEditor(p),d_ide(p),d_list(0)
    {
        setCharPerTab(3);
        setShowNumbers(true);
        setPaintIndents(false);
        setReadOnly(true);
        installDefaultPopup();
        d_hl = new Lisp::Highlighter( document() );
        for(int i = 0; Lisp::Builtins::functions[i]; i++)
        {
            const char* str = Lisp::Builtins::functions[i];
            d_hl->addFunction(Lisp::Token::getSymbol(QByteArray::fromRawData(str, strlen(str))).constData());
            str++;
        }
        for(int i = 0; Lisp::Builtins::variables[i]; i++)
        {
            const char* str = Lisp::Builtins::variables[i];
            d_hl->addVariable(Lisp::Token::getSymbol(QByteArray::fromRawData(str, strlen(str))).constData());
            str++;
        }

        updateTabWidth();
#if defined(Q_OS_WIN32)
        QFont monospace("Consolas");
#elif defined(Q_OS_MAC)
        QFont monospace("SF Mono");
#else
        QFont monospace("Monospace");
#endif
        if( !monospace.exactMatch() )
            monospace = QFont("DejaVu Sans Mono");
        setFont(monospace);
    }

    ~Viewer()
    {
    }

    Navigator* d_ide;
    Lisp::Highlighter* d_hl;
    Lisp::Reader::List* d_list;

    void clearBackHisto()
    {
        d_backHisto.clear();
    }

    void markNonTerms(const Lisp::Reader::Refs& syms)
    {
        d_nonTerms.clear();
        QTextCharFormat format;
        format.setBackground( QColor(237,235,243) );
        foreach( const Lisp::Reader::Ref& s, syms )
        {
            QTextCursor c( document()->findBlockByNumber( qMax(int(s.pos.row - 1),0)) );
            c.setPosition( c.position() + qMax(int(s.pos.col - 1), 0) );
            int pos = c.position();
            c.setPosition( pos + s.len, QTextCursor::KeepAnchor );

            QTextEdit::ExtraSelection sel;
            sel.format = format;
            sel.cursor = c;

            d_nonTerms << sel;
        }
        updateExtraSelections();
    }

    static inline void crosslineColors(QTextCharFormat& f, int level)
    {
        level++;
        QColor bgClr = QColor::fromRgb( 255, 254, 225 ); // Gelblich
        bgClr = QColor::fromHsv( bgClr.hue(), bgClr.saturation(), bgClr.value() - ( level*2 - 1 ) * 3 );
        f.setBackground(bgClr);
    }

    static inline QColor pastelColor() {
        return QColor(180 + rand() % 76, 180 + rand() % 76, 180 + rand() % 76);
    }

    void colorList( ESL& sum, Lisp::Reader::List* l, int level = 0 )
    {
        const Lisp::RowCol start = l->getStart();

        QTextCursor a( document()->findBlockByNumber( start.row - 1) );
        //a.setPosition( a.position() + start.col - 1 + 1 ); // without lpar
        a.setPosition( a.position() + start.col - 1 ); // with lpar
        QTextCursor b( document()->findBlockByNumber( l->end.row - 1) );
        b.setPosition( b.position() + l->end.col - 1 );
        const int posb = b.position();
        //a.setPosition( posb, QTextCursor::KeepAnchor ); // without rpar
        a.setPosition( posb + 1, QTextCursor::KeepAnchor ); // with rpar

        QTextEdit::ExtraSelection sel;
#if 1
        //sel.format.setBackground(QColor(Qt::red).lighter(195 - (level % 10) * 5));
        sel.format.setBackground(QColor(Qt::red).lighter(195 - level * 5));
#else
        if( level < 9 )
            sel.format.setBackground(QColor(Qt::red).lighter(195 - level * 5));
        else
            sel.format.setBackground(QColor(Qt::magenta).lighter(195 - (level-7) * 5));
#endif
        // crosslineColors(sel.format);
        sel.cursor = a;
        sum << sel;
        for( int i = 0; i < l->list.size(); i++ )
        {
            if( l->list[i].type() == Lisp::Reader::Object::List_ )
                colorList(sum, l->list[i].getList(), level + 1);
        }
    }

    void updateExtraSelections()
    {
        ESL sum;

        QTextEdit::ExtraSelection line;
        line.format.setBackground(QColor(Qt::yellow).lighter(170));
        line.format.setProperty(QTextFormat::FullWidthSelection, true);
        line.cursor = textCursor();
        line.cursor.clearSelection();
        sum << line;

        if( d_list && d_list->outer )
            colorList(sum, d_list);

        sum << d_nonTerms;


        sum << d_link;


        setExtraSelections(sum);
    }

    void mousePressEvent(QMouseEvent* e)
    {
        if( !d_link.isEmpty() )
        {
            QTextCursor cur = cursorForPosition(e->pos());
            d_ide->pushLocation( Navigator::Location( getPath(), cur.blockNumber(), cur.positionInBlock(), verticalScrollBar()->value() ) );
            QApplication::restoreOverrideCursor();
            d_link.clear();
        }
        if( QApplication::keyboardModifiers() == Qt::ControlModifier )
        {
            QTextCursor cur = cursorForPosition(e->pos());
            QPair<Lisp::Reader::List*,int> res = d_ide->findSymbolBySourcePos(
                        getPath(),cur.blockNumber() + 1,cur.positionInBlock() + 1);
            if( res.first && res.first->outer )
                d_list = res.first;
            else
                d_list = 0;
            if( res.first )
            {
                d_ide->pushLocation( Navigator::Location( getPath(), cur.blockNumber(), cur.positionInBlock(), verticalScrollBar()->value() ) );
                if( res.second >= 0 )
                    d_ide->showPosition( res.first->elementPositions[res.second] );
            }else
                d_list = 0;
            updateExtraSelections();
        }else
            QPlainTextEdit::mousePressEvent(e);
    }

    void mouseMoveEvent(QMouseEvent* e)
    {
        QPlainTextEdit::mouseMoveEvent(e);
        if( QApplication::keyboardModifiers() == Qt::ControlModifier )
        {
            QTextCursor cur = cursorForPosition(e->pos());
            QPair<Lisp::Reader::List*,int> res = d_ide->findSymbolBySourcePos(
                        getPath(),cur.blockNumber() + 1,cur.positionInBlock() + 1);
            const bool alreadyArrow = !d_link.isEmpty();
            d_link.clear();
            if( res.first && res.second >= 0 && res.first->list[res.second].type() == Lisp::Reader::Object::Atom)
            {
                Q_ASSERT( res.first );
                const int off = cur.positionInBlock() + 1 - res.first->elementPositions[res.second].col;
                cur.setPosition(cur.position() - off);
                cur.setPosition( cur.position() + res.first->list[res.second].getAtomLen(), QTextCursor::KeepAnchor );

                QTextEdit::ExtraSelection sel;
                sel.cursor = cur;
                sel.format.setFontUnderline(true);
                d_link << sel;
                if( !alreadyArrow )
                    QApplication::setOverrideCursor(Qt::ArrowCursor);
            }
            if( alreadyArrow && d_link.isEmpty() )
                QApplication::restoreOverrideCursor();
            updateExtraSelections();
        }else if( !d_link.isEmpty() )
        {
            QApplication::restoreOverrideCursor();
            d_link.clear();
            updateExtraSelections();
        }
    }

    void onUpdateModel()
    {
        markNonTerms(Lisp::Reader::Refs());
    }
};

Navigator::Navigator(QWidget *parent)
    : QMainWindow(parent), d_pushBackLock(false),d_lock3(false)
{
    QWidget* pane = new QWidget(this);
    QVBoxLayout* vbox = new QVBoxLayout(pane);
    vbox->setMargin(0);
    vbox->setSpacing(0);

    title = new QLabel(this);
    title->setMargin(2);
    title->setWordWrap(true);
    title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vbox->addWidget(title);

    viewer = new Viewer(this);
    connect(viewer,SIGNAL(cursorPositionChanged()),this,SLOT(onCursor()));
    connect(viewer,SIGNAL(sigUpdateLocation(int,int)),this,SLOT(onUpdateLocation(int,int)));

    vbox->addWidget(viewer);

    setCentralWidget(pane);

    setDockNestingEnabled(true);
    setCorner( Qt::BottomRightCorner, Qt::RightDockWidgetArea );
    setCorner( Qt::BottomLeftCorner, Qt::LeftDockWidgetArea );
    setCorner( Qt::TopRightCorner, Qt::RightDockWidgetArea );
    setCorner( Qt::TopLeftCorner, Qt::LeftDockWidgetArea );

    createSourceTree();
    createXref();
    createLog();
    createAtomList();

    s_this = this;
    s_oldHandler = qInstallMessageHandler(messageHander);

    QSettings s;

    const QRect screen = QApplication::desktop()->screenGeometry();
    resize( screen.width() - 20, screen.height() - 30 ); // so that restoreState works
    if( s.value("Fullscreen").toBool() )
        showFullScreen();
    else
        showMaximized();

    const QVariant state = s.value( "DockState" );
    if( !state.isNull() )
        restoreState( state.toByteArray() );

    new Gui::AutoShortcut( tr("ALT+Left"), this, this, SLOT(handleGoBack()) );
    new Gui::AutoShortcut( tr("ALT+Right"), this, this, SLOT(handleGoForward()) );
    new QShortcut(tr("CTRL+SHIFT+F"),this,SLOT(onSearchAtom()));
    new QShortcut(tr("CTRL+SHIFT+A"),this,SLOT(onSelectAtom()));
    new QShortcut(tr("CTRL+O"),this,SLOT(onOpen()) );

}

Navigator::~Navigator()
{

}

static inline QString debang( const QString& str )
{
    const int pos = str.lastIndexOf('!');
    if( pos == -1 )
        return str;
    else
        return str.left(pos);
}

static QByteArray decode(const QByteArray& source)
{
    QByteArray bytes;
    for( int i = 0; i < source.size(); i++ )
    {
        const char ch = source[i];
        if( ch == '\r' )
            bytes += '\n';
        else if( ch == '_' )
            bytes += "←";
        else if( ch == '^' )
            bytes += "↑";
        else if( isprint(ch) || isspace(ch) )
            bytes += ch;
        else if( !bytes.isEmpty() && bytes[bytes.size()-1] == '%' )
            bytes += ' ';
    }
    return bytes;
}

static QStringList collectFiles( const QDir& dir )
{
    QStringList res;
    QStringList files = dir.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );

    foreach( const QString& f, files )
        res += collectFiles( QDir( dir.absoluteFilePath(f) ) );

    const QStringList all = dir.entryList(QDir::Files, QDir::Name);
    QMap<QString,QString> check;
    bool hasCompiled = false;
    foreach( const QString& a, all )
    {
        QFileInfo info(a);
        const QString suff = debang(info.suffix().toLower());
        if( suff.isEmpty() || suff == "lisp" )
            check.insert(debang(info.baseName().toLower()), a);
        // not relibale; some source trees have lcom/dcom for a random subset of source files
        // if( suff == "lcom" || suff == "dfasl" || suff == "dcom" )
        //    hasCompiled = true;
    }

    foreach( const QString& a, all )
    {
        QFileInfo info(a);
        if( !hasCompiled )
        {
            if( info.suffix() == "dump" )
                continue;
            QFile f(dir.absoluteFilePath(a));
            if( f.open(QIODevice::ReadOnly) )
            {
                const QByteArray header = decode(f.read(20));
                if( header.startsWith("(FILECREATED") )
                    res.append(f.fileName());
                else
                    qDebug() << "no source file" << f.fileName();
            }
        }else
        {
            const QString suff = debang(info.suffix().toLower());
            if( suff == "lcom" || suff == "dfasl" || suff == "dcom" )
            {
                const QString name = check.value(debang(info.baseName().toLower()));
                if( !name.isEmpty() )
                    res.append(dir.absoluteFilePath(name));
                else
                    qDebug() << "no source found for" << dir.absoluteFilePath(a);
            }
        }
    }
    return res;
}

void Navigator::load(const QString& path)
{
    setWindowTitle(QString("%1 - Interlisp Navigator %2").arg(path).arg(QApplication::applicationVersion()));
    QDir::setCurrent(path);
    tree->clear();
    title->clear();
    viewer->clear();
    asts.clear();
    xref.clear();
    atomList->clear();
    root = path;
    sourceFiles = collectFiles(path);
    QMap<QString,QTreeWidgetItem*> dirs;
    QFileIconProvider fip;
    foreach( const QString& f, sourceFiles)
    {
        QFileInfo info(f);
        QString prefix = info.path().mid(path.size()+1);
        QTreeWidgetItem* super = 0;
        if( !prefix.isEmpty() )
        {
            super = dirs.value(prefix);
            if( super == 0 )
            {
                super = new QTreeWidgetItem(tree,1);
                super->setText(0,prefix);
                super->setIcon(0, fip.icon(QFileIconProvider::Folder));
                dirs[prefix] = super;
            }
        }
        QTreeWidgetItem* item;
        if( super )
            item = new QTreeWidgetItem(super);
        else
            item = new QTreeWidgetItem(tree);
        item->setText(0, debang(info.baseName()) );
        item->setIcon(0, fip.icon(QFileIconProvider::File));
        item->setData(0,Qt::UserRole, f);
        item->setToolTip(0,f);

#if 0
        Lisp::Lexer lex;
        lex.setStream(&in, f);
        Lisp::Token t = lex.nextToken();
        while(t.isValid())
        {
            qDebug() << t.getName() << t.pos.row << t.pos.col << t.val;
            t = lex.nextToken();
        }
        if( !t.isEof() )
            qCritical() << t.getName() << t.pos.row << t.pos.col << t.val;
#endif
    }
    QTimer::singleShot(500,this,SLOT(onRunParser()));
}

void Navigator::logMessage(const QString& str)
{
    d_msgLog->parentWidget()->show();
    d_msgLog->appendPlainText(str);
}

void Navigator::fileDoubleClicked(QTreeWidgetItem* item, int)
{
    if( item->type() )
        return;
    showFile(item->data(0, Qt::UserRole).toString());
}

void Navigator::handleGoBack()
{
    ENABLED_IF( d_backHisto.size() > 1 );

    d_pushBackLock = true;
    d_forwardHisto.push_back( d_backHisto.last() );
    d_backHisto.pop_back();
    showFile( d_backHisto.last() );
    d_pushBackLock = false;
}

void Navigator::handleGoForward()
{
    ENABLED_IF( !d_forwardHisto.isEmpty() );

    Location cur = d_forwardHisto.last();
    d_forwardHisto.pop_back();
    showFile( cur );
}

void Navigator::onXrefDblClicked()
{
    QTreeWidgetItem* item = d_xref->currentItem();
    if( item )
    {
        Lisp::Reader::Ref sym = item->data(0,Qt::UserRole).value<Lisp::Reader::Ref>(); // Ref
        QString path = item->data(1,Qt::UserRole).toString(); // path
        d_lock3 = true;
        showFile( path, sym.pos);
        d_lock3 = false;
    }
}

void Navigator::onCursor()
{
    fillXref();
}

void Navigator::onUpdateLocation(int line, int col)
{
    if( line == 0 || col == 0 )
        return;
    viewer->clearBackHisto();
    pushLocation(Location(viewer->getPath(), line,col,viewer->verticalScrollBar()->value()));
}

void Navigator::onSearchAtom()
{
    const QString pname = QInputDialog::getText(this, "Search Atom", "Enter an atom pname (case sensitive)");
    if( pname.isEmpty() )
        return;
    const char* atom = Lisp::Token::getSymbol(pname.toUtf8());
    fillXrefForAtom(atom, Lisp::RowCol());
}

void Navigator::onSelectAtom()
{
    const QString pname = QInputDialog::getItem(this, "Select Atom", "Select an atom from the list:",
                                                Lisp::Token::getAllSymbols() );
    if( pname.isEmpty() )
        return;
    const char* atom = Lisp::Token::getSymbol(pname.toUtf8());
    fillXrefForAtom(atom, Lisp::RowCol());
}

void Navigator::onAtomDblClicked(QListWidgetItem* item)
{
    const char* atom = Lisp::Token::getSymbol(item->text().toUtf8());
    fillXrefForAtom(atom, Lisp::RowCol());
}

void Navigator::onRunParser()
{
    QElapsedTimer t;
    t.start();
    QApplication::setOverrideCursor(Qt::WaitCursor);

    foreach( const QString& f, sourceFiles)
    {
        QFile in(f);
        QFileInfo info(f);
        if( !in.open(QFile::ReadOnly) )
        {
            qCritical() << "cannot open file for reading" << info.baseName();
            continue;
        }
        Lisp::Reader r;
        qDebug() << "*** parsing" << info.baseName();
        if( !r.read(&in, f) )
        {
            qCritical() << "ERROR " << info.baseName() << r.getPos().row << r.getError();
        }
        // else
        {
            Lisp::Reader::Object ast = r.getAst();
            asts.insert(f, ast);
            Lisp::Reader::Xref::const_iterator i;
            for( i = r.getXref().begin(); i != r.getXref().end(); ++i )
                xref[i.key()][f].append(i.value() );
#if 0
            QTextStream out;
            QFile file(f + ".dump");
            if( file.open(QFile::WriteOnly) )
            {
                out.setDevice(&file);
                ast.print(out);
            }
#endif
#if 0
            QFile file(f + ".lisp");
            if( file.open(QFile::WriteOnly) )
            {
                in.reset();
                QByteArray code = decode(in.readAll());
                file.write( code );
            }
#endif
        }
    }

    QApplication::restoreOverrideCursor();
    qDebug() << "parsed" << sourceFiles.size() << "files in" << t.elapsed() << "[ms]";

    atomList->addItems(Lisp::Token::getAllSymbols());
}

void Navigator::onOpen()
{
    QString path = QFileDialog::getExistingDirectory(this,tr("Open Project Directory"),QDir::currentPath() );
    if( path.isEmpty() )
        return;
    load(path);
}

void Navigator::pushLocation(const Navigator::Location& loc)
{
    if( d_pushBackLock )
        return;
    if( !d_backHisto.isEmpty() && d_backHisto.last() == loc )
        return; // o ist bereits oberstes Element auf dem Stack.
    d_backHisto.removeAll( loc );
    d_backHisto.push_back( loc );
}

void Navigator::showFile(const QString& file)
{
    QFile f(file);
    if( !f.open(QIODevice::ReadOnly) )
    {
        viewer->setPlainText(tr("; cannot open file %1").arg(f.fileName()));
        title->clear();
        return;
    }
    title->setText(debang(f.fileName().mid(root.size()+1)));
    const QString text = QString::fromUtf8(decode(f.readAll()));
    viewer->loadFromString(text, file);
}

void Navigator::showFile(const QString& file, const Lisp::RowCol& pos)
{
    showFile(file);
    if( !file.isEmpty() )
        showPosition(pos);
}

void Navigator::showPosition(const Lisp::RowCol& pos)
{
    if( pos.row > 0 && pos.col > 0 )
    {
        viewer->setCursorPosition( pos.row-1, pos.col-1, true );
    }
}

void Navigator::showFile(const Navigator::Location& loc)
{
    showFile(loc.d_file, Lisp::RowCol(loc.d_line+1, loc.d_col+1) );
    viewer->verticalScrollBar()->setValue(loc.d_yoff);
}

void Navigator::createSourceTree()
{
    QDockWidget* dock = new QDockWidget( tr("Source Tree"), this );
    dock->setObjectName("SourceTree");
    dock->setAllowedAreas( Qt::AllDockWidgetAreas );
    dock->setFeatures( QDockWidget::DockWidgetMovable );
    tree = new QTreeWidget(dock);
    tree->setAlternatingRowColors(true);
    tree->setHeaderHidden(true);
    tree->setSortingEnabled(false);
    tree->setAllColumnsShowFocus(true);
    tree->setRootIsDecorated(true);
    dock->setWidget(tree);
    addDockWidget( Qt::LeftDockWidgetArea, dock );
    connect( tree,SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),this, SLOT(fileDoubleClicked(QTreeWidgetItem*,int)));
}

void Navigator::createXref()
{
    QDockWidget* dock = new QDockWidget( tr("Xref"), this );
    dock->setObjectName("Xref");
    dock->setAllowedAreas( Qt::AllDockWidgetAreas );
    dock->setFeatures( QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable );
    QWidget* pane = new QWidget(dock);
    QVBoxLayout* vbox = new QVBoxLayout(pane);
    vbox->setMargin(0);
    vbox->setSpacing(0);
    d_xrefTitle = new QLabel(pane);
    d_xrefTitle->setMargin(2);
    d_xrefTitle->setWordWrap(true);
    vbox->addWidget(d_xrefTitle);
    d_xref = new QTreeWidget(pane);
    d_xref->setAlternatingRowColors(true);
    d_xref->setHeaderHidden(true);
    d_xref->setAllColumnsShowFocus(true);
    d_xref->setRootIsDecorated(false);
    vbox->addWidget(d_xref);
    dock->setWidget(pane);
    addDockWidget( Qt::RightDockWidgetArea, dock );
    connect(d_xref, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(onXrefDblClicked()) );
}

void Navigator::createLog()
{
    QDockWidget* dock = new QDockWidget( tr("Message Log"), this );
    dock->setObjectName("Log");
    dock->setAllowedAreas( Qt::AllDockWidgetAreas );
    dock->setFeatures( QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable );
    d_msgLog = new QPlainTextEdit(dock);
    d_msgLog->setReadOnly(true);
    d_msgLog->setLineWrapMode( QPlainTextEdit::NoWrap );
    dock->setWidget(d_msgLog);
    addDockWidget( Qt::BottomDockWidgetArea, dock );
    new QShortcut(tr("ESC"), dock, SLOT(close()) );
}

void Navigator::createAtomList()
{
    QDockWidget* dock = new QDockWidget( tr("Atoms"), this );
    dock->setObjectName("AtomList");
    dock->setAllowedAreas( Qt::AllDockWidgetAreas );
    dock->setFeatures( QDockWidget::DockWidgetMovable );
    atomList = new QListWidget(dock);
    atomList->setAlternatingRowColors(true);
    atomList->setSortingEnabled(false);
    dock->setWidget(atomList);
    addDockWidget( Qt::LeftDockWidgetArea, dock );
    connect( atomList,SIGNAL(itemDoubleClicked(QListWidgetItem*)),this,SLOT(onAtomDblClicked(QListWidgetItem*)));
}

void Navigator::closeEvent(QCloseEvent* event)
{
    QSettings s;
    s.setValue( "DockState", saveState() );
    event->setAccepted(true);
}

void Navigator::fillXref()
{
    int line, col;
    viewer->getCursorPosition( &line, &col );
    line += 1;
    col += 1;

    QPair<Lisp::Reader::List*,int> res = findSymbolBySourcePos(viewer->getPath(), line, col);
    if( res.first )
    {
        viewer->d_list = res.first;
        if( res.second >= 0 && res.first->list[res.second].type() == Lisp::Reader::Object::Atom )
        {
            const char* atom = res.first->list[res.second].getAtom();
            Lisp::RowCol rc;
            if( res.second < res.first->elementPositions.size() )
                rc = res.first->elementPositions[res.second];
            fillXrefForAtom(atom, rc);
            //TODO syncModView(hit->decl);

            QHash<QString,Lisp::Reader::Refs> usage = xref.value(atom);
            viewer->markNonTerms(usage.value(viewer->getPath()));
        }else
        {
            d_xrefTitle->clear();
            d_xref->clear();
            viewer->updateExtraSelections();
        }
    }else
    {
        viewer->d_list = 0;
        viewer->markNonTerms(Lisp::Reader::Refs());
    }
}

static bool sortExList( const Lisp::Reader::Ref& lhs, const Lisp::Reader::Ref& rhs )
{
    return lhs.pos.row < rhs.pos.row || (!(rhs.pos.row < lhs.pos.row) && lhs.pos.col < rhs.pos.col);
}

static inline QString roleToStr(quint8 r)
{
    switch(r)
    {
    case Lisp::Reader::Ref::Call:
        return "call";
    case Lisp::Reader::Ref::Decl:
        return "decl";
    default:
        return "";
    }
}

void Navigator::fillXrefForAtom(const char* atom, const Lisp::RowCol& rc)
{
    d_xref->clear();

    QHash<QString,Lisp::Reader::Refs> usage = xref.value(atom);

    QFont f = d_xref->font();
    f.setBold(true);

    d_xrefTitle->setText(tr("Atom: %1").arg(atom));
    const QString curMod = viewer->getPath();

    QTreeWidgetItem* black = 0;
    QHash<QString,Lisp::Reader::Refs>::const_iterator i;
    for( i = usage.begin(); i != usage.end(); ++i )
    {
        Lisp::Reader::Refs refs = i.value();
        std::sort( refs.begin(), refs.end(), sortExList );
        const QString modName = debang(QFileInfo(i.key()).baseName());
        foreach( const Lisp::Reader::Ref& s, refs )
        {
            QTreeWidgetItem* item = new QTreeWidgetItem(d_xref);
            item->setText( 0, QString("%1 (%2:%3 %4)")
                        .arg(modName)
                        .arg(s.pos.row).arg(s.pos.col)
                        .arg( roleToStr(s.role) ));
            if( curMod == i.key() && s.pos == rc )
            {
                item->setFont(0,f);
                black = item;
            }
            item->setToolTip( 0, item->text(0) );
            item->setData( 0, Qt::UserRole, QVariant::fromValue( s ) );
            item->setData( 1, Qt::UserRole, QVariant::fromValue( i.key()) );
            if( i.key() != curMod )
                item->setForeground( 0, Qt::gray );
        }
    }
    if( black && !d_lock3 )
    {
        d_xref->scrollToItem(black, QAbstractItemView::PositionAtCenter);
        d_xref->setCurrentItem(black);
    }
}

QPair<Lisp::Reader::List*, int> Navigator::findSymbolBySourcePos(const QString& file, quint32 line, quint16 col)
{
    Lisp::Reader::Object obj = asts.value(file);
    if( obj.type() != Lisp::Reader::Object::List_)
        return qMakePair((Lisp::Reader::List*)0,-1);
    Lisp::Reader::List* l = obj.getList();
    return findSymbolBySourcePos(l, line, col);
}

QPair<Lisp::Reader::List*, int> Navigator::findSymbolBySourcePos(Lisp::Reader::List* l, quint32 line, quint16 col)
{
    Q_ASSERT(l);
    for( int i = 0; i < l->list.size(); i++ )
    {
        Q_ASSERT( i < l->elementPositions.size() );
        Lisp::RowCol r = l->elementPositions[i];

#if 0
        const QByteArray string = l->list[i].toString();
        qDebug() << "element" << i << "at" << r.row << r.col << string.left(40);
#endif

        switch( l->list[i].type() )
        {
        case Lisp::Reader::Object::Float:
        case Lisp::Reader::Object::Integer:
        case Lisp::Reader::Object::String_:
            // we're not interested in these objects
            break;
        case Lisp::Reader::Object::Atom:
            if( line == r.row && col >= r.col && col <= r.col + l->list[i].getAtomLen(true) )
                return qMakePair(l,i);
            break;
        case Lisp::Reader::Object::List_: {
                const Lisp::RowCol end = l->list[i].getList()->end;
                if( (line > r.row && line < end.row) ||
                        ( line == r.row && r.row == end.row && col >= r.col && col <= end.col) ||
                        (line == r.row && r.row != end.row && col >= r.col) ||
                        (line == end.row && r.row != end.row && col <= end.col) )
                    return findSymbolBySourcePos(l->list[i].getList(), line, col);
                break;
            }
        default:
            break;
        }
    }
    return qMakePair(l, -1);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("me@rochus-keller.ch");
    a.setOrganizationDomain("github.com/rochus-keller/Interlisp");
    a.setApplicationName("InterlispNavigator");
    a.setApplicationVersion("0.3.1");
    a.setStyle("Fusion");

    QFontDatabase::addApplicationFont(":/fonts/DejaVuSansMono.ttf");
#ifdef Q_OS_LINUX
    QFontDatabase::addApplicationFont(":/fonts/NotoSans.ttf");
    QFont af("Noto Sans",9);
    a.setFont(af);
#endif

    Navigator w;
    w.showMaximized();
    if( a.arguments().size() > 1 )
    {
        w.load(a.arguments()[1]);
    }

    return a.exec();
}

