#include <gtest/gtest.h>

#include "utils/t9dictionary.h"

namespace
{

T9Dictionary sampleDictionary()
{
    T9Dictionary dictionary;
    dictionary.setWords(
        { "he", "go", "in", "inn", "gold", "hold", "gone", "good", "home", "hood", "going" });
    return dictionary;
}

}

TEST(T9DictionaryTest, DigitsForMapsLettersToKeys)
{
    EXPECT_EQ(T9Dictionary::digitsFor("good"), "4663");
    EXPECT_EQ(T9Dictionary::digitsFor("Sync"), "7962");
    EXPECT_EQ(T9Dictionary::digitsFor("naïve"), "");
}

TEST(T9DictionaryTest, SetWordsSkipsDuplicatesAndWordsOutsideTheKeys)
{
    T9Dictionary dictionary;

    dictionary.setWords({ "good", "Good", "don't", "", "café", "gone" });

    EXPECT_EQ(dictionary.size(), 2);
    EXPECT_TRUE(dictionary.contains("good"));
    EXPECT_FALSE(dictionary.contains("don't"));
}

TEST(T9DictionaryTest, ExactMatchesKeepListOrderWithoutUsage)
{
    const T9Dictionary dictionary = sampleDictionary();

    EXPECT_EQ(dictionary.candidates("4663", 10), QStringList({ "gone", "good", "home", "hood" }));
}

TEST(T9DictionaryTest, ExactMatchesComeBeforeLongerWords)
{
    const T9Dictionary dictionary = sampleDictionary();

    EXPECT_EQ(dictionary.candidates("46", 10),
              QStringList(
                  { "go", "in", "inn", "gold", "hold", "gone", "good", "home", "hood", "going" }));
}

TEST(T9DictionaryTest, LongerWordsKeepListOrderWhenThereIsNoExactMatch)
{
    const T9Dictionary dictionary = sampleDictionary();

    EXPECT_EQ(dictionary.candidates("4", 3), QStringList({ "he", "go", "in" }));
    EXPECT_EQ(dictionary.candidates("465", 5), QStringList({ "gold", "hold" }));
}

TEST(T9DictionaryTest, FrequentLongerWordComesBeforeRareShorterOne)
{
    T9Dictionary dictionary;
    dictionary.setWords({ "going", "gone", "hoof" });

    EXPECT_EQ(dictionary.candidates("46", 3), QStringList({ "going", "gone", "hoof" }));
}

TEST(T9DictionaryTest, LearnedRareWordBeatsFrequentOne)
{
    T9Dictionary dictionary;
    dictionary.setWords({ "good", "home", "gone", "hood" });
    dictionary.learn("hood", 1);

    EXPECT_EQ(dictionary.candidates("4663", 4), QStringList({ "hood", "good", "home", "gone" }));
}

TEST(T9DictionaryTest, CandidatesRespectTheLimit)
{
    const T9Dictionary dictionary = sampleDictionary();

    EXPECT_EQ(dictionary.candidates("4663", 2), QStringList({ "gone", "good" }));
    EXPECT_TRUE(dictionary.candidates("4663", 0).isEmpty());
}

TEST(T9DictionaryTest, UnknownOrInvalidDigitsGiveNoCandidates)
{
    const T9Dictionary dictionary = sampleDictionary();

    EXPECT_TRUE(dictionary.candidates("46639", 5).isEmpty());
    EXPECT_TRUE(dictionary.candidates("", 5).isEmpty());
    EXPECT_TRUE(dictionary.candidates("41", 5).isEmpty());
}

TEST(T9DictionaryTest, LearnedWordRanksFirst)
{
    T9Dictionary dictionary = sampleDictionary();

    const T9Dictionary::Usage usage = dictionary.learn("good", 1000);

    EXPECT_EQ(usage, (T9Dictionary::Usage { 1, 1000 }));
    EXPECT_EQ(dictionary.candidates("4663", 2), QStringList({ "good", "gone" }));
}

TEST(T9DictionaryTest, MoreFrequentWordWinsOverMoreRecentOne)
{
    T9Dictionary dictionary = sampleDictionary();
    dictionary.learn("home", 1);
    dictionary.learn("home", 2);
    dictionary.learn("hood", 3);

    EXPECT_EQ(dictionary.candidates("4663", 3), QStringList({ "home", "hood", "gone" }));
}

TEST(T9DictionaryTest, RecentWordWinsBetweenEquallyFrequentOnes)
{
    T9Dictionary dictionary = sampleDictionary();
    dictionary.learn("hood", 1);
    dictionary.learn("good", 2);

    EXPECT_EQ(dictionary.candidates("4663", 2), QStringList({ "good", "hood" }));
}

TEST(T9DictionaryTest, LearnedCompletionComesFirstAmongLongerWords)
{
    T9Dictionary dictionary = sampleDictionary();
    dictionary.learn("going", 1);

    EXPECT_EQ(dictionary.candidates("46", 3), QStringList({ "go", "in", "going" }));
}

TEST(T9DictionaryTest, LearningAnUnknownWordAddsIt)
{
    T9Dictionary dictionary = sampleDictionary();

    dictionary.learn("Goof", 5);

    EXPECT_TRUE(dictionary.contains("goof"));
    EXPECT_EQ(dictionary.candidates("4663", 1), QStringList({ "goof" }));
    EXPECT_EQ(dictionary.usage("goof"), (T9Dictionary::Usage { 1, 5 }));
}

TEST(T9DictionaryTest, LearningAWordOutsideTheKeysIsIgnored)
{
    T9Dictionary dictionary = sampleDictionary();

    EXPECT_EQ(dictionary.learn("don't", 5), T9Dictionary::Usage());
    EXPECT_EQ(dictionary.size(), 11);
}

TEST(T9DictionaryTest, SetUsageRestoresStoredCountsAndAddsUnknownWords)
{
    T9Dictionary dictionary = sampleDictionary();

    dictionary.setUsage("hood", { 4, 10 });
    dictionary.setUsage("hoof", { 2, 20 });

    EXPECT_EQ(dictionary.candidates("4663", 3), QStringList({ "hood", "hoof", "gone" }));
    EXPECT_EQ(dictionary.learn("hood", 30), (T9Dictionary::Usage { 5, 30 }));
}

TEST(T9DictionaryTest, SetWordsResetsUsage)
{
    T9Dictionary dictionary = sampleDictionary();
    dictionary.learn("good", 1);

    dictionary.setWords({ "gone", "good" });

    EXPECT_EQ(dictionary.usage("good"), T9Dictionary::Usage());
    EXPECT_EQ(dictionary.candidates("4663", 2), QStringList({ "gone", "good" }));
}
