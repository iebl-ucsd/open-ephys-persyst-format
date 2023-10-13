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

    std::map<uint16, Array<String>> channelNamesByStreamID;

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

        channelNamesByStreamID[channelInfo->getStreamId()].set(localIndex,channelInfo ->getName());

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

        
        PersystLayFileFormat layoutFile = PersystLayFileFormat::create(layoutFilePath,
                                                                         ch->getSampleRate(),
                                                                         ch->getBitVolts(),
                                                                         channelCounts[streamIndex])
                                                                .withDataFile(dataFileName);
        
        ScopedPointer<FileOutputStream> layoutFileStream  = new FileOutputStream(layoutFile.getLayoutFilePath());
        if(layoutFileStream -> openedOk()){
            layoutFileStream -> writeText(layoutFile.toString(), false, false, nullptr);
            layoutFileStream -> writeText("[ChannelMap]\n", false, false, nullptr);
            //Persyst uses first index = 1
            int persystChannelIndex = 1;
            for(auto channelName : channelNamesByStreamID[ch->getStreamId()]) {
                layoutFileStream -> writeText(channelName + String("=") + String(persystChannelIndex++) + String("\n"), false, false, nullptr);
            }
            layoutFileStream -> writeText("[SampleTimes]\n", false, false, nullptr);
            layoutFiles.add(layoutFileStream.release());
        }
        else {
            layoutFiles.add(nullptr);
        }
    }
    
    //Event data files
    String eventPath(basepath + "events" + File::getSeparatorString());
    Array<var> eventChannelJSON;

    std::map<String, int> ttlMap;

    for (int ev = 0; ev < getNumRecordedEventChannels(); ev++)
    {

        const EventChannel* chan = getEventChannel(ev);
        String eventName;
        NpyType type;
        String dataFileName;

        switch (chan->getType())
        {
        case EventChannel::TEXT:
            LOGD("Got text channel");
            eventName = "MessageCenter" + File::getSeparatorString();
            type = NpyType(BaseType::CHAR, chan->getLength());
            dataFileName = "text";
            break;
        case EventChannel::TTL:
            LOGD("Got TTL channel");
            eventName = getProcessorString(chan);
            if (ttlMap.count(eventName))
                ttlMap[eventName]++;
            else
                ttlMap[eventName] = 0;
            eventName += "TTL" + (ttlMap[eventName] ? "_" + String(ttlMap[eventName]) : "") + File::getSeparatorString();
            type = NpyType(BaseType::INT16, 1);
            dataFileName = "states";
            break;
        default:
            LOGD("Got BINARY group");
            eventName = getProcessorString(chan);
            eventName += "BINARY_group";
            type = NpyType(chan->getEquivalentMetadataType(), chan->getLength());
            dataFileName = "data_array";
            break;
        }

        ScopedPointer<EventRecording> rec = new EventRecording();

        rec->data = std::make_unique<NpyFile>(eventPath + eventName + dataFileName + ".npy", type);
        rec->samples = std::make_unique<NpyFile>(eventPath + eventName + "sample_numbers.npy", NpyType(BaseType::INT64, 1));
        rec->timestamps = std::make_unique<NpyFile>(eventPath + eventName + "timestamps.npy", NpyType(BaseType::DOUBLE, 1));
        if (chan->getType() == EventChannel::TTL && m_saveTTLWords)
        {
            rec->extraFile = std::make_unique<NpyFile>(eventPath + eventName + "full_words.npy", NpyType(BaseType::UINT64, 1));
        }

        DynamicObject::Ptr jsonChannel = new DynamicObject();
        jsonChannel->setProperty("folder_name", eventName.replace(File::getSeparatorString(), "/"));
        jsonChannel->setProperty("channel_name", chan->getName());
        jsonChannel->setProperty("description", chan->getDescription());

        jsonChannel->setProperty("identifier", chan->getIdentifier());
        jsonChannel->setProperty("sample_rate", chan->getSampleRate());
        jsonChannel->setProperty("type", jsonTypeValue(type.getType()));
        jsonChannel->setProperty("source_processor", chan->getSourceNodeName());
        jsonChannel->setProperty("stream_name", chan->getStreamName());

        if (chan->getType() == EventChannel::TTL)
        {
            jsonChannel->setProperty("initial_state", int(chan->getTTLWord()));
        }

        createChannelMetadata(chan, jsonChannel);

        //rec->metaDataFile = createEventMetadataFile(chan, eventPath + eventName + "metadata.npy", jsonChannel);
        m_eventFiles.add(rec.release());
        eventChannelJSON.add(var(jsonChannel));
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
    
    m_eventFiles.clear();

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
    

    /* If is first channel in stream, then write timestamp for sample */
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

    const EventChannel* info = getEventChannel(eventChannel);

    EventPtr ev = Event::deserialize(event, info);
    EventRecording* rec = m_eventFiles[eventChannel];

    if (!rec) return;

    if (ev->getEventType() == EventChannel::TTL)
    {

        TTLEvent* ttl = static_cast<TTLEvent*>(ev.get());

        int16 state = (ttl->getLine() + 1) * (ttl->getState() ? 1 : -1);
        rec->data->writeData(&state, sizeof(int16));

        int64 sampleIdx = ev->getSampleNumber();
        rec->samples->writeData(&sampleIdx, sizeof(int64));

        double ts = ev->getTimestampInSeconds();
        rec->timestamps->writeData(&ts, sizeof(double));

        if (rec->extraFile)
        {
            uint64 fullWord = ttl->getWord();
            rec->extraFile->writeData(&fullWord, sizeof(uint64));
        }

    }
    else if (ev->getEventType() == EventChannel::TEXT)
    {

        TextEvent* text = static_cast<TextEvent*>(ev.get());

        int64 sampleIdx = text->getSampleNumber();
        rec->samples->writeData(&sampleIdx, sizeof(int64));

        double ts = text->getTimestampInSeconds();
        rec->timestamps->writeData(&ts, sizeof(double));

        rec->data->writeData(ev->getRawDataPointer(), info->getDataSize());
    }

    // NOT IMPLEMENTED
    //writeEventMetadata(ev.get(), rec->metaDataFile.get());

    increaseEventCounts(rec);

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

void PersystRecordEngine::increaseEventCounts(EventRecording* rec)
{
    rec->data->increaseRecordCount();
    rec->samples->increaseRecordCount();
    rec->timestamps->increaseRecordCount();
    if (rec->channels) rec->channels->increaseRecordCount();
    if (rec->extraFile) rec->extraFile->increaseRecordCount();
}

String PersystRecordEngine::jsonTypeValue(BaseType type)
{
    switch (type)
    {
    case BaseType::CHAR:
        return "string";
    case BaseType::INT8:
        return "int8";
    case BaseType::UINT8:
        return "uint8";
    case BaseType::INT16:
        return "int16";
    case BaseType::UINT16:
        return "uint16";
    case BaseType::INT32:
        return "int32";
    case BaseType::UINT32:
        return "uint32";
    case BaseType::INT64:
        return "int64";
    case BaseType::UINT64:
        return "uint64";
    case BaseType::FLOAT:
        return "float";
    case BaseType::DOUBLE:
        return "double";
    default:
        return String();
    }
}

template <typename TO, typename FROM>
void dataToVar(var& dataTo, const void* dataFrom, int length)
{
    const FROM* buffer = reinterpret_cast<const FROM*>(dataFrom);
    for (int i = 0; i < length; i++)
    {
        dataTo.append(static_cast<TO>(*(buffer + i)));
    }
}

void PersystRecordEngine::createChannelMetadata(const MetadataObject* channel, DynamicObject* jsonFile)
{
    int nMetadata = channel->getMetadataCount();
    if (nMetadata < 1) return;

    Array<var> jsonMetadata;
    for (int i = 0; i < nMetadata; i++)
    {
        const MetadataDescriptor* md = channel->getMetadataDescriptor(i);
        const MetadataValue* mv = channel->getMetadataValue(i);
        DynamicObject::Ptr jsonValues = new DynamicObject();
        MetadataDescriptor::MetadataType type = md->getType();
        unsigned int length = md->getLength();
        jsonValues->setProperty("name", md->getName());
        jsonValues->setProperty("description", md->getDescription());
        jsonValues->setProperty("identifier", md->getIdentifier());
        jsonValues->setProperty("type", jsonTypeValue(type));
        jsonValues->setProperty("length", (int)length);
        var val;

        if (type == MetadataDescriptor::CHAR)
        {
            String tmp;
            mv->getValue(tmp);
            val = tmp;
        }
        else
        {
            const void* buf = mv->getRawValuePointer();
            switch (type)
            {
            case MetadataDescriptor::INT8:
                dataToVar<int, int8>(val, buf, length);
                break;
            case MetadataDescriptor::UINT8:
                dataToVar<int, uint8>(val, buf, length);
                break;
            case MetadataDescriptor::INT16:
                dataToVar<int, int16>(val, buf, length);
                break;
            case MetadataDescriptor::UINT16:
                dataToVar<int, uint16>(val, buf, length);
                break;
            case MetadataDescriptor::INT32:
                dataToVar<int, int32>(val, buf, length);
                break;
                //A full uint32 doesn't fit in a regular int, so we increase size
            case MetadataDescriptor::UINT32:
                dataToVar<int64, uint8>(val, buf, length);
                break;
            case MetadataDescriptor::INT64:
                dataToVar<int64, int64>(val, buf, length);
                break;
                //This might overrun and end negative if the uint64 is really big, but there is no way to store a full uint64 in a var
            case MetadataDescriptor::UINT64:
                dataToVar<int64, uint64>(val, buf, length);
                break;
            case MetadataDescriptor::FLOAT:
                dataToVar<float, float>(val, buf, length);
                break;
            case MetadataDescriptor::DOUBLE:
                dataToVar<double, double>(val, buf, length);
                break;
            default:
                val = "invalid";
            }
        }
        jsonValues->setProperty("value", val);
        jsonMetadata.add(var(jsonValues));
    }
    jsonFile->setProperty("channel_metadata", jsonMetadata);
}

void PersystRecordEngine::setParameter(EngineParameter& parameter)
{
    boolParameter(0, m_saveTTLWords);
}


