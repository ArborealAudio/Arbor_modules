/**
* RingBuffer.h
* Simple class for ring buffer of audio samples
*/

#pragma once
#include <JuceHeader.h>
#include <algorithm>

template <typename T>
struct RingBuffer
{
	void setSize(int numChannels, int numSamples)
	{
		this->numChannels = numChannels;
		size = numSamples;
		data = (T**)malloc(sizeof(T*) * numChannels);
		for (size_t ch = 0; ch < numChannels; ++ch)
			data[ch] = (T*)malloc(sizeof(T) * numSamples);

		writePos.resize(numChannels);
		readPos.resize(numChannels);
	}

	void writeData(const T *const *input, int numChannels, int numSamples)
	{
		int numToWrite = jmin(numSamples, size);
		for (size_t ch = 0; ch < numChannels; ++ch)
		{
			for (size_t i = 0; i < numToWrite; ++i)
			{
				data[ch][writePos[ch]] = input[ch][i];
				writePos[ch] = (writePos[ch] + size - 1) % size;
			}
		}
	}

	// read a subset of this buffer's data into output
	void readData(T *const *output, int numChannels, int numSamples)
	{
		for (size_t ch = 0; ch < numChannels; ++ch)
		{
			for (size_t i = 0; i < numSamples; ++i)
			{
				output[ch][i] = data[ch][readPos[ch]];
				readPos[ch] = (readPos[ch] + size - 1) % size;
			}
		}
	}

	// fill buffer with zeroes
	void clear()
	{
		for (size_t ch = 0; ch < numChannels; ++ch)
		{
			for (size_t i = 0; i < size; ++i)
			{
				data[ch][i] = (T)0.0;
			}
		}
	}

  T **getData() { return data; }

  int size = 0;
	int numChannels = 0;
	
private:

	T **data;

	std::vector<size_t> writePos;
	std::vector<size_t> readPos;
};
