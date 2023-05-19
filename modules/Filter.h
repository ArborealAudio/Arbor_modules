/*
	Module for non-cramping filter, based on "Matched Second Order Filters" by Martin Vicanek (2016)
	(c) Arboreal Audio, LLC 2023
*/

#include <cassert>
#include <cmath>
#include <vector>

template <typename T>
struct Filter
{
	Filter() {}

	void init(int numChannels, double sampleRate, double cutoff, double reso)
	{
		this->sampleRate = sampleRate;
		this->cutoff = cutoff;
		this->reso = reso;

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
	
	// just a lowpass for now!
	void setCoeffs()
	{
		const auto w0 = 2.0 * M_PI * (cutoff / sampleRate);
		const auto q = 1.0 / (2.0 * reso);
		const auto alpha = q * std::sin(w0);

		a1 = -2.0 * std::cos(w0) / (1.0 + alpha);
		a2 = (1.0 - alpha) / (1.0 + alpha);

		const auto f0 = cutoff / sampleRate;
		const auto freq2 = f0 * f0;
		const auto r0 = 1.0 + a1 + a2;
		const auto r1_num = (1.0 - a1 + a2) * freq2;
		const auto r1_f2 = (1.0 - freq2) * (1.0 - freq2);
		const auto r1_denom = std::sqrt(r1_f2 + freq2 / (reso*reso));

		const auto r1 = r1_num / r1_denom;

		b0 = (r0 + r1) / 2.0;
		b1 = r0 - b0;
		b2 = 0.0;
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

	T cutoff, reso;

private:

	double sampleRate = 44100.0;
	
	std::vector<T> xn [2];
	std::vector<T> yn [2];

	T a1, a2, b0, b1, b2;
};
