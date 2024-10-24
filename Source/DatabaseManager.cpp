#include "DatabaseManager.h"

DatabaseManager::DatabaseManager() = default;

DatabaseManager::~DatabaseManager()
{
    CloseDatabase();
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
        LOGC("SampleTimes table created successfully: ", createSampleTimesTableSql);

    const char* createCommentsTableSql =
        "CREATE TABLE IF NOT EXISTS Comments ("
        "Timestamp      DOUBLE  NOT NULL, "
        "Duration       DOUBLE  NOT NULL, "
        "DurationInt    INT     NOT NULL, "
        "EventType      INT     NOT NULL, "
        "Text           TEXT    NOT NULL);";

    error = sqlite3_exec(mDatabase, createCommentsTableSql, nullptr, 0, &errMsg);

    if (error != SQLITE_OK) 
    {
        LOGC(errMsg);
        sqlite3_free(errMsg);

        return false;
    } 
    else 
        LOGC("Comments table created successfully: ", createCommentsTableSql);

    const char* createPatientTableSql =
        "CREATE TABLE IF NOT EXISTS Patient ("
        "FirstName      TEXT    NOT NULL,"
        "MiddleName     TEXT    NOT NULL,"
        "LastName       TEXT    NOT NULL,"
        "PatientId      TEXT    NOT NULL,"
        "Gender         TEXT    NOT NULL,"
        "Hand           TEXT    NOT NULL,"
        "DateOfBirth    TEXT    NOT NULL,"
        "Physician      TEXT    NOT NULL,"
        "Technician     TEXT    NOT NULL,"
        "Medications    TEXT    NOT NULL,"
        "History        TEXT    NOT NULL,"
        "Extra1         TEXT    NOT NULL,"
        "Extra2         TEXT    NOT NULL,"
        "TestDate       TEXT    NOT NULL,"
        "TestTime       TEXT    NOT NULL);";

    error = sqlite3_exec(mDatabase, createPatientTableSql, nullptr, 0, &errMsg);

    if (error != SQLITE_OK) 
    {
        LOGC(errMsg);
        sqlite3_free(errMsg);

        return false;
    } 
    else 
        LOGC("Patient table created successfully: ", createPatientTableSql);

    const char* channelsTableSql =
        "CREATE TABLE IF NOT EXISTS Channels ("
        "ChannelName    TEXT    NOT NULL,"
        "ChannelNumber  INT     NOT NULL);";

    error = sqlite3_exec(mDatabase, channelsTableSql, nullptr, 0, &errMsg);

    if (error != SQLITE_OK) 
    {
        LOGC(errMsg);
        sqlite3_free(errMsg);

        return false;
    } 
    else 
        LOGC("Channels table created successfully: ", channelsTableSql);

    const char* fileInfoTableSql =
        "CREATE TABLE IF NOT EXISTS FileInfo ("
        "File           TEXT    NOT NULL,"
        "WaveformCount  INT     NOT NULL,"
        "SamplingRate   DOUBLE  NOT NULL,"
        "Calibration    DOUBLE  NOT NULL,"
        "FileType       TEXT    NOT NULL,"
        "DataType       INT     NOT NULL);";

    error = sqlite3_exec(mDatabase, fileInfoTableSql, nullptr, 0, &errMsg);

    if (error != SQLITE_OK) 
    {
        LOGC(errMsg);
        sqlite3_free(errMsg);

        return false;
    } 
    else 
        LOGC("FileInfo table created successfully: ", fileInfoTableSql);

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

void DatabaseManager::InsertIntoCommentsTable(double timestamp, double duration, int durationInt, int eventType, const char* text)
{
    const char* insertAnnotationSql = 
        "INSERT INTO Comments (Timestamp, Duration, DurationInt, EventType, Text) "
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
    sqlite3_bind_text(stmt, 5, text, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        LOGC("Failed to insert data: (", timestamp, ", ", duration, ", ", durationInt, ", ", eventType, ", ", text, ")")
    else
        LOGC("Inserted data: (", timestamp, ", ", duration, ", ", durationInt, ", ", eventType, ", ", text, ")");

    sqlite3_finalize(stmt);
}

Array<Comment> DatabaseManager::GetCommentsFromDatabase() const
{
    Array<Comment> comments;

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
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        comments.add(Comment(timestamp, duration, durationInt, eventType, text));
    }

    sqlite3_finalize(stmt);

    return comments;
}

void DatabaseManager::WriteSampleTimesFromDatabaseToLayoutFile(FileOutputStream* layFile)
{
    const char* selectSampleTimesSql = "SELECT * FROM SampleTimes;";
    sqlite3_stmt* stmt;
    int error = sqlite3_prepare_v2(mDatabase, selectSampleTimesSql, -1, &stmt, nullptr);

    if (error != SQLITE_OK)
        LOGC("Failed to prepare statement: ", selectSampleTimesSql);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int baseSampleNumber = sqlite3_column_int(stmt, 0);
        double timestamp = sqlite3_column_double(stmt, 1);

        layFile->writeText(String(baseSampleNumber) + 
            String("=") + 
            String(timestamp) + 
            String("\n"), 
            false, 
            false, 
            nullptr);
    }

    sqlite3_finalize(stmt);
}

void DatabaseManager::WriteCommentsFromDatabaseToLayoutFile(FileOutputStream* layFile)
{
    const char* selectAnnotationsSql = "SELECT * FROM Comments;";
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
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        layFile->writeText(String(timestamp) + "," +
            String(duration) + "," +
            String(durationInt) + "," +
            String(bufferSize) + "," +
            String(text) +
            String("\n"),
            false,
            false,
            nullptr);
    }

    sqlite3_finalize(stmt);
}

bool DatabaseManager::IsDatabaseConstructed() const
{
    return mDatabase;
}

void DatabaseManager::CloseDatabase()
{
    if (mDatabase)
        sqlite3_close(mDatabase);
}

int DatabaseManager::GetSampleTimesPosition() const
{
    return 0;
}
