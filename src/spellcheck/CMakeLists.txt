# French spell checking (Hunspell is compiled with OpenBoard)
target_sources(openboard PRIVATE
    UBSpellChecker.cpp
    UBSpellChecker.h
    UBSpellHighlighter.cpp
    UBSpellHighlighter.h
    hunspell/affentry.cxx
    hunspell/affixmgr.cxx
    hunspell/csutil.cxx
    hunspell/filemgr.cxx
    hunspell/hashmgr.cxx
    hunspell/hunspell.cxx
    hunspell/hunzip.cxx
    hunspell/phonet.cxx
    hunspell/replist.cxx
    hunspell/suggestmgr.cxx
)

target_compile_definitions(openboard PRIVATE HUNSPELL_STATIC)
