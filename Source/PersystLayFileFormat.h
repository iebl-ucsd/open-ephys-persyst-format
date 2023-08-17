//
//  PersystLayFileFormatter.hpp
//  open-ephys-persyst-format
//
//  Created by Allen Munk on 4/5/23.
//

#ifndef PersystLayFileFormatter_h
#define PersystLayFileFormatter_h

#include <JuceHeader.h>

enum DataSubType {bits16, bits32};

class PersystLayFileFormat {
public:
    
    static PersystLayFileFormat create(String layoutFile, int samplingRate, float calibration, int waveformCount);
    
    PersystLayFileFormat& withDataFile(String dataFile);
    PersystLayFileFormat& withFileType(String fileType);
    PersystLayFileFormat& withHeaderLength(int headerLength);
    PersystLayFileFormat& withDataType(DataSubType dataType);
        
    String toString();
    
    String getLayoutFilePath();
private:
    
    PersystLayFileFormat(String layoutFile, int samplingRate, float calibration, int waveformCount);
    
    String addField(String field, var value);
    
    String m_layoutFile;
    String m_dataFile;
    String m_fileType;
    int m_samplingRate;
    int m_headerLength;
    float m_calibration;
    int m_waveformCount;
    int m_dataType;
};


#endif /* PersystLayFileFormatter_h */
