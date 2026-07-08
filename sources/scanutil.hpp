// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: MIT

#pragma once

#include <QList>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QTextBlock;
QT_END_NAMESPACE

namespace StickyScroll {

int parenBalance(const QTextBlock &block);
QStringList logLines(const QList<int> &blockNumbers);

}
