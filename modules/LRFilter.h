#pragma once

enum class LRFilterType
{
    lowpass,
    highpass,
    allpass
};

template <typename SampleType>
class LinkwitzRileyFilter
{
public:
    //==============================================================================
    using Type = LRFilterType;

    //==============================================================================
    /** Constructor. */
    LinkwitzRileyFilter()
    {
        update();
    }

    //==============================================================================
    /** Sets the filter type. */
    void setType (Type newType)
    {
        filterType = newType;
    }

    /** Sets the cutoff frequency of the filter in Hz. */
    void setCutoffFrequency (float newCutoffFrequencyHz)
    {
        jassert (isPositiveAndBelow (newCutoffFrequencyHz, static_cast<float> (sampleRate * 0.5)));

        cutoffFrequency = newCutoffFrequencyHz;
        update();
    }

    //==============================================================================
    /** Returns the type of the filter. */
    Type getType() const noexcept                      { return filterType; }

    /** Returns the cutoff frequency of the filter. */
    SampleType getCutoffFrequency() const noexcept     { return cutoffFrequency; }

    //==============================================================================
    /** Initialises the filter. */
    void prepare (const dsp::ProcessSpec& spec)
    {
        jassert (spec.sampleRate > 0);
        jassert (spec.numChannels > 0);

        sampleRate = spec.sampleRate;
        update();

        s1.resize (spec.numChannels);
        s2.resize (spec.numChannels);
        s3.resize (spec.numChannels);
        s4.resize (spec.numChannels);

        reset();
    }

    /** Resets the internal state variables of the filter. */
    void reset()
    {
        for (auto s : { &s1, &s2, &s3, &s4 })
            std::fill (s->begin(), s->end(), static_cast<SampleType> (0));
    }

    //==============================================================================
    /** Processes the input and output samples supplied in the processing context. */
    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        const auto& inputBlock = context.getInputBlock();
        auto& outputBlock      = context.getOutputBlock();
        const auto numChannels = outputBlock.getNumChannels();
        const auto numSamples  = outputBlock.getNumSamples();

        jassert (inputBlock.getNumChannels() <= s1.size());
        jassert (inputBlock.getNumChannels() == numChannels);
        jassert (inputBlock.getNumSamples()  == numSamples);

        if (context.isBypassed)
        {
            outputBlock.copyFrom (inputBlock);
            return;
        }

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* inputSamples = inputBlock.getChannelPointer (channel);
            auto* outputSamples = outputBlock.getChannelPointer (channel);

            for (size_t i = 0; i < numSamples; ++i)
                outputSamples[i] = processSample ((int) channel, inputSamples[i]);
        }
    }

    /** Performs the filter operation on a single sample at a time. */
    SampleType processSample (int channel, SampleType inputValue)
    {
        auto yH = (inputValue - (R2 + g) * s1[(size_t) channel] - s2[(size_t) channel]) * h;

        auto yB = g * yH + s1[(size_t) channel];
        s1[(size_t) channel] = g * yH + yB;

        auto yL = g * yB + s2[(size_t) channel];
        s2[(size_t) channel] = g * yB + yL;

        if (filterType == Type::allpass)
            return yL - R2 * yB + yH;

        auto yH2 = ((filterType == Type::lowpass ? yL : yH) - (R2 + g) * s3[(size_t) channel] - s4[(size_t) channel]) * h;

        auto yB2 = g * yH2 + s3[(size_t) channel];
        s3[(size_t) channel] = g * yH2 + yB2;

        auto yL2 = g * yB2 + s4[(size_t) channel];
        s4[(size_t) channel] = g * yB2 + yL2;

        return filterType == Type::lowpass ? yL2 : yH2;
    }

    /** Performs the filter operation on a single sample at a time, and returns both
        the low-pass and the high-pass outputs of the TPT structure.
    */
    void processSample (int channel, SampleType inputValue, SampleType &outputLow, SampleType &outputHigh)
    {
        auto yH = (inputValue - (R2 + g) * s1[(size_t) channel] - s2[(size_t) channel]) * h;

        auto yB = g * yH + s1[(size_t) channel];
        s1[(size_t) channel] = g * yH + yB;

        auto yL = g * yB + s2[(size_t) channel];
        s2[(size_t) channel] = g * yB + yL;

        auto yH2 = (yL - (R2 + g) * s3[(size_t) channel] - s4[(size_t) channel]) * h;

        auto yB2 = g * yH2 + s3[(size_t) channel];
        s3[(size_t) channel] = g * yH2 + yB2;

        auto yL2 = g * yB2 + s4[(size_t) channel];
        s4[(size_t) channel] = g * yB2 + yL2;

        outputLow = yL2;
        outputHigh = yL - R2 * yB + yH - yL2;
    }

private:
    //==============================================================================
    void update()
    {
        if constexpr (std::is_same<SampleType, double>::value) {
            g  = (SampleType) std::tan (MathConstants<double>::pi * cutoffFrequency / sampleRate);
            R2 = (SampleType) std::sqrt (2.0);
        }
        else {
            g = xsimd::tan (MathConstants<double>::pi * cutoffFrequency / sampleRate);
            R2 = xsimd::sqrt (2.0);
        }
        
        h  = (SampleType) (1.0 / (1.0 + R2 * g + g * g));
    }

    //==============================================================================
    SampleType g, R2, h;
    std::vector<SampleType> s1, s2, s3, s4;

    double sampleRate = 44100.0;
    SampleType cutoffFrequency = 2000.0;
    Type filterType = Type::lowpass;
};