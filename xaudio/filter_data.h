#pragma once

#include <xaudio2.h>
#include <xaudio2fx.h>
#include <x3daudio.h>
#include <xapo.h>
#include <xapobase.h>

#include <complex>
#include <valarray>

typedef std::complex<double> Complex;
typedef std::valarray<Complex> CArray;

struct data_buffer {
    CArray* pdata;
    unsigned size;
};

struct raw_data {
    WAVEFORMATEXTENSIBLE* wfx;
    XAUDIO2_BUFFER* buffer;
};

struct fir {
    CArray* left;
    CArray* right;
};

struct filter_data {
    raw_data data;
    fir fir;
    int elevation;
    unsigned angle;
};

// Prepare data for FFT, takes data from p and outputs in arr.
// Fills with 0's to match nfft size
inline void transformData(CArray& arr, const float* p, const uint32_t size, const uint32_t nfft = 512) {

    if (size > nfft) {
        assert(0);
    }

    arr.resize(nfft);

    for (int i = 0; i < size; i += 1) {
        arr[i] = Complex(p[i], 0);
    }

    for (int i = size; i < nfft; i += 1) {
        arr[i] = Complex(0, 0);
    }
}

// Prepare data for FFT, takes data from p and outputs in arr, in double values.
// Fills with 0's to match nfft size
inline void transformData(CArray& arr, const int16_t* p, const uint32_t size, const uint32_t nfft = 512) {

    if (size > nfft) {
        assert(0);
    }
    arr.resize(nfft);

    for (int i = 0; i < size; i++) {
        constexpr int16_t oldMin = (std::numeric_limits<int16_t>::min)();
        constexpr int16_t oldMax = (std::numeric_limits<int16_t>::max)();

        const double newMin = -1.0;
        const double newMax = 1.0;

        const int32_t oldRange = (int32_t)oldMax - (int32_t)oldMin;
        const double newRange = newMax - newMin;

        arr[i] = Complex((double)((((double)p[i] - (double)oldMin) * newRange) / oldRange) + newMin, 0);
    }

    for (int i = size; i < nfft; i += 1) {
        arr[i] = Complex(0, 0);
    }
}