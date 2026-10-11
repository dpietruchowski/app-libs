#include "utils/syllabifier.h"

#include <gtest/gtest.h>

namespace
{

QStringList split(const char* language, const char* word)
{
    return Syllabifier(QString::fromUtf8(language)).split(QString::fromUtf8(word));
}

QStringList expected(std::initializer_list<const char*> parts)
{
    QStringList result;
    for (const char* part : parts)
        result.append(QString::fromUtf8(part));
    return result;
}

}

TEST(SyllabifierTest, PolishKeepsDigraphsAndSofteningI)
{
    EXPECT_EQ(split("pl", "nieprzewidywalny"), expected({ "nie", "prze", "wi", "dy", "wal", "ny" }));
    EXPECT_EQ(split("pl", "przyjaciel"), expected({ "przy", "ja", "ciel" }));
    EXPECT_EQ(split("pl", "szczęście"), expected({ "szczęś", "cie" }));
    EXPECT_EQ(split("pl", "nauka"), expected({ "na", "u", "ka" }));
}

TEST(SyllabifierTest, EnglishHandlesSilentEAndVowelTeams)
{
    EXPECT_EQ(split("en", "unbelievable"), expected({ "un", "be", "lie", "va", "ble" }));
    EXPECT_EQ(split("en", "playing"), expected({ "play", "ing" }));
    EXPECT_EQ(split("en", "pocket"), expected({ "pock", "et" }));
    EXPECT_EQ(split("en", "complicated"), expected({ "com", "pli", "ca", "ted" }));
    EXPECT_EQ(split("en", "beautiful"), expected({ "beau", "ti", "ful" }));
    EXPECT_EQ(split("en", "happy"), expected({ "hap", "py" }));
    EXPECT_EQ(split("en", "make"), expected({ "make" }));
}

TEST(SyllabifierTest, GermanKeepsSchAndDiphthongs)
{
    EXPECT_EQ(split("de", "entschuldigung"), expected({ "ent", "schul", "di", "gung" }));
    EXPECT_EQ(split("de", "zucker"), expected({ "zu", "cker" }));
}

TEST(SyllabifierTest, FrenchSilencesFinalEExceptAfterLiquidCluster)
{
    EXPECT_EQ(split("fr", "maintenant"), expected({ "main", "te", "nant" }));
    EXPECT_EQ(split("fr", "table"), expected({ "ta", "ble" }));
    EXPECT_EQ(split("fr", "fenêtre"), expected({ "fe", "nê", "tre" }));
    EXPECT_EQ(split("fr", "chocolat"), expected({ "cho", "co", "lat" }));
}

TEST(SyllabifierTest, RomanceLanguagesKeepDiphthongs)
{
    EXPECT_EQ(split("es", "biblioteca"), expected({ "bi", "blio", "te", "ca" }));
    EXPECT_EQ(split("it", "famiglia"), expected({ "fa", "mi", "glia" }));
}

TEST(SyllabifierTest, CyrillicAndTurkishSplitEveryVowel)
{
    EXPECT_EQ(split("ru", "молоко"), expected({ "мо", "ло", "ко" }));
    EXPECT_EQ(split("tr", "güzellik"), expected({ "gü", "zel", "lik" }));
    EXPECT_EQ(split("tr", "saat"), expected({ "sa", "at" }));
}

TEST(SyllabifierTest, HangulSplitsPerBlock)
{
    EXPECT_EQ(split("ko", "안녕하세요"), expected({ "안", "녕", "하", "세", "요" }));
}

TEST(SyllabifierTest, FallsBackToChunksWithoutVowels)
{
    EXPECT_EQ(split("ar", "مرحبا"), expected({ "مرح", "با" }));
}

TEST(SyllabifierTest, SingleNucleusStaysWhole)
{
    EXPECT_EQ(split("en", "strengths"), expected({ "strengths" }));
}

TEST(SyllabifierTest, LowercasesInput)
{
    EXPECT_EQ(split("pl", "Nauka"), expected({ "na", "u", "ka" }));
}
