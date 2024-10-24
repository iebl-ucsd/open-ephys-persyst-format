#include <stdio.h>

#include "gtest/gtest.h"

#include <Processors/RecordNode/RecordNode.h>
#include <Processors/PluginManager/OpenEphysPlugin.h>
#include "../Source/PersystRecordEngine.h"
#include <ModelProcessors.h>
#include <ModelApplication.h>
#include <TestFixtures.h>
#include <chrono>
#include <thread>
#include <iterator>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

struct DirectorySearchParameters
{
public:
    int experimentIndex = 1;
    int recordingIndex = 1;
    std::optional<String> streamDirName = std::nullopt;
};


class PersystRecordEngineUnitTests : public testing::Test
{
protected:
    void SetUp() override
    {
        mTester = std::make_unique<ProcessorTester>(FakeSourceNodeParams{
            mNumChannels,
            mSampleRate,
            mBitVolts,
            mStreams
            });

        mParentRecordingDir = std::filesystem::temp_directory_path() / "persyst_record_engine_tests";
        if (std::filesystem::exists(mParentRecordingDir))
        {
            std::filesystem::remove_all(mParentRecordingDir);
        }
        std::filesystem::create_directory(mParentRecordingDir);

        // Set this before creating the record node
        mTester->setRecordingParentDirectory(mParentRecordingDir.string());
        mProcessor = mTester->createProcessor<RecordNode>(Plugin::Processor::RECORD_NODE);
        std::unique_ptr<RecordEngineManager> recordEngineManager = std::unique_ptr<RecordEngineManager>(PersystRecordEngine::getEngineManager());
        //mProcessor->overrideRecordEngine(recordEngineManager.get());
    }

    void TearDown() override
    {
        // Swallow errors
        std::error_code ec;
        std::filesystem::remove_all(mParentRecordingDir, ec);
    }

    AudioBuffer<float> CreateBuffer(float startingVal, float step, int mNumChannels, int numSamples)
    {
        AudioBuffer<float> inputBuffer(mNumChannels, numSamples);

        // in microvolts
        float currVal = startingVal;
        for (int chidx = 0; chidx < mNumChannels; chidx++)
        {
            for (int sampleIdx = 0; sampleIdx < numSamples; sampleIdx++)
            {
                inputBuffer.setSample(chidx, sampleIdx, currVal);
                currVal += step;
            }
        }
        return inputBuffer;
    }

    void WriteBlock(AudioBuffer<float>& buffer, TTLEvent* maybeTtlEvent = nullptr)
    {
        auto outBuffer = mTester->processBlock(mProcessor, buffer, maybeTtlEvent);
        // Assert the buffer hasn't changed after process()
        ASSERT_EQ(outBuffer.getNumSamples(), buffer.getNumSamples());
        ASSERT_EQ(outBuffer.getNumChannels(), buffer.getNumChannels());
        for (int chidx = 0; chidx < outBuffer.getNumChannels(); chidx++)
        {
            for (int sampleIdx = 0; sampleIdx < outBuffer.getNumSamples(); ++sampleIdx)
            {
                ASSERT_EQ(
                    outBuffer.getSample(chidx, sampleIdx),
                    buffer.getSample(chidx, sampleIdx));
            }
        }
    }

    bool EventsPathFor(const std::string& baseName, std::filesystem::path* path, DirectorySearchParameters parameters = DirectorySearchParameters())
    {
        std::filesystem::path partialPath;
        auto success = SubRecordingPathFor("events", "TTL", &partialPath, parameters);
        if (!success)
        {
            return false;
        }
        auto ret = partialPath / baseName;
        if (std::filesystem::exists(ret))
        {
            *path = ret;
            return true;
        }
        else
        {
            return false;
        }
    }

    std::vector<char> LoadNpyFileBinaryFullpath(const std::string& fullPath)
    {
        std::ifstream dataIfStream(fullPath, std::ios::binary | std::ios::in);

        dataIfStream.seekg(0, std::ios::end);
        std::streampos fileSize = dataIfStream.tellg();
        dataIfStream.seekg(0, std::ios::beg);

        std::vector<char> persistedData(fileSize);
        dataIfStream.read(persistedData.data(), fileSize);
        return persistedData;
    }

    void CompareBinaryFilesHex(const std::string& fileName, const std::vector<char> binData, const std::string& expectedBinDataHex)
    {
        std::vector<char> expectedBinData;
        for (int i = 0; i < expectedBinDataHex.length(); i += 2)
        {
            std::string byteString = expectedBinDataHex.substr(i, 2);
            char byte = (char)strtol(byteString.c_str(), nullptr, 16);
            expectedBinData.push_back(byte);
        }

        // Create a string rep of the actual sample numbers bin in case it fails, to help debugging
        std::stringstream binDataHexSs;
        binDataHexSs << "Expected data for " << fileName << " in hex to be=" << expectedBinDataHex
            << " but received=";
        binDataHexSs << std::hex;
        for (int i = 0; i < binData.size(); i++)
        {
            binDataHexSs << std::setw(2) << std::setfill('0') << (int)binData[i];
        }
        std::string errMsg = binDataHexSs.str();

        ASSERT_EQ(binData.size(), expectedBinData.size()) << errMsg;
        for (int i = 0; i < binData.size(); i++)
        {
            ASSERT_EQ(binData[i], expectedBinData[i])
                << errMsg
                << " (error on index " << i << ")";
        }
    }

    bool SubRecordingPathFor(
        const std::string& subRecordingDirName,
        const std::string& baseName,
        std::filesystem::path* path,
        DirectorySearchParameters parameters)
    {
        // Do verifications:
        auto recordingDir = std::filesystem::directory_iterator(mParentRecordingDir)->path();
        std::stringstream ss;
        ss << "Record Node " << mProcessor->getNodeId();

        std::stringstream experimentStr;
        experimentStr << "experiment" << parameters.experimentIndex;
        std::stringstream recordingStr;
        recordingStr << "recording" << parameters.recordingIndex;
        auto recordingDir2 = recordingDir / ss.str() / experimentStr.str() / recordingStr.str() / subRecordingDirName;
        if (!std::filesystem::exists(recordingDir2))
        {
            return false;
        }

        std::filesystem::path recordingDir3;
        for (const auto& subdir : std::filesystem::directory_iterator(recordingDir2))
        {
            auto subDirBaseName = subdir.path().filename().string();
            if (parameters.streamDirName.has_value())
            {
                if (subDirBaseName == parameters.streamDirName.value())
                {
                    recordingDir3 = subdir.path();
                }
            }
            else
            {
                //Needs to work for multiple streams
                if (subDirBaseName.find("FakeSourceNode") != std::string::npos)
                {
                    recordingDir3 = subdir.path();
                }
            }
        }

        if (!std::filesystem::exists(recordingDir3))
        {
            return false;
        }

        auto ret = recordingDir3 / baseName;
        if (!std::filesystem::exists(ret))
        {
            return false;
        }
        *path = ret;
        return true;
    }

    bool ContinuousPathFor(const std::string& baseName, std::filesystem::path* path, DirectorySearchParameters parameters)
    {
        return SubRecordingPathFor("continuous", baseName, path, parameters);
    }


    void MaybeLoadContinuousDatFile(std::vector<int16_t>* output, bool* success, DirectorySearchParameters parameters)
    {
        // Do verifications:
        std::filesystem::path continuousDatPath;
        *success = ContinuousPathFor("recording.dat", &continuousDatPath, parameters);
        if (!*success)
        {
            return;
        }

        std::ifstream continuousIfStream(continuousDatPath.string(), std::ios::binary | std::ios::in);

        continuousIfStream.seekg(0, std::ios::end);
        std::streampos fileSize = continuousIfStream.tellg();
        continuousIfStream.seekg(0, std::ios::beg);
        if (fileSize % sizeof(int16_t) != 0)
        {
            *success = false;
            return;
        }

        std::vector<int16_t> persistedData(fileSize / sizeof(int16_t));
        continuousIfStream.read((char*)persistedData.data(), fileSize);
        *success = true;
        *output = persistedData;
    }

    void LoadContinuousDatFile(std::vector<int16_t>* output, DirectorySearchParameters parameters = DirectorySearchParameters())
    {
        bool success = false;
        MaybeLoadContinuousDatFile(output, &success, parameters);
        ASSERT_TRUE(success);
    }

    void MaybeLoadLayoutFile(boost::property_tree::ptree& pt, bool* success, DirectorySearchParameters parameters)
    {
        // Do verifications:
        std::filesystem::path continuousDatPath;
        *success = ContinuousPathFor("recording.lay", &continuousDatPath, parameters);
        if (!*success)
        {
            return;
        }

        try
        {
            boost::property_tree::ini_parser::read_ini(continuousDatPath.string(), pt);
        }
        catch (const boost::property_tree::ini_parser_error& e)
        {
            *success = false;
            return;
        }
        *success = true;
        return;

    }

    void LoadLayoutFile(boost::property_tree::ptree& pt, DirectorySearchParameters parameters = DirectorySearchParameters())
    {
        bool success = false;
        MaybeLoadLayoutFile(pt, &success, parameters);
        ASSERT_TRUE(success);
    }

    void CheckLayoutFileInfo(const boost::property_tree::ptree& pt) const
    {
        boost::property_tree::ptree::const_assoc_iterator exists = pt.find("FileInfo");
        if (exists == pt.not_found())
        {
            FAIL() << "Test failed; layout file didn't have a FileInfo section";
        }
        ASSERT_EQ(pt.get_child("FileInfo").size(), 7);
        ASSERT_EQ(pt.get<std::string>("FileInfo.File"), "recording.dat");
        ASSERT_EQ(pt.get<std::string>("FileInfo.FileType"), "Interleaved");
        ASSERT_EQ(pt.get<float>("FileInfo.SamplingRate"), mSampleRate);
        ASSERT_EQ(pt.get<int>("FileInfo.HeaderLength"), 0);
        ASSERT_EQ(pt.get<float>("FileInfo.Calibration"), mBitVolts);
        ASSERT_EQ(pt.get<int>("FileInfo.WaveformCount"), mNumChannels);
        ASSERT_EQ(pt.get<int>("FileInfo.DataType"), 0);
    }

    void CheckLayoutChannelMap(const boost::property_tree::ptree& pt) const
    {
        boost::property_tree::ptree::const_assoc_iterator exists = pt.find("ChannelMap");
        if (exists == pt.not_found())
        {
            FAIL() << "Test failed; layout file didn't have a ChannelMap section";
        }
        ASSERT_EQ(pt.get_child("ChannelMap").size(), mNumChannels);
        for (int i = 0; i < mNumChannels; i++)
        {
            String channelName = String("ChannelMap.CH") + String(i);
            ASSERT_EQ(pt.get<int>(channelName.toStdString()), i + 1);

        }
    }

    bool IsStringAPositiveInteger(std::string s)
    {
        return !s.empty() && (std::find_if(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); }) != s.end());
    }

    void CheckLayoutSampleTimes(const boost::property_tree::ptree& pt, int sampleRate, int samplesPerBlock)
    {
        boost::property_tree::ptree::const_assoc_iterator exists = pt.find("SampleTimes");
        if (exists == pt.not_found())
        {
            FAIL() << "Test failed; layout file didn't have a SampleTimes section";
        }

        int expectedSampleIdx = 0;
        double expectedSampleTime = 0;
        boost::property_tree::ptree referenceSamples = pt.get_child("SampleTimes");
        for (boost::property_tree::ptree::const_iterator it = referenceSamples.begin(); it != referenceSamples.end(); it++)
        {
            //Key should be an integer corresponding to the reference sample's index
            if (IsStringAPositiveInteger((*it).first))
            {
                ASSERT_EQ(std::stoi((*it).first), expectedSampleIdx);
            }
            else
            {
                FAIL() << "Test failed; SampleTimes key not a positive integer";
            }

            //Value should be a ptree whose value is a floating point timestamp
            boost::optional<double> sampleTime = ((*it).second.get_value_optional<double>());
            if (sampleTime.has_value())
            {
                ASSERT_NEAR(sampleTime.value(), expectedSampleTime, .001);
            }
            else
            {
                FAIL() << "Test failed; SampleTimes value not a floating point number";

            }
            expectedSampleIdx += samplesPerBlock;
            expectedSampleTime += ((double)samplesPerBlock / sampleRate);
        }
    }

    void UpdateSourceNodesStreamParams()
    {
        FakeSourceNode* sn = dynamic_cast<FakeSourceNode*>(mTester->getSourceNode());
        sn->setParams(FakeSourceNodeParams{
            mNumChannels,
            mSampleRate,
            mBitVolts
            });
        mTester->updateSourceNodeSettings();

    }

    String BuildStreamFileName(const DataStream* stream)
    {
        String returnStr;
        returnStr = stream->getSourceNodeName().replaceCharacters(" @", "__") + "-";
        returnStr += stream->getSourceNodeId();
        returnStr += "." + stream->getName();
        return returnStr;
    }


    static int16_t MinValPossible()
    {
        // The min value is actually -32767 in the math in RecordNode, not -32768 like the "true" min for int16_t
        return (std::numeric_limits<int16_t>::min)() + 1;
    }

    static int16_t MaxValPossible()
    {
        return (std::numeric_limits<int16_t>::max)();
    }

    RecordNode* mProcessor;
    int mNumChannels = 8;
    float mBitVolts = 1.0;
    std::unique_ptr<ProcessorTester> mTester;
    std::filesystem::path mParentRecordingDir;
    float mSampleRate = 1.0;
    int mStreams = 1;
};

//From RecordNodeTests.cpp - uses same binary file writes
TEST_F(PersystRecordEngineUnitTests, TestInputOutput_Continuous_Single)
{
    GTEST_SKIP() << "Requires headless mode support.";

    int numSamples = 100;
    mTester->startAcquisition(true);

    auto inputBuffer = CreateBuffer(1000.0, 20.0, mNumChannels, numSamples);
    WriteBlock(inputBuffer);

    // The record node always flushes its pending writes when stopping acquisition, so we don't need to sleep before
    // stopping.
    mTester->stopAcquisition();

    std::vector<int16_t> persistedData;
    LoadContinuousDatFile(&persistedData);
    ASSERT_EQ(persistedData.size(), mNumChannels * numSamples);

    int persistedDataIdx = 0;
    // File is channel-interleaved, so ensure we iterate in the correct order:
    for (int sampleIdx = 0; sampleIdx < numSamples; sampleIdx++)
    {
        for (int chidx = 0; chidx < mNumChannels; chidx++)
        {
            auto expected_microvolts = inputBuffer.getSample(chidx, sampleIdx);
            ASSERT_EQ(persistedData[persistedDataIdx], expected_microvolts);
            persistedDataIdx++;
        }
    }
}

//From RecordNodeTests.cpp - uses same binary file writes
TEST_F(PersystRecordEngineUnitTests, TestInputOutput_Continuous_Multiple)
{
    GTEST_SKIP() << "Requires headless mode support.";

    mTester->startAcquisition(true);

    int numSamplesPerBlock = 100;
    int numBlocks = 8;
    std::vector<AudioBuffer<float>> inputBuffers;
    for (int i = 0; i < numBlocks; i++)
    {
        auto inputBuffer = CreateBuffer(1000.0f * i, 20.0, mNumChannels, numSamplesPerBlock);
        WriteBlock(inputBuffer);
        inputBuffers.push_back(inputBuffer);
    }

    mTester->stopAcquisition();

    std::vector<int16_t> persistedData;
    LoadContinuousDatFile(&persistedData);
    ASSERT_EQ(persistedData.size(), mNumChannels * numSamplesPerBlock * numBlocks);

    int persistedDataIdx = 0;
    // File is channel-interleaved, so ensure we iterate in the correct order:
    for (int blockIdx = 0; blockIdx < numBlocks; blockIdx++)
    {
        const auto& inputBuffer = inputBuffers[blockIdx];
        for (int sampleIdx = 0; sampleIdx < numSamplesPerBlock; sampleIdx++)
        {
            for (int chidx = 0; chidx < mNumChannels; chidx++)
            {
                auto expectedMicroVolts = inputBuffer.getSample(chidx, sampleIdx);
                ASSERT_EQ(persistedData[persistedDataIdx], expectedMicroVolts);
                persistedDataIdx++;
            }
        }
    }
}

//From RecordNodeTests.cpp - uses same binary file writes
TEST_F(PersystRecordEngineUnitTests, TestEmpty)
{
    GTEST_SKIP() << "Requires headless mode support.";

    mTester->startAcquisition(true);
    mTester->stopAcquisition();

    std::vector<int16_t> persistedData;
    LoadContinuousDatFile(&persistedData);
    ASSERT_EQ(persistedData.size(), 0);
}

TEST_F(PersystRecordEngineUnitTests, TestLayoutFormat)
{
    GTEST_SKIP() << "Requires headless mode support.";

    mTester->startAcquisition(true);
    mTester->stopAcquisition();
    boost::property_tree::ptree pt;
    LoadLayoutFile(pt);
    CheckLayoutFileInfo(pt);
    CheckLayoutChannelMap(pt);
}

TEST_F(PersystRecordEngineUnitTests, TestSampleIndexes_Continuous_Multiple)
{
    GTEST_SKIP() << "Requires headless mode support.";

    mSampleRate = 100;
    UpdateSourceNodesStreamParams();

    mTester->startAcquisition(true);

    int numSamplesPerBlock = 110;
    int num_blocks = 8;
    std::vector<AudioBuffer<float>> inputBuffers;
    for (int i = 0; i < num_blocks; i++)
    {
        auto inputBuffer = CreateBuffer(1000.0f * i, 20.0, mNumChannels, numSamplesPerBlock);
        WriteBlock(inputBuffer);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        inputBuffers.push_back(inputBuffer);
    }

    mTester->stopAcquisition();

    boost::property_tree::ptree pt;
    LoadLayoutFile(pt);
    CheckLayoutFileInfo(pt);
    CheckLayoutChannelMap(pt);
    CheckLayoutSampleTimes(pt, mSampleRate, numSamplesPerBlock);
}

TEST_F(PersystRecordEngineUnitTests, TestLayoutFormatChangedFiles)
{
    GTEST_SKIP() << "Requires headless mode support.";

    mTester->startAcquisition(true);
    mTester->stopAcquisition();
    boost::property_tree::ptree pt;
    DirectorySearchParameters parameters;
    LoadLayoutFile(pt, parameters);
    CheckLayoutFileInfo(pt);

    mBitVolts = 0.195;
    UpdateSourceNodesStreamParams();
    mTester->startAcquisition(true);
    mTester->stopAcquisition();
    parameters.experimentIndex++;
    LoadLayoutFile(pt, parameters);
    CheckLayoutFileInfo(pt);

    mSampleRate = 1000;
    UpdateSourceNodesStreamParams();
    mTester->startAcquisition(true);
    mTester->stopAcquisition();
    parameters.experimentIndex++;
    LoadLayoutFile(pt, parameters);
    CheckLayoutFileInfo(pt);

    mNumChannels = 32;
    UpdateSourceNodesStreamParams();
    mTester->startAcquisition(true);
    mTester->stopAcquisition();
    parameters.experimentIndex++;
    LoadLayoutFile(pt, parameters);
    CheckLayoutFileInfo(pt);
    CheckLayoutChannelMap(pt);
}

//From RecordNodeTests.cpp - uses same event file writes
TEST_F(PersystRecordEngineUnitTests, Test_PersistsEvents)
{
    GTEST_SKIP() << "Requires headless mode support.";

    mProcessor->setRecordEvents(true);
    mProcessor->updateSettings();

    mTester->startAcquisition(true);
    int numSamples = 5;

    auto streamId = mProcessor->getDataStreams()[0]->getStreamId();
    auto eventChannels = mTester->getSourceNodeDataStream(streamId)->getEventChannels();
    ASSERT_GE(eventChannels.size(), 1);
    TTLEventPtr eventPtr = TTLEvent::createTTLEvent(
        eventChannels[0],
        1,
        2,
        true);
    auto inputBuffer = CreateBuffer(1000.0, 20.0, mNumChannels, numSamples);
    WriteBlock(inputBuffer, eventPtr.get());
    mTester->stopAcquisition();

    std::filesystem::path sampleNumbersPath;
    ASSERT_TRUE(EventsPathFor("sample_numbers.npy", &sampleNumbersPath));
    auto sampleNumbersBin = LoadNpyFileBinaryFullpath(sampleNumbersPath.string());

    /**
     * Same logic as above:
     *      import numpy as np, io, binascii; b = io.BytesIO(); np.save(b, np.array([1], dtype=np.int64)); b.seek(0); print(binascii.hexlify(b.read()))
     */
    std::string expectedSampleNumbersHex =
        "934e554d5059010076007b276465736372273a20273c6938272c2027666f727472616e5f6f72646572273a2046616c73652c2027736861"
        "7065273a2028312c292c207d20202020202020202020202020202020202020202020202020202020202020202020202020202020202020"
        "20202020202020202020202020202020200a0100000000000000";
    CompareBinaryFilesHex("sample_numbers.npy", sampleNumbersBin, expectedSampleNumbersHex);

    std::filesystem::path fullWordsPath;
    ASSERT_TRUE(EventsPathFor("full_words.npy", &fullWordsPath));
    auto fullWordsBin = LoadNpyFileBinaryFullpath(fullWordsPath.string());

    /**
     * Same logic as above:
     *      import numpy as np, io, binascii; b = io.BytesIO(); np.save(b, np.array([4], dtype=np.uint64)); b.seek(0); print(binascii.hexlify(b.read()))
     */
    std::string expectedFullWordsHex =
        "934e554d5059010076007b276465736372273a20273c7538272c2027666f727472616e5f6f72646572273a2046616c73652c2027736861"
        "7065273a2028312c292c207d20202020202020202020202020202020202020202020202020202020202020202020202020202020202020"
        "20202020202020202020202020202020200a0400000000000000";
    CompareBinaryFilesHex("full_words.npy", fullWordsBin, expectedFullWordsHex);
}

class CustomBitVolts_PersystRecordEngineTests : public PersystRecordEngineUnitTests
{
    void SetUp() override
    {
        mBitVolts = 0.195;
        PersystRecordEngineUnitTests::SetUp();
    }
};

//From RecordNodeTests.cpp - uses same binary file writes
TEST_F(CustomBitVolts_PersystRecordEngineTests, Test_RespectsBitVolts)
{
    GTEST_SKIP() << "Requires headless mode support.";

    int numSamples = 100;
    mTester->startAcquisition(true);
    auto inputBuffer = CreateBuffer(1000.0, 20.0, mNumChannels, numSamples);
    WriteBlock(inputBuffer);
    mTester->stopAcquisition();

    std::vector<int16_t> persistedData;
    LoadContinuousDatFile(&persistedData);
    ASSERT_EQ(persistedData.size(), mNumChannels * numSamples);

    int persistedDataIdx = 0;
    // File is channel-interleaved, so ensure we iterate in the correct order:
    for (int sampleIdx = 0; sampleIdx < numSamples; sampleIdx++)
    {
        for (int chidx = 0; chidx < mNumChannels; chidx++)
        {
            auto expected_microvolts = inputBuffer.getSample(chidx, sampleIdx);
            auto expected_converted = expected_microvolts / mBitVolts;

            // Rounds to nearest int, like BinaryRecording does, and clamp within bounds
            int expected_rounded = juce::roundToInt(expected_converted);
            int16_t expected_persisted = (int16_t)std::clamp(
                expected_rounded,
                (int)MinValPossible(),
                (int)MaxValPossible());
            ASSERT_EQ(persistedData[persistedDataIdx], expected_persisted);
            persistedDataIdx++;
        }
    }
}

class MultipleStreams_PersystRecordEngineTests : public PersystRecordEngineUnitTests
{
    void SetUp() override
    {
        mStreams = 2;
        PersystRecordEngineUnitTests::SetUp();
    }
};

TEST_F(MultipleStreams_PersystRecordEngineTests, TestCorrectDirectories_MultipleStreams)
{
    GTEST_SKIP() << "Requires headless mode support.";

    mTester->startAcquisition(true, true);
    mTester->stopAcquisition();
    int i = 0;
    for (const auto& stream : mProcessor->getDataStreams())
    {

        std::filesystem::path path_result;
        DirectorySearchParameters parameters;
        parameters.streamDirName = BuildStreamFileName(stream);
        ASSERT_EQ(parameters.streamDirName, "Record_Node-2.FakeSourceNode" + String(i));
        bool search_result = ContinuousPathFor("recording.dat", &path_result, parameters);
        ASSERT_EQ(search_result, true);

        i++;
    }
}

TEST_F(MultipleStreams_PersystRecordEngineTests, TestInputOutput_MultipleStreamsContinous)
{
    GTEST_SKIP() << "Requires headless mode support.";

    mTester->startAcquisition(true, true);

    int numSamplesPerBlock = 100;
    int num_blocks = 8;
    std::vector<AudioBuffer<float>> inputBuffers;
    for (int i = 0; i < num_blocks; i++)
    {
        auto inputBuffer = CreateBuffer(100.0f * i, 10.0, mNumChannels * mStreams, numSamplesPerBlock);
        WriteBlock(inputBuffer);
        inputBuffers.push_back(inputBuffer);
    }

    mTester->stopAcquisition();

    int streamIdx = 0;
    for (const auto& stream : mProcessor->getDataStreams())
    {
        std::vector<int16_t> persistedData;
        DirectorySearchParameters parameters;
        parameters.streamDirName = BuildStreamFileName(stream);
        LoadContinuousDatFile(&persistedData, parameters);
        ASSERT_EQ(persistedData.size(), mNumChannels * numSamplesPerBlock * num_blocks);

        int persistedDataIdx = 0;
        // File is channel-interleaved, so ensure we iterate in the correct order:
        for (int blockIdx = 0; blockIdx < num_blocks; blockIdx++)
        {
            const auto& inputBuffer = inputBuffers[blockIdx];
            for (int sampleIdx = 0; sampleIdx < numSamplesPerBlock; sampleIdx++)
            {
                for (int chidx = 0; chidx < mNumChannels; chidx++)
                {
                    auto expected_microvolts = inputBuffer.getSample(chidx + streamIdx * mNumChannels, sampleIdx);
                    ASSERT_EQ(persistedData[persistedDataIdx], expected_microvolts);
                    persistedDataIdx++;
                }
            }
        }
        streamIdx++;
    }
}

TEST_F(MultipleStreams_PersystRecordEngineTests, TestLayoutFormat_MultipleStreams)
{
    GTEST_SKIP() << "Requires headless mode support.";

    mTester->startAcquisition(true, true);
    mTester->stopAcquisition();
    for (const auto& stream : mProcessor->getDataStreams())
    {
        boost::property_tree::ptree pt;
        DirectorySearchParameters parameters;
        parameters.streamDirName = BuildStreamFileName(stream);
        LoadLayoutFile(pt, parameters);
        CheckLayoutFileInfo(pt);
        CheckLayoutChannelMap(pt);
    }
}

