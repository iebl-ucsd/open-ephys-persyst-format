# Persyst Format Record Engine

This repository contains a Record Engine that formats recordings from the [Open Ephys GUI](https://github.com/open-ephys/plugin-GUI) into files in the Persyst proprietary format. Record Engine plugins allow the GUI's Record Node to write data into a new format. 

## Persyst Format

The Persyst format produces two files: a binary data file containing channel voltage data (.dat) and a layout file that describes the format of the binary file (.lay)

### Layout File (.lay)

The layout file is organized into sections with a \[Section Header\] followed by rows of section attributes.

#### \[FileInfo\]
- **File** Name of the Dat file or OEM record. May be either a full path or just the file name, in which case the file must be in the same folder as the .lay file.
- **FileType** Type of  file. Should be set as "Interleaved", which desribes an interleaved binary file.
- **SamplingRate** Sampling rate in Hz.
- **HeaderLength** Length of the header in the .dat file. Default is 0.
- **Calibration** Coefficient to convert  values in the .dat file to microvolts (uV).
- **WaveformCount** Number of channels in the .dat file recording.
- **Data Type** Data sub-type. Set to 0 for 16-bit recording, 7 for 32-bit recording.

#### \[SampleTimes\]
The attributes in this section are used to synchronize samples with timestamps. Each row should be formatted as: *Sample Index(int)*=*Timestamp(float in seconds)*.


#### Example .lay Files:

\[FileInfo\]  
File=recording.dat  
FileType=Interleaved   
SamplingRate=40000  
HeaderLength=0  
Calibration=0.05  
WaveformCount=16  
DataType=0  
\[SampleTimes\]  
0=0  
20000=0.51  
40000=1.02  
60000=1.53  
80000=2.04  

### Data File (.dat)

The data stored in the .dat file should correlate to what is described in the .lay file. Channels should interleaved and ordered by sample e.g. `Sample0Channel0Sample0Channel1...Sample0ChannelNSample1Channel0`. Values will be read as either 16 or 32 bit **signed** integers depending on the DataType. To convert to uV, the binary integers are multiplied by the Calibration value.

## Installation

This plugin should be installed using the pre-compiled library in the releases tab. Currently only Windows is supported. The Open Ephys GUI should be installed beforehand. To install, download the plugin .zip and extract contents. Move the plugin to the `plugins/` directory under the open-ephys executable.