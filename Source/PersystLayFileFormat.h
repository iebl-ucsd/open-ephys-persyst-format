//
//  PersystLayFileFormatter.hpp
//  open-ephys-persyst-format
//
//  Created by Allen Munk on 4/5/23.
//

#ifndef PersystLayFileFormatter_h
#define PersystLayFileFormatter_h

#include <JuceHeader.h>

enum DataSubType { bits16, bits32 };

class PersystLayFileFormat
{
public:
    static PersystLayFileFormat Create(String layoutFile, int samplingRate, float calibration, int waveformCount);

    PersystLayFileFormat& WithDataFile(String dataFile);
    PersystLayFileFormat& WithFileType(String fileType);
    PersystLayFileFormat& WithHeaderLength(int headerLength);
    PersystLayFileFormat& WithDataType(DataSubType dataType);

    String ToString() const;

    String GetLayoutFilePath() const;

private:
    PersystLayFileFormat(String layoutFile, int samplingRate, float calibration, int waveformCount);

    String AddField(String field, var value) const;

private:
    String mLayoutFile;
    String mDataFile;
    String mFileType;
    int mSamplingRate;
    int mHeaderLength;
    float mCalibration;
    int mWaveformCount;
    int mDataType;
};
#endif /* PersystLayFileFormatter_h */