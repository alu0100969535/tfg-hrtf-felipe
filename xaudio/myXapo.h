#pragma once
#include <xaudio2.h>
#include <xaudio2fx.h>
#include <x3daudio.h>
#include <xapo.h>
#include <xapobase.h>
#include <assert.h>

#include "filter_data.h"

#include <complex>
#include <iostream>
#include <valarray>
#include <limits>       // std::numeric_limits

#include <fstream> // basic file operations
#include <string>


#include <algorithm> // std::max
#include <math.h>       /* ceil */

#ifdef _XBOX //Big-Endian
#define fourccRIFF 'RIFF'
#define fourccDATA 'data'
#define fourccFMT 'fmt '
#define fourccWAVE 'WAVE'
#define fourccXWMA 'XWMA'
#define fourccDPDS 'dpds'
#endif

#ifndef _XBOX //Little-Endian
#define fourccRIFF 'FFIR'
#define fourccDATA 'atad'
#define fourccFMT ' tmf'
#define fourccWAVE 'EVAW'
#define fourccXWMA 'AMWX'
#define fourccDPDS 'sdpd'
#endif

// Debug function, writes to file so we can plot some samples
// TODO: Remove this when done
inline void outputToFile(CArray& arr, const int tries, const bool real = false) {
    std::ofstream myfile;
    const std::string file = "C:\\Users\\faaa9\\AppData\\Roaming\\WickedEngineDev\\example" + std::to_string(tries) + ".txt";
    myfile.open(file);

    if (!myfile.is_open()) {
        assert(0);
    }

    int limit = real ? arr.size() : arr.size() / 2;

    for (int i = 0; i < limit; i += 1) {
        double magnitude = sqrt(arr[i].real() * arr[i].real() + arr[i].imag() * arr[i].imag());

        // Only real part needed when plotting time-domain 
        if (real) {
            myfile << (arr[i].real()) << "," << i << std::endl;
        } else {
            myfile << magnitude << "," << i * (44100 / arr.size()) << std::endl;
        }
    }

    myfile.close();
}

// Cooley-Tukey FFT (in-place, divide-and-conquer)
// Higher memory requirements and redundancy although more intuitive
inline void fft(CArray& x) {
    const size_t N = x.size();
    if (N <= 1) return;

    // divide
    CArray even = x[std::slice(0, N / 2, 2)];
    CArray  odd = x[std::slice(1, N / 2, 2)];

    // conquer
    fft(even);
    fft(odd);

    // combine
    for (size_t k = 0; k < N / 2; ++k)
    {
        Complex t = std::polar(1.0, -2.0 * PI * (double)k / (double)N) * odd[k];
        x[k] = even[k] + t;
        x[k + N / 2] = even[k] - t;
    }
};

// Cooley–Tukey FFT (in-place, divide-and-conquer)
// Higher memory requirements and redundancy although more intuitive
inline void fft(data_buffer& x) {
    const size_t N = x.size;
    if (N <= 1) return;

    // divide
    CArray even = (*x.pdata)[std::slice(0, N / 2, 2)];
    CArray  odd = (*x.pdata)[std::slice(1, N / 2, 2)];

    data_buffer evendb = { &even, even.size() };
    data_buffer odddb = { &odd, odd.size() };

    // conquer
    fft(evendb);
    fft(odddb);

    // combine
    for (size_t k = 0; k < N / 2; ++k)
    {
        Complex t = std::polar(1.0, -2.0 * PI * (double)k / (double)N) * (*odddb.pdata)[k];
        (*x.pdata)[k] = (*evendb.pdata)[k] + t;
        (*x.pdata)[k + N / 2] = (*evendb.pdata)[k] - t;
    }
};

// inverse fft (in-place)
inline void ifft(data_buffer& x)
{
    // conjugate the complex numbers
    (*x.pdata) = (*x.pdata).apply(std::conj);

    // forward fft
    fft(x);

    // conjugate the complex numbers again
    (*x.pdata) = (*x.pdata).apply(std::conj);

    // scale the numbers
    (*x.pdata) /= x.size;
}


// Perform hannWindow in-place only to real part
inline void applyHannWindow(data_buffer& arr) {

    const size_t size = arr.size;

    for (int i = 0; i < size; i++) {
        double multiplier = 0.5 * (1 - cos(2 * PI * i / (size - 1)));
        (*arr.pdata)[i] = Complex(multiplier * (*arr.pdata)[i].real(), 0);
    }
}

class myXapo : public CXAPOBase {

private:
    WORD m_uChannels;
    WORD m_uBytesPerSample;
    UINT32 max_frame_count;

    size_t fft_n;
    size_t step_size;
    size_t fir_size;

    data_buffer new_samples;
    data_buffer saved_samples;
    data_buffer process_samples;
    
    stereo_data_buffer processed_samples;
    stereo_data_buffer ready_samples;
    stereo_data_buffer to_sum_samples;

    filter_data* hrtf_database;


    // DEBUG
    bool done = false;
    int index = 0;
    int tries = 0;

    

public:

    myXapo(XAPO_REGISTRATION_PROPERTIES* pRegistrationProperties, filter_data* filters, size_t filters_size);
    ~myXapo();

    STDMETHOD(LockForProcess) (UINT32 InputLockedParameterCount,
        const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pInputLockedParameters,
        UINT32 OutputLockedParameterCount,
        const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pOutputLockedParameters)
    {
        assert(!IsLocked());
        assert(InputLockedParameterCount == 1);
        assert(OutputLockedParameterCount == 1);
        assert(pInputLockedParameters != NULL);
        assert(pOutputLockedParameters != NULL);
        assert(pInputLockedParameters[0].pFormat != NULL);
        assert(pOutputLockedParameters[0].pFormat != NULL);

        m_uChannels = pInputLockedParameters[0].pFormat->nChannels;
        m_uBytesPerSample = (pInputLockedParameters[0].pFormat->wBitsPerSample >> 3);
        max_frame_count = pOutputLockedParameters->MaxFrameCount;

        return CXAPOBase::LockForProcess(
            InputLockedParameterCount,
            pInputLockedParameters,
            OutputLockedParameterCount,
            pOutputLockedParameters);
    }
    STDMETHOD_(void, Process)(UINT32 InputProcessParameterCount,
        const XAPO_PROCESS_BUFFER_PARAMETERS* pInputProcessParameters,
        UINT32 OutputProcessParameterCount,
        XAPO_PROCESS_BUFFER_PARAMETERS* pOutputProcessParameters,
        BOOL IsEnabled)
    {
        assert(IsLocked());
        assert(InputProcessParameterCount == 1);
        assert(OutputProcessParameterCount == 1);
        assert(NULL != pInputProcessParameters);
        assert(NULL != pOutputProcessParameters);


        XAPO_BUFFER_FLAGS inFlags = pInputProcessParameters[0].BufferFlags;
        XAPO_BUFFER_FLAGS outFlags = pOutputProcessParameters[0].BufferFlags;

        // assert buffer flags are legitimate
        assert(inFlags == XAPO_BUFFER_VALID || inFlags == XAPO_BUFFER_SILENT);
        assert(outFlags == XAPO_BUFFER_VALID || outFlags == XAPO_BUFFER_SILENT);

        // check input APO_BUFFER_FLAGS
        switch (inFlags)
        {
        case XAPO_BUFFER_VALID:
        {
            void* pvSrc = pInputProcessParameters[0].pBuffer; //input: process this data
            assert(pvSrc != NULL);

            void* pvDst = pOutputProcessParameters[0].pBuffer; //output: put processed data here 
            assert(pvDst != NULL);

            float* pvSrcf = static_cast<float*>(pvSrc); // input, as float, mono
            float* pvDstf = static_cast<float*>(pvDst); // output, as float, stereo

            UINT32 len = pInputProcessParameters[0].ValidFrameCount * m_uChannels;

            // Prepare array to perform FFT
            transformData(*new_samples.pdata, pvSrcf, len, len);
            new_samples.size = len;

            // Start overlap-add
            size_t old_len = saved_samples.size;
            size_t new_len = old_len + new_samples.size;

            // If we don't have enough samples, save them and don't return anything
            if (new_len < step_size) {
                for (unsigned i = old_len; i < new_len; i++) {
                    (*saved_samples.pdata)[i] = (*new_samples.pdata)[i - old_len];
                }
                saved_samples.size = new_len;
            }
            // If we have enough, process them and put them in a queue to return
            else {
                // Put all needed samples into process_samples
                for (unsigned i = 0; i < old_len; i++) {
                    (*process_samples.pdata)[i] = (*saved_samples.pdata)[i];
                }
                for (unsigned i = old_len; i < step_size; i++) {
                    (*process_samples.pdata)[i] = (*new_samples.pdata)[i - old_len];
                }
                for (unsigned i = step_size; i < fft_n; i++) {
                    (*process_samples.pdata)[i] = Complex(0, 0); // Pad with 0's to match N

                }
                process_samples.size = fft_n;

                // Save samples we're not using this cycle
                size_t extra_samples = new_len - step_size;
                size_t index = new_samples.size - extra_samples;

                for (unsigned i = index; i < new_samples.size; i++) {
                    (*saved_samples.pdata)[i - index] = (*new_samples.pdata)[i];
                }
                saved_samples.size = extra_samples;

                //// Start overlap-add ////

                // Apply Hann Window
                //applyHannWindow(process_samples);

                // Convert to freq domain
                fft(process_samples);

                // Set DC bin to 0
                (*process_samples.pdata)[0] = Complex(0, 0);

                // Apply convolution, for each channel
                for (unsigned i = 0; i < process_samples.size; i++) {
                    (*processed_samples.left.pdata)[i] = (*process_samples.pdata)[i] * (*hrtf_database[0].fir.left)[i];
                    (*processed_samples.right.pdata)[i] = (*process_samples.pdata)[i] * (*hrtf_database[0].fir.right)[i];
                }
                processed_samples.left.size = processed_samples.right.size = process_samples.size;

                // Convert back to time domain, each channel
                ifft(processed_samples.left);
                ifft(processed_samples.right);

                // Sum last computed frames to first freshly computed ones, on each channel
                if (to_sum_samples.left.size > 0) {  
                    for (unsigned i = 0; i < to_sum_samples.left.size; i++) {
                        (*processed_samples.left.pdata)[i] += (*to_sum_samples.left.pdata)[i];
                        (*processed_samples.right.pdata)[i] += (*to_sum_samples.right.pdata)[i];
                    }
                    to_sum_samples.left.size = to_sum_samples.right.size = 0;
                }

                //processed samples, go to a queue, each channel separately
                size_t sizepr = processed_samples.left.size;
                size_t overlap = fir_size - 1;
                
                for (unsigned i = 0; i < sizepr - overlap; i++) {
                    (*ready_samples.left.pdata)[i + ready_samples.left.size] = (*processed_samples.left.pdata)[i];
                    (*ready_samples.right.pdata)[i + ready_samples.right.size] = (*processed_samples.right.pdata)[i];
                }
                ready_samples.left.size = ready_samples.right.size = ready_samples.left.size + sizepr - overlap;

                // save M - 1 to sum, on each channel
                for (unsigned i = sizepr - overlap; i < sizepr; i++) {
                    (*to_sum_samples.left.pdata)[i - (sizepr - overlap)] = (*processed_samples.left.pdata)[i];
                    (*to_sum_samples.right.pdata)[i - (sizepr - overlap)] = (*processed_samples.right.pdata)[i];
                }
                to_sum_samples.left.size = to_sum_samples.right.size = sizepr - (sizepr - overlap);

                //// End overlap-add ////
            }

            // If we have processed samples, return them to xaudio2
            if (ready_samples.left.size > 0) {

                UINT32 limit = min((UINT32)ready_samples.left.size, this->max_frame_count * 2);
                size_t size_obuffer = limit - (limit % 2);

                //Put into output buffer
                for (unsigned i = 0; i < size_obuffer; i+=2) {
                    pvDstf[i]     = (float)(*ready_samples.left.pdata)[i / 2].real();
                    pvDstf[i + 1] = (float)(*ready_samples.right.pdata)[i / 2].real();
                }

                size_t ch_size = size_obuffer / 2;  // Each channel outputs half buffer frames

                //Rotate and refresh size counter, on each channel
                for (unsigned i = ch_size; i < ready_samples.left.size; i++) {
                    (*ready_samples.left.pdata)[i - ch_size] = (*ready_samples.left.pdata)[i];
                    (*ready_samples.right.pdata)[i - ch_size] = (*ready_samples.right.pdata)[i];
                }
                ready_samples.left.size = ready_samples.left.size - ch_size;
                ready_samples.right.size = ready_samples.right.size - ch_size;


                pOutputProcessParameters[0].ValidFrameCount = size_obuffer;
            }
            // We don't have any processed sample, don't output anything
            else {
                pOutputProcessParameters[0].ValidFrameCount = 0;
            }

            new_samples.size = 0;

            //// DEBUG ////
            /*
            if (!done) {
                tries++;
                if (tries > 10) {
                    done = true;
                }

                outputToFile(*(ready_samples.left.pdata), tries, true);
            }

            if(new_len > step_size)
                std::cout << "samples\tprocessed " << step_size << " real frames, " << process_samples.size << " total" << std::endl;
            std::cout << "samples\tsaved: " << saved_samples.size << " frames in total" << std::endl;
            std::cout << "samples\tready: " << ready_samples.left.size << " frames" << std::endl;
            std::cout << "samples\tto sum: " << to_sum_samples.left.size << " frames" << std::endl;
            std::cout << "buffer_output: " << pOutputProcessParameters[0].ValidFrameCount << " frames" << std::endl;
            std::cout << "--------------------" << std::endl;
            
            */
            //// DEBUG-END ////

            // Copy result to output buffer
            //memcpy(pvDst, pvSrc, pInputProcessParameters[0].ValidFrameCount * m_uChannels * m_uBytesPerSample);

            break;
        }

        case XAPO_BUFFER_SILENT:
        {
            // All that needs to be done for this case is setting the
            // output buffer flag to XAPO_BUFFER_SILENT which is done below.
            break;
        }

        }

        // set buffer flags
        pOutputProcessParameters[0].BufferFlags = pInputProcessParameters[0].BufferFlags;     // set destination buffer flags same as source

    }

};