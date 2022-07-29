// Delay.h

template <typename SampleType>
class Delay
{
public:
    //==============================================================================
    /** Default constructor. */
    Delay() : Delay(0)
    {
    }

    /** Constructor. */
    explicit Delay (int maximumDelayInSamples)
    {
        jassert (maximumDelayInSamples >= 0);

        sampleRate = 44100.0;

        setMaximumDelayInSamples (maximumDelayInSamples);
    }

    //==============================================================================
    /** Sets the delay in samples. */
    void setDelay (double newDelayInSamples)
    {
        auto upperLimit = (double) getMaximumDelayInSamples();
        jassert (isPositiveAndNotGreaterThan (newDelayInSamples, upperLimit));

        delay     = jlimit ((double) 0, upperLimit, newDelayInSamples);
        delayInt  = static_cast<int> (std::floor (delay));
        delayFrac = delay - (double) delayInt;

        // updateInternalVariables();
    }

    void setDelay (xsimd::batch<double> newDelayInSamples)
    {
        auto upperLimit = (SampleType) getMaximumDelayInSamples();
        jassert (isPositiveAndNotGreaterThan (newDelayInSamples, upperLimit));

        delay = xsimd::max((SampleType)0.0, xsimd::min(newDelayInSamples, upperLimit));
        delayInt = static_cast<int>(std::floor(xsimd::hadd(delay)));
        delayFrac = delay - (SampleType) delayInt;
    }

    /** Returns the current delay in samples. */
    // SampleType getDelay() const
    // {
    //     return delay;
    // }

    //==============================================================================
    /** Initialises the processor. */
    void prepare (const dsp::ProcessSpec& spec)
    {
        jassert (spec.numChannels > 0);

        // bufferData.setSize ((int) spec.numChannels, totalSize, false, false, true);
        bufferData.resize(spec.numChannels);
        for (auto& buf : bufferData)
            buf.resize(totalSize);

        writePos.resize (spec.numChannels);
        readPos.resize  (spec.numChannels);

        sampleRate = spec.sampleRate;

        reset();
    }

    /** Sets a new maximum delay in samples.

        Also clears the delay line.

        This may allocate internally, so you should never call it from the audio thread.
    */
    void setMaximumDelayInSamples (int maxDelayInSamples)
    {
        jassert (maxDelayInSamples >= 0);
        totalSize = jmax (4, maxDelayInSamples + 1);
        // bufferData.setSize ((int) bufferData.getNumChannels(), totalSize, false, false, true);
        for (auto& buf : bufferData)
            buf.resize(totalSize);
        reset();
    }

    /** Gets the maximum possible delay in samples.

        For very short delay times, the result of getMaximumDelayInSamples() may
        differ from the last value passed to setMaximumDelayInSamples().
    */
    int getMaximumDelayInSamples() const noexcept       { return totalSize - 1; }

    /** Resets the internal state variables of the processor. */
    void reset()
    {
        for (auto vec : { &writePos, &readPos })
            std::fill (vec->begin(), vec->end(), 0);

        for (auto& buf : bufferData)
            std::fill(buf.begin(), buf.end(), 0.0);
    }

    //==============================================================================
    /** Pushes a single sample into one channel of the delay line.

        Use this function and popSample instead of process if you need to modulate
        the delay in real time instead of using a fixed delay value, or if you want
        to code a delay effect with a feedback loop.

        @see setDelay, popSample, process
    */
    void pushSample (int channel, SampleType sample)
    {
        // bufferData.setSample (channel, writePos[(size_t) channel], sample);
        bufferData[channel][writePos[channel]] = sample;
        writePos[(size_t) channel] = (writePos[(size_t) channel] + totalSize - 1) % totalSize;
    }

    /** Pops a single sample from one channel of the delay line.

        Use this function to modulate the delay in real time or implement standard
        delay effects with feedback.

        @param channel              the target channel for the delay line.

        @param delayInSamples       sets the wanted fractional delay in samples, or -1
                                    to use the value being used before or set with
                                    setDelay function.

        @param updateReadPointer    should be set to true if you use the function
                                    once for each sample, or false if you need
                                    multi-tap delay capabilities.

        @see setDelay, pushSample, process
    */
    SampleType popSample (int channel, double delayInSamples = -1, bool updateReadPointer = true)
    {
        if (delayInSamples >= 0)
            setDelay(delayInSamples);

        auto result = interpolateSample (channel);

        if (updateReadPointer)
            readPos[(size_t) channel] = (readPos[(size_t) channel] + totalSize - 1) % totalSize;

        return result;
    }

    xsimd::batch<double> popSample (int channel, xsimd::batch<double> delayInSamples = -1, bool updateReadPointer = true)
    {
        if (xsimd::any(delayInSamples >= 0))
            setDelay(delayInSamples);

        auto result = interpolateSample (channel);

        if (updateReadPointer)
            readPos[(size_t) channel] = (readPos[(size_t) channel] + totalSize - 1) % totalSize;

        return result;
    }

    //==============================================================================
    /** Processes the input and output samples supplied in the processing context.

        Can be used for block processing when the delay is not going to change
        during processing. The delay must first be set by calling setDelay.

        @see setDelay
    */
    template <class Block>
    void processBlock (Block& block) noexcept
    {
        const auto numChannels = block.getNumChannels();
        const auto numSamples  = block.getNumSamples();

        jassert (block.getNumChannels() == numChannels);
        jassert (block.getNumChannels() == writePos.size());
        jassert (block.getNumSamples()  == numSamples);

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* in = block.getChannelPointer (channel);

            for (size_t i = 0; i < numSamples; ++i)
            {
                pushSample ((int) channel, in[i]);
                in[i] = popSample ((int) channel);
            }
        }
    }

private:
    //==============================================================================

    SampleType interpolateSample (int channel) const
    {
        auto index1 = readPos[(size_t) channel] + delayInt;
        auto index2 = index1 + 1;

        if (index2 >= totalSize)
        {
            index1 %= totalSize;
            index2 %= totalSize;
        }

        // auto value1 = bufferData.getSample (channel, index1);
        // auto value2 = bufferData.getSample (channel, index2);
        auto value1 = bufferData[channel][index1];
        auto value2 = bufferData[channel][index2];

        return value1 + delayFrac * (value2 - value1);
    }

    //==============================================================================
    double sampleRate = 44100.0;

    //==============================================================================
    // AudioBuffer<SampleType> bufferData;
    std::vector<std::vector<SampleType>> bufferData;
    std::vector<int> writePos, readPos;
    double delay = 0.0, delayFrac = 0.0;
    int delayInt = 0, totalSize = 4;
    SampleType alpha = 0.0;
};