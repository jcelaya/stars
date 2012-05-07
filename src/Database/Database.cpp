/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2008 Javier Celaya
 *
 *  This file is part of PeerComp.
 *
 *  PeerComp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  PeerComp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with PeerComp; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "Database.hpp"
#include "Logger.hpp"
using namespace std;
using namespace boost;


Database::Database(const boost::filesystem::path & dbFile) : db(NULL) {
	LogMsg("Database", DEBUG) << "Opening database in " << dbFile;
	if (sqlite3_open(dbFile.string().c_str(), &db))
		throw Exception(*this) << "Error opening database";
	// We usually want foreign key constraints
	execute("pragma foreign_keys = on");
}


Database::Query::Query(Database & d, const std::string & sql) : db(d), nextCol(0), nextPar(1) {
	map<std::string, sqlite3_stmt *>::iterator it = d.queryCache.find(sql);
	if (it == d.queryCache.end()) {
		const char * tmp;
		if (sqlite3_prepare_v2(db.getDatabase(), sql.c_str(), -1, &statement, &tmp)) {
			throw Database::Exception(db) << "Unable to prepare query " << sql;
		} else {
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
