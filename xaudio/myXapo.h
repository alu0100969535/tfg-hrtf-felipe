#pragma once
#include <xaudio2.h>
#include <xaudio2fx.h>
#include <x3daudio.h>
#include <xapo.h>
#include <xapobase.h>
#include <assert.h>

#include <complex>
#include <iostream>
#include <valarray>
#include <limits>       // std::numeric_limits

#include <fstream> // basic file operations
#include <string>


#include <algorithm> // std::max

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

const double PI = 3.141592653589793238460;


typedef std::complex<double> Complex;
typedef std::valarray<Complex> CArray;

// Debug function, writes to file so we can plot some samples
// TODO: Remove this when done
inline void outputToFile(CArray& arr, const int tries, const bool real = false) {
    std::ofstream myfile;
    const std::string file = "C:\\Users\\faaa9\\AppData\\Roaming\\WickedEngineDev\\example" + std::to_string(tries) + ".txt";
    myfile.open(file);

    if (!myfile.is_open()) {
        assert(0);
    }

    for (int i = 0; i < arr.size()/2; i += 1) {
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

// Cooley–Tukey FFT (in-place, divide-and-conquer)
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

// inverse fft (in-place)
inline void ifft(CArray& x)
{
    // conjugate the complex numbers
    x = x.apply(std::conj);

    // forward fft
    fft(x);

    // conjugate the complex numbers again
    x = x.apply(std::conj);

    // scale the numbers
    x /= x.size();
}

// Performs overlap add method on DFTs, in-place
// TODO: Filter is implemented this way, this function will do all the work
// TODO: Not finished
inline void overlap_add(CArray& input, CArray& filter, CArray& i_kemar, CArray& add, size_t i_size, size_t f_size) {

    // Construct blocks of N samples
    // N = L + M - 1
    // L -> input_size
    // M -> filter_size  -> pad with 0s to match N

    size_t n = i_size + f_size - 1;

    //std::cout << "overlap-add, n: " << n << ", input: " << i_size << ", filter: " << f_size << std::endl;

    //outputToFile(input, 1);

    // Prepare filter, pad with 0s
    CArray filter_block = CArray();
    filter_block.resize(n);
    for (int j = 0; j < f_size; j++) {
        filter_block[j] = filter[j];
    }

    for (int j = f_size; j < n; j++) {
        filter_block[j] = Complex(0, 0);
    }

    // Do this until no more input
    for (int i = 0; i < input.size(); i += n) {
        // Prepare block, pad with 0s
        CArray block = CArray();
        block.resize(n);

        int j_start = i;
        int j_end = i + i_size;
        if (j_end > input.size() - 1) {
            j_end = input.size() - 1;
        }

        for (int j = j_start; j < j_end; j++) {
            block[j] = input[j];
        }
        for (int j = j_end; j < n; j++) {
            block[j] = Complex(0, 0);
        }


        // Indexes for multiplication, overlap
        int start = i - (f_size - 1);
        if (start < 0) {
            start = 0;
        }
        int end = start + block.size();
        if (end > input.size() - 1) {
            end = input.size() - 1;
        }

        // Multiply 1 vs 1 and add on overlapping sections
        for (int j = start; j < end; j++) {
            int current = j - start;
            // std::cout<< "i: "<< i << ", j: " << j << ", current: " << current << std::endl;
            if (j < i) {
                input[j] = block[current] * filter_block[current] * i_kemar[current];
            }
            else {
                input[j] = block[current] * filter_block[current] * i_kemar[current];
            }
        }
    }

    // Save M - 1 samples
    CArray lastOverlapBlock = CArray();
    lastOverlapBlock.resize(f_size - 1);

    for (int i = 0; i < lastOverlapBlock.size(); i++) {

    }

    //outputToFile(input, 2);
    //outputToFile(block, 3);
    //outputToFile(filter_block, 4);

}

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


    /*for (int i = 0; i < size; i += 1) {
        arr[i] = Complex(p[i], 0);
    }*/

    for (int i = size; i < nfft; i += 1) {
        arr[i] = Complex(0, 0);
    }
}

// Perform hannWindow in-place only to real part
inline void applyHannWindow(CArray& arr) {

    const size_t size = arr.size();

    for (int i = 0; i < size; i++) {
        double multiplier = 0.5 * (1 - cos(2 * PI * i / (size - 1)));
        arr[i] = Complex(multiplier * arr[i].real(), 0);
    }
}

inline HRESULT FindChunk2(HANDLE hFile, DWORD fourcc, DWORD& dwChunkSize, DWORD& dwChunkDataPosition)
{
    HRESULT hr = S_OK;
    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, 0, NULL, FILE_BEGIN))
        return HRESULT_FROM_WIN32(GetLastError());

    DWORD dwChunkType;
    DWORD dwChunkDataSize;
    DWORD dwRIFFDataSize = 0;
    DWORD dwFileType;
    DWORD bytesRead = 0;
    DWORD dwOffset = 0;

    while (hr == S_OK)
    {
        DWORD dwRead;
        if (0 == ReadFile(hFile, &dwChunkType, sizeof(DWORD), &dwRead, NULL))
            hr = HRESULT_FROM_WIN32(GetLastError());

        if (0 == ReadFile(hFile, &dwChunkDataSize, sizeof(DWORD), &dwRead, NULL))
            hr = HRESULT_FROM_WIN32(GetLastError());

        switch (dwChunkType)
        {
        case fourccRIFF:
            dwRIFFDataSize = dwChunkDataSize;
            dwChunkDataSize = 4;
            if (0 == ReadFile(hFile, &dwFileType, sizeof(DWORD), &dwRead, NULL))
                hr = HRESULT_FROM_WIN32(GetLastError());
            break;

        default:
            if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, dwChunkDataSize, NULL, FILE_CURRENT))
                return HRESULT_FROM_WIN32(GetLastError());
        }

        dwOffset += sizeof(DWORD) * 2;

        if (dwChunkType == fourcc)
        {
            dwChunkSize = dwChunkDataSize;
            dwChunkDataPosition = dwOffset;
            return S_OK;
        }

        dwOffset += dwChunkDataSize;

        if (bytesRead >= dwRIFFDataSize) return S_FALSE;

    }

    return S_OK;

}

inline HRESULT ReadChunkData2(HANDLE hFile, void* buffer, DWORD buffersize, DWORD bufferoffset)
{
    HRESULT hr = S_OK;
    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, bufferoffset, NULL, FILE_BEGIN))
        return HRESULT_FROM_WIN32(GetLastError());
    DWORD dwRead;
    if (0 == ReadFile(hFile, buffer, buffersize, &dwRead, NULL))
        hr = HRESULT_FROM_WIN32(GetLastError());
    return hr;
}

inline HRESULT openWav2(const TCHAR* name, WAVEFORMATEXTENSIBLE& wfx, XAUDIO2_BUFFER& buffer) {
    // Open the file
    HANDLE hFile = CreateFile(
        name,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (INVALID_HANDLE_VALUE == hFile)
        return HRESULT_FROM_WIN32(GetLastError());

    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, 0, NULL, FILE_BEGIN))
        return HRESULT_FROM_WIN32(GetLastError());

    // Read the file
    DWORD dwChunkSize;
    DWORD dwChunkPosition;
    //check the file type, should be fourccWAVE or 'XWMA'
    FindChunk2(hFile, fourccRIFF, dwChunkSize, dwChunkPosition);
    DWORD filetype;
    ReadChunkData2(hFile, &filetype, sizeof(DWORD), dwChunkPosition);
    if (filetype != fourccWAVE)
        return S_FALSE;

    FindChunk2(hFile, fourccFMT, dwChunkSize, dwChunkPosition);
    ReadChunkData2(hFile, &wfx, dwChunkSize, dwChunkPosition);

    //fill out the audio data buffer with the contents of the fourccDATA chunk
    FindChunk2(hFile, fourccDATA, dwChunkSize, dwChunkPosition);
    BYTE* pDataBuffer = new BYTE[dwChunkSize];
    ReadChunkData2(hFile, pDataBuffer, dwChunkSize, dwChunkPosition);

    buffer.AudioBytes = dwChunkSize;  //size of the audio buffer in bytes
    buffer.pAudioData = pDataBuffer;  //buffer containing audio data
    buffer.Flags = XAUDIO2_END_OF_STREAM; // tell the source voice not to expect any data after this buffer
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    buffer.LoopBegin = UINT32(0);
}

class myXapo : public CXAPOBase {

private:
    WORD m_uChannels;
    WORD m_uBytesPerSample;

    unsigned fft_size;
    unsigned* indexes;

    // DEBUG
    bool done = false;
    int tries = 0;
    
    CArray hrtf = CArray();
    CArray i_kemar = CArray();

public:

    myXapo(XAPO_REGISTRATION_PROPERTIES* pRegistrationProperties);
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

            float* pvSrcf = static_cast<float*>(pvSrc); // input, as float

            UINT32 len = pInputProcessParameters[0].ValidFrameCount * m_uChannels;

            // Prepare array to perform FFT
            CArray arr;
            transformData(arr, pvSrcf, len);

            // Apply Hann Window
            applyHannWindow(arr);
            
            // Convert to freq domain
            fft(arr);

            // Set DC bin to 0
            arr[0] = Complex(0, 0);

            // HRTF Processing here

            for (int i = 0; i < arr.size(); i++) {
                arr[i] *= hrtf[i];
                //arr[i] *= i_kemar[i];
            }

            // TODO: This doesn't go here anymore, inside this method should go HRTF processing
            //overlap_add(arr, hrtf, i_kemar, 257, 256);
            
            // Convert to time domain
            ifft(arr);

            // DEBUG
            /*
            if (!done) {
                tries++;
                if (tries > 10) {
                    done = true;
                }

                outputToFile(arr, tries);
            }*/

            //DEBUG-END

            // Copy result to output buffer
            memcpy(pvDst, pvSrc, pInputProcessParameters[0].ValidFrameCount * m_uChannels * m_uBytesPerSample);

            break;
        }

        case XAPO_BUFFER_SILENT:
        {
            // All that needs to be done for this case is setting the
            // output buffer flag to XAPO_BUFFER_SILENT which is done below.
            break;
        }

        }

        // set destination valid frame count, and buffer flags
        pOutputProcessParameters[0].ValidFrameCount = pInputProcessParameters[0].ValidFrameCount; // set destination frame count same as source
        pOutputProcessParameters[0].BufferFlags = pInputProcessParameters[0].BufferFlags;     // set destination buffer flags same as source

    }

};