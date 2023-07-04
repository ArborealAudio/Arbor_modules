// MSMatrix.h
/*Helpers for encoding/decoding M/S, processing stereo width*/

#pragma once

/**
 * Static functions for encoding & decoding M/S
*/
struct MSMatrix
{
    template <typename T>
    inline static void msEncode (dsp::AudioBlock<T>& block, float sideGain = 1.f)
    {
        jassert(block.getNumChannels() > 1);

        T* L = block.getChannelPointer(0);
        T* R = block.getChannelPointer(1);

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            auto mid = (L[i] + R[i]) * 0.5f;
            auto side = (L[i] - R[i]) * (0.5f * sideGain);

            L[i] = mid;
            R[i] = side;
        }
    }

    template <typename T>
    inline static void msDecode (dsp::AudioBlock<T>& block)
    {
        jassert(block.getNumChannels() > 1);

        T* mid = block.getChannelPointer(0);
        T* side = block.getChannelPointer(1);

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            auto left = side[i] + mid[i];
            auto right = mid[i] - side[i];

            mid[i] = left;
            side[i] = right;
        }
    }
};

/**
 * Stereo widener, with capability for doing smooth parameter changes
*/
struct Balance
{
    Balance(){}

    /**
     * static method for in-place stereo widening of an AudioBlock. Does not need to manage its own state if invoked in this way
     * @param balance width level
     * @param ms whether the block is M/S-encoded
     * @param lastBalance a reference to last width level, can be modified by this funciton
     * @param updateBalance control for whether lastBalance should be modified
    */
    template <typename T>
    static void processBalance(dsp::AudioBlock<T>& block, float balance, bool ms, float& lastBalance, bool updateBalance = true)
    {
        if (lastBalance != balance) {
            if (ms)
                SmoothGain<T>::applySmoothGain(block.getChannelPointer(1), block.getNumSamples(), balance, lastBalance, updateBalance);
            else {
                MSMatrix::msEncode(block);
                SmoothGain<T>::applySmoothGain(block.getChannelPointer(1), block.getNumSamples(), balance, lastBalance, updateBalance);
                MSMatrix::msDecode(block);
            }
            return;
        }

        if (ms)
            block.getSingleChannelBlock(1).multiplyBy(balance);
        else {
            MSMatrix::msEncode(block);
            block.getSingleChannelBlock(1).multiplyBy(balance);
            MSMatrix::msDecode(block);
        }
    }

    void reset()
    {
        prevBalance = 0.f;
    }

    /**
     * Method for stateful processing of stereo width
     * @param balance stereo width level
     * @param ms Whether block is M/S-encoded
    */
    template <typename T>
    void process(dsp::AudioBlock<T>& block, float balance, bool ms)
    {
        if (prevBalance != balance) {
            if (ms)
                SmoothGain<T>::applySmoothGain(block.getChannelPointer(1), block.getNumSamples(), balance, prevBalance);
            else {
                MSMatrix::msEncode(block);
                SmoothGain<T>::applySmoothGain(block.getChannelPointer(1), block.getNumSamples(), balance, prevBalance);
                MSMatrix::msDecode(block);
            }
            return;
        }

        if (ms)
            block.getSingleChannelBlock(1).multiplyBy(balance);
        else {
            MSMatrix::msEncode(block);
            block.getSingleChannelBlock(1).multiplyBy(balance);
            MSMatrix::msDecode(block);
        }
    }

private:
    float prevBalance = 0.f;
};

/**
 * Class for taking a mono signal and synthesizing a stereo one from it,
 * using a short delay and an its inverse
*/
template <typename T>
class MonoToStereo
{
public:
    MonoToStereo() = default;

    MonoToStereo(int maxDelay) : delay(maxDelay + 1)
    {}

    void prepare(const dsp::ProcessSpec& spec)
    {
        SR = spec.sampleRate;
        delay.prepare(spec);
        sm.reset(spec.sampleRate, 0.01f);
    }

    void reset()
    {
        delay.reset();
    }

    void setDelayTime(int delayInMs)
    {
        delay.setDelay((delayInMs / 1000.0) * SR);
    }

    template <typename Block>
    void process(Block& block, T mult)
    {
        assert(block.getNumChannels() > 1); /* we need 2 output channels */

        if (mult != sm.getCurrentValue())
            sm.setTargetValue(mult);

        auto left = block.getChannelPointer(0);
        auto right = block.getChannelPointer(1);

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            auto amt = sm.getNextValue();
            delay.pushSample(0, left[i]);
            T d = amt * delay.popSample(0, -1.0);

            auto mid = (left[i] + right[i]) * 0.5;
            auto side = (left[i] - right[i]) * 0.5;
            side += d * 0.5;

            left[i] = mid + side;
            right[i] = mid - side;
        }
    }

private:
    strix::Delay<T> delay{4410};
    double SR = 44100.0;

    juce::SmoothedValue<T> sm;
};

/**
 * Mono->stereo utility, based on Michael Gruhn's JS effect
*/
struct GruhnStereoEnhancer
{
    /**
     * @param block expects a block with 2 channels
     * @param amt expects a value 0-1, converted to an angle btw 0-90
    */
    template <typename T>
    static void process(dsp::AudioBlock<T>& block, T amt)
    {
        auto rot = 0.017453292 * jmap(amt, (T)0.f, (T)90.f);

        auto left = block.getChannelPointer(0);
        auto right = block.getChannelPointer(1);

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            T s0 = left[i] >= 0 ? 1.f : -1.f;
            T s1 = right[i] >= 0 ? 1.f : -1.f;
            auto angle = std::atan( left[i] / right[i] );
            if((s0 == 1 && s1 == -1) || (s0 == -1 && s1 == -1)) angle += 3.141592654f;
            if(s0 == -1 && s1 == 1) angle += 6.283185307f;
            if(right[i] == 0) {
                if(left[i] > 0)
                    angle = 1.570796327;
                else
                    angle = 4.71238898f;
            }
            if(left[i] == 0) {
                if (right[i] > 0)
                    angle = 0;
                else
                    angle = 3.141592654f;
            }
            angle -= rot;
            auto radius = std::sqrt( (left[i]*left[i]) + (right[i]*right[i]) );
            left[i] = sin(angle)*radius;
            right[i] = cos(angle)*radius;
        }
    }
};