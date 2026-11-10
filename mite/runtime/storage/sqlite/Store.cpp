/** Execute real SQLite connections/statements on workers with bounded results and cancellation. */
#include "Store.h"
#include <AK/Base64.h>
#include <AK/JsonArray.h>
#include <AK/ScopeGuard.h>
#include <charconv>
#include <cmath>
#include <cstring>
#include <sqlite3.h>

static_assert(SQLITE_VERSION_NUMBER == 3053004, "Review the directly pinned SQLite version");

namespace Mite::SQLite {
namespace {
    StringView ctext(char const* value)
    {
        return { value, strlen(value) };
    }

    char const* code(int status)
    {
        switch (status & 0xff) {
        case SQLITE_ERROR:
            return "SQLITE_ERROR";
        case SQLITE_AUTH:
            return "SQLITE_AUTH";
        case SQLITE_NOMEM:
            return "SQLITE_NOMEM";
        case SQLITE_ABORT:
            return "SQLITE_ABORT";
        case SQLITE_SCHEMA:
            return "SQLITE_SCHEMA";
        case SQLITE_PERM:
            return "SQLITE_PERM";
        case SQLITE_MISUSE:
            return "SQLITE_MISUSE";
        case SQLITE_BUSY:
            return "SQLITE_BUSY";
        case SQLITE_LOCKED:
            return "SQLITE_LOCKED";
        case SQLITE_READONLY:
            return "SQLITE_READONLY";
        case SQLITE_INTERRUPT:
            return "SQLITE_INTERRUPT";
        case SQLITE_IOERR:
            return "SQLITE_IOERR";
        case SQLITE_CORRUPT:
            return "SQLITE_CORRUPT";
        case SQLITE_CANTOPEN:
            return "SQLITE_CANTOPEN";
        case SQLITE_CONSTRAINT:
            return "SQLITE_CONSTRAINT";
        case SQLITE_MISMATCH:
            return "SQLITE_MISMATCH";
        case SQLITE_RANGE:
            return "SQLITE_RANGE";
        case SQLITE_TOOBIG:
            return "SQLITE_TOOBIG";
        case SQLITE_NOTADB:
            return "SQLITE_NOTADB";
        case SQLITE_FULL:
            return "SQLITE_FULL";
        default:
            return "SQLITE_MISUSE";
        }
    }

    JsonObject success()
    {
        JsonObject value;
        value.set("ok"sv, true);
        return value;
    }
}

Store::~Store()
{
    for (auto const& [id, statement] : m_statements) {
        (void)id;
        sqlite3_finalize(statement.value);
    }

    for (auto const& [id, database] : m_databases) {
        (void)id;
        sqlite3_close_v2(database);
    }
}

JsonObject Store::failure(sqlite3* database, int status)
{
    JsonObject result;
    result.set("ok"sv, false);
    result.set("code"sv, ctext(code(status)));
    result.set("errno"sv, status & 0xff);
    result.set("message"sv,
        MUST(String::from_utf8(
            ctext(database ? sqlite3_errmsg(database) : sqlite3_errstr(status)))));
    return result;
}

int Store::bind(sqlite3_stmt* statement, JsonValue const& parameters)
{
    if (!parameters.is_array() || parameters.as_array().size() > 1024)
        return SQLITE_RANGE;
    sqlite3_reset(statement);
    sqlite3_clear_bindings(statement);
    auto const& values = parameters.as_array().values();
    for (size_t i = 0; i < values.size(); ++i) {
        auto const& value = values[i];
        int status;
        if (value.is_null())
            status = sqlite3_bind_null(statement, i + 1);
        else if (value.is_string())
            status = sqlite3_bind_text(statement, i + 1,
                value.as_string().is_empty() ? ""
                                             : value.as_string()
                                                   .bytes_as_string_view()
                                                   .characters_without_null_termination(),
                value.as_string().byte_count(), SQLITE_TRANSIENT);
        else if (value.is_number()) {
            double number = *value.get_double_with_precision_loss();
            if (!std::isfinite(number))
                return SQLITE_MISMATCH;
            if (std::floor(number) == number && std::abs(number) <= 9007199254740991.0)
                status = sqlite3_bind_int64(statement, i + 1, static_cast<sqlite3_int64>(number));
            else
                status = sqlite3_bind_double(statement, i + 1, number);
        } else if (value.is_object() && value.as_object().size() == 1
            && value.as_object().get_string("blob"sv).has_value()) {
            auto decoded
                = decode_base64(value.as_object().get_string("blob"sv)->bytes_as_string_view(),
                    AK::LastChunkHandling::Strict);
            if (decoded.is_error())
                return SQLITE_MISMATCH;
            status = decoded.value().is_empty()
                ? sqlite3_bind_zeroblob(statement, i + 1, 0)
                : sqlite3_bind_blob(statement, i + 1, decoded.value().data(),
                      decoded.value().size(), SQLITE_TRANSIENT);
        } else
            return SQLITE_MISMATCH;
        if (status != SQLITE_OK)
            return status;
    }

    return SQLITE_OK;
}

JsonObject Store::query(
    sqlite3* database, sqlite3_stmt* statement, JsonObject const& input, bool persistent)
{
    ScopeGuard finalize = [&] {
        if (persistent)
            sqlite3_reset(statement);
        else
            sqlite3_finalize(statement);
    };

    auto parameters = input.get("params"sv);
    auto status = bind(statement, parameters.value_or(JsonArray {}));
    if (status != SQLITE_OK)
        return failure(database, status);
    auto mode = input.get_string("mode"sv).value_or("run"_string);
    if (mode != "run"sv && mode != "get"sv && mode != "all"sv)
        return failure(database, SQLITE_MISUSE);
    JsonArray rows;
    size_t budget = 0;
    while ((status = sqlite3_step(statement)) == SQLITE_ROW) {
        if (mode == "run"sv)
            continue;
        JsonObject row;
        int count = sqlite3_column_count(statement);
        for (int column = 0; column < count; ++column) {
            auto name = String::from_utf8(ctext(sqlite3_column_name(statement, column)));
            if (name.is_error())
                return failure(database, SQLITE_MISMATCH);
            JsonValue value;
            switch (sqlite3_column_type(statement, column)) {
            case SQLITE_NULL:
                break;
            case SQLITE_INTEGER:
                value = static_cast<double>(sqlite3_column_int64(statement, column));
                break;
            case SQLITE_FLOAT:
                value = sqlite3_column_double(statement, column);
                break;
            case SQLITE_BLOB: {
                auto bytes = ReadonlyBytes { static_cast<u8 const*>(
                                                 sqlite3_column_blob(statement, column)),
                    static_cast<size_t>(sqlite3_column_bytes(statement, column)) };
                JsonObject blob;
                blob.set("blob"sv, MUST(encode_base64(bytes)));
                value = move(blob);
                break;
            }

            case SQLITE_TEXT: {
                auto text = String::from_utf8(StringView {
                    reinterpret_cast<char const*>(sqlite3_column_text(statement, column)),
                    static_cast<size_t>(sqlite3_column_bytes(statement, column)) });
                if (text.is_error())
                    return failure(database, SQLITE_MISMATCH);
                value = text.release_value();
                break;
            }

            default:
                return failure(database, SQLITE_MISMATCH);
            }

            row.set(name.release_value(), move(value));
        }

        budget += row.serialized().byte_count();
        if (budget > 8 * 1024 * 1024 || rows.size() >= 100000)
            return failure(database, SQLITE_TOOBIG);
        MUST(rows.append(move(row)));
        if (mode == "get"sv)
            break;
    }

    if (status != SQLITE_DONE && status != SQLITE_ROW)
        return failure(database, status);
    auto result = success();
    result.set("rows"sv, move(rows));
    result.set("lastID"sv, static_cast<double>(sqlite3_last_insert_rowid(database)));
    result.set("changes"sv, sqlite3_changes(database));
    return result;
}

JsonObject Store::execute(JsonObject const& input)
{
    if (m_stopped.load())
        return failure(nullptr, SQLITE_INTERRUPT);
    auto operation = input.get_string("op"sv).value_or(String {});
    int id = input.get_i32("db"sv).value_or(0);
    auto sql = input.get_string("sql"sv).value_or(String {});
    if (operation == "open"sv) {
        if (m_databases.size() >= 32 || m_databases.contains(id))
            return failure(nullptr, SQLITE_TOOBIG);
        auto filename = input.get_string("path"sv).value_or(String {});
        auto flags = input.get_i32("flags"sv).value_or(SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
        if (filename.is_empty() || filename.contains('\0') || filename.byte_count() > 32768
            || (flags != SQLITE_OPEN_READONLY && flags != SQLITE_OPEN_READWRITE
                && flags != (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)))
            return failure(nullptr, SQLITE_MISUSE);
        sqlite3* database = nullptr;
        auto status = sqlite3_open_v2(filename.to_byte_string().characters(), &database,
            flags | SQLITE_OPEN_FULLMUTEX, nullptr);
        if (status != SQLITE_OK) {
            auto result = failure(database, status);
            if (database)
                sqlite3_close_v2(database);
            return result;
        }
        // A script PRAGMA must not turn lock contention into an unbounded
        // worker/VM shutdown delay. Normal Code-OSS timeout values are retained.
        sqlite3_set_authorizer(
            database,
            [](void*, int action, char const* first, char const* second, char const*, char const*) {
                if (action != SQLITE_PRAGMA || !first || !second
                    || sqlite3_stricmp(first, "busy_timeout") != 0)
                    return SQLITE_OK;
                int value = 0;
                auto end = second + strlen(second);
                auto parsed = std::from_chars(second, end, value);
                return parsed.ec == std::errc {} && parsed.ptr == end && value >= 0
                        && value <= 10000
                    ? SQLITE_OK
                    : SQLITE_DENY;
            },
            nullptr);
        sqlite3_limit(database, SQLITE_LIMIT_LENGTH, 8 * 1024 * 1024);
        sqlite3_limit(database, SQLITE_LIMIT_SQL_LENGTH, 1024 * 1024);
        sqlite3_limit(database, SQLITE_LIMIT_COLUMN, 512);
        sqlite3_limit(database, SQLITE_LIMIT_VARIABLE_NUMBER, 1024);
        m_databases.emplace(id, database);
        auto result = success();
        result.set("db"sv, id);
        result.set("version"sv, ctext(sqlite3_libversion()));
        return result;
    }

    auto found = m_databases.find(id);
    if (found == m_databases.end())
        return failure(nullptr, SQLITE_MISUSE);
    auto* database = found->second;
    m_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    sqlite3_progress_handler(
        database, 1000,
        [](void* data) {
            auto& store = *static_cast<Store*>(data);
            return store.m_stopped.load() || std::chrono::steady_clock::now() >= store.m_deadline
                ? 1
                : 0;
        },
        this);
    if (operation == "dispose"sv) {
        for (auto entry = m_statements.begin(); entry != m_statements.end();) {
            if (entry->second.database == id) {
                sqlite3_finalize(entry->second.value);
                entry = m_statements.erase(entry);
            } else
                ++entry;
        }
    }

    if (operation == "close"sv || operation == "dispose"sv) {
        auto status = sqlite3_close(database);
        if (status != SQLITE_OK)
            return failure(database, status);
        m_databases.erase(found);
        return success();
    }

    if (operation == "finalize"sv || operation == "statement"sv) {
        auto statement_id = input.get_i32("statement"sv).value_or(0);
        auto entry = m_statements.find(statement_id);
        if (entry == m_statements.end() || entry->second.database != id)
            return failure(database, SQLITE_MISUSE);
        if (operation == "statement"sv)
            return query(database, entry->second.value, input, true);
        auto status = sqlite3_finalize(entry->second.value);
        m_statements.erase(entry);
        return status == SQLITE_OK ? success() : failure(database, status);
    }

    if (sql.is_empty() || sql.contains('\0') || sql.byte_count() > 1024 * 1024)
        return failure(database, SQLITE_TOOBIG);
    if (operation == "exec"sv) {
        auto status
            = sqlite3_exec(database, sql.to_byte_string().characters(), nullptr, nullptr, nullptr);
        return status == SQLITE_OK ? success() : failure(database, status);
    }

    if (operation != "prepare"sv && operation != "query"sv)
        return failure(database, SQLITE_MISUSE);
    auto bytes = sql.to_byte_string();
    sqlite3_stmt* statement = nullptr;
    char const* tail = nullptr;
    auto status
        = sqlite3_prepare_v2(database, bytes.characters(), bytes.length(), &statement, &tail);
    if (status != SQLITE_OK)
        return failure(database, status);
    if (!statement)
        return failure(database, SQLITE_MISUSE);
    // sqlite3's first-statement semantics are preserved, as in node-sqlite3.
    if (operation == "query"sv)
        return query(database, statement, input, false);
    if (m_statements.size() >= 512 || m_next_statement == 2147483647) {
        sqlite3_finalize(statement);
        return failure(database, SQLITE_TOOBIG);
    }

    auto statement_id = m_next_statement++;
    m_statements.emplace(statement_id, Statement { statement, id });
    auto result = success();
    result.set("statement"sv, statement_id);
    result.set("db"sv, id);
    return result;
}
}
