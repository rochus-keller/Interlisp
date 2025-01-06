/*
* Copyright 2024 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the Interlisp project.
*
* The following is the license that applies to this copy of the
* library. For a license to use the library under conditions
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

// Adopted from the Luon project

#include "LispHighlighter.h"
#include "LispLexer.h"
#include <QBuffer>
#include <QTextDocument>
#include <QtDebug>
using namespace Lisp;

static const char* QUOTE;


Highlighter::Highlighter(QTextDocument* parent) :
    QSyntaxHighlighter(parent)
{
    QUOTE = Token::getSymbol("QUOTE").constData();

    const int pointSize = parent->defaultFont().pointSize();
    for( int i = 0; i < C_Max; i++ )
    {
        d_format[i].setFontWeight(QFont::Normal);
        d_format[i].setForeground(Qt::black);
        d_format[i].setBackground(Qt::transparent);
    }
    d_format[C_Num].setForeground(QColor(0, 153, 153));
    d_format[C_Str].setForeground(QColor(208, 16, 64));
    d_format[C_Cmt].setForeground(QColor(153, 153, 136));
    d_format[C_Func].setForeground(QColor(68, 85, 136));
    d_format[C_Func].setFontWeight(QFont::Bold);
    d_format[C_Op1].setForeground(QColor(153, 0, 0));
    d_format[C_Op1].setFontWeight(QFont::Bold);
    d_format[C_Op2].setForeground(QColor(153, 0, 0));
    d_format[C_Op2].setFontWeight(QFont::Bold);
    d_format[C_Op2].setFontPointSize(pointSize * 1.2);
    //d_format[C_Op3].setForeground(QColor(153, 0, 0).lighter(125)); // QColor(0, 134, 179));
    d_format[C_Op3].setFontWeight(QFont::Bold);
    d_format[C_Var].setForeground(QColor(153, 0, 115));
    d_format[C_Var].setFontWeight(QFont::Bold);
    d_format[C_Pp].setFontWeight(QFont::Bold);
    d_format[C_Pp].setForeground(QColor(0, 128, 0));
    d_format[C_Pp].setBackground(QColor(230, 255, 230));

    //d_builtins = createBuiltins();
    d_syntax.insert(Token::getSymbol("NIL").constData());
    d_syntax.insert(Token::getSymbol("T").constData());
    d_syntax.insert(Token::getSymbol("LAMBDA").constData());
    d_syntax.insert(Token::getSymbol("NLAMBDA").constData());
}

void Highlighter::addFunction(const char* bi)
{
    d_functions << bi;
}

void Highlighter::addVariable(const char* bi)
{
    d_variables << bi;
}

QTextCharFormat Highlighter::formatForCategory(int c) const
{
    return d_format[c];
}

void Highlighter::clearFromHere(quint32 line)
{
    LineState::iterator i = lineState.upperBound(line);
    while( i != lineState.end() )
        i = lineState.erase(i);
}

#if 0
QSet<QByteArray> Highlighter::createBuiltins(bool withLowercase)
{
    QSet<QByteArray> res = CodeModel::getBuitinIdents().toSet();
    if( withLowercase )
    {
        QSet<QByteArray> tmp = res;
        QSet<QByteArray>::const_iterator i;
        for( i = tmp.begin(); i != tmp.end(); ++i )
            res.insert( (*i).toLower() );
    }
    return res;
}
#endif

static inline void setBackground(QTextCharFormat& f, int level)
{
    if( level == 0 )
    {
        f.setBackground(Qt::white);
        return;
    }
    QColor bgClr = QColor::fromRgb( 255, 254, 225 ); // Gelblich
    bgClr = QColor::fromHsv( bgClr.hue(), bgClr.saturation(), bgClr.value() - ( level*2 - 1 ) * 3 );
    f.setBackground(bgClr);
}

static bool punctuation(const QString& str, int pos, int len)
{
    for( int i = pos; i < qMin(pos + len, str.size()); i++ )
    {
        if( str[i].isLetterOrNumber() )
            return false;
    }
    return true;
}

void Highlighter::highlightBlock(const QString& text)
{
    const int previousBlockState_ = previousBlockState();
    quint8 lexerState = 0,
            braceDepth = 0,  // nesting level of [] or ()
            commentLevel = 0; // 0 or braceDepth where comment started
    if (previousBlockState_ != -1) {
        lexerState = previousBlockState_ & 0xff;
        braceDepth = (previousBlockState_ >> 8) & 0xff;
        commentLevel = (previousBlockState_ >> 16) & 0xff;
    }

    const quint32 line = currentBlock().blockNumber() + 1;
    clearFromHere(line);

    // we can be in a (, [, (*, " or QUOTE
    // [ requires memorizing the previous open occurences
    // ( and [ are the same category, just with different termination rules
    // QUOTE can start with ( or [
    // A comment can include all others and behave compatible, even if the whole thing is not evaluated
    // But a comment can terminate with ] which affects previous [ and (
    // The [ ( (* QUOTE state is separate from the " state

    bool inString = lexerState & 1;
    bool inQuote = lexerState & 2;

    Lexer lex;
    lex.setEmitComments(true);
    lex.setPacked(false);
    lex.setStream(text.toLatin1(), "");

    Token t;
    if( inString )
    {
        t = lex.readString();
        setFormat( 0, t.len, formatForCategory(C_Str) );
        if( t.val.endsWith('"') && ((t.pos.col + t.len) < 2 || text[t.pos.col + t.len - 2] != '%') ) // ": -1, %: -2
            inString = false;
        else
        {
            // the whole line is in the string
            // lexer state remains the same
            setCurrentBlockState(previousBlockState_);
            return;
        }
    }

    t = lex.nextToken();
    while( t.isValid() )
    {
        QTextCharFormat f;
        switch(t.type)
        {
        case Tok_lpar:
            braceDepth++;
            if( commentLevel )
                f = formatForCategory(C_Cmt);
            else
                f = formatForCategory(C_Op2);
            break;
        case Tok_rpar:
            if( commentLevel )
            {
                if( braceDepth == commentLevel )
                    commentLevel = 0;
                f = formatForCategory(C_Cmt);
            }else
                f = formatForCategory(C_Op2);
            braceDepth--;
            break;
        case Tok_lbrack:
            lineState[line] = braceDepth;
            braceDepth++;
            if( commentLevel )
                f = formatForCategory(C_Cmt);
            else
                f = formatForCategory(C_Op2);
            break;
        case Tok_rbrack:
            if( commentLevel )
                f = formatForCategory(C_Cmt);
            else
                f = formatForCategory(C_Op2);
            if( !lineState.isEmpty() )
            {
                const quint32 lastLine = lineState.lastKey();
                braceDepth = lineState.value(lastLine);
                lineState.remove(lastLine);
                if( braceDepth <= commentLevel )
                    commentLevel = 0;
            }
            break;
        case Tok_atom:
            if( commentLevel )
                f = formatForCategory(C_Cmt);
            else if( d_syntax.contains(t.val.constData()) )
                f = formatForCategory(C_Op1);
            else if( d_functions.contains(t.val.constData()) )
                f = formatForCategory(C_Func);
            else if( d_variables.contains(t.val.constData() ) )
                f = formatForCategory(C_Var);
            //else if( punctuation(text, t.pos.col-1, t.len ) )
            //    f = formatForCategory(C_Op3);
            else
                f = formatForCategory(C_Ident);
            if( !inString && commentLevel == 0 && t.val.constData() == QUOTE )
                inQuote = true;
            break;
        case Tok_float:
        case Tok_integer:
            if( commentLevel )
                f = formatForCategory(C_Cmt);
            else
                f = formatForCategory(C_Num);
            break;
        case Tok_Lattr:
            braceDepth++;
            if( commentLevel == 0 )
                commentLevel = braceDepth;
            f = formatForCategory(C_Cmt);
            break;
        case Tok_DblQuote:
            Q_ASSERT( !inString );
            inString = true;
            const Token t2 = lex.readString();
            t.len += t2.len;
            if( t2.val.endsWith('"') )
                inString = false; // string ended on the same line
            // else look for string end on next line
            if( commentLevel == 0 )
                f = formatForCategory(C_Str);
            else
                f = formatForCategory(C_Cmt);
            break;
        }
        if( f.isValid() )
            setFormat( t.pos.col-1, t.len, f );
        t = lex.nextToken();
    }

    lexerState = 0;
    if( inString )
        lexerState |= 1;
    if( inQuote )
        lexerState |= 2;
    setCurrentBlockState((commentLevel << 16) | (braceDepth << 8) | lexerState );
}



