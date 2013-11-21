/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya, María Ángeles Giménez
 *
 *  This file is part of STaRS.
 *
 *  STaRS is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  STaRS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with STaRS; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DATABASE_H_
#define DATABASE_H_

#include <map>
#include <sqlite3.h>
#include <stdexcept>
#include <sstream>
#include <boost/filesystem.hpp>


/**
 * \brief Interface with an SQLite3 database.
 *
 * This class provides an object-oriented, easy interface to an SQLite3 database. It can
 * manage transactions and cache queries.
 */
class Database {
    sqlite3 * db;
    std::map<std::string, sqlite3_stmt *> queryCache;
    friend class Query;

public:

    /**
     * \brief A query interface for SQLite3.
     *
     * This class provides an obejct-oriented proxy for persistent statements on an SQLite3 database.
     */
    class Query {
        sqlite3_stmt * statement;
        unsigned int nextCol;
        unsigned int nextPar;

        // Disable copy
        Query(const Query & copy) {}

    public:
        Query(Database & d, const std::string & sql);

        // Statement is reset on query destruction
        ~Query() {
            sqlite3_reset(statement);
        }

        // Set parameters, only works if query is reset

        Query & par(int64_t i) {
            sqlite3_bind_int64(statement, nextPar++, i);
            return *this;
        }

        Query & par(const std::string & s) {
            sqlite3_bind_text(statement, nextPar++, s.c_str(), -1, SQLITE_TRANSIENT);
            return *this;
        }

        // Get results

        bool fetchNextRow() {
            nextCol = 0;
            bool result = sqlite3_step(statement) == SQLITE_ROW;
            if (!result) reset();
            return result;
        }

        bool execute() {
            bool bad = sqlite3_step(statement) != SQLITE_DONE;
            reset();
            return !bad;
        }

        int64_t getInt() {
            return sqlite3_column_int64(statement, nextCol++);
        }

        std::string getStr() {
            const unsigned char *tmp = sqlite3_column_text(statement, nextCol++);
            return tmp ? std::string((const char *)tmp) : std::string("");
        }

        void reset() {
            sqlite3_reset(statement);
            nextPar = 1;
        }
    };

    Database() : db(NULL) {}
    ~Database() {
        close();
    }

    bool open(const boost::filesystem::path & dbFile);
    void close();
    int save(const boost::filesystem::path & dbFile);

    bool isOpen() const { return db != NULL; }

    sqlite3 * getDatabase() const {
        return db;
    }

    /**
     * Easy interface to execute an SQL query.
     * @param sql Query definition.
     */
    bool execute(std::string sql) {
        return db && !sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL);
    }

    void beginTransaction() {
        execute("BEGIN");
    }

    // A commit may fail
    void commitTransaction() {
        execute("COMMIT");
    }

    // A failing rollback is no-op
    void rollbackTransaction();

    int64_t getLastRowid();

    int getChangedRows();
};

#endif //DATABASE_H_
