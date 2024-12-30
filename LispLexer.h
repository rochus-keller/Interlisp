#ifndef LISPLEXER_H
#define LISPLEXER_H

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

#include <QObject>
#include "LispRowCol.h"

class QIODevice;

namespace Lisp
{

enum TokenType {
    Tok_Invalid,
    Tok_Eof,
    Tok_lpar,
    Tok_rpar,
    Tok_lbrack,
    Tok_rbrack,
    Tok_string,
    Tok_atom,
    Tok_float,
    Tok_integer,
    Tok_comment
};

struct Token
{
#ifdef _DEBUG
    union
    {
    int type; // TokenType
    TokenType tokenType;
    };
#else
    quint16 type; // TokenType
#endif
    quint16 len;

    RowCol pos;

    QByteArray val;
    QString sourcePath;
    Token(quint16 t = Tok_Invalid, const RowCol& rc = RowCol(), quint16 len = 0, const QByteArray& val = QByteArray() ):
        type(t),pos(rc),len(len),val(val){}
    bool isValid() const;
    bool isEof() const;
    const char* getName() const;
    const char* getString() const;

    static QByteArray getSymbol( const QByteArray& );
    static QStringList getAllSymbols();
};

class Lexer : public QObject
{
public:
    Lexer(QObject *parent = 0);

    void setStream( QIODevice*, const QString& sourcePath );
    bool setStream(const QString& sourcePath);

    Token nextToken();
    void unget(const Token&);
    QList<Token> tokens( const QString& code );
    QList<Token> tokens( const QByteArray& code, const QString& path = QString() );
    QString getSource() const { return sourcePath; }
    void setEmitComments(bool on) { emitComments = on; }
    void setPacked(bool on) { packed = on; }
    RowCol getPos() const { return pos; }
    void startQuote();
    void endQuote();

    static bool atom_delimiter(char);

protected:
    Token nextTokenImp();
    char readc();
    void ungetc(char c);
    void ungetstr(const QByteArray& str);
    Token token(TokenType tt, int len = 0, const QByteArray &val = QByteArray());
    Token number();
    Token atom();
    Token string();
    Token comment();
    void skipWhiteSpace();

private:
    QIODevice* in;
    RowCol pos, start;
    QString sourcePath;
    QList<Token> buffer;
    bool emitComments;
    bool packed;
    bool inQuote;
};

}

#endif // LISPLEXER_H
