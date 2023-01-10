/** Buffer.h
 * Custom class for xsimd-compatible buffers
 */
#pragma once
namespace buffer_clear
{
template <typename Type>
std::enable_if_t<std::is_floating_point_v<Type>, void>
    clear(Type** channels, int startChannel, int endChannel, int startSample, int endSample) noexcept
    {
        for (int ch = startChannel; ch < endChannel; ++ch)
            juce::FloatVectorOperations::clear(channels[ch] + startSample, endSample, startSample);
    }

template <typename Type>
std::enable_if_t<std::is_same_v<Type, xsimd::batch<double>>, void>
    clear(Type** channels, int startChannel, int endChannel, int startSample, int endSample) noexcept
    {
        for (int ch = startChannel; ch < endChannel; ++ch)
            std::fill(channels[ch] + startSample, channels[ch] + endSample, Type{});
    }
}

/**
 * Debug macro
 * checks a buffer of samples or audio block for NaNs
 * will simply assert if it finds a NaN
*/
#if DEBUG
    #define CHECK_BUFFER(in, numSamples) {for (size_t i = 0; i < numSamples; ++i) \
    assert(!std::isnan(in[i]));}
    #define CHECK_BLOCK(block) \
    { auto L = block.getChannelPointer(0); \
    CHECK_BUFFER(L, block.getNumSamples()) \
    if (block.getNumChannels() > 1) \
    { auto R = block.getChannelPointer(1); CHECK_BUFFER(R, block.getNumSamples()) } }
#else
    #define CHECK_BUFFER(a, b)
    #define CHECK_BLOCK(a)
#endif

template <typename Type>
class Buffer
{
public:
    Buffer() = default;

    /* Creates a buffer with pre-allocated channels */
    Buffer(int channels, int samples)
    {
        setSize(channels, samples);
    }

    /* Move constructor */
    Buffer(Buffer<Type> &&) noexcept = default;

    /* Move assignment */
    Buffer<Type> &operator=(Buffer<Type> &&) noexcept = default;

    int getNumChannels() const noexcept { return nChannels; }
    int getNumSamples() const noexcept { return nSamples; }

    /* ALWAYS reallocates. Don't call this from the audio thread or anywhere you can't
    tolerate locking */
    void setSize(int newNumChannels, int newNumSamples)
    {
        jassert(isPositiveAndBelow(newNumChannels, 8));
        jassert(newNumSamples > 0);

        rawData.clear();
        rawData.resize((size_t)newNumChannels, ChannelData((size_t)newNumSamples, Type{}));
        isCleared = true;

        for (int ch = 0; ch < newNumChannels; ++ch)
            channelPointers[ch] = rawData[ch].data();

        nChannels = newNumChannels;
        nSamples = newNumSamples;
    }

    Type* getWritePointer(int channel) noexcept
    {
        isCleared = false;
        return channelPointers[channel];
    }

    const Type* getReadPointer(int channel) noexcept
    {
        return channelPointers[channel];
    }

    Type** getArrayOfWritePointers() noexcept
    {
        isCleared = false;
        return channelPointers.data();
    }

    const Type** getArrayOfReadPointers() const noexcept
    {
        return const_cast<const Type**>(channelPointers.data());
    }

    void clear() noexcept
    {
        if (isCleared)
            return;

        buffer_clear::clear<Type>(channelPointers.data(), 0, nChannels, 0, nSamples);
        isCleared = true;
    }

    template <typename OtherType>
    void makeCopyOf(const Buffer<OtherType>& other, bool avoidReallocating = false)
    {
        if (other.isCleared)
            clear();
        else
        {
            isCleared = false;

            for (int ch = 0; ch < nChannels; ++ch)
            {
                auto *dest = channelPointers[ch];
                auto *src = other.getReadPointer(ch);

                for (int i = 0; i < nSamples; ++i)
                    dest[i] = static_cast<Type>(src[i]);
            }
        }
    }

    void applyGain(double gain) noexcept
    {
        if (gain != 1.0 && !isCleared)
        {
            if (gain == 0.0)
                clear();
            else {
                for (int ch = 0; ch < nChannels; ++ch)
                    for (int i = 0; i < nSamples; ++i)
                        channelPointers[ch][i] *= gain;
            }
        }
    }

    void copyFrom(int destChan, int startSample, const Type* source, int numSamples) noexcept
    {
        jassert(isPositiveAndBelow(destChan, nChannels));
        jassert(numSamples < nSamples);
        jassert(source != nullptr);
        
        if (numSamples > 0)
        {
            isCleared = false;
            memcpy(channelPointers[destChan], source, (size_t)numSamples * sizeof(Type));
        }
    }

    void addFrom(int destChan, int startSample, const Type* source, int numSamples, double gain = 1.0) noexcept
    {
        if (gain != 0.0 && numSamples > 0)
        {
            if (isCleared)
            {
                isCleared = false;
                if (gain != 1.0)
                    for (int i = 0; i < numSamples; ++i)
                        channelPointers[destChan][i] = source[i] * gain;
                else
                    memcpy(channelPointers[destChan], source, (size_t)numSamples * sizeof(Type));
            }
            else
            {
                if (gain != 1.0)
                    for (int i = 0; i < numSamples; ++i)
                        channelPointers[destChan][i] += source[i] * gain;
                else
                    for (int i = 0; i < numSamples; ++i)
                        channelPointers[destChan][i] += source[i];
            }
        }
    }

private:
    using Allocator = xsimd::default_allocator<Type>;
    using ChannelData = std::vector<Type, Allocator>;
    std::vector<ChannelData> rawData;

    int nChannels = 0, nSamples = 0;
    bool isCleared = false;

    size_t allocatedBytes = 0;

    /* using max of 8 channels for now */
    std::array<Type*, (size_t)8> channelPointers{};

    JUCE_LEAK_DETECTOR (Buffer)
};