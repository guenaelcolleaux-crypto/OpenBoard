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

#ifndef UBSPELLCHECKER_H
#define UBSPELLCHECKER_H

#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <memory>

class Hunspell;

/**
 * @brief French spell checker based on Hunspell.
 *
 * The dictionary (fr.aff / fr.dic) is read from the "dictionaries" folder of
 * the application. Words added by the user are kept in a personal dictionary
 * stored in the user data directory (on the USB key in portable mode).
 */
class UBSpellChecker
{
    public:
        static UBSpellChecker* instance();

        bool isAvailable();
        bool isCorrect(const QString& word);
        QStringList suggestions(const QString& word);
        void addToPersonalDictionary(const QString& word);

        //! Words of a text, as (start, length) pairs, in the way the spell checker splits them
        static QList<QPair<int, int>> words(const QString& text);

        //! True if the word should be checked at all (not a number, an acronym, a unit...)
        static bool shouldCheck(const QString& word);

    private:
        UBSpellChecker();
        ~UBSpellChecker();
        void load();

        bool mLoaded = false;
        std::unique_ptr<Hunspell> mHunspell;
        QHash<QString, bool> mCache;
        QString mPersonalDictionaryPath;
};

#endif // UBSPELLCHECKER_H
