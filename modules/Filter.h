/*
	Module for non-cramping filter, based on "Matched Second Order Filters" by Martin Vicanek (2016)
	(c) Arboreal Audio, LLC 2023
*/


#include <cassert>
#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>
#include <vector>

// moved this from SVTFilter.h
// certain types will not be implemented yet
enum FilterType
{
    lowpass,
    bandpass,
    highpass,
    notch,
    peak,
    firstOrderLowpass,
    firstOrderHighpass,
    firstOrderLowshelf,
    firstOrderHighshelf,
    allpass
};

template <typename T>
struct Filter
{
	Filter(FilterType _type) : type(_type)
	{}

	void init(int numChannels, double sampleRate/* , double cutoff, double reso */)
	{
		this->sampleRate = sampleRate;
		// this->cutoff = cutoff;
		// this->reso = reso;

		// setCoeffs();

		xn[0].resize(numChannels);
		xn[1].resize(numChannels);
		yn[0].resize(numChannels);
		yn[1].resize(numChannels);

		for (auto &x : xn)
			for (auto &ch : x)
				ch = 0.0;

		for (auto &y : yn)
			for (auto &ch : y)
				ch = 0.0;
	}
	
	void setCoeffs()
	{
		// TODO: Better optimize based on filter type, since 1-pole filters
		// require different zeroes than below
		const auto w0 = 2.0 * M_PI * (cutoff / sampleRate);
		const auto q = 1.0 / (2.0 * reso);
		const auto tmp = exp(-q * w0);
		a1 = -2.0 * tmp;
		if (q <= 1.0)
			a1 *= cos(sqrt(1.0 - q * q) * w0);
		else
			a1 *= cosh(sqrt(q * q - 1.0) * w0);
		a2 = tmp * tmp;

		const auto f0 = cutoff / (sampleRate * 0.5);
		const auto freq2 = f0 * f0;
		const auto fac = (1.0 - freq2) * (1.0 - freq2);

		double r0, r1, r1_num, r1_denom;
		switch (type) {
		case lowpass:
			r0 = 1.0 + a1 + a2;
			r1_num = (1.0 - a1 + a2) * freq2;
			r1_denom = sqrt(fac + freq2 / (reso*reso));
			r1 = r1_num / r1_denom;

			b0 = (r0 + r1) / 2.0;
			b1 = r0 - b0;
			b2 = 0.0;
			break;
		case highpass:
			r1_num = 1.0 - a1 + a2;
			r1_denom = sqrt(fac + freq2 / (reso*reso));
			r1 = r1_num / r1_denom;

			b0 = r1 / 4.0;
			b1 = -2.0 * b0;
			b2 = b0;
			break;
		case bandpass:
			r0 = (1.0 + a1 + a2) / (M_PI * f0 * reso);
			r1_num = (1.0 - a1 + a2) * (f0 / reso);
			r1_denom = sqrt(fac + (freq2 / (reso*reso)));
			r1 = r1_num / r1_denom;

			b1 = -r1 / 2.0;
			b0 = (r0 - b1) / 2.0;
			b2 = -b0 - b1;
			break;
		case firstOrderHighpass: {
			auto fc = cutoff / sampleRate;
			a1 = -exp(-fc * 2.0 * M_PI);
			const auto gain_nyq = sqrt(0.25 / (0.25 + fc*fc));
			b0 = 0.5 * gain_nyq * (1.0 - a1);
			b1 = -b0;
			a2 = b2 = 0.0;
			break;
		}
		case firstOrderLowpass: {
			auto fc = cutoff / sampleRate;
			a1 = -exp(-fc * 2.0 * M_PI);
			const auto gain_nyq = sqrt(fc*fc / (0.25 + fc*fc));
			b0 = 0.5 * (gain_nyq * (1.0 - a1) + 1 + a1);
			b1 = 1.0 + a1 - b0;
			a2 = b2 = 0.0;
			break;
		}
		case firstOrderHighshelf: {
			const auto pi_sqr_2 = 2.0 / (M_PI*M_PI);
			const auto alpha = pi_sqr_2 * (1 + 1/(gain*freq2)) - 0.5;
			const auto beta = pi_sqr_2 * (1 + gain/freq2) - 0.5;
			a1 = -alpha / (1.0 + alpha + sqrt(1.0 + 2.0 * alpha));
			const auto b = -beta / (1.0 + beta + sqrt(1.0 + 2.0 * beta));
			b0 = (1.0 + a1) / (1.0 + b);
			b1 = b * b0;
			a2 = b2 = 0.0;
			break;
		}
		case firstOrderLowshelf: {
			auto igain = 1.0 / gain;
			const auto pi_sqr_2 = 2.0 / (M_PI*M_PI);
			const auto alpha = pi_sqr_2 * (1 + 1/(igain*freq2)) - 0.5;
			const auto beta = pi_sqr_2 * (1 + igain/freq2) - 0.5;
			a1 = -alpha / (1.0 + alpha + sqrt(1.0 + 2.0 * alpha));
			const auto b = -beta / (1.0 + beta + sqrt(1.0 + 2.0 * beta));
			b0 = gain * ((1.0 + a1) / (1.0 + b));
			b1 = b * b0;
			a2 = b2 = 0.0;
			break;
		} default: break;
		}
	}

	void setCutoff(double newCutoff)
	{
		cutoff = newCutoff;
		setCoeffs();
	}

	void setReso(double newReso)
	{
		reso = newReso;
		setCoeffs();
	}

	void reset()
	{
		for (auto &x : xn)
			for (auto &ch : x)
				ch = 0.0;
		for (auto &y : yn)
			for (auto &ch : y)
				ch = 0.0;
	}

	T processSample(int ch, T x)
	{
		const auto b = b0 * x + b1 * xn[0][ch] + b2 * xn[1][ch];
		const auto a = -a1 * yn[0][ch] - a2 * yn[1][ch];

		const auto out = a + b;
		xn[1][ch] = xn[0][ch];
		xn[0][ch] = x;
		yn[1][ch] = yn[0][ch];
		yn[0][ch] = out;
		return out;
	}

	void process(T **in, size_t numChannels, size_t numSamples)
	{
		assert(numChannels <= xn[0].size());

		for (size_t ch = 0; ch < numChannels; ++ch)
			for (size_t i = 0; i < numSamples; ++i)
				processSample(ch, in[ch][i]);
	}

	void processBlock(juce::dsp::AudioBlock<T> block)
	{
		const auto numChannels = block.getNumChannels();
		const auto numSamples = block.getNumSamples();
		assert(numChannels <= xn[0].size());

		for (size_t ch = 0; ch < numChannels; ++ch)
		{
			auto in = block.getChannelPointer(ch);
			for (size_t i = 0; i < numSamples; ++i)
				in[i] = processSample(ch, in[i]);
		}
	}

	double cutoff, reso, gain;

private:

	FilterType type;

	double sampleRate = 44100.0;
	
	std::vector<T> xn [2];
	std::vector<T> yn [2];

	double a1, a2, b0, b1, b2;
};
