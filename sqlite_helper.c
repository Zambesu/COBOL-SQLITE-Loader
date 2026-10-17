#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
typedef void (__cdecl *sqlite3_destructor_type)(void *);

typedef int (__cdecl *sqlite3_open_fn)(const char *, sqlite3 **);
typedef int (__cdecl *sqlite3_close_fn)(sqlite3 *);
typedef int (__cdecl *sqlite3_exec_fn)(sqlite3 *, const char *, int (__cdecl *)(void *, int, char **, char **), void *, char **);
typedef int (__cdecl *sqlite3_prepare_v2_fn)(sqlite3 *, const char *, int, sqlite3_stmt **, const char **);
typedef int (__cdecl *sqlite3_bind_text_fn)(sqlite3_stmt *, int, const char *, int, sqlite3_destructor_type);
typedef int (__cdecl *sqlite3_step_fn)(sqlite3_stmt *);
typedef int (__cdecl *sqlite3_reset_fn)(sqlite3_stmt *);
typedef int (__cdecl *sqlite3_finalize_fn)(sqlite3_stmt *);
typedef const char *(__cdecl *sqlite3_errmsg_fn)(sqlite3 *);
typedef void (__cdecl *sqlite3_free_fn)(void *);

static HMODULE sqlite_module;
static sqlite3 *database;
static sqlite3_stmt *insert_statement;
static char last_error[512] = "";

static sqlite3_open_fn sqlite_open;
static sqlite3_close_fn sqlite_close;
static sqlite3_exec_fn sqlite_exec;
static sqlite3_prepare_v2_fn sqlite_prepare_v2;
static sqlite3_bind_text_fn sqlite_bind_text;
static sqlite3_step_fn sqlite_step;
static sqlite3_reset_fn sqlite_reset;
static sqlite3_finalize_fn sqlite_finalize;
static sqlite3_errmsg_fn sqlite_errmsg;
static sqlite3_free_fn sqlite_free;

static void set_error(const char *message)
{
    if (message == NULL) {
        message = "Erro SQLite sem descricao.";
    }
    strncpy(last_error, message, sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
}

static void set_sqlite_error(void)
{
    set_error(database != NULL ? sqlite_errmsg(database) : NULL);
}

static int load_sqlite(void)
{
    char system_directory[MAX_PATH];
    UINT directory_length = GetSystemDirectoryA(system_directory, MAX_PATH);
    FARPROC symbol;

    if (directory_length == 0 || directory_length >= MAX_PATH - 20) {
        set_error("Nao foi possivel localizar a pasta do sistema Windows.");
        return 1;
    }
    strcat(system_directory, "\\winsqlite3.dll");
    sqlite_module = LoadLibraryA(system_directory);
    if (sqlite_module == NULL) {
        set_error("Nao foi possivel carregar winsqlite3.dll.");
        return 1;
    }

#define LOAD_SQLITE_FUNCTION(target, name) \
    do { \
        symbol = GetProcAddress(sqlite_module, name); \
        if (symbol == NULL) { \
            set_error("A DLL do sistema nao expoe uma funcao SQLite necessaria."); \
            FreeLibrary(sqlite_module); \
            sqlite_module = NULL; \
            return 1; \
        } \
        memcpy(&(target), &symbol, sizeof(target)); \
    } while (0)

    LOAD_SQLITE_FUNCTION(sqlite_open, "sqlite3_open");
    LOAD_SQLITE_FUNCTION(sqlite_close, "sqlite3_close");
    LOAD_SQLITE_FUNCTION(sqlite_exec, "sqlite3_exec");
    LOAD_SQLITE_FUNCTION(sqlite_prepare_v2, "sqlite3_prepare_v2");
    LOAD_SQLITE_FUNCTION(sqlite_bind_text, "sqlite3_bind_text");
    LOAD_SQLITE_FUNCTION(sqlite_step, "sqlite3_step");
    LOAD_SQLITE_FUNCTION(sqlite_reset, "sqlite3_reset");
    LOAD_SQLITE_FUNCTION(sqlite_finalize, "sqlite3_finalize");
    LOAD_SQLITE_FUNCTION(sqlite_errmsg, "sqlite3_errmsg");
    LOAD_SQLITE_FUNCTION(sqlite_free, "sqlite3_free");
#undef LOAD_SQLITE_FUNCTION

    return 0;
}

static int execute_sql(const char *sql)
{
    char *error_message = NULL;
    int result = sqlite_exec(database, sql, NULL, NULL, &error_message);
    if (result != 0) {
        set_error(error_message != NULL ? error_message : sqlite_errmsg(database));
        if (error_message != NULL) {
            sqlite_free(error_message);
        }
    }
    return result;
}

int __cdecl LLDB_OPEN(const char *path, int path_length)
{
    char database_path[261];
    int result;

    last_error[0] = '\0';
    if (path_length <= 0 || path_length >= (int)sizeof(database_path)) {
        set_error("O caminho do banco de dados e invalido ou longo demais.");
        return 1;
    }
    memcpy(database_path, path, (size_t)path_length);
    database_path[path_length] = '\0';

    if (load_sqlite() != 0) {
        return 1;
    }
    result = sqlite_open(database_path, &database);
    if (result != 0) {
        set_sqlite_error();
        return result;
    }
    if (execute_sql("CREATE TABLE IF NOT EXISTS records ("
                    "id INTEGER PRIMARY KEY, content TEXT NOT NULL)") != 0 ||
        execute_sql("BEGIN IMMEDIATE") != 0 ||
        sqlite_prepare_v2(database,
                          "INSERT INTO records (content) VALUES (?)",
                          -1, &insert_statement, NULL) != 0) {
        set_sqlite_error();
        return 1;
    }
    return 0;
}

int __cdecl LLDB_INSERT(const char *record, int record_length)
{
    int result;
    if (insert_statement == NULL || record_length < 0) {
        set_error("A insercao nao foi inicializada corretamente.");
        return 1;
    }
    result = sqlite_bind_text(insert_statement, 1, record, record_length,
                              (sqlite3_destructor_type)-1);
    if (result == 0) {
        result = sqlite_step(insert_statement);
    }
    if (result != 101) {
        set_sqlite_error();
        return result != 0 ? result : 1;
    }
    result = sqlite_reset(insert_statement);
    if (result != 0) {
        set_sqlite_error();
    }
    return result;
}

int __cdecl LLDB_FINISH(void)
{
    int result = 0;
    if (insert_statement != NULL) {
        sqlite_finalize(insert_statement);
        insert_statement = NULL;
    }
    if (database != NULL) {
        result = execute_sql("COMMIT");
        if (result != 0) {
            execute_sql("ROLLBACK");
        }
        sqlite_close(database);
        database = NULL;
    }
    if (sqlite_module != NULL) {
        FreeLibrary(sqlite_module);
        sqlite_module = NULL;
    }
    return result;
}

void __cdecl LLDB_ABORT(void)
{
    if (insert_statement != NULL) {
        sqlite_finalize(insert_statement);
        insert_statement = NULL;
    }
    if (database != NULL) {
        sqlite_exec(database, "ROLLBACK", NULL, NULL, NULL);
        sqlite_close(database);
        database = NULL;
    }
    if (sqlite_module != NULL) {
        FreeLibrary(sqlite_module);
        sqlite_module = NULL;
    }
}

void __cdecl LLDB_SHOW_ERROR(void)
{
    fprintf(stderr, "%s\n", last_error[0] != '\0' ? last_error :
            "Falha inesperada na integracao SQLite.");
}