/** Serialized SQLite C-API owner. Workers carry plain data, never VM/Qt objects. */
#pragma once
#include <AK/JsonObject.h>
#include <atomic>
#include <chrono>
#include <unordered_map>
struct sqlite3;
struct sqlite3_stmt;

namespace Mite::SQLite {
class Store {
public:
    ~Store();

    JsonObject execute(JsonObject const&);

    void stop()
    {
        m_stopped.store(true);
    }

private:
    struct Statement {
        sqlite3_stmt* value;
        int database;
    };

    JsonObject failure(sqlite3*, int);

    JsonObject query(sqlite3*, sqlite3_stmt*, JsonObject const&, bool persistent);

    int bind(sqlite3_stmt*, JsonValue const&);

    std::unordered_map<int, sqlite3*> m_databases;
    std::unordered_map<int, Statement> m_statements;
    int m_next_statement { 1 };
    std::atomic<bool> m_stopped {};
    std::chrono::steady_clock::time_point m_deadline;
};
}
