#ifndef LISPHIGHLIGHTER_H
#define LISPHIGHLIGHTER_H

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

// derived from Luon Highlighter

#include <QSyntaxHighlighter>
#include <QSet>
#include <QMap>

namespace Lisp
{
    class Highlighter : public QSyntaxHighlighter
    {
    public:
        enum { TokenProp = QTextFormat::UserProperty };
        explicit Highlighter(QTextDocument *parent = 0);
        void addFunction(const char* bi);
        void addVariable(const char* bi);

    protected:
        QTextCharFormat formatForCategory(int) const;
        void clearFromHere(quint32 line);

        // overrides
        void highlightBlock(const QString &text);

    private:
        enum Category { C_Num, C_Str, C_Func, C_Var, C_Ident, C_Op1, C_Op2, C_Op3, C_Pp, C_Cmt, C_Max };
        QTextCharFormat d_format[C_Max];
        QSet<const char*> d_functions, d_variables, d_syntax;
        typedef QMap<quint32,quint16> LineState;
        LineState lineState; // line -> level
    };
}

#endif // LISPHIGHLIGHTER_H
