#pragma once

#include "LayFileComment.h"

#include <sqlite3.h>
#include <RecordingLib.h>

class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    bool ConstructDatabase(const String& path);

    void InsertIntoSampleTimesTable(int64 baseSampleNumber, double timestamp);
    void InsertIntoCommentsTable(double timestamp, double duration, int durationInt, int eventType, const char* comment);
    Array<Comment> GetCommentsFromDatabase() const;

    void WriteSampleTimesFromDatabaseToLayoutFile(FileOutputStream* layFile);
    void WriteCommentsFromDatabaseToLayoutFile(FileOutputStream* layFile);

    bool IsDatabaseConstructed() const;
    void CloseDatabase();

private:
    int GetSampleTimesPosition() const;

private:
    sqlite3* mDatabase{ nullptr };
};