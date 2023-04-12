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

#include "PersystRecordEngine.h"
#include "PersystLayFileFormat.h"

#define MAX_BUFFER_SIZE 40960

PersystRecordEngine::PersystRecordEngine() 
{ 
    m_bufferSize = MAX_BUFFER_SIZE;
    m_scaledBuffer.malloc(MAX_BUFFER_SIZE);
    m_intBuffer.malloc(MAX_BUFFER_SIZE);

}
	
PersystRecordEngine::~PersystRecordEngine()
{

}


RecordEngineManager* PersystRecordEngine::getEngineManager()
{
	RecordEngineManager* man = new RecordEngineManager("PERSYST", "Persyst",
		&(engineFactory<PersystRecordEngine>));
	
	return man;
}

String PersystRecordEngine::getEngineId() const 
{
	return "PERSYST";
}

String PersystRecordEngine::getProcessorString(const InfoObject* channelInfo)
{
    /* Format: Neuropixels-PXI-100.ProbeA-LFP */
    /* Convert spaces or @ symbols in source node name to underscore */
    String fName = channelInfo->getSourceNodeName().replaceCharacters(" @", "__") + "-";
    fName += String(((ChannelInfoObject*)channelInfo)->getSourceNodeId());
    fName += "." + String(((ChannelInfoObject*)channelInfo)->getStreamName());
    fName += File::getSeparatorString();
    return fName;
}

void PersystRecordEngine::openFiles(File rootFolder, int experimentNumber, int recordingNumber)
{

    
    m_channelIndexes.insertMultiple(0, 0, getNumRecordedContinuousChannels());
    m_fileIndexes.insertMultiple(0, 0, getNumRecordedContinuousChannels());
    m_samplesWritten.insertMultiple(0, 0, getNumRecordedContinuousChannels());
    
    String basepath = rootFolder.getFullPathName() + rootFolder.getSeparatorString() + "experiment" + String(experimentNumber)
        + File::getSeparatorString() + "recording" + String(recordingNumber + 1) + File::getSeparatorString();

    String contPath = basepath + "continuous" + File::getSeparatorString();

    int streamIndex = -1;
    uint16 lastStreamId = 0;
    int indexWithinStream = 0;
    Array<const ContinuousChannel*> firstChannels;
    Array<int> channelCounts;

    for (int ch = 0; ch < getNumRecordedContinuousChannels(); ch++)
    {

        int globalIndex = getGlobalIndex(ch); // the global channel index
        int localIndex = getLocalIndex(ch); // the local channel index (within a stream)

        const ContinuousChannel* channelInfo = getContinuousChannel(globalIndex); // channel info object

        int streamId = channelInfo->getStreamId();


        
        if (streamId != lastStreamId)
        {
            firstChannels.add(channelInfo);
            streamIndex++;

            if (streamIndex > 0)
            {
                channelCounts.add(indexWithinStream);
            }

            indexWithinStream = 0;
        }

        m_fileIndexes.set(ch, streamIndex);
        m_channelIndexes.set(ch, indexWithinStream++);

        lastStreamId = streamId;

    }

    channelCounts.add(indexWithinStream);

    streamIndex = -1;

    
    for (auto ch : firstChannels)
    {
        streamIndex++;

        String datPath = getProcessorString(ch);
        String dataFileName = "recording.dat";
        String dataFilePath = contPath + datPath + dataFileName;
        String layoutFilePath = contPath + datPath + "recording.lay";


        ScopedPointer<SequentialBlockFile> bFile = new SequentialBlockFile(channelCounts[streamIndex], samplesPerBlock);

        if (bFile->openFile(dataFilePath))
            m_continuousFiles.add(bFile.release());
        else
            m_continuousFiles.add(nullptr);

        
        PersystLayFileFormat layoutFile(layoutFilePath,
                             dataFileName,
                             ch->getSampleRate(),
                             ch->getBitVolts(),
                             channelCounts[streamIndex]);
        
        ScopedPointer<FileOutputStream> layoutFileStream  = new FileOutputStream(layoutFile.getLayoutFilePath());
        if(layoutFileStream -> openedOk()){
            layoutFileStream -> writeText(layoutFile.toString(), false, false, nullptr);
            layoutFileStream -> writeText("[SampleTimes]\n", false, false, nullptr);
            layoutFiles.add(layoutFileStream.release());
        }
        else
            layoutFiles.add(nullptr);
    }

}

void PersystRecordEngine::closeFiles()
{

    for(auto layoutFile : layoutFiles){
        layoutFile -> flush();
    }
    layoutFiles.clear();
    m_continuousFiles.clear();

    m_channelIndexes.clear();
    m_fileIndexes.clear();

    m_scaledBuffer.malloc(MAX_BUFFER_SIZE);
    m_intBuffer.malloc(MAX_BUFFER_SIZE);

    m_samplesWritten.clear();

}

void PersystRecordEngine::writeContinuousData(int writeChannel, 
											   int realChannel, 
											   const float* dataBuffer, 
											   const double* ftsBuffer, 
											   int size)
{
    if (!size)
        return;

    /* If our internal buffer is too small to hold the data... */
    if (size > m_bufferSize) //shouldn't happen, but if does, this prevents crash...
    {
        std::cerr << "[RN] Write buffer overrun, resizing from: " << m_bufferSize << " to: " << size << std::endl;
        m_scaledBuffer.malloc(size);
        m_intBuffer.malloc(size);
        m_bufferSize = size;
    }

    /* Convert signal from float to int w/ bitVolts scaling */
    double multFactor = 1 / (float(0x7fff) * getContinuousChannel(realChannel)->getBitVolts());
    FloatVectorOperations::copyWithMultiply(m_scaledBuffer.getData(), dataBuffer, multFactor, size);
    AudioDataConverters::convertFloatToInt16LE(m_scaledBuffer.getData(), m_intBuffer.getData(), size);

    /* Get the file index that belongs to the current recording channel */
    int fileIndex = m_fileIndexes[writeChannel];

    /* Write the data to that file */
    m_continuousFiles[fileIndex]->writeChannel(
        m_samplesWritten[writeChannel],
        m_channelIndexes[writeChannel],
        m_intBuffer.getData(),
        size);
    

    /* If is first channel in subprocessor */
    if (m_channelIndexes[writeChannel] == 0)
    {

        int64 baseSampleNumber = m_samplesWritten[writeChannel];
        String timestampString = String(baseSampleNumber) + String("=") + String(ftsBuffer[0]) + String("\n");
        layoutFiles[fileIndex]  -> writeText(timestampString, false, false, nullptr);        
    }
    
    m_samplesWritten.set(writeChannel, m_samplesWritten[writeChannel] + size);

}

void PersystRecordEngine::writeEvent(int eventChannel, const EventPacket& event)
{

}


void PersystRecordEngine::writeSpike(int electrodeIndex, const Spike* spike)
{

}


void PersystRecordEngine::writeTimestampSyncText(
	uint64 streamId, 
	int64 timestamp, 
	float sourceSampleRate, 
	String text)
{

}

