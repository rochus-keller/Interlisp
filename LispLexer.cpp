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

#include "LispLexer.h"
#include <QBuffer>
#include <QFile>
#include <QHash>
#include <QFileInfo>
#include <QtDebug>
using namespace Lisp;

static QHash<QByteArray,QByteArray> d_symbols;

bool Token::isValid() const
{
    return type != Tok_Eof && type != Tok_Invalid;
}

bool Token::isEof() const
{
    return type == Tok_Eof;
}

const char*Token::getName() const
{
    switch(type)
    {
    case Tok_Invalid:
        return "Tok_Invalid";
    case Tok_Eof:
        return "Tok_Eof";
    case Tok_lpar:
        return "Tok_lpar";
    case Tok_rpar:
        return "Tok_rpar";
    case Tok_lbrack:
        return "Tok_lbrack";
    case Tok_rbrack:
        return "Tok_rbrack";
    case Tok_string:
        return "Tok_string";
    case Tok_atom:
        return "Tok_atom";
    case Tok_float:
        return "Tok_float";
    case Tok_integer:
        return "Tok_integer";
    }

    return "???";
}

const char*Token::getString() const
{
    return ""; // TODO tokenTypeString(d_type);
}

QByteArray Token::getSymbol(const QByteArray& str)
{
    if( str.isEmpty() )
        return str;
    QByteArray& sym = d_symbols[str];
    if( sym.isEmpty() )
        sym = str;
    return sym;
}

char Lexer::readc()
{
    char res;
    if( in && !in->atEnd() && in->getChar(&res) )
    {
        if( res == '\r' )
            res = '\n';
        else if( res == 0 || (!isprint(res) && !isspace(res)) )
            return readc(); // ignore all clutter
        if( res == '\n' )
        {
            pos.row++;
            pos.col = 1;
        }else
            pos.col++;
        if( res == 0 )
            res = -1;
        Q_ASSERT(isspace(res) || isprint(res));
        return res;
    }else
    {
        if( in && in->atEnd() && in->parent() == this )
        {
            in->deleteLater();
            in = 0;
        }
        return 0;
    }
}

void Lexer::ungetc(char c)
{
    if( c == 0 )
        return;
    Q_ASSERT(isspace(c) || isprint(c));

    if( in && c && !isspace(c) ) // don't unget whitespace
    {
        pos.col--;
        in->ungetChar(c);
    }
}

void Lexer::ungetstr(const QByteArray& str)
{
    for( int i = str.size() - 1; i >= 0; i-- )
        ungetc(str[i]);
}

Lexer::Lexer(QObject* parent):QObject(parent),in(0),emitComments(false),packed(true)
{

}

void Lexer::setStream(QIODevice* in, const QString& sourcePath)
{
    if( in == 0 )
        setStream( sourcePath );
    else
    {
        this->in = in;
        pos.row = 1;
        pos.col = 1;
        this->sourcePath = sourcePath;
    }
}

bool Lexer::setStream(const QString& sourcePath)
{
    QIODevice* in = 0;

    QFile* file = new QFile(sourcePath, this);
    if( !file->open(QIODevice::ReadOnly) )
    {
        delete file;
        return false;
    }
    in = file;

     // else
    setStream( in, sourcePath );
    return true;
}

Token Lexer::nextToken()
{
    if( !buffer.isEmpty() )
        return buffer.takeLast();
    else if( emitComments )
        return nextTokenImp();
    // else
    Token res = nextTokenImp();
    while( res.isValid() && res.type == Tok_comment )
        res = nextTokenImp();
    return res;
}

void Lexer::unget(const Token& t)
{
    buffer.push_front(t);
}

Token Lexer::nextTokenImp()
{
    skipWhiteSpace();
    start = pos;
    const char c = readc();
    if( c == 0 )
        return token(Tok_Eof);
    else if( isdigit(c) )
    {
        // this is a number
        ungetc(c);
        return number();
    }else if(  c == '+' || c == '-' || c == '.' )
    {
        const char c2 = readc();
        ungetc(c2);
        ungetc(c);
        if( isdigit(c2) )
            return number();
        else
            return atom();
    }else if( c == '"' )
    {
        // this is a string
        ungetc(c);
        return string();
    }else if( c == '(' || c == '[' || c == ')' || c == ']' )
    {
        const char c2 = readc();
        ungetc(c2);
        if( c == '(' && c2 == '*' )
        {
            ungetc(c);
            return comment();
        }
        switch( c )
        {
        case '(':
            return token(Tok_lpar,1);
        case '[':
            return token(Tok_lbrack,1);
        case ')':
            return token(Tok_rpar,1);
        case ']':
            return token(Tok_rbrack,1);
        }
    }else
    {
        // this is an atom
        ungetc(c);
        return atom();
    }
    return Token();
}

QList<Token> Lexer::tokens(const QString& code)
{
    return tokens( code.toLatin1() );
}

QList<Token> Lexer::tokens(const QByteArray& code, const QString& path)
{
    QBuffer in;
    in.setData( code );
    in.open(QIODevice::ReadOnly);
    setStream( &in, path );

    QList<Token> res;
    Token t = nextToken();
    while( t.isValid() )
    {
        res << t;
        t = nextToken();
    }
    return res;
}

Token Lexer::token(TokenType tt, int len, const QByteArray& val)
{
    Token t( tt, start, len, val );
    t.sourcePath = sourcePath;
    return t;
}

static inline int digit(char c)
{
    if( c >= '0' && c <= '9' )
        return c - '0';
    else
        return -1;
}

bool Lexer::atom_delimiter(char c)
{
    return isspace(c) ||
           c == '(' || c == ')' || c == '[' || c == ']' || c == '"';
}

Token Lexer::number()
{
    enum Status { idle, dec_seq, dec_or_oct_seq, fraction, exponent, exponent2 } status = idle;
    char c = readc();
    QByteArray number;
    number += c;
    if( c == '+' || c == '-' )
    {
        status = dec_or_oct_seq;
        c = readc();
        number += c;
    }else if( c == '.' )
    {
        status = fraction;
        c = readc();
        number += c;
    }

    while(true)
    {
        const int d = digit(c);
        switch(status)
        {
        case idle:
            if( d >= 0 && d <= 7 )
                status = dec_or_oct_seq;
            else if( d == 8 || d == 9 )
                status = dec_seq;
            else
                Q_ASSERT(false);
            break;
        case dec_or_oct_seq:
        case dec_seq:
            if( c == 'Q' )
            {
                if( status == dec_seq )
                    return token(Tok_Invalid, number.size(), "invalid decimal number");
                else
                {
                    // octal number found
                    bool ok;
                    number.left(number.size()-1).toLongLong(&ok, 8);
                    if( !ok )
                        return token(Tok_Invalid, number.size(), "invalid octal number");
                    else
                        return token(Tok_integer, number.size(), number);
                }
            }else if( c == '.' )
                status = fraction;
            else if( c == 'E' )
                status = exponent;
            else if( atom_delimiter(c) )
            {
                // not a digit and not an atom, break here
                number.chop(1);
                ungetc(c);
                return token(Tok_integer, number.size(), number);
            }else if( !isdigit(c) )
            {
                ungetstr(number);
                return atom();
            }
            // else: it's a digit
            break;
        case fraction:
            if( c == 'E' )
                status = exponent;
            else if( atom_delimiter(c) )
            {
                // not a digit and not and atom, break here
                number.chop(1);
                ungetc(c);
                bool ok;
                number.toDouble(&ok);
                if( !ok )
                    return token(Tok_Invalid, number.size(), "invalid float");
                else
                    return token(Tok_float, number.size(), number);
            }else if( !isdigit(c) )
            {
                ungetstr(number);
                return atom();
            }
            // else: it's a digit
            break;
        case exponent:
            if( c == '+' || c == '-' || d >= 0 )
                status = exponent2;
            else
                return token(Tok_Invalid, number.size(), "invalid exponent");
            break;
        case exponent2:
            if( atom_delimiter(c) )
            {
                // not a digit and not an atom, break here
                number.chop(1);
                ungetc(c);
                bool ok;
                number.toDouble(&ok);
                if( !ok )
                    return token(Tok_Invalid, number.size(), "invalid float");
                else
                    return token(Tok_float, number.size(), number);
            }else if( !isdigit(c) )
            {
                ungetstr(number);
                return atom();
            }
            // else: it's a digit
            break;
        default:
            Q_ASSERT(false);
        }
        c = readc();
        number += c;
    }
    Q_ASSERT(false);
    return Token();
}

Token Lexer::atom()
{
    QByteArray a;
    int extra = 0;
    while( true )
    {
        char c = readc();
        if( c == '%' )
        {
            extra++;
            c = readc(); // escape
        }else if( c == 0 || atom_delimiter(c) || !isprint(c) )
        {
            // atom can be terminated by 0x06 0x01, thus isprint(c)
            ungetc(c);
            break;
        }
        a += c;
    }
    a = Token::getSymbol(a);
    return token(Tok_atom, a.size() + extra, a);
}

Token Lexer::string()
{
    char c = readc();
    QByteArray str;
    str += c;
    int extra = 0; // count left and right quote
    while( true )
    {
        c = readc();
        if( c == '%' )
        {
            extra++;
            c = readc(); // escape
        }else if( c == '"' )
        {
            str += c;
            break;
        }else if( c == 0 )
            break;
        str += c;
    }
    if( packed && c != '"' )
        return token(Tok_Invalid, str.size() + extra, "unterminated string" );
    return token(Tok_string, str.size() + extra, str);
}

Token Lexer::comment()
{
    QByteArray str;
    char c = readc();
    str += c;
    c = readc();
    str += c;
    int level = 0;
    bool inString = false;
    QList<int> brackets; // bracket at level
    int extra = 0;
    while( true )
    {
        c = readc();
        if( c == 0 )
            break;
        if( c == '%' )
        {
            extra++;
            c = readc();
        }else if( c == '"' )
        {
            inString = !inString;
        }else if( !inString && c == '(' )
            level++;
        else if( !inString && c == '[' )
        {
            brackets << level;
            level++;
        }else if( !inString && c == ']' )
        {
            if( brackets.isEmpty() )
            {
                // the string is terminated by ], and we pass it back so that it
                // can continue to terminate until [
                ungetc(c);
                c = ')';
                str += c;
                break;
            }
            level = brackets.takeLast();
        }else if( !inString && c == ')' )
        {
            if( level == 0 )
            {
                if( !brackets.isEmpty() )
                    return token(Tok_Invalid, str.size() + extra, "unterminated bracket in comment");
                str += c;
                break;
            }
            level--;
        }
        str += c;
    }
    if( packed && c != ')')
        return token(Tok_Invalid, str.size() + extra, "unterminated comment" );
    return token(Tok_comment, str.size() + extra, str);
}

void Lexer::skipWhiteSpace()
{
    char c = readc();
    if( c == 0 )
        return;

    while( true )
    {
        while( isspace(c) || !isprint(c) )
           // seen in Fugue-2: c == 0x06 || c == 0x1e || c == 12 || c == 1 || c == 27 || c == 127 || c == -32 )
        {
            if( c == 0x06 )
                readc(); // skip next
#if 0
            else if( !isspace(c) )
                qDebug() << "*** special char in" << QFileInfo(sourcePath).baseName() <<
                            pos.row << pos.col << ": " << quint8(c); // TEST
#endif
            c = readc();
            if( c == 0 )
                return;
        }
#if 0
        if( c != ';' )
            break;
        while( c != '\n' && c != '\r' && c != 0 )
            c = readc();
#else
        break;
#endif
    }
    ungetc(c);
}
