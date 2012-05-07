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
	 * \brief Failed database access exception.
	 *
	 * This exception is thrown when a database access is not correctly performed
	 */
	class Exception : public std::exception {
		std::ostringstream msg;

	public:
		/// Default constructor, copies SQLite error msg.
		Exception(const Database & db) throw () {
			msg << sqlite3_errmsg(db.getDatabase()) << ". ";
		}
		/// Copy constructor, just copies msg.
		Exception(const Exception & copy) throw () { msg << copy.msg.str(); }
		/// Destructor, does nothing.
		~Exception() throw () {}

		/**
		 * Returns the message of this exception.
		 * @return The message stored in msg.
		 */
		const char * what() const throw () { return msg.str().c_str(); }

		/**
		 * Adds information to the message of the exception.
		 * @param o New information to be appended to the message.
		 * @return Reference to this object, to concatenate many calls to operator<<.
		 */
		template<class T> Exception & operator<<(const T & o) {
			msg << o;
			return *this;
		}
	};

	/**
	 * \brief A query interface for SQLite3.
	 *
	 * This class provides an obejct-oriented proxy for persistent statements on an SQLite3 database.
	 */
	class Query {
		const Database & db;
		sqlite3_stmt * statement;
		unsigned int nextCol;
		unsigned int nextPar;

		// Disable copy
		Query(const Query & copy) : db(copy.db) {}

	public:
		Query(Database & d, const std::string & sql);

		// Statement is reset on query destruction
		~Query() { sqlite3_reset(statement); }

		// Set parameters, only works if query is reset

		Query & par(long int i) {
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

		void execute() {
			bool bad = sqlite3_step(statement) != SQLITE_DONE;
			reset();
			if (bad) throw Database::Exception(db) << "Failed executing " << sqlite3_sql(statement);
		}

		sqlite3_int64 getInt() { return sqlite3_column_int64(statement, nextCol++); }

		std::string getStr() {
			const unsigned char *tmp = sqlite3_column_text(statement, nextCol++);
			return tmp ? std::string((const char *)tmp) : std::string("");
		}

		void reset() { sqlite3_reset(statement); nextPar = 1; }
	};

	Database(const boost::filesystem::path & dbFile);

	~Database() throw () {
		sqlite3_close(db);
	}

	sqlite3 * getDatabase() const { return db; }

	/**
	 * Easy interface to execute an SQL query.
	 * @param sql Query definition.
	 */
	void execute(std::string sql) {
		if (!db || sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL))
			throw Exception(*this) << "Failed executing " << sql;
	}

	void beginTransaction() { execute("BEGIN"); }

	// A commit may fail
	void commitTransaction() { execute("COMMIT"); }

	// A failing rollback is no-op, so no exception is thrown
	void rollbackTransaction();

	long int getLastRowid();

	int getChangedRows();
};

#endif //DATABASE_H_
