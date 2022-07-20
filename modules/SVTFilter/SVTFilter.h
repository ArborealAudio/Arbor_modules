//SVTFilter.h

/***
BEGIN_JUCE_MODULE_DECLARATION
ID: SVTFilter
vendor: Arboreal Audio
version: 1.0
name: SVTFilter
description: Custom SVT Filter with capability for SIMD
dependencies: juce_core, juce_dsp, chowdsp_simd
END_JUCE_MODULE_DECLARATION
***/

#pragma once
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

namespace strix
{
    using namespace juce;
    
    enum class FilterType
    {
        lowpass,
        bandpass,
        highpass,
        notch,
        peak,
        firstOrderLowpass,
        firstOrderHighpass,
        allpass
    };

    template <typename T>
    class SVTFilter
    {
        double sampleRate = 44100.0;
        T g, h, R2;
        std::vector<T> s1{2}, s2{2};

        T cutoffFrequency = (T)1000.0,
          resonance = (T)1.0 / std::sqrt(2.0),
          gain = (T)1.0;

        void update()
        {
            if constexpr (std::is_same_v<T, double>)
                g = (double)(std::tan(MathConstants<double>::pi * cutoffFrequency / sampleRate));
            else
                g = (T)(xsimd::tan(MathConstants<double>::pi * cutoffFrequency / sampleRate));

            R2 = (T)(1.0 / resonance);
            h = (T)(1.0 / (1.0 + R2 * g + g * g));
        }

        FilterType type = FilterType::lowpass;

    public:

        SVTFilter()
        {
            update();
        }

        void setType(FilterType newType) { type = newType; }

        void setCutoffFreq(T newFreq)
        {
            cutoffFrequency = newFreq;
            update();
        }

        void setResonance(T newRes)
        {
            resonance = newRes;
            update();
        }

        void setGain(T newGain)
        {
            gain = newGain;
        }

        FilterType getType() { return type; }

        T getCutoffFreq() { return cutoffFrequency; }

        T getResonance() { return resonance; }

        void reset()
        {
            for (auto v : {&s1, &s2})
                std::fill(v->begin(), v->end(), 0.0);
        }

        void prepare(const dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;

            s1.resize(spec.numChannels);
            s2.resize(spec.numChannels);

            reset();
            update();
        }

        template <class Block>
        void processBlock(Block& block)
        {
            for (auto ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto in = block.getChannelPointer(ch);

                for (auto i = 0; i < block.getNumSamples(); ++i)
                {
                    in[i] = processSample(ch, in[i]);
                }
            }
        }

        T processSample(int channel, T in)
        {
            auto &ls1 = s1[channel];
            auto &ls2 = s2[channel];

            auto yHP = h * (in - ls1 * (g + R2) - ls2);

            auto yBP = yHP * g + ls1;
            ls1 = yHP * g + yBP;

            auto yLP = yBP * g + ls2;
            ls2 = yBP * g + yLP;

            switch (type)
            {
            case FilterType::lowpass:
                return yLP;
            case FilterType::highpass:
                return yHP;
            case FilterType::bandpass:
                return yBP;
            case FilterType::notch:
                return (yLP + yHP);
            case FilterType::peak:
                return (yLP - yHP);
            case FilterType::firstOrderLowpass:
                return (yLP + yBP);
            case FilterType::firstOrderHighpass:
                return (yHP + yBP);
            case FilterType::allpass:
                return (in - ((yBP * R2) + (yBP * R2)));
            default:
                return yLP;
            }
        }
    };
} // namespace strix
