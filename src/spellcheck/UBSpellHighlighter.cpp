/*
 * Copyright (C) 2026 OpenBoard contributors
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License,
 * with a specific linking exception for the OpenSSL project's
 * "OpenSSL" library (or with modified versions of it that use the
 * same license as the "OpenSSL" library).
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */

#include "UBSpellHighlighter.h"

#include "UBSpellChecker.h"

UBSpellHighlighter::UBSpellHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document)
{
}

void UBSpellHighlighter::setActive(bool active)
{
    if (active == mActive)
        return;

    mActive = active;
    rehighlight();
}

void UBSpellHighlighter::highlightBlock(const QString& text)
{
    if (!mActive || text.isEmpty())
        return;

    UBSpellChecker* checker = UBSpellChecker::instance();

    if (!checker->isAvailable())
        return;

    QTextCharFormat misspelled;
    misspelled.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    misspelled.setUnderlineColor(QColor(220, 0, 0));

    for (const auto& word : UBSpellChecker::words(text))
    {
        if (!checker->isCorrect(text.mid(word.first, word.second)))
            setFormat(word.first, word.second, misspelled);
    }
}
