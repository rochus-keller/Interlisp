#ifndef LISPREADER_H
#define LISPREADER_H

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

#include <QString>
#include <QList>
#include <QTextStream>
#include "LispRowCol.h"

class QIODevice;

namespace Lisp
{

class Lexer;
class Token;

class Reader
{
public:
    class List;
    class String;
    class Object
    {
        union
        {
            double dbl;
            quint64 bits;
        };
        enum { signbit = 1LL << (64-1),
               quiet_nan_mask = 0xfffLL << 51,
               pointer_type_mask = 7LL << 48,
               int_mask = (1LL << 50) - 1,
               int_sign = 1LL << 50 };
    public:
        enum Type {
            Float = 0,
            Integer = 1,
            Nil_ = 1LL << 48,
            String_ = 2LL << 48,
            List_ = 3LL << 48,
            Atom = 4LL << 48,
        };

        Object();
        Object(double);
        Object(qint64);
        Object(const char*);
        Object(List*);
        Object(String*);
        Object(const Object&);
        ~Object();

        Object& operator=(const Object&);

        void set(double);
        double getDouble() const;
        void set(qint64);
        qint64 getInt() const;
        void set(const char*); // Atom
        void set(String*);
        String* getStr() const;
        const char* getAtom() const;
        int getAtomLen(bool inCode = false) const;
        void set(List*);
        List* getList() const;
        Type type() const;
        void nil();
        void dump(QTextStream& out) const;
        void print(QTextStream& out, int level = 0) const;
        QByteArray toString() const;
    };

    struct List
    {
        quint32 refcount;
    public:
        QList<Object> list;
        RowCol end;
        List* outer;
        QList<RowCol> elementPositions;

        List():refcount(0),outer(0){}
        void addRef();
        void release();
        Object getOuterFirst() const;
        RowCol getStart() const;
    };

    struct String
    {
        quint32 refcount;
    public:
        QByteArray str;

        String(const QByteArray& str = QByteArray()):refcount(0),str(str){}
        void addRef();
        void release();
    };

    struct Ref
    {
        RowCol pos;
        enum Role { Use, Call, Decl };
        quint8 role;
        quint16 len;
        Ref(const RowCol& rc = RowCol(), quint16 l = 0, Role r = Use):pos(rc),role(r),len(l){}
    };
    typedef QList<Ref> Refs;
    typedef QHash<const char*,Refs> Xref;

    Reader();

    bool read(QIODevice*, const QString& path);
    const QString getError() const { return error; }
    const RowCol& getPos() const { return pos; }
    const Object& getAst() const { return ast; }
    const Xref& getXref() const { return xref; }

private:
    Object next(Lexer&, List* outer, bool inQuote);
    Object list(Lexer& in, bool brack, List* outer);
    void report(const Token&);
    void report(const Token&, const QString&);

    Object ast;
    QString error;
    RowCol pos;
    const char* STOP;
    const char* NIL;
    const char* DEFINEQ;
    const char* QUOTE;
    Xref xref;
};

}

Q_DECLARE_METATYPE(Lisp::Reader::Object)
Q_DECLARE_METATYPE(Lisp::Reader::Ref)

#endif // LISPREADER_H
