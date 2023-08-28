//
//  PersystLayFileFormatter.cpp
//  open-ephys-persyst-format
//
//  Created by Allen Munk on 4/5/23.
//

#include "PersystLayFileFormat.h"


PersystLayFileFormat PersystLayFileFormat::create(String layoutFile, int samplingRate, float calibration, int waveformCount) {
    return PersystLayFileFormat(layoutFile, samplingRate, calibration, waveformCount);
}

String PersystLayFileFormat::addField(String field, var value){
    return field+String("=")+String(value.toString())+String("\n");
}

String PersystLayFileFormat::toString(){
    String returnString;
    returnString += String("[FileInfo]\n");
    returnString += addField("File", m_dataFile);
    returnString += addField("FileType", m_fileType);
    returnString += addField("SamplingRate", m_samplingRate);
    returnString += addField("HeaderLength", m_headerLength);
    returnString += addField("Calibration", m_calibration);
    returnString += addField("WaveformCount", m_waveformCount);
    returnString += addField("DataType", m_dataType);
    return returnString;
}

String PersystLayFileFormat::getLayoutFilePath(){
    return m_layoutFile;
}

PersystLayFileFormat::PersystLayFileFormat(String layoutFile,
                                           int samplingRate,
                                           float calibration,
                                           int waveformCount
                                           ) :
                                            m_layoutFile(layoutFile),
                                            m_samplingRate(samplingRate),
                                            m_calibration(calibration),
                                            m_waveformCount(waveformCount),
                                            m_dataFile("recording.dat"),
                                            m_fileType("Interleaved"),
                                            m_headerLength(0),
                                            m_dataType(DataSubType::bits16){}


PersystLayFileFormat& PersystLayFileFormat::withDataFile(String dataFile) {
    m_dataFile = dataFile;
    return *this;
}
PersystLayFileFormat& PersystLayFileFormat::withFileType(String fileType) {
    m_fileType = fileType;
    return *this;
}
PersystLayFileFormat& PersystLayFileFormat::withHeaderLength(int headerLength) {
    m_headerLength = headerLength;
    return *this;
}
PersystLayFileFormat& PersystLayFileFormat::withDataType(DataSubType dataType) {
    switch (dataType) {
        case DataSubType::bits16 : {
            m_dataType = 0;
            break;
        }
        case DataSubType::bits32 : {
            m_dataType = 7;
            break;
        }
        default : {
            m_dataType = 0;
        }
    }
    return *this;
}
