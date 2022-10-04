/* IIRFilter.h
Custom IIR Filter for xsimd compatibility (& ditching convoluted JUCE processing classes) */

#pragma once

template <typename Type>
class IIRFilter
{
    using CoefficientsPtr = typename dsp::IIR::Coefficients<double>::Ptr;

public:
    IIRFilter() = default;

    IIRFilter(CoefficientsPtr coefficientsToUse) : coefficients(std::move(coefficientsToUse))
    {
        reset();
    }

    IIRFilter(const IIRFilter &) = default;
    IIRFilter(IIRFilter &&) = default;
    IIRFilter &operator=(const IIRFilter &) = default;
    IIRFilter &operator=(IIRFilter &&) = default;

    CoefficientsPtr coefficients;

    void reset() { reset(Type{0}); }
    void reset(Type resetToValue)
    {
        auto newOrder = coefficients->getFilterOrder();

        if (newOrder != order)
        {
            memory.malloc(jmax(order, newOrder, static_cast<size_t>(3)) + 1);
            state = snapPointerToAlignment(memory.getData(), sizeof(Type));
            order = newOrder;
        }

        for (size_t i = 0; i < order; ++i)
            state[i] = resetToValue;
    }

    void prepare(const dsp::ProcessSpec&) noexcept
    {
        reset();
    }

    template <typename Block>
    void process(Block& block) noexcept
    {
        check();

        auto numSamples = block.getNumSamples();
        auto *src = block.getChannelPointer(0); /* expects only one channel, ofc */
        auto* dst = src;
        auto *coeffs = coefficients->getRawCoefficients();

        switch (order)
        {
            case 1:
            {
                auto b0 = coeffs[0];
                auto b1 = coeffs[1];
                auto a1 = coeffs[2];

                auto lv1 = state[0];

                for (size_t i = 0; i < numSamples; ++i)
                {
                    auto input = src[i];
                    auto output = input * b0 + lv1;

                    dst[i] = output;

                    lv1 = (input * b1) - (output * a1);
                }
            }
            break;

            case 2:
            {
                auto b0 = coeffs[0];
                auto b1 = coeffs[1];
                auto b2 = coeffs[2];
                auto a1 = coeffs[3];
                auto a2 = coeffs[4];

                auto lv1 = state[0];
                auto lv2 = state[1];

                for (size_t i = 0; i < numSamples; ++i)
                {
                    auto input = src[i];
                    auto output = (input * b0) + lv1;
                    dst[i] = output;

                    lv1 = (input * b1) - (output* a1) + lv2;
                    lv2 = (input * b2) - (output* a2);
                }
            }
            break;

            case 3:
            {
                auto b0 = coeffs[0];
                auto b1 = coeffs[1];
                auto b2 = coeffs[2];
                auto b3 = coeffs[3];
                auto a1 = coeffs[4];
                auto a2 = coeffs[5];
                auto a3 = coeffs[6];

                auto lv1 = state[0];
                auto lv2 = state[1];
                auto lv3 = state[2];

                for (size_t i = 0; i < numSamples; ++i)
                {
                    auto input = src[i];
                    auto output = (input * b0) + lv1;
                    dst[i] = output;

                    lv1 = (input * b1) - (output* a1) + lv2;
                    lv2 = (input * b2) - (output* a2) + lv3;
                    lv3 = (input * b3) - (output* a3);
                }
            }
            break;

            default:
            {
                for (size_t i = 0; i < numSamples; ++i)
                {
                    auto input = src[i];
                    auto output= (input * coeffs[0]) + state[0];
                    dst[i] = output;

                    for (size_t j = 0; j < order - 1; ++j)
                        state[j] = (input * coeffs[j + 1]) - (output* coeffs[order + j + 1]) + state[j + 1];

                    state[order - 1] = (input * coeffs[order]) - (output* coeffs[order * 2]);
                }
            }
        }
    }

    Type processSample(Type sample) noexcept
    {
        check();
        auto *c = coefficients->getRawCoefficients();

        auto output = (c[0] * sample) + state[0];

        for (size_t j = 0; j < order; ++j)
            state[j] = (c[j + 1] * sample) - (c[order + j + 1] * output) + state[j + 1];

        state[order - 1] = (c[order] * sample) - (c[order * 2] * output);

        return output;
    }

private:
    void check()
    {
        jassert(coefficients != nullptr);

        if (order != coefficients->getFilterOrder())
            reset();
    }

    juce::HeapBlock<Type> memory;
    Type *state = nullptr;
    size_t order = 0;
};