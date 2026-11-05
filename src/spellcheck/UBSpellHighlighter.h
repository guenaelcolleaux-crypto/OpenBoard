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

#ifndef UBSPELLHIGHLIGHTER_H
#define UBSPELLHIGHLIGHTER_H

#include <QSyntaxHighlighter>

/**
 * @brief Underlines the misspelled words of a text zone.
 *
 * Underlines are only shown while the text zone is being edited, so that they
 * never appear on the board in front of the class or in exported documents.
 */
class UBSpellHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

    public:
        explicit UBSpellHighlighter(QTextDocument* document);

        void setActive(bool active);
        bool isActive() const { return mActive; }

    protected:
        void highlightBlock(const QString& text) override;

    private:
        bool mActive = false;
};

#endif // UBSPELLHIGHLIGHTER_H
