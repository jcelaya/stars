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

#include "Database.hpp"
#include "Logger.hpp"
using namespace std;
using namespace boost;


bool Database::open(const boost::filesystem::path & dbFile) {
    if (sqlite3_open(dbFile.string().c_str(), &db) == SQLITE_OK) {
        // We usually want foreign key constraints
        execute("pragma foreign_keys = on");
        return true;
    } else {
        sqlite3_close(db);
        db = NULL;
        return false;
    }
}


void Database::close() {
    for (map<std::string, sqlite3_stmt *>::iterator it = queryCache.begin(); it != queryCache.end(); ++it)
        sqlite3_finalize(it->second);
    sqlite3_close(db);
}


Database::Query::Query(Database & d, const std::string & sql) : db(d), nextCol(0), nextPar(1) {
    map<std::string, sqlite3_stmt *>::iterator it = d.queryCache.find(sql);
    if (it == d.queryCache.end()) {
        const char * tmp;
        if (!sqlite3_prepare_v2(db.getDatabase(), sql.c_str(), -1, &statement, &tmp)) {
            d.queryCache[sql] = statement;
        }
    } else
        statement = it->second;
}


void Database::rollbackTransaction() {
    if (db) {
        // Reset all queries before calling rollback, otherwise it will fail
        if (sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL)) {
            LogMsg("Database", ERROR) << "Rollback failed!!";
        }
    }
}


long int Database::getLastRowid() {
    return sqlite3_last_insert_rowid(db);
}


int Database::getChangedRows() {
    return sqlite3_changes(db);
}
