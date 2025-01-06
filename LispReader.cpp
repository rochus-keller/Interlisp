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

#include "LispReader.h"
#include "LispLexer.h"
#include <QIODevice>
#include <QHash>
#include <QtDebug>
#include <QFile>
using namespace Lisp;

static QHash<QByteArray,QByteArray> symbols;

static const quint64 signbit_mask = 1LL << (64-1);
static const quint64 quiet_nan_mask = 0xfffLL << 51;
static const quint64 pointer_type_mask = 7LL << 48;
static const quint64 pointer_mask = (1LL << 48)-1;
static const quint64 int_mask = (1LL << 50) - 1;
static const quint64 int_sign = 1LL << 50;
static const quint64 Nil_mask = 1LL << 48;
static const quint64 String_mask = 2LL << 48;
static const quint64 List_mask = 3LL << 48;
static const quint64 Atom_mask = 4LL << 48;
static const char* STOP;
static const char* NIL;
static const char* DEFINEQ;
static const char* QUOTE;
static const char* PUTPROPS;
static const char* PUTPROP;
static const char* SET;
static const char* SETQ;
static const char* SETQQ;
static const char* RPAQ;
static const char* RPAQQ;


Reader::Reader()
{

}

bool Reader::read(QIODevice* in, const QString& path)
{
    List* l = new List();
    ast.set(l);
    xref.clear();
    atoms.clear();
    STOP = Token::getSymbol("STOP").constData();
    NIL = Token::getSymbol("NIL").constData();
    DEFINEQ = Token::getSymbol("DEFINEQ").constData();
    QUOTE = Token::getSymbol("QUOTE").constData();
    PUTPROPS = Token::getSymbol("PUTPROPS").constData();
    PUTPROP = Token::getSymbol("PUTPROP").constData();
    SET = Token::getSymbol("SET").constData();
    SETQ = Token::getSymbol("SETQ").constData();
    SETQQ = Token::getSymbol("SETQQ").constData();
    RPAQ = Token::getSymbol("RPAQ").constData();
    RPAQQ = Token::getSymbol("RPAQQ").constData();

    Lexer lex;
    lex.setStream(in, path);

    while( true )
    {
        const Token t = lex.nextToken();
        lex.unget(t);
        Object res = next(lex, l, false);
        if( !error.isEmpty() )
            return false;
        if( res.type() != Object::Nil_ )
        {
            if( res.type() == Object::Atom_ )
            {
                const char* atom = res.getAtom();
                if( atom == NIL || atom == STOP )
                    break;
                xref[atom] << Ref(t.pos, t.len);
            }
            l->list.append(res);
            l->elementPositions.append(t.pos);

        }else
            break;
    }
    return error.isEmpty();
}

Reader::Object Reader::next(Lexer& in, List* outer, bool inQuote)
{
    Object res;
    if( inQuote )
        in.startQuote();
    Token t = in.nextToken();
    if( inQuote )
        in.endQuote();
    if( t.isEof() )
        return Object();
    pos = t.pos;
    if( !t.isValid() )
    {
        report(t);
        return Object();
    }
    switch( t.type )
    {
    case Tok_integer:
        if( t.val.endsWith('Q') )
            res = Object(t.val.left(t.val.size()-1).toLongLong());
        else
            res = Object(t.val.toLongLong());
        break;
    case Tok_float:
        res = Object(t.val.toDouble());
        break;
    case Tok_string:
        res = Object(new String(t.val));
        break;
    case Tok_atom:
        res = Object(t.val.constData());
        break;
    case Tok_lpar:
    case Tok_lbrack:
        res = list(in, t.type == Tok_lbrack, outer);
        break;
    case Tok_rpar:
        report(t, QString("unexpected token ')'"));
        return Object();
    case Tok_rbrack:
        report(t, QString("unexpected token ']'"));
        return Object();
    }

    // res.dump(out, line);
    return res;
}

Reader::Object Reader::list(Lexer& in, bool brack, List* outer)
{
    List* l = new List();
    l->outer = outer;
    Object res(l);
    bool quoteList = false;

    while( true )
    {
        Token t = in.nextToken();
        if( !t.isValid() )
        {
            report(t);
            res = Object();
            break;
        }
        if( t.type == Tok_rpar )
        {
            l->end = t.pos;
            if( brack )
            {
                report(t,"terminating '[' by ')'");
                res = Object();
            }
            break;
        }
        if( t.type == Tok_rbrack )
        {
            if( !brack )
                in.unget(t); // shortcut to close all '(' lists up to '['
            l->end = t.pos;
            break;
        }
        in.unget(t);
        Object res = next(in, l, quoteList);
        if( !error.isEmpty() )
        {
            res = Object();
            break;
        }
        l->list.append(res);
        l->elementPositions.append(t.pos);
        if( res.type() == Object::Atom_ )
        {
            if( l->list.size() == 1 && res.getAtom() == QUOTE )
                quoteList = true;

            Ref::Role r = Ref::Use;
            if( l->list.size() == 1 )
            {
                r = Ref::Call;
                Object o = l->getOuterFirst();
                if( o.type() == Object::Atom_ && o.getAtom() == DEFINEQ )
                    r = Ref::Decl;
            }else if( l->list.size() == 2 &&
                      (l->list.first().getAtom() == PUTPROP ||
                       l->list.first().getAtom() == PUTPROPS ||
                       l->list.first().getAtom() == SET || l->list.first().getAtom() == SETQ ||
                       l->list.first().getAtom() == SETQQ || l->list.first().getAtom() == RPAQ ||
                       l->list.first().getAtom() == RPAQQ ) )
            {
                r = Ref::Lhs;
            }
            xref[res.getAtom()] << Ref(t.pos, t.len, r);
        }
        if( l->list.size() == 4 && (l->list[0].getAtom() == PUTPROPS || l->list[0].getAtom() == PUTPROP) )
        {
            //qDebug() << "Property of atom" << l->list[1].getAtom() << ":" << l->list[2].toString() << "=" << res.toString();
            atoms[l->list[1].getAtom()].props[l->list[2].getAtom()] = res;
        }else if( l->list.size() >= 6 && l->list.size() % 2 == 0 && l->list[0].getAtom() == PUTPROPS )
        {
            atoms[l->list[1].getAtom()].props[l->list[l->list.size()-2].getAtom()] = res;
        }

    }
    return res;
}

void Reader::report(const Token& t)
{
    report(t, t.val);
}

void Reader::report(const Token& t, const QString& msg)
{
    pos = t.pos;
    error = msg;
}

Reader::Object::Object():bits(0)
{
    nil();
}

Reader::Object::Object(double d):bits(0)
{
    set(d);
}

Reader::Object::Object(qint64 i):bits(0)
{
    set(i);
}

Reader::Object::Object(const char* s):bits(0)
{
    set(s);
}

Reader::Object::Object(List* l):bits(0)
{
    set(l);
}

Reader::Object::Object(Reader::String*s):bits(0)
{
    set(s);
}

Reader::Object::Object(const Reader::Object& rhs):bits(0)
{
    *this = rhs;
}

Reader::Object::~Object()
{
    nil();
}

Reader::Object&Reader::Object::operator=(const Reader::Object& rhs)
{
    nil();
    bits = rhs.bits;
    switch( type() )
    {
    case List_:
        getList()->addRef();
        break;
    case String_:
        getStr()->addRef();
        break;
    }
    return *this;
}

void Reader::Object::set(qint64 i)
{
    nil();
    bits = 0;
    const quint64 u = qAbs(i);
    Q_ASSERT(u <= int_mask);
    bits |= quiet_nan_mask | u;
    if( i < 0 )
        bits |= int_sign;
}

qint64 Reader::Object::getInt() const
{
    Q_ASSERT( type() == Integer);
    qint64 res = bits & int_mask;
    if( bits & int_sign )
        res = -res;
    return res;
}

void Reader::Object::set(const char* s)
{
    nil();
    bits = 0;
    union { const char* ss; quint64 u; };
    u = 0;
    ss = s; // otherwise the upper 32 bits are not initialized
    Q_ASSERT( u <= pointer_mask );
    bits |= signbit_mask | quiet_nan_mask | Atom_mask | u;
}

void Reader::Object::set(Reader::String* s)
{
    nil();
    if( s == 0 )
        return;
    s->addRef();
    bits = 0;
    union { String* ss; quint64 u; };
    u = 0;
    ss = s;
    Q_ASSERT( u <= pointer_mask );
    bits |= signbit_mask | quiet_nan_mask | String_mask | u;
}

Reader::String* Reader::Object::getStr() const
{
    Q_ASSERT( type() == String_);
    return (String*)(bits & pointer_mask);
}

const char*Reader::Object::getAtom() const
{
    if( type() != Atom_)
        return 0;
    const char* res = (const char*)(bits & pointer_mask);
    return res;
}

int Reader::Object::getAtomLen(bool inCode) const
{
    const char* atom = getAtom();
    int res = 0;
    while( *atom )
    {
        res++;
        if( inCode && Lexer::atom_delimiter(*atom) )
            res++; // escaped in source code
        atom++;
    }
    return res;
}

void Reader::Object::set(Reader::List* l)
{
    nil();
    if( l == 0 )
        return;
    l->addRef();
    bits = 0;
    union { List* ss; quint64 u; };
    u = 0;
    ss = l;
    Q_ASSERT( u <= pointer_mask );
    bits |= signbit_mask | quiet_nan_mask | List_mask | u;
}

Reader::List*Reader::Object::getList() const
{
    Q_ASSERT( type() == List_);
    return (List*)(bits & pointer_mask);
}

void Reader::Object::set(double d)
{
    nil();
    dbl = d;
}

double Reader::Object::getDouble() const
{
    Q_ASSERT( type() == Float);
    return dbl;
}

Reader::Object::Type Reader::Object::type() const
{
    if( (bits & quiet_nan_mask) != quiet_nan_mask )
        return Float;
    else if( bits & signbit_mask )
    {
        const quint64 tmp = pointer_type_mask & bits;
        if( tmp == Nil_mask )
            return Nil_;
        if( tmp == String_mask )
            return String_;
        if( tmp == List_mask )
            return List_;
        if( tmp == Atom_mask )
            return Atom_;
        // else
        Q_ASSERT( false);
    }else
        return Integer;
}

void Reader::Object::nil()
{
    switch( type() )
    {
    case List_:
        getList()->release();
        break;
    case String_:
        getStr()->release();
        break;
    }
    bits = 0;
    bits |= signbit_mask | quiet_nan_mask | Nil_mask;
}

void Reader::Object::dump(QTextStream& out) const
{
    switch( type() )
    {
    case Float:
        out << "Float:" << getDouble();
        break;
    case Integer:
        out << "Integer:" << getInt();
        break;
    case Nil_:
        out << "NIL";
        break;
    case String_:
        out << "String: " << getStr()->str;
        break;
    case List_:
        out << "List with " << getList()->list.size() << " elements";
        break;
    case Atom_:
        out << "Atom: " << getAtom();
        break;
    default:
        out << "ERROR unknown object";
        break;
    }
}

void Reader::Object::print(QTextStream& out, int level) const
{
    switch( type() )
    {
    case Float:
        out << getDouble();
        break;
    case Integer:
        out << getInt();
        break;
    case Nil_:
        out << "NIL";
        break;
    case String_:
        out << getStr()->str;
        break;
    case List_: {
            List* l = getList();
            if( l->outer )
                out << "(";
            if( !l->list.isEmpty() )
                l->list[0].print(out, level + 1);
            if( l->list.size() > 1 )
            {
                if( !l->elementPositions.isEmpty() )
                    out << " (*" << l->elementPositions[0].row << ":" << l->elementPositions[0].col << ")";
                out << endl;
            }
            for(int i = 1; i < l->list.size(); i++ )
            {
                out << QByteArray(level*3,' ');
                l->list[i].print(out, level + 1);
                if( i < l->list.size() - 1 )
                {
                    if( i < l->elementPositions.size() )
                        out << " (*" << l->elementPositions[i].row << ":" << l->elementPositions[i].col << ")";
                    out << endl;
                }
            }
            if( l->outer )
                out << ")";
            break;
        }
    case Atom_:
        out << getAtom();
        break;
    default:
        out << "ERROR unknown object";
        break;
    }
}

QByteArray Reader::Object::toString(bool fullList) const
{
    switch( type() )
    {
    case Float:
        return QByteArray::number(getDouble());
    case Integer:
        return QByteArray::number(getInt());
    case Nil_:
        return "NIL";
    case String_:
        return getStr()->str;
    case List_: {
            List* l = getList();
            if( l->list.isEmpty() )
                return "()";
            if( fullList )
            {
                QByteArray res = "( ";
                for( int i = 0; i < l->list.size(); i++ )
                {
                    if( i != 0 )
                        res += " ";
                    res += l->list[i].toString(true);
                }
                res += " )";
                return res;
            }else
                return "( " + l->list.first().toString() + " ... " + QByteArray::number(l->list.size()) + " )";
        }
    case Atom_:
        return getAtom();
    default:
        return "???";
    }
}

void Reader::List::addRef()
{
    refcount++;
}

void Reader::List::release()
{
    refcount--;
    if( refcount == 0 )
        delete this;
}

Reader::Object Reader::List::getOuterFirst() const
{
    if( outer && !outer->list.isEmpty() )
        return outer->list.first();
    else
        return Object();
}

RowCol Reader::List::getStart() const
{
    if( outer == 0 || outer->elementPositions.isEmpty() )
        return RowCol();
    for( int i = 0; i < outer->list.size(); i++ )
    {
        if( outer->list[i].type() == Object::List_ && outer->list[i].getList() == this )
            return outer->elementPositions[i];
    }
    return RowCol();
}

void Reader::String::addRef()
{
    refcount++;
}

void Reader::String::release()
{
    refcount--;
    if( refcount == 0 )
        delete this;
}
