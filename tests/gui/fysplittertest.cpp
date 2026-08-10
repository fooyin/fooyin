/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gui/splitters/fysplitter.h"

#include <QApplication>
#include <QSplitter>
#include <QWidget>

#include <gtest/gtest.h>

namespace Fooyin::Testing {
namespace {
class TestSplitterHandle : public FySplitterHandle
{
public:
    using FySplitterHandle::FySplitterHandle;

    void moveTo(int position)
    {
        moveSplitter(position);
    }
};

class TestSplitter : public FySplitter
{
public:
    using FySplitter::FySplitter;

    [[nodiscard]] TestSplitterHandle* testHandle(int index) const
    {
        return static_cast<TestSplitterHandle*>(handle(index));
    }

protected:
    FySplitterHandle* createHandle() override
    {
        return new TestSplitterHandle(orientation(), this);
    }
};

QList<QWidget*> addWidgets(FySplitter& splitter, int count)
{
    QList<QWidget*> widgets;
    widgets.reserve(count);
    for(int i{0}; i < count; ++i) {
        auto* widget = new QWidget;
        splitter.addWidget(widget);
        widget->show();
        widgets.append(widget);
    }
    return widgets;
}

void showAndProcess(FySplitter& splitter, const QSize& size)
{
    splitter.resize(size);
    splitter.show();
    QApplication::processEvents();
}
} // namespace

TEST(FySplitterTest, RestoresLegacyQSplitterStateAfterOrientationChange)
{
    const QByteArray state = QByteArray::fromBase64("AAAA/wAAAAEAAAAEAAAAggAABqEAAAA+AAAAHAD/////AQAAAAEA");

    FySplitter splitter{Qt::Vertical};
    splitter.setOrientation(Qt::Horizontal);
    splitter.resize(100, 100);

    for(int i{0}; i < 4; ++i) {
        auto* widget = new QWidget;
        widget->setMinimumWidth(22);
        splitter.addWidget(widget);
        widget->show();
    }

    ASSERT_TRUE(splitter.restoreState(state));

    splitter.resize(1920, 100);
    splitter.show();
    QApplication::processEvents();

    const QList<int> sizes = splitter.sizes();
    ASSERT_EQ(4, sizes.size());
    EXPECT_GT(sizes.at(1), sizes.at(0) * 5);
    EXPECT_GT(sizes.at(0), sizes.at(2));
    EXPECT_GT(sizes.at(2), sizes.at(3));

    for(int i{0}; i < splitter.count(); ++i) {
        EXPECT_FALSE(splitter.isLocked(i));
    }
}

TEST(FySplitterTest, LockedChildKeepsSizeDuringAutomaticResize)
{
    TestSplitter splitter{Qt::Horizontal};
    addWidgets(splitter, 3);
    showAndProcess(splitter, {600, 100});
    splitter.setSizes({120, 200, 280});

    ASSERT_TRUE(splitter.setLocked(0, true));
    const int lockedSize = splitter.sizes().at(0);

    splitter.resize(900, 100);
    QApplication::processEvents();
    EXPECT_EQ(lockedSize, splitter.sizes().at(0));

    splitter.resize(700, 100);
    QApplication::processEvents();
    EXPECT_EQ(lockedSize, splitter.sizes().at(0));
}

TEST(FySplitterTest, DirectHandleMoveRebasesLockedSize)
{
    TestSplitter splitter{Qt::Horizontal};
    addWidgets(splitter, 2);
    showAndProcess(splitter, {600, 100});
    splitter.setSizes({200, 400});

    ASSERT_TRUE(splitter.setLocked(0, true));
    const int initialLockedSize = splitter.sizes().at(0);

    splitter.testHandle(1)->moveTo(300);
    const int manuallyResizedSize = splitter.sizes().at(0);
    EXPECT_NE(initialLockedSize, manuallyResizedSize);

    splitter.resize(800, 100);
    QApplication::processEvents();
    EXPECT_EQ(manuallyResizedSize, splitter.sizes().at(0));
}

TEST(FySplitterTest, AncestorHandleMoveRebasesNestedLockedSize)
{
    TestSplitter outer{Qt::Horizontal};
    auto* inner = new TestSplitter{Qt::Horizontal};
    addWidgets(*inner, 2);
    inner->show();

    outer.addWidget(inner);
    auto* sibling = new QWidget;
    outer.addWidget(sibling);
    sibling->show();

    showAndProcess(outer, {800, 100});
    outer.setSizes({400, 400});
    inner->setSizes({120, 280});
    ASSERT_TRUE(inner->setLocked(0, true));
    const int initialLockedSize = inner->sizes().at(0);

    outer.testHandle(1)->moveTo(600);
    const int manuallyResizedSize = inner->sizes().at(0);
    EXPECT_NE(initialLockedSize, manuallyResizedSize);

    outer.resize(1000, 100);
    QApplication::processEvents();
    EXPECT_EQ(manuallyResizedSize, inner->sizes().at(0));
}

TEST(FySplitterTest, SaveStateRoundTripPreservesSizesButNotLocks)
{
    TestSplitter source{Qt::Horizontal};
    addWidgets(source, 3);
    showAndProcess(source, {600, 100});
    source.setSizes({150, 200, 250});
    ASSERT_TRUE(source.setLocked(0, true));
    const QList<int> expectedSizes = source.sizes();

    TestSplitter restored{Qt::Horizontal};
    addWidgets(restored, 3);
    showAndProcess(restored, {600, 100});

    ASSERT_TRUE(restored.restoreState(source.saveState()));
    const QList<int> restoredSizes = restored.sizes();
    ASSERT_EQ(expectedSizes.size(), restoredSizes.size());
    for(int i{0}; i < expectedSizes.size(); ++i) {
        EXPECT_NEAR(expectedSizes.at(i), restoredSizes.at(i), 2);
    }
    EXPECT_FALSE(restored.isLocked(0));
    EXPECT_FALSE(restored.isLocked(1));
    EXPECT_FALSE(restored.isLocked(2));
}

TEST(FySplitterTest, StateCanBeRestoredByQSplitter)
{
    TestSplitter source{Qt::Horizontal};
    addWidgets(source, 3);
    showAndProcess(source, {600, 100});
    source.setSizes({150, 200, 250});
    ASSERT_TRUE(source.setLocked(0, true));
    const QList<int> expectedSizes = source.sizes();

    QSplitter restored{Qt::Horizontal};
    for(int i{0}; i < 3; ++i) {
        auto* widget = new QWidget;
        restored.addWidget(widget);
        widget->show();
    }
    restored.resize(600, 100);
    restored.show();
    QApplication::processEvents();

    ASSERT_TRUE(restored.restoreState(source.saveState()));
    const QList<int> restoredSizes = restored.sizes();
    ASSERT_EQ(expectedSizes.size(), restoredSizes.size());
    for(int i{0}; i < expectedSizes.size(); ++i) {
        EXPECT_NEAR(expectedSizes.at(i), restoredSizes.at(i), 2);
    }
}

TEST(FySplitterTest, RestorePreservesSeparateLockAtSavedSize)
{
    TestSplitter source{Qt::Horizontal};
    addWidgets(source, 3);
    showAndProcess(source, {600, 100});
    source.setSizes({150, 200, 250});
    ASSERT_TRUE(source.setLocked(0, true));
    const int expectedLockedSize = source.sizes().at(0);
    const QByteArray state       = source.saveState();

    TestSplitter restored{Qt::Horizontal};
    addWidgets(restored, 3);
    showAndProcess(restored, {300, 100});
    ASSERT_TRUE(restored.setLocked(0, true));

    ASSERT_TRUE(restored.restoreState(state));
    EXPECT_EQ(expectedLockedSize, restored.sizes().at(0));
    EXPECT_TRUE(restored.isLocked(0));

    restored.resize(600, 100);
    QApplication::processEvents();

    EXPECT_TRUE(restored.isLocked(0));
    EXPECT_EQ(expectedLockedSize, restored.sizes().at(0));
}

TEST(FySplitterTest, LastUnlockedChildCannotBeLocked)
{
    TestSplitter splitter{Qt::Horizontal};
    addWidgets(splitter, 3);
    showAndProcess(splitter, {600, 100});

    EXPECT_TRUE(splitter.setLocked(0, true));
    EXPECT_TRUE(splitter.setLocked(1, true));
    EXPECT_FALSE(splitter.setLocked(2, true));
    EXPECT_FALSE(splitter.isLocked(2));

    EXPECT_TRUE(splitter.setLocked(0, false));
    EXPECT_TRUE(splitter.setLocked(2, true));
    EXPECT_TRUE(splitter.isLocked(2));
}

TEST(FySplitterTest, OrientationChangeRebasesLockedSize)
{
    TestSplitter splitter{Qt::Horizontal};
    addWidgets(splitter, 2);
    showAndProcess(splitter, {600, 400});
    splitter.setSizes({200, 400});
    ASSERT_TRUE(splitter.setLocked(0, true));

    splitter.setOrientation(Qt::Vertical);
    QApplication::processEvents();
    const int rebasedSize = splitter.sizes().at(0);

    splitter.resize(600, 600);
    QApplication::processEvents();
    EXPECT_EQ(rebasedSize, splitter.sizes().at(0));
}

TEST(FySplitterTest, HiddenLockedChildRecoversStoredSize)
{
    TestSplitter splitter{Qt::Horizontal};
    const QList<QWidget*> widgets = addWidgets(splitter, 3);
    showAndProcess(splitter, {600, 100});
    splitter.setSizes({150, 200, 250});
    ASSERT_TRUE(splitter.setLocked(0, true));
    const int lockedSize = splitter.sizes().at(0);

    widgets.at(0)->hide();
    QApplication::processEvents();
    splitter.resize(800, 100);
    QApplication::processEvents();
    widgets.at(0)->show();
    QApplication::processEvents();

    EXPECT_EQ(lockedSize, splitter.sizes().at(0));
}
} // namespace Fooyin::Testing

int main(int argc, char** argv)
{
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
#ifdef Q_OS_WIN
        qputenv("QT_QPA_PLATFORM", "windows");
#else
        qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    }

    const QApplication app{argc, argv};
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
