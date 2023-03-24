// SVTFilter.h
#pragma once

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

/// @brief Custom SVT Filter with xsimd compatibility
/// @tparam T sample data type (float/double/xsimd register)
/// @tparam useSmoother whether to use a built-in smoother for parameter changes
template <typename T, bool useSmoother = false>
class SVTFilter
{
    double sampleRate = 44100.0;
    T g, h, R2;
    std::vector<T> s1{2}, s2{2};

    float cutoffFrequency = 1000.0, resonance = 1.0 / std::sqrt(2.0),
      gain = 1.0;

    SmoothedValue<float> sm_reso;
    SmoothedValue<float, ValueSmoothingTypes::Multiplicative> sm_freq;

    void update()
    {
        if constexpr (std::is_same<T, double>::value)
            g = (double)(std::tan(MathConstants<double>::pi * cutoffFrequency / sampleRate));
        else
            g = (T)(xsimd::tan((T)MathConstants<double>::pi * cutoffFrequency / (T)sampleRate));

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

    template <typename FloatType>
    void setCutoffFreq(FloatType newFreq)
    {
        if constexpr (useSmoother)
        {
            sm_freq.setTargetValue(newFreq);
            if (!sm_freq.isSmoothing())
                cutoffFrequency = newFreq;
        }
        else
            cutoffFrequency = newFreq;
        update();
    }

    template <typename FloatType>
    void setResonance(FloatType newRes)
    {
        if constexpr (useSmoother)
        {
            sm_reso.setTargetValue(newRes);
            if (!sm_reso.isSmoothing())
                resonance = newRes;
        }
        else
            resonance = newRes;
        update();
    }

    void setGain(T newGain)
    {
        gain = newGain;
    }

    FilterType getType() { return type; }

    T getCutoffFreq() { if constexpr (useSmoother) return sm_freq.getTargetValue(); else return cutoffFrequency; }

    T getResonance() { if constexpr (useSmoother) return sm_reso.getTargetValue(); else return resonance; }

    T getGain() { return gain; }

    void reset()
    {
        std::fill(s1.begin(), s1.end(), 0.0);
        std::fill(s2.begin(), s2.end(), 0.0);
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        sampleRate = spec.sampleRate;

        s1.resize(spec.numChannels);
        s2.resize(spec.numChannels);

        sm_freq.reset(spec.sampleRate, 0.01f);
        sm_reso.reset(spec.sampleRate, 0.01f);

        sm_freq.setCurrentAndTargetValue(cutoffFrequency);
        sm_reso.setCurrentAndTargetValue(resonance);

        reset();
        update();
    }

    template <class Block>
    void processBlock(Block &block)
    {
        if constexpr (useSmoother)
        {
            if (sm_freq.isSmoothing() || sm_reso.isSmoothing())
            {
                for (auto i = 0; i < block.getNumSamples(); ++i)
                {
                    cutoffFrequency = sm_freq.getNextValue();
                    resonance = sm_reso.getNextValue();
                    update();
                    for (auto ch = 0; ch < block.getNumChannels(); ++ch)
                    {
                        auto in = block.getChannelPointer(ch);
                        in[i] = processSample(ch, in[i]);
                    }
                }
            }
            else
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
        }
        else
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
    }

    void processChannel(T *in, size_t ch, size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            in[i] = processSample(ch, in[i]);
        }
    }

    inline T processSample(size_t channel, T in)
    {
        assert(s1.size() > channel && s1.size() > 0);
        assert(s2.size() > channel && s2.size() > 0);

        auto &ls1 = s1[channel];
        auto &ls2 = s2[channel];

        in *= gain;

        auto yHP = h * (in - ls1 * (g + R2) - ls2);

        auto yBP = yHP * g + ls1;
        ls1 = yHP * g + yBP;

        auto yLP = yBP * g + ls2;
        ls2 = yBP * g + yLP;

        switch (type)
        {
        case FilterType::lowpass:
            return yLP /*  * gain */;
        case FilterType::highpass:
            return yHP /*  * gain */;
        case FilterType::bandpass:
            return yBP /*  * gain */;
        case FilterType::notch:
            return (yLP + yHP) /*  * gain */;
        case FilterType::peak:
            return (yLP - yHP) /*  * gain */;
        case FilterType::firstOrderLowpass:
            return (yLP + yBP) /*  * gain */;
        case FilterType::firstOrderHighpass:
            return (yHP + yBP) /*  * gain */;
        case FilterType::allpass:
            return (in - ((yBP * R2) + (yBP * R2))) /*  * gain */;
        default:
            return yLP /*  * gain */;
        }
    }
};