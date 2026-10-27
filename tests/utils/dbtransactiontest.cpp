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
 *
 */

#include <utils/database/dbtransaction.h>

#include <QCoreApplication>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <atomic>
#include <optional>
#include <vector>

using namespace Qt::StringLiterals;

namespace {
void ensureCoreApplication()
{
    if(QCoreApplication::instance()) {
        return;
    }

    static int argc{1};
    static char name[]  = "fooyin-dbtransaction-test";
    static char* argv[] = {name, nullptr};
    static QCoreApplication application{argc, argv};
}

void closeDatabase(QSqlDatabase& database)
{
    if(!database.isValid()) {
        return;
    }

    const auto name = database.connectionName();
    database.close();
    database = {};
    QSqlDatabase::removeDatabase(name);
}
} // namespace

namespace Fooyin::Testing {
class DbTransactionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ensureCoreApplication();

        static std::atomic_int nextConnection{0};
        m_connectionName = u"fooyin-dbtransaction-test-%1"_s.arg(nextConnection.fetch_add(1));
        m_database       = QSqlDatabase::addDatabase(u"QSQLITE"_s, m_connectionName);
        m_database.setDatabaseName(u":memory:"_s);
        ASSERT_TRUE(m_database.open()) << m_database.lastError().text().toStdString();
        ASSERT_TRUE(execute(m_database, u"CREATE TABLE TestValues (Value INTEGER PRIMARY KEY)"_s));
    }

    void TearDown() override
    {
        closeDatabase(m_otherDatabase);
        closeDatabase(m_database);
    }

    bool openOtherDatabase(const QString& filename = u":memory:"_s)
    {
        m_otherDatabase = QSqlDatabase::addDatabase(u"QSQLITE"_s, m_connectionName + u"-other"_s);
        m_otherDatabase.setDatabaseName(filename);
        return m_otherDatabase.open()
            && execute(m_otherDatabase, u"CREATE TABLE IF NOT EXISTS TestValues (Value INTEGER PRIMARY KEY)"_s);
    }

    bool execute(const QSqlDatabase& database, const QString& statement) const
    {
        QSqlQuery query{database};
        return query.exec(statement);
    }

    bool insert(int value)
    {
        return execute(m_database, u"INSERT INTO TestValues VALUES (%1)"_s.arg(value));
    }

    std::vector<int> values(const QSqlDatabase& database) const
    {
        QSqlQuery query{database};
        EXPECT_TRUE(query.exec(u"SELECT Value FROM TestValues ORDER BY Value"_s))
            << query.lastError().text().toStdString();

        std::vector<int> result;
        while(query.next()) {
            result.push_back(query.value(0).toInt());
        }
        EXPECT_FALSE(query.lastError().isValid()) << query.lastError().text().toStdString();
        return result;
    }

    QTemporaryDir m_tempDir;
    QString m_connectionName;
    QSqlDatabase m_database;
    QSqlDatabase m_otherDatabase;
};

TEST_F(DbTransactionTest, CommitPersistsOutermostScope)
{
    DbTransaction transaction{m_database};
    ASSERT_TRUE(transaction);
    EXPECT_FALSE(transaction.lastError().isValid());
    ASSERT_TRUE(insert(1));
    ASSERT_TRUE(transaction.commit());
    EXPECT_FALSE(transaction);
    EXPECT_FALSE(transaction.lastError().isValid());
    EXPECT_EQ(values(m_database), (std::vector{1}));
}

TEST_F(DbTransactionTest, DestructorRollsBackOutermostScope)
{
    {
        const DbTransaction transaction{m_database};
        ASSERT_TRUE(transaction);
        ASSERT_TRUE(insert(1));
    }

    EXPECT_TRUE(values(m_database).empty());
    ASSERT_TRUE(m_database.transaction());
    ASSERT_TRUE(m_database.rollback());
}

TEST_F(DbTransactionTest, ExplicitRollbackReturnsSuccessAndEndsScope)
{
    DbTransaction transaction{m_database};
    ASSERT_TRUE(transaction);
    ASSERT_TRUE(insert(1));
    ASSERT_TRUE(transaction.rollback());
    EXPECT_FALSE(transaction);
    EXPECT_FALSE(transaction.lastError().isValid());
    EXPECT_TRUE(values(m_database).empty());
}

TEST_F(DbTransactionTest, NestedCommitIsIncludedInOuterCommit)
{
    DbTransaction outer{m_database};
    ASSERT_TRUE(outer);
    ASSERT_TRUE(insert(1));
    {
        DbTransaction inner{m_database};
        ASSERT_TRUE(inner);
        ASSERT_TRUE(insert(2));
        ASSERT_TRUE(inner.commit());
        EXPECT_FALSE(inner);
    }
    ASSERT_TRUE(insert(3));
    ASSERT_TRUE(outer.commit());
    EXPECT_EQ(values(m_database), (std::vector{1, 2, 3}));
}

TEST_F(DbTransactionTest, OuterRollbackUndoesCommittedInnerScope)
{
    DbTransaction outer{m_database};
    ASSERT_TRUE(outer);
    ASSERT_TRUE(insert(1));
    {
        DbTransaction inner{m_database};
        ASSERT_TRUE(inner);
        ASSERT_TRUE(insert(2));
        ASSERT_TRUE(inner.commit());
    }
    ASSERT_TRUE(outer.rollback());
    EXPECT_TRUE(values(m_database).empty());
}

TEST_F(DbTransactionTest, OuterDestructorUndoesCommittedInnerScope)
{
    {
        const DbTransaction outer{m_database};
        ASSERT_TRUE(outer);
        ASSERT_TRUE(insert(1));
        DbTransaction inner{m_database};
        ASSERT_TRUE(inner);
        ASSERT_TRUE(insert(2));
        ASSERT_TRUE(inner.commit());
    }
    EXPECT_TRUE(values(m_database).empty());
}

TEST_F(DbTransactionTest, InnerRollbackPreservesOuterChanges)
{
    DbTransaction outer{m_database};
    ASSERT_TRUE(outer);
    ASSERT_TRUE(insert(1));
    {
        DbTransaction inner{m_database};
        ASSERT_TRUE(inner);
        ASSERT_TRUE(insert(2));
        ASSERT_TRUE(inner.rollback());
    }
    ASSERT_TRUE(insert(3));
    ASSERT_TRUE(outer.commit());
    EXPECT_EQ(values(m_database), (std::vector{1, 3}));
}

TEST_F(DbTransactionTest, InnerDestructorPreservesOuterChanges)
{
    DbTransaction outer{m_database};
    ASSERT_TRUE(outer);
    ASSERT_TRUE(insert(1));
    {
        const DbTransaction inner{m_database};
        ASSERT_TRUE(inner);
        ASSERT_TRUE(insert(2));
    }
    ASSERT_TRUE(insert(3));
    ASSERT_TRUE(outer.commit());
    EXPECT_EQ(values(m_database), (std::vector{1, 3}));
}

TEST_F(DbTransactionTest, MiddleRollbackUndoesItsCommittedChildAndAllowsSibling)
{
    DbTransaction outer{m_database};
    ASSERT_TRUE(outer);
    ASSERT_TRUE(insert(1));
    {
        DbTransaction middle{m_database};
        ASSERT_TRUE(middle);
        ASSERT_TRUE(insert(2));
        {
            DbTransaction inner{m_database};
            ASSERT_TRUE(inner);
            ASSERT_TRUE(insert(3));
            ASSERT_TRUE(inner.commit());
        }
        ASSERT_TRUE(middle.rollback());
    }
    {
        DbTransaction sibling{m_database};
        ASSERT_TRUE(sibling);
        ASSERT_TRUE(insert(4));
        ASSERT_TRUE(sibling.commit());
    }
    ASSERT_TRUE(outer.commit());
    EXPECT_EQ(values(m_database), (std::vector{1, 4}));
}

TEST_F(DbTransactionTest, MovingOuterScopeTransfersRollbackOwnership)
{
    std::optional<DbTransaction> moved;
    {
        DbTransaction original{m_database};
        ASSERT_TRUE(original);
        ASSERT_TRUE(insert(1));
        moved.emplace(std::move(original));
        EXPECT_FALSE(original);
        EXPECT_FALSE(original.commit());
        EXPECT_FALSE(original.rollback());
        EXPECT_EQ(original.lastError().type(), QSqlError::TransactionError);
        EXPECT_TRUE(*moved);
        EXPECT_FALSE(moved->lastError().isValid());
    }
    EXPECT_EQ(values(m_database), (std::vector{1}));
    moved.reset();
    EXPECT_TRUE(values(m_database).empty());
}

TEST_F(DbTransactionTest, MovingNestedScopePreservesItsSavepoint)
{
    DbTransaction outer{m_database};
    ASSERT_TRUE(outer);
    ASSERT_TRUE(insert(1));
    std::optional<DbTransaction> moved;
    {
        DbTransaction inner{m_database};
        ASSERT_TRUE(inner);
        ASSERT_TRUE(insert(2));
        moved.emplace(std::move(inner));
        EXPECT_FALSE(inner);
        EXPECT_FALSE(inner.commit());
        EXPECT_FALSE(inner.rollback());
        EXPECT_EQ(inner.lastError().type(), QSqlError::TransactionError);
        EXPECT_TRUE(*moved);
    }
    ASSERT_TRUE(moved->commit());
    moved.reset();
    ASSERT_TRUE(outer.rollback());
    EXPECT_TRUE(values(m_database).empty());
}

TEST_F(DbTransactionTest, ConnectionsKeepIndependentScopes)
{
    ASSERT_TRUE(openOtherDatabase());
    DbTransaction first{m_database};
    ASSERT_TRUE(first);
    ASSERT_TRUE(insert(1));
    {
        DbTransaction second{m_otherDatabase};
        ASSERT_TRUE(second);
        ASSERT_TRUE(execute(m_otherDatabase, u"INSERT INTO TestValues VALUES (2)"_s));
        {
            DbTransaction nestedFirst{m_database};
            ASSERT_TRUE(nestedFirst);
            ASSERT_TRUE(insert(3));
            ASSERT_TRUE(nestedFirst.commit());
        }
        ASSERT_TRUE(second.rollback());
    }
    ASSERT_TRUE(first.commit());
    EXPECT_EQ(values(m_database), (std::vector{1, 3}));
    EXPECT_TRUE(values(m_otherDatabase).empty());
}

TEST_F(DbTransactionTest, InnerCommitIsInvisibleToOtherConnectionUntilOuterCommit)
{
    ASSERT_TRUE(m_tempDir.isValid());
    m_database.close();
    const auto filename = m_tempDir.filePath(u"transactions.db"_s);
    m_database.setDatabaseName(filename);
    ASSERT_TRUE(m_database.open());
    ASSERT_TRUE(execute(m_database, u"CREATE TABLE TestValues (Value INTEGER PRIMARY KEY)"_s));
    ASSERT_TRUE(openOtherDatabase(filename));

    DbTransaction outer{m_database};
    ASSERT_TRUE(outer);
    ASSERT_TRUE(insert(1));
    {
        DbTransaction inner{m_database};
        ASSERT_TRUE(inner);
        ASSERT_TRUE(insert(2));
        ASSERT_TRUE(inner.commit());
    }
    EXPECT_TRUE(values(m_otherDatabase).empty());
    ASSERT_TRUE(outer.commit());
    EXPECT_EQ(values(m_otherDatabase), (std::vector{1, 2}));
}

TEST_F(DbTransactionTest, NestsInsideExistingTransaction)
{
    ASSERT_TRUE(m_database.transaction());
    ASSERT_TRUE(insert(1));
    {
        DbTransaction inner{m_database};
        ASSERT_TRUE(inner);
        ASSERT_TRUE(insert(2));
        ASSERT_TRUE(inner.commit());
    }
    ASSERT_TRUE(m_database.rollback());
    EXPECT_TRUE(values(m_database).empty());
}

TEST_F(DbTransactionTest, ClosedConnectionLeavesTransactionInactiveWithError)
{
    m_database.close();

    DbTransaction transaction{m_database};
    EXPECT_FALSE(transaction);
    EXPECT_EQ(transaction.lastError().type(), QSqlError::ConnectionError);
    EXPECT_FALSE(transaction.commit());
    EXPECT_FALSE(transaction.rollback());
}

TEST_F(DbTransactionTest, CommitFailureKeepsScopeAvailableForRollback)
{
    ASSERT_TRUE(execute(m_database, u"PRAGMA foreign_keys = ON"_s));
    ASSERT_TRUE(execute(m_database, u"CREATE TABLE Parent (Id INTEGER PRIMARY KEY)"_s));
    ASSERT_TRUE(execute(m_database, u"CREATE TABLE Child (ParentId INTEGER REFERENCES Parent(Id) "
                                    "DEFERRABLE INITIALLY DEFERRED)"_s));

    DbTransaction transaction{m_database};
    ASSERT_TRUE(transaction);
    ASSERT_TRUE(insert(1));
    ASSERT_TRUE(execute(m_database, u"INSERT INTO Child VALUES (99)"_s));
    EXPECT_FALSE(transaction.commit());
    EXPECT_TRUE(transaction);
    EXPECT_TRUE(transaction.lastError().isValid());
    ASSERT_TRUE(transaction.rollback());
    EXPECT_FALSE(transaction.lastError().isValid());
    EXPECT_TRUE(values(m_database).empty());
}

TEST_F(DbTransactionTest, DestructorRollsBackAfterCommitFailure)
{
    ASSERT_TRUE(execute(m_database, u"PRAGMA foreign_keys = ON"_s));
    ASSERT_TRUE(execute(m_database, u"CREATE TABLE Parent (Id INTEGER PRIMARY KEY)"_s));
    ASSERT_TRUE(execute(m_database, u"CREATE TABLE Child (ParentId INTEGER REFERENCES Parent(Id) "
                                    "DEFERRABLE INITIALLY DEFERRED)"_s));
    {
        DbTransaction transaction{m_database};
        ASSERT_TRUE(transaction);
        ASSERT_TRUE(insert(1));
        ASSERT_TRUE(execute(m_database, u"INSERT INTO Child VALUES (99)"_s));
        EXPECT_FALSE(transaction.commit());
    }
    EXPECT_TRUE(values(m_database).empty());
    ASSERT_TRUE(m_database.transaction());
    ASSERT_TRUE(m_database.rollback());
}

TEST_F(DbTransactionTest, FailedStatementOnlyRollsBackItsInnerScope)
{
    DbTransaction outer{m_database};
    ASSERT_TRUE(outer);
    ASSERT_TRUE(insert(1));
    {
        const DbTransaction inner{m_database};
        ASSERT_TRUE(inner);
        ASSERT_TRUE(insert(2));
        EXPECT_FALSE(insert(2));
    }
    ASSERT_TRUE(insert(3));
    ASSERT_TRUE(outer.commit());
    EXPECT_EQ(values(m_database), (std::vector{1, 3}));
}

TEST_F(DbTransactionTest, InactiveScopeCannotFinishAnotherTransaction)
{
    DbTransaction finished{m_database};
    ASSERT_TRUE(finished);
    ASSERT_TRUE(insert(1));
    ASSERT_TRUE(finished.commit());

    DbTransaction current{m_database};
    ASSERT_TRUE(current);
    ASSERT_TRUE(insert(2));
    EXPECT_FALSE(finished.commit());
    EXPECT_FALSE(finished.rollback());
    EXPECT_EQ(finished.lastError().type(), QSqlError::TransactionError);
    ASSERT_TRUE(current.rollback());
    EXPECT_EQ(values(m_database), (std::vector{1}));
}
} // namespace Fooyin::Testing
