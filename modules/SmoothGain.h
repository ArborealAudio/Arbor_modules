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
    inline static void process(const T* dryIn, T* out, size_t numSamples)
    {
        float gain = 0.f;
        float inc = 1.f / (float)numSamples;
        for (size_t i = 0; i < numSamples; ++i)
        {
            out[i] = (out[i] * gain) + (1.f - gain) * dryIn[i];
            gain += inc;
        }
    }

    template <typename Block>
    inline static void process(const Block &dryBlock, Block &outBlock)
    {
        for (size_t ch = 0; ch < outBlock.getNumChannels(); ++ch)
        {
            process(dryBlock.getChannelPointer(ch), outBlock.getChannelPointer(ch), outBlock.getNumSamples());
        }
    }
};
#endif