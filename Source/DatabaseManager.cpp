#include "DatabaseManager.h"

DatabaseManager::DatabaseManager() = default;

DatabaseManager::~DatabaseManager()
{
    if (mDatabase)
        sqlite3_close(mDatabase);
}

bool DatabaseManager::ConstructDatabase(const String& path)
{
    int error = sqlite3_open(path.getCharPointer(), &mDatabase);

    if (error)
    {
        LOGC(sqlite3_errmsg(mDatabase), ": ", path);
        sqlite3_close(mDatabase);

        return false;
    }
    else
        LOGC("Opened database successfully: ", path);

    char* errMsg = nullptr;

    const char* createSampleTimesTableSql = 
        "CREATE TABLE IF NOT EXISTS SampleTimes ("
        "BaseSampleNumber   INT      NOT NULL, "
        "Timestamp          DOUBLE   NOT NULL);";

    error = sqlite3_exec(mDatabase, createSampleTimesTableSql, nullptr, 0, &errMsg);

    if (error != SQLITE_OK) 
    {
        LOGC(errMsg);
        sqlite3_free(errMsg);

        return false;
    } 
    else 
        LOGC("SampleTimes Table created successfully: ", createSampleTimesTableSql);

    const char* createAnnotationsTableSql =
        "CREATE TABLE IF NOT EXISTS Annotations ("
        "Timestamp      DOUBLE  NOT NULL, "
        "Duration       DOUBLE  NOT NULL, "
        "DurationInt    INT     NOT NULL, "
        "EventType      INT     NOT NULL, "
        "Annotation     TEXT    NOT NULL);";

    error = sqlite3_exec(mDatabase, createAnnotationsTableSql, nullptr, 0, &errMsg);

    if (error != SQLITE_OK) 
    {
        LOGC(errMsg);
        sqlite3_free(errMsg);

        return false;
    } 
    else 
        LOGC("Annotations Table created successfully: ", createAnnotationsTableSql);

    return true;
}

void DatabaseManager::InsertIntoSampleTimesTable(int64 baseSampleNumber, double timestamp)
{
    const char* insertSampleTimeSql = 
        "INSERT INTO SampleTimes (BaseSampleNumber, Timestamp) "
        "VALUES (?, ?);";

    sqlite3_stmt* stmt;
    int error = sqlite3_prepare_v2(mDatabase, insertSampleTimeSql, -1, &stmt, nullptr);

    if (error != SQLITE_OK)
        LOGC("Failed to prepare statement: ", insertSampleTimeSql);

    sqlite3_bind_int(stmt, 1, baseSampleNumber);
    sqlite3_bind_double(stmt, 2, timestamp);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        LOGC("Failed to insert data: (", baseSampleNumber, ", ", timestamp, ")");

    sqlite3_finalize(stmt);
}

void DatabaseManager::InsertIntoAnnotationsTable(double timestamp, double duration, int durationInt, int eventType, const char* comment)
{
    const char* insertAnnotationSql = 
        "INSERT INTO Annotations (Timestamp, Duration, DurationInt, EventType, Annotation) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt;
    int error = sqlite3_prepare_v2(mDatabase, insertAnnotationSql, -1, &stmt, nullptr);

    if (error != SQLITE_OK)
        LOGC("Failed to prepare statement: ", insertAnnotationSql);

    if (timestamp < 0)
        timestamp = 0;

    sqlite3_bind_double(stmt, 1, timestamp);
    sqlite3_bind_double(stmt, 2, duration);
    sqlite3_bind_int(stmt, 3, durationInt);
    sqlite3_bind_int(stmt, 4, eventType);
    sqlite3_bind_text(stmt, 5, comment, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        LOGC("Failed to insert data: (", timestamp, ", ", duration, ", ", durationInt, ", ", eventType, ", ", comment, ")");

    sqlite3_finalize(stmt);
}

Array<Annotation> DatabaseManager::GetAnnotationsFromDatabase() const
{
    Array<Annotation> annotations;

    const char* selectAnnotationsSql = "SELECT * FROM Annotations;";
    sqlite3_stmt* stmt;
    int error = sqlite3_prepare_v2(mDatabase, selectAnnotationsSql, -1, &stmt, nullptr);

    if (error != SQLITE_OK)
        LOGC("Failed to prepare statement: ", selectAnnotationsSql);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        double timestamp = sqlite3_column_double(stmt, 0);
        double duration = sqlite3_column_double(stmt, 1);
        int durationInt = sqlite3_column_int(stmt, 2);
        int eventType = sqlite3_column_int(stmt, 3);
        const char* comment = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        annotations.add(Annotation(timestamp, duration, durationInt, eventType, comment));
    }

    sqlite3_finalize(stmt);

    return annotations;
}

void DatabaseManager::WriteSampleTimesFromDatabaseToLayoutFile(int writeChannel, 
    Array<unsigned int>& fileIndexes, 
    OwnedArray<FileOutputStream>& layoutFiles)
{
    const char* selectSampleTimesSql = "SELECT * FROM SampleTimes;";
    sqlite3_stmt* stmt;
    int error = sqlite3_prepare_v2(mDatabase, selectSampleTimesSql, -1, &stmt, nullptr);

    if (error != SQLITE_OK)
        LOGC("Failed to prepare statement: ", selectSampleTimesSql);

    int fileIndex = fileIndexes[writeChannel];

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int baseSampleNumber = sqlite3_column_int(stmt, 0);
        double timestamp = sqlite3_column_double(stmt, 1);

        layoutFiles[fileIndex]->writeText(String(baseSampleNumber) + 
            String("=") + 
            String(timestamp) + 
            String("\n"), 
            false, 
            false, 
            nullptr);
    }

    sqlite3_finalize(stmt);
}

void DatabaseManager::WriteAnnotationsFromDatabaseToLayoutFile(int writeChannel,
    Array<unsigned int>& fileIndexes, 
    OwnedArray<FileOutputStream>& layoutFiles)
{
    int fileIndex = fileIndexes[writeChannel];
    layoutFiles[fileIndex]->writeText("[Comments]\n", false, false, nullptr);

    const char* selectAnnotationsSql = "SELECT * FROM Annotations;";
    sqlite3_stmt* stmt;
    int error = sqlite3_prepare_v2(mDatabase, selectAnnotationsSql, -1, &stmt, nullptr);

    if (error != SQLITE_OK)
        LOGC("Failed to prepare statement: ", selectAnnotationsSql);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        double timestamp = sqlite3_column_double(stmt, 0);
        double duration = sqlite3_column_double(stmt, 1);
        int durationInt = sqlite3_column_int(stmt, 2);
        int bufferSize = sqlite3_column_int(stmt, 3);
        const char* comment = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        layoutFiles[fileIndex]->writeText(String(timestamp) + "," +
            String(duration) + "," +
            String(durationInt) + "," +
            String(bufferSize) + "," +
            String(comment) +
            String("\n"),
            false,
            false,
            nullptr);
    }

    sqlite3_finalize(stmt);
}
