/**
 * SmoothGain.h
 * Helpers for computing a smoothed gain on a block of audio samples
*/
#pragma once

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