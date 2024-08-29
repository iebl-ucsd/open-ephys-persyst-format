#pragma once

#include "LayFileAnnotationExtractor.h"

#include <sqlite3.h>
#include <RecordingLib.h>

class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    bool ConstructDatabase(const String& path);

    void InsertIntoSampleTimesTable(int64 baseSampleNumber, double timestamp);
    void InsertIntoAnnotationsTable(double timestamp, double duration, int durationInt, int eventType, const char* comment);
    Array<Annotation> GetAnnotationsFromDatabase() const;

    void WriteSampleTimesFromDatabaseToLayoutFile(int writeChannel, 
        Array<unsigned int>& fileIndexes, 
        OwnedArray<FileOutputStream>& layoutFiles);
    void WriteAnnotationsFromDatabaseToLayoutFile(int writeChannel,
        Array<unsigned int>& fileIndexes, 
        OwnedArray<FileOutputStream>& layoutFiles);

private:
    sqlite3* mDatabase{ nullptr };
};