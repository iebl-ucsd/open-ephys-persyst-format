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

#include <RecordingLib.h>

class TESTABLE PersystRecordEngine : public RecordEngine
{
public:

	/** Constructor */
	PersystRecordEngine();
	
	/** Destructor */
	~PersystRecordEngine();

	/** Launches the manager for this Record Engine, and instantiates any parameters */
	static RecordEngineManager* getEngineManager();

	/** Returns a string that can be used to identify this record engine*/
	String getEngineId() const;
    
    String getProcessorString(const InfoObject* channelInfo);

	/** Called when recording starts to open all needed files */
	void openFiles(File rootFolder, int experimentNumber, int recordingNumber);

	/** Called when recording stops to close all files and do all the necessary cleanup */
	void closeFiles();

	/** Write continuous data for a channel, including synchronized float timestamps for each sample */
	void writeContinuousData(int writeChannel, 
						       int realChannel, 
							   const float* dataBuffer, 
							   const double* ftsBuffer, 
							   int size);

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
    
    static String jsonTypeValue(BaseType type);
    void createChannelMetadata(const MetadataObject* channel, DynamicObject* jsonObject);
    void increaseEventCounts(EventRecording* rec);

    Array<unsigned int> m_channelIndexes;
    Array<unsigned int> m_fileIndexes;
    
    OwnedArray<FileOutputStream> layoutFiles;
    OwnedArray<EventRecording> m_eventFiles;

    HeapBlock<float> m_scaledBuffer;
    HeapBlock<int16> m_intBuffer;
    
    Array<int64> m_samplesWritten;

    bool m_saveTTLWords{ true };
    
    int m_bufferSize;
    
    OwnedArray<SequentialBlockFile> m_continuousFiles;
    
    const int samplesPerBlock{ 4096 };


};

#endif
