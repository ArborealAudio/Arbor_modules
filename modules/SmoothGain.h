/**
 * SmoothGain.h
 * Helpers for computing a smoothed gain on a block of audio samples, or for processing a crossfade btw audio blocks
*/
#ifndef SMOOTHGAIN_H
#define SMOOTHGAIN_H

template <typename T>
struct SmoothGain
{
    /// @brief apply a smoothed gain to an array of samples
    /// @tparam T sample type
    /// @param lastGain reference to gain state which can be update if needed
    /// @param updateGain whether or not to update the gain state, true by default
    template <typename FloatType>
    inline static void applySmoothGain(T *in, size_t numSamples, FloatType currentGain, FloatType& lastGain, bool updateGain = true)
    {
        if (lastGain == currentGain)
        {
            for (size_t i = 0; i < numSamples; ++i)
                in[i] *= lastGain;
            return;
        }

        auto inc = (currentGain - lastGain) / numSamples;
        
        for (size_t i = 0; i < numSamples; ++i)
        {
            in[i] *= lastGain;
            lastGain += inc;
        }

        if (updateGain)
            lastGain = currentGain;
    }

    /// @brief apply a smoothed gain to a block of samples
    /// @tparam T sample type
    /// @param lastGain reference to gain state which can be update if needed
    /// @param updateGain whether or not to update the gain state, true by default
    template <typename Block, typename FloatType>
    inline static void applySmoothGain(Block& block, FloatType currentGain, FloatType& lastGain, bool updateGain = true)
    {
        if (lastGain == currentGain)
        {
            block.multiplyBy(lastGain);
            return;
        }

        auto inc = (currentGain - lastGain) / block.getNumSamples();

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                block.getChannelPointer(ch)[i] *= (T)lastGain;
            
            lastGain += inc;
        }

        if (updateGain)
            lastGain = currentGain;
    }
};

/**
 * A struct for doing smooth crossfades between processed and un-processed audio
*/
struct Crossfade
{
    template <typename T>
    inline static void process(const T *dryIn, T *out, size_t numSamples, float startGain = 0.f, float endGain = 1.f)
    {
        float inc = (endGain - startGain) / (float)numSamples;
        for (size_t i = 0; i < numSamples; ++i)
        {
            out[i] = (out[i] * startGain) + (1.f - startGain) * dryIn[i];
            startGain += inc;
        }
    }

    // fades from dry to wet, optionally with a custom gain ramp
    template <typename T>
    inline static void process(const AudioBuffer<T> &dry, AudioBuffer<T> &wet, float startGain = 0.f, float endGain = 1.f)
    {
        assert(dry.getNumSamples() == wet.getNumSamples());
        for (size_t ch = 0; ch < dry.getNumChannels(); ++ch)
        {
            process(dry.getReadPointer(ch), wet.getWritePointer(ch), dry.getNumSamples(), startGain, endGain);
        }
    }

    template <typename Block>
    inline static void process(const Block &dryBlock, Block &outBlock)
    {
        assert(dryBlock.getNumSamples() == outBlock.getNumSamples());
        for (size_t ch = 0; ch < outBlock.getNumChannels(); ++ch)
        {
            process(dryBlock.getChannelPointer(ch), outBlock.getChannelPointer(ch), outBlock.getNumSamples());
        }
    }

    bool complete = false;
    float fadeTimeSec = 1.f;
    int fadeLengthSamples = 0;
    float startGain, endGain = 0.f;

    void setFadeTime(float sampleRate, float newFadeTimeSec)
    {
        fadeTimeSec = newFadeTimeSec;
        fadeLengthSamples = sampleRate * newFadeTimeSec;
        startGain = 0.f;
        complete = false;
    }

    template <typename T>
    inline void processWithState(const AudioBuffer<T> &dry, AudioBuffer<T> &wet)
    {
        if (complete)
            return;
        auto numSamples = dry.getNumSamples();
        endGain = startGain + (1.f / (fadeLengthSamples / numSamples));
        process(dry, wet, startGain, endGain);
        startGain = endGain;
        if (endGain >= 1.f)
            complete = true;
    }
};
#endif