// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#include "scanutil.hpp"

#include "stickyscrolltypes.hpp"

#include <texteditor/textdocumentlayout.h>

#include <QString>
#include <QTextBlock>

using namespace TextEditor;

namespace StickyScroll {

Q_LOGGING_CATEGORY(stickyLog, "qtc.stickyscroll", QtWarningMsg)

int parenBalance(const QTextBlock &block)
{
    const Parentheses parens = TextBlockUserData::parentheses(block);
    if (!parens.isEmpty()) {
        int balance = 0;
        for (const Parenthesis &paren : parens) {
            if (paren.chr == QLatin1Char('('))
                ++balance;
            else if (paren.chr == QLatin1Char(')'))
                --balance;
        }
        return balance;
    }
    const QString text = block.text();
    return static_cast<int>(text.count(QLatin1Char('(')) - text.count(QLatin1Char(')')));
}

QStringList logLines(const QList<int> &blockNumbers)
{
    QStringList result;
    for (const int number : blockNumbers)
        result << QString::number(number + 1);
    return result;
}

}
