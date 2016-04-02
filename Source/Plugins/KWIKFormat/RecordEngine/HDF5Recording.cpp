/*
 ------------------------------------------------------------------

 This file is part of the Open Ephys GUI
 Copyright (C) 2014 Florian Franzen

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

#include "HDF5Recording.h"
#define MAX_BUFFER_SIZE 10000

HDF5Recording::HDF5Recording() : processorIndex(-1), hasAcquired(false)
{
    //timestamp = 0;
    scaledBuffer = new float[MAX_BUFFER_SIZE];
    intBuffer = new int16[MAX_BUFFER_SIZE];
}

HDF5Recording::~HDF5Recording()
{
    delete scaledBuffer;
    delete intBuffer;
}

String HDF5Recording::getEngineID()
{
    return "KWIK";
}

// void HDF5Recording::updateTimeStamp(int64 timestamp)
// {
//     this->timestamp = timestamp;
// }

void HDF5Recording::registerProcessor(const GenericProcessor* proc)
{
    HDF5RecordingInfo* info = new HDF5RecordingInfo();
	//This is a VERY BAD thig to do. temporary only until we fix const-correctness on GenericEditor methods
	//(which implies modifying all the plugins and processors)
    info->sample_rate = const_cast<GenericProcessor*>(proc)->getSampleRate();
    info->bit_depth = 16;
    info->multiSample = false;
    infoArray.add(info);
    fileArray.add(new KWDFile());
    bitVoltsArray.add(new Array<float>);
    sampleRatesArray.add(new Array<float>);
	channelsPerProcessor.add(0);
    processorIndex++;
}

void HDF5Recording::resetChannels()
{
    processorIndex = -1;
    fileArray.clear();
	channelsPerProcessor.clear();
    bitVoltsArray.clear();
    sampleRatesArray.clear();
    processorMap.clear();
    infoArray.clear();
	recordedChanToKWDChan.clear();
    if (spikesFile)
        spikesFile->resetChannels();
}

void HDF5Recording::addChannel(int index,const Channel* chan)
{
    processorMap.add(processorIndex);
}

void HDF5Recording::openFiles(File rootFolder, int experimentNumber, int recordingNumber)
{
    String basepath = rootFolder.getFullPathName() + rootFolder.separatorString + "experiment" + String(experimentNumber);
    //KWE file
    eventFile->initFile(basepath);
    eventFile->open();

    //KWX file
    spikesFile->initFile(basepath);
    spikesFile->open();
    spikesFile->startNewRecording(recordingNumber);

    //Let's just put the first processor (usually the source node) on the KWIK for now
    infoArray[0]->name = String("Open Ephys Recording #") + String(recordingNumber);

   /* if (hasAcquired)
        infoArray[0]->start_time = (*timestamps)[getChannel(0)->sourceNodeId]; //(*timestamps).begin()->first;
    else
        infoArray[0]->start_time = 0;*/
	infoArray[0]->start_time = getTimestamp(0);

    infoArray[0]->start_sample = 0;
    eventFile->startNewRecording(recordingNumber,infoArray[0]);

    //KWD files
	recordedChanToKWDChan.clear();
	Array<int> processorRecPos;
	processorRecPos.insertMultiple(0, 0, fileArray.size());
	for (int i = 0; i < getNumRecordedChannels(); i++)
	{
		int index = processorMap[getRealChannel(i)];
		if (!fileArray[index]->isOpen())
		{
			fileArray[index]->initFile(getChannel(getRealChannel(i))->nodeId, basepath);
			infoArray[index]->start_time = getTimestamp(i);
		}

		channelsPerProcessor.set(index, channelsPerProcessor[index] + 1);
		bitVoltsArray[index]->add(getChannel(getRealChannel(i))->bitVolts);
		sampleRatesArray[index]->add(getChannel(getRealChannel(i))->sampleRate);
		if (getChannel(getRealChannel(i))->sampleRate != infoArray[index]->sample_rate)
		{
			infoArray[index]->multiSample = true;
		}
		int procPos = processorRecPos[index];
		recordedChanToKWDChan.add(procPos);
		processorRecPos.set(index, procPos+1);
	} 
#if 0
    for (int i = 0; i < processorMap.size(); i++)
    {
        int index = processorMap[i];
        if (getChannel(i)->getRecordState())
        {
			if (!fileArray[index]->isOpen())
            {
                fileArray[index]->initFile(getChannel(i)->nodeId,basepath);
                if (hasAcquired)
                    infoArray[index]->start_time = (*timestamps)[getChannel(i)->sourceNodeId]; //the timestamps of the first channel
                else
                    infoArray[index]->start_time = 0;
            }
			channelsPerProcessor.set(index, channelsPerProcessor[index] + 1);
            bitVoltsArray[index]->add(getChannel(i)->bitVolts);
            sampleRatesArray[index]->add(getChannel(i)->sampleRate);
            if (getChannel(i)->sampleRate != infoArray[index]->sample_rate)
            {
                infoArray[index]->multiSample = true;
            }
        }
    }
#endif
    for (int i = 0; i < fileArray.size(); i++)
    {
		if ((!fileArray[i]->isOpen()) && (fileArray[i]->isReadyToOpen()))
		{
			fileArray[i]->open(channelsPerProcessor[i]);
		}
        if (fileArray[i]->isOpen())
        {
           // File f(fileArray[i]->getFileName());
           // eventFile->addKwdFile(f.getFileName());

            infoArray[i]->name = String("Open Ephys Recording #") + String(recordingNumber);
            //           infoArray[i]->start_time = timestamp;
            infoArray[i]->start_sample = 0;
            infoArray[i]->bitVolts.clear();
            infoArray[i]->bitVolts.addArray(*bitVoltsArray[i]);
            infoArray[i]->channelSampleRates.clear();
            infoArray[i]->channelSampleRates.addArray(*sampleRatesArray[i]);
            fileArray[i]->startNewRecording(recordingNumber,bitVoltsArray[i]->size(),infoArray[i]);
        }
    }

    hasAcquired = true;
}

void HDF5Recording::closeFiles()
{
    eventFile->stopRecording();
    eventFile->close();
    spikesFile->stopRecording();
    spikesFile->close();
    for (int i = 0; i < fileArray.size(); i++)
    {
        if (fileArray[i]->isOpen())
        {
            fileArray[i]->stopRecording();
            fileArray[i]->close();
            bitVoltsArray[i]->clear();
			sampleRatesArray[i]->clear();
        }
		channelsPerProcessor.set(i, 0);
    }
}

void HDF5Recording::writeData(int writeChannel, int realChannel, const float* buffer, int size)
{
//	int64 t1 = Time::getHighResolutionTicks();
	double multFactor = 1 / (float(0x7fff) * getChannel(realChannel)->bitVolts);
	int index = processorMap[getChannel(realChannel)->recordIndex];
	FloatVectorOperations::copyWithMultiply(scaledBuffer, buffer, multFactor, size);
	AudioDataConverters::convertFloatToInt16LE(scaledBuffer, intBuffer, size);
	fileArray[index]->writeRowData(intBuffer, size, recordedChanToKWDChan[writeChannel]);
//	int64 t2 = Time::getHighResolutionTicks();
//	std::cout << "record time: " << float(t2 - t1) / float(Time::getHighResolutionTicksPerSecond()) << std::endl;
}

void HDF5Recording::writeEvent(int eventType, const MidiMessage& event, int64 timestamp)
{
    const uint8* dataptr = event.getRawData();
    if (eventType == GenericProcessor::TTL)
        eventFile->writeEvent(0,*(dataptr+2),*(dataptr+1),(void*)(dataptr+3),timestamp);
    else if (eventType == GenericProcessor::MESSAGE)
        eventFile->writeEvent(1,*(dataptr+2),*(dataptr+1),(void*)(dataptr+6),timestamp);
}

void HDF5Recording::addSpikeElectrode(int index, const SpikeRecordInfo* elec)
{
    spikesFile->addChannelGroup(elec->numChannels);
}
void HDF5Recording::writeSpike(int electrodeIndex, const SpikeObject& spike, int64 /*timestamp*/)
{
    spikesFile->writeSpike(electrodeIndex,spike.nSamples,spike.data,spike.timestamp);
}

void HDF5Recording::startAcquisition()
{
    eventFile = new KWEFile();
    eventFile->addEventType("TTL",HDF5FileBase::U8,"event_channels");
    eventFile->addEventType("Messages",HDF5FileBase::STR,"Text");
    spikesFile = new KWXFile();
}

RecordEngineManager* HDF5Recording::getEngineManager()
{
    RecordEngineManager* man = new RecordEngineManager("KWIK","Kwik",&(engineFactory<HDF5Recording>));
    return man;
}