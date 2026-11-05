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

#include "UBSpellChecker.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "hunspell/hunspell.hxx"

#include "core/UBSettings.h"
#include "frameworks/UBPlatformUtils.h"

namespace
{
    // Hunspell expects UTF-8 paths; on Windows, the long path prefix makes it
    // open the file with the wide-char API (needed for non-ASCII folder names).
    std::string hunspellPath(const QString& path)
    {
#ifdef Q_OS_WIN
        return (QString("\\\\?\\") + QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath())).toUtf8().toStdString();
#else
        return QFile::encodeName(path).toStdString();
#endif
    }
}

UBSpellChecker* UBSpellChecker::instance()
{
    static UBSpellChecker checker;
    return &checker;
}

UBSpellChecker::UBSpellChecker()
{
}

UBSpellChecker::~UBSpellChecker()
{
}

void UBSpellChecker::load()
{
    if (mLoaded)
        return;

    mLoaded = true;

    const QString dictionaryDir = UBPlatformUtils::applicationResourcesDirectory() + "/dictionaries";
    const QString affPath = dictionaryDir + "/fr.aff";
    const QString dicPath = dictionaryDir + "/fr.dic";

    if (!QFile::exists(affPath) || !QFile::exists(dicPath))
    {
        qWarning() << "French dictionary not found in" << dictionaryDir;
        return;
    }

    mHunspell.reset(new Hunspell(hunspellPath(affPath).c_str(), hunspellPath(dicPath).c_str()));

    // personal dictionary (one word per line)
    mPersonalDictionaryPath = UBSettings::userDataDirectory() + "/dictionnaire-personnel.txt";

    QFile personal(mPersonalDictionaryPath);
    if (personal.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&personal);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        in.setCodec("UTF-8");
#endif
        while (!in.atEnd())
        {
            const QString word = in.readLine().trimmed();
            if (!word.isEmpty())
                mHunspell->add(word.toUtf8().toStdString());
        }
    }
}

bool UBSpellChecker::isAvailable()
{
    load();
    return mHunspell != nullptr;
}

bool UBSpellChecker::isCorrect(const QString& word)
{
    if (!isAvailable() || !shouldCheck(word))
        return true;

    auto it = mCache.constFind(word);
    if (it != mCache.constEnd())
        return it.value();

    const bool correct = mHunspell->spell(word.toUtf8().toStdString());

    if (mCache.size() > 20000)
        mCache.clear();

    mCache.insert(word, correct);
    return correct;
}

QStringList UBSpellChecker::suggestions(const QString& word)
{
    QStringList result;

    if (!isAvailable())
        return result;

    for (const std::string& suggestion : mHunspell->suggest(word.toUtf8().toStdString()))
        result << QString::fromUtf8(suggestion.c_str());

    return result;
}

void UBSpellChecker::addToPersonalDictionary(const QString& word)
{
    if (!isAvailable() || word.isEmpty())
        return;

    mHunspell->add(word.toUtf8().toStdString());
    mCache.insert(word, true);

    QFile personal(mPersonalDictionaryPath);
    if (personal.open(QIODevice::Append | QIODevice::Text))
    {
        personal.write(word.toUtf8());
        personal.write("\n");
    }
}

QList<QPair<int, int>> UBSpellChecker::words(const QString& text)
{
    // letters, with apostrophes and hyphens inside words (l'école, aujourd'hui, peut-être)
    static const QRegularExpression wordExpression(QString::fromUtf8("[\\p{L}\\p{M}]+(?:['’\\-][\\p{L}\\p{M}]+)*"));

    QList<QPair<int, int>> result;
    QRegularExpressionMatchIterator it = wordExpression.globalMatch(text);

    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        result << qMakePair(int(match.capturedStart()), int(match.capturedLength()));
    }

    return result;
}

bool UBSpellChecker::shouldCheck(const QString& word)
{
    if (word.length() < 2)
        return false;

    // capitals after the first letter: acronyms, units, symbols (ADN, kJ, OpenBoard)
    for (int i = 1; i < word.length(); ++i)
    {
        if (word.at(i).isUpper())
            return false;
    }

    return true;
}
