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

Highlighter::Highlighter(QTextDocument* parent) :
    QSyntaxHighlighter(parent)
{
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
    int lexerState = 0, braceDepth = 0;
    if (previousBlockState_ != -1) {
        lexerState = previousBlockState_ & 0xff;
        braceDepth = (previousBlockState_ >> 8) & 0xff;
    }


    int start = 0;
    if( lexerState == 1 )
    {
        // wir sind in einem Multi Line Comment
        // suche das Ende
        QTextCharFormat f = formatForCategory(C_Cmt);
        // f.setProperty( TokenProp, int(Tok_Comment) );
        int pos = text.indexOf(')');
        if( pos == -1 )
        {
            // the whole block ist part of the comment
            lexerState = 1; // lexerState can have higher value if nested
            setFormat( start, text.size(), f );
            setCurrentBlockState( (braceDepth << 8) | lexerState);
            return;
        }else
        {
            // End of Comment found
            pos++;
            setFormat( start, pos , f );
            lexerState = 0;
            braceDepth--;
            start = pos;
        }
    }else if( lexerState == 2 )
    {
        // wir sind in einem multi line string
        QTextCharFormat f = formatForCategory(C_Str);
        // f.setProperty( TokenProp, int(Tok_hexstring) );
        int pos = text.indexOf('"');
        if( pos == -1 )
        {
            // the whole block ist part of the hex string
            setFormat( start, text.size(), f );
            setCurrentBlockState( (braceDepth << 8) | lexerState);
            return;
        }else
        {
            // End of hex string found
            pos++;
            setFormat( start, pos , f );
            lexerState = 0;
            braceDepth--;
            start = pos;
        }
    }

    Lexer lex;
    lex.setEmitComments(true);
    lex.setPacked(false);

    //QTextCharFormat bg;
    //setBackground(bg, braceDepth);
    //setFormat(0, text.size(), bg);

    QList<Token> tokens = lex.tokens(text.mid(start));
    for( int i = 0; i < tokens.size(); ++i )
    {
        Token &t = tokens[i];
        t.pos.col += start;

        QTextCharFormat f;
        if( t.type == Tok_comment )
        {
            f = formatForCategory(C_Cmt);
            if( !t.val.endsWith(')') )
            {
                braceDepth++;
                lexerState = 1; // multiline comment, RISK
            }else
                lexerState = 0;
        }else if( t.type == Tok_string )
        {
            f = formatForCategory(C_Str);
            if( t.val.size() <= 1 || !t.val.endsWith('"') )
            {
                braceDepth++;
                lexerState = 2; // multiline string
            }else
                lexerState = 0;
        }else if( t.type == Tok_float || t.type == Tok_integer )
            f = formatForCategory(C_Num);
        else if(t.type == Tok_lpar || t.type == Tok_lbrack )
        {
            if( t.type == Tok_lpar )
                braceDepth++;
            //setBackground(bg, braceDepth);
            //setFormat(t.pos.col-1, text.size() - (t.pos.col-1), bg);
            f = formatForCategory(C_Op2);
        }else if( t.type == Tok_rpar || t.type == Tok_rbrack )
        {
            if( t.type == Tok_rpar )
                braceDepth = qMax(0, braceDepth-1);
            if( t.type == Tok_rbrack )
                braceDepth = 1; // TODO
            //setBackground(bg, braceDepth);
            //setFormat(t.pos.col-1, text.size() - (t.pos.col-1), bg);
            f = formatForCategory(C_Op2);
        }else if( t.type == Tok_atom )
        {
            if( d_syntax.contains(t.val.constData()) )
                f = formatForCategory(C_Op1);
            else if( d_functions.contains(t.val.constData()) )
                f = formatForCategory(C_Func);
            else if( d_variables.contains(t.val.constData() ) )
                f = formatForCategory(C_Var);
            //else if( punctuation(text, t.pos.col-1, t.len ) )
            //    f = formatForCategory(C_Op3);
            else
                f = formatForCategory(C_Ident);
        }

        //setBackground(f, braceDepth);
#if 0
        if( lexerState == 3 )
            setFormat( startPp, t.pos.col - startPp + t.len, formatForCategory(C_Pp) );
        else
#endif
        if( f.isValid() )
            setFormat( t.pos.col-1, t.len, f );
    }

    setCurrentBlockState((braceDepth << 8) | lexerState );
}



