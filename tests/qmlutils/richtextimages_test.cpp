#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "qmlutils/richtextimages.h"

class RichTextImagesTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(dir.isValid());
        saveImage(QStringLiteral("wide.png"), 900, 300);
        saveImage(QStringLiteral("narrow.png"), 150, 600);
        saveImage(QStringLiteral("narrow.dark.png"), 150, 600);
    }

    void saveImage(const QString& name, int width, int height)
    {
        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        ASSERT_TRUE(image.save(dir.filePath(name)));
    }

    RichTextImages::Options options(qreal maxWidth = 0, const QString& variant = {}) const
    {
        return { QUrl::fromLocalFile(dir.path() + QLatin1Char('/')), maxWidth, 3, variant };
    }

    QTemporaryDir dir;
};

TEST_F(RichTextImagesTest, ImageGetsItsNaturalWidthAtTheSourcePixelRatio)
{
    EXPECT_EQ(RichTextImages::fit(QStringLiteral("<img src=\"narrow.png\">"), options()),
              QStringLiteral("<img width=\"50\" src=\"narrow.png\">"));
}

TEST_F(RichTextImagesTest, ImageWiderThanTheSpaceIsCappedAtMaxWidth)
{
    EXPECT_EQ(RichTextImages::fit(QStringLiteral("<img src=\"wide.png\">"), options(250)),
              QStringLiteral("<img width=\"250\" src=\"wide.png\">"));
    EXPECT_EQ(RichTextImages::fit(QStringLiteral("<img src=\"wide.png\">"), options(400)),
              QStringLiteral("<img width=\"300\" src=\"wide.png\">"));
}

TEST_F(RichTextImagesTest, VariantIsUsedOnlyWhereItExists)
{
    const QString html = QStringLiteral("<img src=\"narrow.png\"><img src=\"wide.png\">");
    EXPECT_EQ(RichTextImages::fit(html, options(0, QStringLiteral(".dark"))),
              QStringLiteral("<img width=\"50\" src=\"narrow.dark.png\">"
                             "<img width=\"300\" src=\"wide.png\">"));
}

TEST_F(RichTextImagesTest, TextAndOtherAttributesArePreserved)
{
    const QString html
        = QStringLiteral("<p>before</p><p class=\"figure\"><img alt=\"x\" src=\"narrow.png\" "
                         "title=\"t\"></p><p>after</p>");
    EXPECT_EQ(RichTextImages::fit(html, options()),
              QStringLiteral("<p>before</p><p class=\"figure\"><img alt=\"x\" width=\"50\" "
                             "src=\"narrow.png\" title=\"t\"></p><p>after</p>"));
}

TEST_F(RichTextImagesTest, ExplicitWidthIsLeftAlone)
{
    const QString html = QStringLiteral("<img width=\"20\" src=\"narrow.png\">");
    EXPECT_EQ(RichTextImages::fit(html, options()), html);
}

TEST_F(RichTextImagesTest, MissingImageKeepsItsTagWithoutWidth)
{
    const QString html = QStringLiteral("<img src=\"missing.png\">");
    EXPECT_EQ(RichTextImages::fit(html, options(0, QStringLiteral(".dark"))), html);
}

TEST_F(RichTextImagesTest, FittedHtmlFollowsTheAvailableWidth)
{
    RichTextImages images;
    images.setBaseUrl(QUrl::fromLocalFile(dir.path() + QLatin1Char('/')));
    images.setSourcePixelRatio(3);
    images.setHtml(QStringLiteral("<img src=\"wide.png\">"));
    EXPECT_EQ(images.fittedHtml(), QStringLiteral("<img width=\"300\" src=\"wide.png\">"));

    QSignalSpy fitted(&images, &RichTextImages::fittedHtmlChanged);
    images.setMaxWidth(120);
    EXPECT_EQ(fitted.count(), 1);
    EXPECT_EQ(images.fittedHtml(), QStringLiteral("<img width=\"120\" src=\"wide.png\">"));
}
