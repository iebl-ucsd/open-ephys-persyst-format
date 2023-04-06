//
//  PersystLayFileFormatter.cpp
//  open-ephys-persyst-format
//
//  Created by Allen Munk on 4/5/23.
//

#include "PersystLayFileFormat.h"

PersystLayFileFormat::PersystLayFileFormat(String layoutFile, String dataFile, int samplingRate, float calibration, int waveformCount, String fileType, int headerLength, int dataType) : layoutFile(layoutFile), dataFile(dataFile),
    samplingRate(samplingRate),
    calibration(calibration),
    waveformCount(waveformCount),
    fileType(fileType),
    headerLength(headerLength),
    dataType(dataType){}

String PersystLayFileFormat::addField(String field, var value){
    return field+String("=")+String(value)+String("\n");
}

String PersystLayFileFormat::toString(){
    String returnString;
    returnString += String("[FileInfo]\n");
    returnString += addField("File", dataFile);
    returnString += addField("FileType", fileType);
    returnString += addField("SamplingRate", samplingRate);
    returnString += addField("HeaderLength", headerLength);
    returnString += addField("Calibration", calibration);
    returnString += addField("WaveformCount", waveformCount);
    returnString += addField("DataType", dataType);
    return returnString;
}

String PersystLayFileFormat::getLayoutFilePath(){
    return layoutFile;
}

