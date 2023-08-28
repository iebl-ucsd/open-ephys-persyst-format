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

struct DirectorySearchParameters{
public:
    int experiment_index = 1;
    int recording_index = 1;
    std::optional<String> stream_dir_name = std::nullopt;
};


class PersystRecordEngineTests :  public ::testing::Test {
protected:
    void SetUp() override {
        tester = std::make_unique<ProcessorTester>(FakeSourceNodeParams{
            num_channels,
            sample_rate_,
            bitVolts_,
            streams_
        });

        parent_recording_dir = std::filesystem::temp_directory_path() / "persyst_record_engine_tests";
        if (std::filesystem::exists(parent_recording_dir)) {
            std::filesystem::remove_all(parent_recording_dir);
        }
        std::filesystem::create_directory(parent_recording_dir);

        // Set this before creating the record node
        tester->setRecordingParentDirectory(parent_recording_dir.string());
        processor = tester->Create<RecordNode>(Plugin::Processor::RECORD_NODE);
        std::unique_ptr<RecordEngineManager> record_engine_manager = std::unique_ptr<RecordEngineManager>(PersystRecordEngine::getEngineManager());
        processor -> overrideRecordEngine(record_engine_manager.get());
    }

    void TearDown() override {
        // Swallow errors
        std::error_code ec;
        std::filesystem::remove_all(parent_recording_dir, ec);
    }

    AudioBuffer<float> CreateBuffer(float starting_value, float step, int num_channels, int num_samples) {
        AudioBuffer<float> input_buffer(num_channels, num_samples);

        // in microvolts
        float cur_value = starting_value;
        for (int chidx = 0; chidx < num_channels; chidx++) {
            for (int sample_idx = 0; sample_idx < num_samples; sample_idx++) {
                input_buffer.setSample(chidx, sample_idx, cur_value);
                cur_value += step;
            }
        }
        return input_buffer;
    }

    void WriteBlock(AudioBuffer<float> &buffer, TTLEvent* maybe_ttl_event = nullptr) {
        auto output_buffer = tester->ProcessBlock(processor, buffer, maybe_ttl_event);
        // Assert the buffer hasn't changed after process()
        ASSERT_EQ(output_buffer.getNumSamples(), buffer.getNumSamples());
        ASSERT_EQ(output_buffer.getNumChannels(), buffer.getNumChannels());
        for (int chidx = 0; chidx < output_buffer.getNumChannels(); chidx++) {
            for (int sample_idx = 0; sample_idx < output_buffer.getNumSamples(); ++sample_idx) {
                ASSERT_EQ(
                    output_buffer.getSample(chidx, sample_idx),
                    buffer.getSample(chidx, sample_idx));
            }
        }
    }

    bool EventsPathFor(const std::string& basename, std::filesystem::path* path, DirectorySearchParameters parameters = DirectorySearchParameters()) {
        std::filesystem::path partial_path;
        auto success = SubRecordingPathFor("events", "TTL", &partial_path, parameters);
        if (!success) {
            return false;
        }
        auto ret = partial_path / basename;
        if (std::filesystem::exists(ret)) {
            *path = ret;
            return true;
        } else {
            return false;
        }
    }
    
    std::vector<char> LoadNpyFileBinaryFullpath(const std::string& fullpath) {
        std::ifstream data_ifstream(fullpath, std::ios::binary | std::ios::in);

        data_ifstream.seekg(0, std::ios::end);
        std::streampos fileSize = data_ifstream.tellg();
        data_ifstream.seekg(0, std::ios::beg);

        std::vector<char> persisted_data(fileSize);
        data_ifstream.read(persisted_data.data(), fileSize);
        return persisted_data;
    }
    
    void CompareBinaryFilesHex(const std::string& filename, const std::vector<char> bin_data, const std::string& expected_bin_data_hex) {
        std::vector<char> expected_bin_data;
        for (int i = 0; i < expected_bin_data_hex.length(); i += 2) {
            std::string byteString = expected_bin_data_hex.substr(i, 2);
            char byte = (char) strtol(byteString.c_str(), nullptr, 16);
            expected_bin_data.push_back(byte);
        }

        // Create a string rep of the actual sample numbers bin in case it fails, to help debugging
        std::stringstream bin_data_hex_ss;
        bin_data_hex_ss << "Expected data for " << filename << " in hex to be=" << expected_bin_data_hex
                        << " but received=";
        bin_data_hex_ss << std::hex;
        for (int i = 0; i < bin_data.size(); i++) {
            bin_data_hex_ss << std::setw(2) << std::setfill('0') << (int)bin_data[i];
        }
        std::string err_msg = bin_data_hex_ss.str();

        ASSERT_EQ(bin_data.size(), expected_bin_data.size()) << err_msg;
        for (int i = 0; i < bin_data.size(); i++) {
            ASSERT_EQ(bin_data[i], expected_bin_data[i])
                                << err_msg
                                << " (error on index " << i << ")";
        }
    }
    
    bool SubRecordingPathFor(
        const std::string& subrecording_dirname,
        const std::string& basename,
        std::filesystem::path* path,
        DirectorySearchParameters parameters)
    {
        // Do verifications:
        auto recording_dir = std::filesystem::directory_iterator(parent_recording_dir)->path();
        std::stringstream ss;
        ss << "Record Node " << processor->getNodeId();
        
        std::stringstream experiment_string;
        experiment_string << "experiment" << parameters.experiment_index;
        std::stringstream recording_string;
        recording_string << "recording" << parameters.recording_index;
        auto recording_dir2 = recording_dir / ss.str() / experiment_string.str()/ recording_string.str() / subrecording_dirname;
        if (!std::filesystem::exists(recording_dir2)) {
            return false;
        }

        std::filesystem::path recording_dir3;
        for (const auto &subdir : std::filesystem::directory_iterator(recording_dir2)) {
            auto subdir_basename = subdir.path().filename().string();
            if(parameters.stream_dir_name.has_value()) {
                if(subdir_basename == parameters.stream_dir_name.value()){
                    recording_dir3 = subdir.path();
                }
            }
            else{
                //Needs to work for multiple streams
                if (subdir_basename.find("FakeSourceNode") != std::string::npos) {
                    recording_dir3 = subdir.path();
                }
            }
        }

        if (!std::filesystem::exists(recording_dir3)) {
            return false;
        }

        auto ret = recording_dir3 / basename;
        if (!std::filesystem::exists(ret)) {
            return false;
        }
        *path = ret;
        return true;
    }

    bool ContinuousPathFor(const std::string& basename, std::filesystem::path* path, DirectorySearchParameters parameters) {
        return SubRecordingPathFor("continuous", basename, path, parameters);
    }
    

    void MaybeLoadContinuousDatFile(std::vector<int16_t> *output, bool *success, DirectorySearchParameters parameters) {
        // Do verifications:
        std::filesystem::path continuous_dat_path;
        *success = ContinuousPathFor("recording.dat", &continuous_dat_path, parameters);
        if (!*success) {
            return;
        }

        std::ifstream continuous_ifstream(continuous_dat_path.string(), std::ios::binary | std::ios::in);

        continuous_ifstream.seekg(0, std::ios::end);
        std::streampos fileSize = continuous_ifstream.tellg();
        continuous_ifstream.seekg(0, std::ios::beg);
        if (fileSize % sizeof(int16_t) != 0) {
            *success = false;
            return;
        }

        std::vector<int16_t> persisted_data(fileSize / sizeof(int16_t));
        continuous_ifstream.read((char *) persisted_data.data(), fileSize);
        *success = true;
        *output = persisted_data;
    }

    void LoadContinuousDatFile(std::vector<int16_t> *output, DirectorySearchParameters parameters = DirectorySearchParameters()) {
        bool success = false;
        MaybeLoadContinuousDatFile(output, &success, parameters);
        ASSERT_TRUE(success);
    }
    
    void MaybeLoadLayoutFile(boost::property_tree::ptree &pt, bool *success, DirectorySearchParameters parameters) {
        // Do verifications:
        std::filesystem::path continuous_dat_path;
        *success = ContinuousPathFor("recording.lay", &continuous_dat_path, parameters);
        if (!*success) {
            return;
        }

        try {
            boost::property_tree::ini_parser::read_ini(continuous_dat_path.string(), pt);
        }
        catch (const boost::property_tree::ini_parser_error & e){
            *success = false;
            return;
        }
        *success = true;
        return;

    }
    
    void LoadLayoutFile(boost::property_tree::ptree &pt, DirectorySearchParameters parameters = DirectorySearchParameters()) {
        bool success = false;
        MaybeLoadLayoutFile(pt, &success, parameters);
        ASSERT_TRUE(success);
    }
    
    void CheckLayoutFileInfo( const boost::property_tree::ptree& pt) {
        boost::property_tree::ptree::const_assoc_iterator exists = pt.find("FileInfo");
        if(exists == pt.not_found()) {
            FAIL() << "Test failed; layout file didn't have a FileInfo section";
        }
        ASSERT_EQ(pt.get_child("FileInfo").size(), 7);
        ASSERT_EQ(pt.get<std::string>("FileInfo.File"), "recording.dat");
        ASSERT_EQ(pt.get<std::string>("FileInfo.FileType"), "Interleaved");
        ASSERT_EQ(pt.get<float>("FileInfo.SamplingRate"), sample_rate_);
        ASSERT_EQ(pt.get<int>("FileInfo.HeaderLength"), 0);
        ASSERT_EQ(pt.get<float>("FileInfo.Calibration"), bitVolts_);
        ASSERT_EQ(pt.get<int>("FileInfo.WaveformCount"), num_channels);
        ASSERT_EQ(pt.get<int>("FileInfo.DataType"), 0);
    }
    
    bool isStringAPositiveInteger(std::string s) {
        return !s.empty() && (std::find_if(s.begin(), s.end(), [](unsigned char c) {return std::isdigit(c);})  != s.end() );
    }
    
    void CheckLayoutSampleTimes(const boost::property_tree::ptree& pt, int sample_rate, int sample_per_block) {
        boost::property_tree::ptree::const_assoc_iterator exists = pt.find("SampleTimes");
        if(exists == pt.not_found()) {
            FAIL() << "Test failed; layout file didn't have a SampleTimes section";
        }
        
        int expected_sample_index = 0;
        double expected_sample_time = 0;
        boost::property_tree::ptree reference_samples = pt.get_child("SampleTimes");
        for(boost::property_tree::ptree::const_iterator it = reference_samples.begin(); it != reference_samples.end(); it++) {
            //Key should be an integer corresponding to the reference sample's index
            if(isStringAPositiveInteger((*it).first)) {
                ASSERT_EQ(std::stoi((*it).first), expected_sample_index);
            }
            else {
                FAIL() << "Test failed; SampleTimes key not a positive integer";
            }
            
            //Value should be a ptree whose value is a floating point timestamp
            boost::optional<double> sample_time = ((*it).second.get_value_optional<double>());
            if(sample_time.has_value()) {
                ASSERT_NEAR(sample_time.value(), expected_sample_time, .001);
            }
            else {
                FAIL() << "Test failed; SampleTimes value not a floating point number";

            }
            expected_sample_index += sample_per_block;
            expected_sample_time += ((double)sample_per_block/sample_rate);
        }
    }
    
    void UpdateSourceNodesStreamParams() {
        FakeSourceNode* sn = dynamic_cast<FakeSourceNode*>(tester->getSourceNode());
        sn->setParams(FakeSourceNodeParams{
            num_channels,
            sample_rate_,
            bitVolts_
        });
        tester->updateSourceNodeSettings();
        
    }
    
    String BuildStreamFileName(const DataStream * stream) {
        String return_string;
        return_string = stream -> getSourceNodeName().replaceCharacters(" @", "__") + "-";
        return_string += stream -> getSourceNodeId();
        return_string += "." + stream -> getName();
        return return_string;
    }


    static int16_t min_val_possible() {
        // The min value is actually -32767 in the math in RecordNode, not -32768 like the "true" min for int16_t
        return (std::numeric_limits<int16_t>::min)() + 1;
    }

    static int16_t max_val_possible() {
        return (std::numeric_limits<int16_t>::max)();
    }

    RecordNode *processor;
    int num_channels = 8;
    float bitVolts_ = 1.0;
    std::unique_ptr<ProcessorTester> tester;
    std::filesystem::path parent_recording_dir;
    float sample_rate_ = 1.0;
    int streams_ = 1;
};

//From RecordNodeTests.cpp - uses same binary file writes
TEST_F(PersystRecordEngineTests, TestInputOutput_Continuous_Single) {
    int num_samples = 100;
    tester->startAcquisition(true);

    auto input_buffer = CreateBuffer(1000.0, 20.0, num_channels, num_samples);
    WriteBlock(input_buffer);

    // The record node always flushes its pending writes when stopping acquisition, so we don't need to sleep before
    // stopping.
    tester->stopAcquisition();

    std::vector<int16_t> persisted_data;
    LoadContinuousDatFile(&persisted_data);
    ASSERT_EQ(persisted_data.size(), num_channels * num_samples);

    int persisted_data_idx = 0;
    // File is channel-interleaved, so ensure we iterate in the correct order:
    for (int sample_idx = 0; sample_idx < num_samples; sample_idx++) {
        for (int chidx = 0; chidx < num_channels; chidx++) {
            auto expected_microvolts = input_buffer.getSample(chidx, sample_idx);
            ASSERT_EQ(persisted_data[persisted_data_idx], expected_microvolts);
            persisted_data_idx++;
        }
    }
}

//From RecordNodeTests.cpp - uses same binary file writes
TEST_F(PersystRecordEngineTests, TestInputOutput_Continuous_Multiple) {
    tester->startAcquisition(true);

    int num_samples_per_block = 100;
    int num_blocks = 8;
    std::vector<AudioBuffer<float>> input_buffers;
    for (int i = 0; i < num_blocks; i++) {
        auto input_buffer = CreateBuffer(1000.0f * i, 20.0, num_channels, num_samples_per_block);
        WriteBlock(input_buffer);
        input_buffers.push_back(input_buffer);
    }

    tester->stopAcquisition();

    std::vector<int16_t> persisted_data;
    LoadContinuousDatFile(&persisted_data);
    ASSERT_EQ(persisted_data.size(), num_channels * num_samples_per_block * num_blocks);

    int persisted_data_idx = 0;
    // File is channel-interleaved, so ensure we iterate in the correct order:
    for (int block_idx = 0; block_idx < num_blocks; block_idx++) {
        const auto& input_buffer = input_buffers[block_idx];
        for (int sample_idx = 0; sample_idx < num_samples_per_block; sample_idx++) {
            for (int chidx = 0; chidx < num_channels; chidx++) {
                auto expected_microvolts = input_buffer.getSample(chidx, sample_idx);
                ASSERT_EQ(persisted_data[persisted_data_idx], expected_microvolts);
                persisted_data_idx++;
            }
        }
    }
}

//From RecordNodeTests.cpp - uses same binary file writes
TEST_F(PersystRecordEngineTests, TestEmpty) {
    tester->startAcquisition(true);
    tester->stopAcquisition();

    std::vector<int16_t> persisted_data;
    LoadContinuousDatFile(&persisted_data);
    ASSERT_EQ(persisted_data.size(), 0);
}

TEST_F(PersystRecordEngineTests, TestLayoutFormat) {
    tester->startAcquisition(true);
    tester->stopAcquisition();
    boost::property_tree::ptree pt;
    LoadLayoutFile(pt);
    CheckLayoutFileInfo(pt);
    
}

TEST_F(PersystRecordEngineTests, TestSampleIndexes_Continuous_Multiple) {
    sample_rate_ = 100;
    UpdateSourceNodesStreamParams();

    tester->startAcquisition(true);

    int num_samples_per_block = 110;
    int num_blocks = 8;
    std::vector<AudioBuffer<float>> input_buffers;
    for (int i = 0; i < num_blocks; i++) {
        auto input_buffer = CreateBuffer(1000.0f * i, 20.0, num_channels, num_samples_per_block);
        WriteBlock(input_buffer);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        input_buffers.push_back(input_buffer);
    }

    tester->stopAcquisition();
    
    boost::property_tree::ptree pt;
    LoadLayoutFile(pt);
    CheckLayoutFileInfo(pt);
    CheckLayoutSampleTimes(pt, sample_rate_, num_samples_per_block);

}

TEST_F(PersystRecordEngineTests, TestLayoutFormatChangedFiles) {
    tester->startAcquisition(true);
    tester->stopAcquisition();
    boost::property_tree::ptree pt;
    DirectorySearchParameters parameters;
    LoadLayoutFile(pt, parameters);
    CheckLayoutFileInfo(pt);
    
    bitVolts_ = 0.195;
    UpdateSourceNodesStreamParams();
    tester->startAcquisition(true);
    tester->stopAcquisition();
    parameters.experiment_index++;
    LoadLayoutFile(pt, parameters);
    CheckLayoutFileInfo(pt);
    
    sample_rate_ = 1000;
    UpdateSourceNodesStreamParams();
    tester->startAcquisition(true);
    tester->stopAcquisition();
    parameters.experiment_index++;
    LoadLayoutFile(pt, parameters);
    CheckLayoutFileInfo(pt);
    
    num_channels = 32;
    UpdateSourceNodesStreamParams();
    tester->startAcquisition(true);
    tester->stopAcquisition();
    parameters.experiment_index++;
    LoadLayoutFile(pt, parameters);
    CheckLayoutFileInfo(pt);
    
}

//From RecordNodeTests.cpp - uses same event file writes
TEST_F(PersystRecordEngineTests, Test_PersistsEvents) {
    processor->setRecordEvents(true);
    processor->updateSettings();

    tester->startAcquisition(true);
    int num_samples = 5;

    auto stream_id = processor->getDataStreams()[0]->getStreamId();
    auto event_channels = tester->GetSourceNodeDataStream(stream_id)->getEventChannels();
    ASSERT_GE(event_channels.size(), 1);
    TTLEventPtr event_ptr = TTLEvent::createTTLEvent(
        event_channels[0],
        1,
        2,
        true);
    auto input_buffer = CreateBuffer(1000.0, 20.0, num_channels, num_samples);
    WriteBlock(input_buffer, event_ptr.get());
    tester->stopAcquisition();

    std::filesystem::path sample_numbers_path;
    ASSERT_TRUE(EventsPathFor("sample_numbers.npy", &sample_numbers_path));
    auto sample_numbers_bin = LoadNpyFileBinaryFullpath(sample_numbers_path.string());

    /**
     * Same logic as above:
     *      import numpy as np, io, binascii; b = io.BytesIO(); np.save(b, np.array([1], dtype=np.int64)); b.seek(0); print(binascii.hexlify(b.read()))
     */
    std::string expected_sample_numbers_hex =
        "934e554d5059010076007b276465736372273a20273c6938272c2027666f727472616e5f6f72646572273a2046616c73652c2027736861"
        "7065273a2028312c292c207d20202020202020202020202020202020202020202020202020202020202020202020202020202020202020"
        "20202020202020202020202020202020200a0100000000000000";
    CompareBinaryFilesHex("sample_numbers.npy", sample_numbers_bin, expected_sample_numbers_hex);

    std::filesystem::path full_words_path;
    ASSERT_TRUE(EventsPathFor("full_words.npy", &full_words_path));
    auto full_words_bin = LoadNpyFileBinaryFullpath(full_words_path.string());

    /**
     * Same logic as above:
     *      import numpy as np, io, binascii; b = io.BytesIO(); np.save(b, np.array([4], dtype=np.uint64)); b.seek(0); print(binascii.hexlify(b.read()))
     */
    std::string expected_full_words_hex =
        "934e554d5059010076007b276465736372273a20273c7538272c2027666f727472616e5f6f72646572273a2046616c73652c2027736861"
        "7065273a2028312c292c207d20202020202020202020202020202020202020202020202020202020202020202020202020202020202020"
        "20202020202020202020202020202020200a0400000000000000";
    CompareBinaryFilesHex("full_words.npy", full_words_bin, expected_full_words_hex);
}

class CustomBitVolts_PersystRecordEngineTests : public PersystRecordEngineTests {
    void SetUp() override {
        bitVolts_ = 0.195;
        PersystRecordEngineTests::SetUp();
    }
};

//From RecordNodeTests.cpp - uses same binary file writes
TEST_F(CustomBitVolts_PersystRecordEngineTests, Test_RespectsBitVolts) {
    int num_samples = 100;
    tester->startAcquisition(true);
    auto input_buffer = CreateBuffer(1000.0, 20.0, num_channels, num_samples);
    WriteBlock(input_buffer);
    tester->stopAcquisition();

    std::vector<int16_t> persisted_data;
    LoadContinuousDatFile(&persisted_data);
    ASSERT_EQ(persisted_data.size(), num_channels * num_samples);

    int persisted_data_idx = 0;
    // File is channel-interleaved, so ensure we iterate in the correct order:
    for (int sample_idx = 0; sample_idx < num_samples; sample_idx++) {
        for (int chidx = 0; chidx < num_channels; chidx++) {
            auto expected_microvolts = input_buffer.getSample(chidx, sample_idx);
            auto expected_converted = expected_microvolts / bitVolts_;

            // Rounds to nearest int, like BinaryRecording does, and clamp within bounds
            int expected_rounded = juce::roundToInt(expected_converted);
            int16_t expected_persisted = (int16_t) std::clamp(
                expected_rounded,
                (int) min_val_possible(),
                (int) max_val_possible());
            ASSERT_EQ(persisted_data[persisted_data_idx], expected_persisted);
            persisted_data_idx++;
        }
    }
}

class MultipleStreams_PersystRecordEngineTests : public PersystRecordEngineTests {
    void SetUp() override {
        streams_ = 2;
        PersystRecordEngineTests::SetUp();
    }
};

TEST_F(MultipleStreams_PersystRecordEngineTests, TestCorrectDirectories_MultipleStreams) {
    tester->startAcquisition(true, true);
    tester->stopAcquisition();
    int i = 0;
    for(const auto & stream: processor->getDataStreams()) {
        
        std::filesystem::path path_result;
        DirectorySearchParameters parameters;
        parameters.stream_dir_name = BuildStreamFileName(stream);
        ASSERT_EQ(parameters.stream_dir_name, "Record_Node-2.FakeSourceNode"+String(i));
        bool search_result = ContinuousPathFor("recording.dat", &path_result, parameters);
        ASSERT_EQ(search_result, true);

        i++;
    }
}

TEST_F(MultipleStreams_PersystRecordEngineTests, TestInputOutput_MultipleStreamsContinous) {
    tester->startAcquisition(true, true);

    int num_samples_per_block = 100;
    int num_blocks = 8;
    std::vector<AudioBuffer<float>> input_buffers;
    for (int i = 0; i < num_blocks; i++) {
        auto input_buffer = CreateBuffer(100.0f * i, 10.0, num_channels * streams_, num_samples_per_block);
        WriteBlock(input_buffer);
        input_buffers.push_back(input_buffer);
    }

    tester->stopAcquisition();
    
    int stream_idx = 0;
    for(const auto & stream: processor->getDataStreams()) {
        std::vector<int16_t> persisted_data;
        DirectorySearchParameters parameters;
        parameters.stream_dir_name = BuildStreamFileName(stream);
        LoadContinuousDatFile(&persisted_data, parameters);
        ASSERT_EQ(persisted_data.size(), num_channels * num_samples_per_block * num_blocks);

        int persisted_data_idx = 0;
        // File is channel-interleaved, so ensure we iterate in the correct order:
        for (int block_idx = 0; block_idx < num_blocks; block_idx++) {
            const auto& input_buffer = input_buffers[block_idx];
            for (int sample_idx = 0; sample_idx < num_samples_per_block; sample_idx++) {
                for (int chidx = 0; chidx < num_channels; chidx++) {
                    auto expected_microvolts = input_buffer.getSample(chidx + stream_idx * num_channels, sample_idx);
                    ASSERT_EQ(persisted_data[persisted_data_idx], expected_microvolts);
                    persisted_data_idx++;
                }
            }
        }
        stream_idx++;
    }
}

TEST_F(MultipleStreams_PersystRecordEngineTests, TestLayoutFormat_MultipleStreams) {
    tester->startAcquisition(true, true);
    tester->stopAcquisition();
    for(const auto & stream: processor->getDataStreams()) {
        boost::property_tree::ptree pt;
        DirectorySearchParameters parameters;
        parameters.stream_dir_name = BuildStreamFileName(stream);
        LoadLayoutFile(pt, parameters);
        CheckLayoutFileInfo(pt);
    }
    
}

