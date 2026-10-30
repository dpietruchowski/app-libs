#include <gtest/gtest.h>

#include "utils/t9dictionary.h"

namespace
{

const QStringList kGood = { "ghi", "mno", "mno", "def" };
const QStringList kGo = { "ghi", "mno" };

T9Dictionary sampleDictionary()
{
    T9Dictionary dictionary;
    dictionary.setWords(
        { "he", "go", "in", "inn", "gold", "hold", "gone", "good", "home", "hood", "going" });
    return dictionary;
}

}

TEST(T9DictionaryTest, NormalizedLowersAndKeepsInnerApostrophesAndHyphens)
{
    EXPECT_EQ(T9Dictionary::normalized("What’s"), "what's");
    EXPECT_EQ(T9Dictionary::normalized(" Well-Known "), "well-known");
    EXPECT_EQ(T9Dictionary::normalized("Źdźbło"), QString::fromUtf8("źdźbło"));
    EXPECT_EQ(T9Dictionary::normalized("'tis"), "");
    EXPECT_EQ(T9Dictionary::normalized("dogs'"), "");
    EXPECT_EQ(T9Dictionary::normalized("a--b"), "");
    EXPECT_EQ(T9Dictionary::normalized("r2d2"), "");
}

TEST(T9DictionaryTest, SetWordsSkipsDuplicatesAndInvalidWords)
{
    T9Dictionary dictionary;

    dictionary.setWords({ "good", "Good", "don't", "", "café", "gone", "x1" });

    EXPECT_EQ(dictionary.size(), 4);
    EXPECT_TRUE(dictionary.contains("good"));
    EXPECT_TRUE(dictionary.contains("don’t"));
    EXPECT_TRUE(dictionary.contains("CAFÉ"));
    EXPECT_FALSE(dictionary.contains("x1"));
}

TEST(T9DictionaryTest, ExactMatchesKeepListOrderWithoutUsage)
{
    const T9Dictionary dictionary = sampleDictionary();

    EXPECT_EQ(dictionary.candidatesForGroups(kGood, 10),
              QStringList({ "gone", "good", "home", "hood" }));
}

TEST(T9DictionaryTest, ExactMatchesComeBeforeLongerWords)
{
    const T9Dictionary dictionary = sampleDictionary();

    EXPECT_EQ(dictionary.candidatesForGroups(kGo, 10),
              QStringList(
                  { "go", "in", "inn", "gold", "hold", "gone", "good", "home", "hood", "going" }));
}

TEST(T9DictionaryTest, LongerWordsKeepListOrderWhenThereIsNoExactMatch)
{
    const T9Dictionary dictionary = sampleDictionary();

    EXPECT_EQ(dictionary.candidatesForGroups({ "ghi" }, 3), QStringList({ "he", "go", "in" }));
    EXPECT_EQ(dictionary.candidatesForGroups({ "ghi", "mno", "jkl" }, 5),
              QStringList({ "gold", "hold" }));
}

TEST(T9DictionaryTest, FrequentLongerWordComesBeforeRareShorterOne)
{
    T9Dictionary dictionary;
    dictionary.setWords({ "going", "gone", "hoof" });

    EXPECT_EQ(dictionary.candidatesForGroups(kGo, 3), QStringList({ "going", "gone", "hoof" }));
}

TEST(T9DictionaryTest, LearnedRareWordBeatsFrequentOne)
{
    T9Dictionary dictionary;
    dictionary.setWords({ "good", "home", "gone", "hood" });
    dictionary.learn("hood", 1);

    EXPECT_EQ(dictionary.candidatesForGroups(kGood, 4),
              QStringList({ "hood", "good", "home", "gone" }));
}

TEST(T9DictionaryTest, CandidatesRespectTheLimit)
{
    const T9Dictionary dictionary = sampleDictionary();

    EXPECT_EQ(dictionary.candidatesForGroups(kGood, 2), QStringList({ "gone", "good" }));
    EXPECT_TRUE(dictionary.candidatesForGroups(kGood, 0).isEmpty());
}

TEST(T9DictionaryTest, UnmatchedOrInvalidGroupsGiveNoCandidates)
{
    const T9Dictionary dictionary = sampleDictionary();

    EXPECT_TRUE(dictionary.candidatesForGroups({ "ghi", "mno", "mno", "def", "wxyz" }, 5).isEmpty());
    EXPECT_TRUE(dictionary.candidatesForGroups({}, 5).isEmpty());
    EXPECT_TRUE(dictionary.candidatesForGroups({ "ghi", "12" }, 5).isEmpty());
}

TEST(T9DictionaryTest, LearnedWordRanksFirst)
{
    T9Dictionary dictionary = sampleDictionary();

    const T9Dictionary::Usage usage = dictionary.learn("good", 1000);

    EXPECT_EQ(usage, (T9Dictionary::Usage { 1, 1000 }));
    EXPECT_EQ(dictionary.candidatesForGroups(kGood, 2), QStringList({ "good", "gone" }));
}

TEST(T9DictionaryTest, MoreFrequentWordWinsOverMoreRecentOne)
{
    T9Dictionary dictionary = sampleDictionary();
    dictionary.learn("home", 1);
    dictionary.learn("home", 2);
    dictionary.learn("hood", 3);

    EXPECT_EQ(dictionary.candidatesForGroups(kGood, 3), QStringList({ "home", "hood", "gone" }));
}

TEST(T9DictionaryTest, RecentWordWinsBetweenEquallyFrequentOnes)
{
    T9Dictionary dictionary = sampleDictionary();
    dictionary.learn("hood", 1);
    dictionary.learn("good", 2);

    EXPECT_EQ(dictionary.candidatesForGroups(kGood, 2), QStringList({ "good", "hood" }));
}

TEST(T9DictionaryTest, LearnedCompletionComesFirstAmongLongerWords)
{
    T9Dictionary dictionary = sampleDictionary();
    dictionary.learn("going", 1);

    EXPECT_EQ(dictionary.candidatesForGroups(kGo, 3), QStringList({ "go", "in", "going" }));
}

TEST(T9DictionaryTest, LearningAnUnknownWordAddsIt)
{
    T9Dictionary dictionary = sampleDictionary();

    dictionary.learn("Goof", 5);

    EXPECT_TRUE(dictionary.contains("goof"));
    EXPECT_EQ(dictionary.candidatesForGroups(kGood, 1), QStringList({ "goof" }));
    EXPECT_EQ(dictionary.usage("goof"), (T9Dictionary::Usage { 1, 5 }));
}

TEST(T9DictionaryTest, LearningAnInvalidWordIsIgnored)
{
    T9Dictionary dictionary = sampleDictionary();

    EXPECT_EQ(dictionary.learn("it.", 5), T9Dictionary::Usage());
    EXPECT_EQ(dictionary.size(), 11);
}

TEST(T9DictionaryTest, SetUsageRestoresStoredCountsAndAddsUnknownWords)
{
    T9Dictionary dictionary = sampleDictionary();

    dictionary.setUsage("hood", { 4, 10 });
    dictionary.setUsage("hoof", { 2, 20 });

    EXPECT_EQ(dictionary.candidatesForGroups(kGood, 3), QStringList({ "hood", "hoof", "gone" }));
    EXPECT_EQ(dictionary.learn("hood", 30), (T9Dictionary::Usage { 5, 30 }));
}

TEST(T9DictionaryTest, GroupsMatchWordsWithOneLetterFromEachGroup)
{
    T9Dictionary dictionary;
    dictionary.setWords({ "we", "re", "ww", "qe", "wet", "west", "rest", "as" });

    EXPECT_EQ(dictionary.candidatesForGroups({ "qw", "er" }, 10),
              QStringList({ "we", "qe", "wet", "west" }));
}

TEST(T9DictionaryTest, GroupsAreCaseInsensitiveAndSkipDigits)
{
    T9Dictionary dictionary;
    dictionary.setWords({ "go", "in" });

    EXPECT_EQ(dictionary.candidatesForGroups({ "GH4", "Op" }, 5), QStringList({ "go" }));
}

TEST(T9DictionaryTest, GroupsMatchLettersWithDiacritics)
{
    T9Dictionary dictionary;
    dictionary.setWords({ QString::fromUtf8("ćma"), "ala", QString::fromUtf8("żółw") });

    EXPECT_EQ(dictionary.candidatesForGroups({ QString::fromUtf8("abcąć"), "mnoń" }, 5),
              QStringList({ QString::fromUtf8("ćma") }));
    EXPECT_EQ(dictionary.candidatesForGroups({ QString::fromUtf8("wxyzźż") }, 5),
              QStringList({ QString::fromUtf8("żółw") }));
}

TEST(T9DictionaryTest, ApostropheGroupCompletesContractions)
{
    T9Dictionary dictionary;
    dictionary.setWords({ "what", "what's", "he", "he'll", "hell" });

    EXPECT_EQ(dictionary.candidatesForGroups({ "wxyz", "ghi", "abc", "tuv", "'-" }, 5),
              QStringList({ "what's" }));
    EXPECT_EQ(dictionary.candidatesForGroups({ "ghi", "def", "'-" }, 5),
              QStringList({ "he'll" }));
}

TEST(T9DictionaryTest, GroupsFindLearnedUnknownWords)
{
    T9Dictionary dictionary;
    dictionary.setWords({ "cat" });

    dictionary.learn("qwop", 1);

    EXPECT_EQ(dictionary.candidatesForGroups({ "qw", "qw" }, 5), QStringList({ "qwop" }));
}

TEST(T9DictionaryTest, SetWordsResetsUsage)
{
    T9Dictionary dictionary = sampleDictionary();
    dictionary.learn("good", 1);

    dictionary.setWords({ "gone", "good" });

    EXPECT_EQ(dictionary.usage("good"), T9Dictionary::Usage());
    EXPECT_EQ(dictionary.candidatesForGroups(kGood, 2), QStringList({ "gone", "good" }));
}
