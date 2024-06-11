//
//  PersystLayFileFormatter.cpp
//  open-ephys-persyst-format
//
//  Created by Allen Munk on 4/5/23.
//

#include "PersystLayFileFormat.h"

PersystLayFileFormat PersystLayFileFormat::Create(String layoutFile, int samplingRate, float calibration, int waveformCount)
{
    return PersystLayFileFormat(layoutFile, samplingRate, calibration, waveformCount);
}

String PersystLayFileFormat::AddField(String field, var value) const
{
    return field + String("=") + String(value.toString()) + String("\n");
}

String PersystLayFileFormat::ToString() const
{
    String returnString;

    returnString += String("[FileInfo]\n");
    returnString += AddField("File", mDataFile);
    returnString += AddField("FileType", mFileType);
    returnString += AddField("SamplingRate", mSamplingRate);
    returnString += AddField("HeaderLength", mHeaderLength);
    returnString += AddField("Calibration", mCalibration);
    returnString += AddField("WaveformCount", mWaveformCount);
    returnString += AddField("DataType", mDataType);

    return returnString;
}

String PersystLayFileFormat::GetLayoutFilePath() const
{
    return mLayoutFile;
}

PersystLayFileFormat::PersystLayFileFormat(
    String layoutFile,
    int samplingRate,
    float calibration,
    int waveformCount) :
    mLayoutFile(layoutFile),
    mSamplingRate(samplingRate),
    mCalibration(calibration),
    mWaveformCount(waveformCount),
    mDataFile("recording.dat"),
    mFileType("Interleaved"),
    mHeaderLength(0),
    mDataType(DataSubType::bits16)
{}

PersystLayFileFormat& PersystLayFileFormat::WithDataFile(String dataFile)
{
    mDataFile = dataFile;

    return *this;
}

PersystLayFileFormat& PersystLayFileFormat::WithFileType(String fileType)
{
    mFileType = fileType;

    return *this;
}

PersystLayFileFormat& PersystLayFileFormat::WithHeaderLength(int headerLength)
{
    mHeaderLength = headerLength;

    return *this;
}

PersystLayFileFormat& PersystLayFileFormat::WithDataType(DataSubType dataType)
{
    switch (dataType)
    {
    case DataSubType::bits16:
    {
        mDataType = 0;
        break;
    }
    case DataSubType::bits32:
    {
        mDataType = 7;
        break;
    }
    default:
    {
        mDataType = 0;
    }
    }

    return *this;
}
