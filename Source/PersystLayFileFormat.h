//
//  PersystLayFileFormatter.hpp
//  open-ephys-persyst-format
//
//  Created by Allen Munk on 4/5/23.
//

#ifndef PersystLayFileFormatter_h
#define PersystLayFileFormatter_h

#include <JuceHeader.h>

class PersystLayFileFormat {
public:
    PersystLayFileFormat(String layoutFile, String dataFile, int samplingRate, float calibration, int waveformCount, String fileType = "Interleaved", int headerLength = 0, int dataType = 0);
    
    String addField(String field, var value);
    
    String toString();
    
    String getLayoutFilePath();
private:
    String layoutFile;
    String dataFile;
    String fileType;
    int samplingRate;
    int headerLength;
    float calibration;
    int waveformCount;
    int dataType;
};


#endif /* PersystLayFileFormatter_h */
