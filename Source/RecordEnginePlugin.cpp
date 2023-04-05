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

#include "RecordEnginePlugin.h"

RecordEnginePlugin::RecordEnginePlugin() 
{ 

}
	
RecordEnginePlugin::~RecordEnginePlugin()
{

}


RecordEngineManager* RecordEnginePlugin::getEngineManager()
{
	RecordEngineManager* man = new RecordEngineManager("CUSTOM", "Custom Format",
		&(engineFactory<RecordEnginePlugin>));
	
	return man;
}

String RecordEnginePlugin::getEngineId() const 
{
	return "CUSTOMENGINE";
}


void RecordEnginePlugin::openFiles(File rootFolder, int experimentNumber, int recordingNumber)
{

}

void RecordEnginePlugin::closeFiles()
{

}

void RecordEnginePlugin::writeContinuousData(int writeChannel, 
											   int realChannel, 
											   const float* dataBuffer, 
											   const double* ftsBuffer, 
											   int size)
{

}

void RecordEnginePlugin::writeEvent(int eventChannel, const EventPacket& event)
{

}


void RecordEnginePlugin::writeSpike(int electrodeIndex, const Spike* spike)
{

}


void RecordEnginePlugin::writeTimestampSyncText(
	uint64 streamId, 
	int64 timestamp, 
	float sourceSampleRate, 
	String text)
{

}

