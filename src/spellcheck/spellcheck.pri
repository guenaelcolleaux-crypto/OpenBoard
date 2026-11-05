# French spell checking (Hunspell is compiled with OpenBoard)
DEFINES += HUNSPELL_STATIC

HEADERS += src/spellcheck/UBSpellChecker.h \
           src/spellcheck/UBSpellHighlighter.h

SOURCES += src/spellcheck/UBSpellChecker.cpp \
           src/spellcheck/UBSpellHighlighter.cpp \
           src/spellcheck/hunspell/affentry.cxx \
           src/spellcheck/hunspell/affixmgr.cxx \
           src/spellcheck/hunspell/csutil.cxx \
           src/spellcheck/hunspell/filemgr.cxx \
           src/spellcheck/hunspell/hashmgr.cxx \
           src/spellcheck/hunspell/hunspell.cxx \
           src/spellcheck/hunspell/hunzip.cxx \
           src/spellcheck/hunspell/phonet.cxx \
           src/spellcheck/hunspell/replist.cxx \
           src/spellcheck/hunspell/suggestmgr.cxx
