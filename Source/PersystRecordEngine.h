/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2022 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef RECORDENGINEPLUGIN_H_DEFINED
#define RECORDENGINEPLUGIN_H_DEFINED

#include "DatabaseManager.h"
#include "LayFileComment.h"

#include <RecordingLib.h>
#include <vector>
#include <utility>
#include <unordered_map>
#include <sqlite3.h>
#include <queue>

class TESTABLE PersystRecordEngine : public RecordEngine
{
public:

    /** Constructor */
    PersystRecordEngine();

    /** Destructor */
    ~PersystRecordEngine() override;

    /** Launches the manager for this Record Engine, and instantiates any parameters */
    static RecordEngineManager* getEngineManager();

    /** Returns a string that can be used to identify this record engine*/
    String getEngineId() const override;

    String GetProcessorString(const InfoObject* channelInfo);

    /** Called when recording starts to open all needed files */
    void openFiles(File rootFolder, int experimentNumber, int recordingNumber) override;

    /** Called when recording stops to close all files and do all the necessary cleanup */
    void closeFiles() override;

    /** Write continuous data for a channel, including synchronized float timestamps for each sample */
    void writeContinuousData(int writeChannel,
        int realChannel,
        const float* dataBuffer,
        const double* ftsBuffer,
        int size) override;

    /** Write a single event to disk (TTL or TEXT) */
    void writeEvent(int eventChannel,
        const EventPacket& event) override;

    /** Write a spike to disk */
    void writeSpike(int electrodeIndex,
        const Spike* spike) override;

    /** Write the timestamp sync text messages to disk */
    void writeTimestampSyncText(uint64 streamId,
        int64 timestamp,
        float sourceSampleRate,
        String text) override;

    void setParameter(EngineParameter& parameter) override;

    int getCommentsPosition() const;

private:
    class EventRecording
    {
    public:
        std::unique_ptr<NpyFile> data;
        std::unique_ptr<NpyFile> samples;
        std::unique_ptr<NpyFile> channels;
        std::unique_ptr<NpyFile> extraFile;
        std::unique_ptr<NpyFile> timestamps;
    };

    class LayFileOverwriteTimer : public Timer
    {
    public:
        LayFileOverwriteTimer(OwnedArray<FileOutputStream>& layFiles);

        void SetLayoutFilePath(const String& path);
        void SetLayoutFileIndex(int index);

    private:
        String mLayFilePath;
        int mFileIndex;
        OwnedArray<FileOutputStream>& mLayoutFiles;

    private:
        // Inherited via Timer
        void timerCallback() override;
    };

private:
    static String JsonTypeValue(BaseType type);
    void CreateChannelMetadata(const MetadataObject* channel, DynamicObject* jsonObject);
    void IncreaseEventCounts(EventRecording* rec);

private:
    Array<unsigned int> mChannelIndexes;
    Array<unsigned int> mFileIndexes;

    OwnedArray<FileOutputStream> mLayoutFiles;
    OwnedArray<EventRecording> mEventFiles;
    OwnedArray<SequentialBlockFile> mContinuousFiles;
    String mLayFilePath;

    HeapBlock<float> mScaledBuffer;
    HeapBlock<int16> mIntBuffer;
    int mBufferSize;

    bool mSaveTTLWords{ true };

    const int mSamplesPerBlock{ 4096 };
    int mSampleTimesPosition{ 0 };
    Array<int64> mSamplesWritten;

    DatabaseManager mDatabaseManager;

    LayFileOverwriteTimer mTimer;
    std::queue<Comment> mComments;
};
#endif