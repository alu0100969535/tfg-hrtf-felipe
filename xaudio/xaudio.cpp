/***********************************************************************************
*
* xaudio.cpp
*
***********************************************************************************
*
* AUTORES: Felipe Andrés Álvarez Avaria
*
* FECHA: 18/01/2021
*
* DESCRIPCION: Aquí inicializamos la librería de sonido xAudio2, y myXapo.cpp
*
************************************************************************************/

#include <iostream>
#include <wrl/client.h> // ComPtr
#include <xaudio2.h>
#include <xaudio2fx.h>
#include <x3daudio.h>
#include <xapo.h>
#include <xapobase.h>
#include "myXapo.h"
#include <stdexcept>
#include <string>
#include <sstream>
#include <iomanip>
#include <array>



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

using namespace std;

/*
Función auxiliar que encuentra un trozo (chunk) de datos específico 
de un fichero .wav.

Devuelve los datos en dwChunkSize y dwChunkDataPosition para leer

*/
HRESULT FindChunk(HANDLE hFile, DWORD fourcc, DWORD& dwChunkSize, DWORD& dwChunkDataPosition)
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

/*
Función auxiliar para leer los datos que se encontraron en un fichero .wav
Nos devuelve el buffer de datos en buffer.
*/
HRESULT ReadChunkData(HANDLE hFile, void* buffer, DWORD buffersize, DWORD bufferoffset)
{
    HRESULT hr = S_OK;
    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, bufferoffset, NULL, FILE_BEGIN))
        return HRESULT_FROM_WIN32(GetLastError());
    DWORD dwRead;
    if (0 == ReadFile(hFile, buffer, buffersize, &dwRead, NULL))
        hr = HRESULT_FROM_WIN32(GetLastError());
    return hr;
}
/*
Función auxiliar que nos carga un fichero .wav en el programa y en la libreria xaudio2
Devuelve una estrucutra de datos y el buffer de xaudio2
*/
HRESULT openWav(const LPCSTR name, WAVEFORMATEXTENSIBLE& wfx, XAUDIO2_BUFFER& buffer) {
    // Open the file
    HANDLE hFile = CreateFileA(
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
    FindChunk(hFile, fourccRIFF, dwChunkSize, dwChunkPosition);
    DWORD filetype;
    ReadChunkData(hFile, &filetype, sizeof(DWORD), dwChunkPosition);
    if (filetype != fourccWAVE)
        return S_FALSE;

    FindChunk(hFile, fourccFMT, dwChunkSize, dwChunkPosition);
    ReadChunkData(hFile, &wfx, dwChunkSize, dwChunkPosition);

    //fill out the audio data buffer with the contents of the fourccDATA chunk
    FindChunk(hFile, fourccDATA, dwChunkSize, dwChunkPosition);
    BYTE* pDataBuffer = new BYTE[dwChunkSize];
    ReadChunkData(hFile, pDataBuffer, dwChunkSize, dwChunkPosition);

    buffer.AudioBytes = dwChunkSize;  //size of the audio buffer in bytes
    buffer.pAudioData = pDataBuffer;  //buffer containing audio data
    buffer.Flags = XAUDIO2_END_OF_STREAM; // tell the source voice not to expect any data after this buffer
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    buffer.LoopBegin = UINT32(0);
}

struct filter_data {
    WAVEFORMATEXTENSIBLE* wfx;
    XAUDIO2_BUFFER* buffer;
    int elevation;
    unsigned angle;
    bool left;
};


// TODO: Load all elevations
// TODO: FFT all buffers here? Maybe in myXapo
int loadFilters(filter_data* filters) {
    
    const array<int,5/*14*/> elevations = {/*-40, -30,*/ -20, -10, 0, 10, 20, /*30, 40, 50, 60, 70, 80, 90*/};
    
    std::string prevPath = "filters";
    std::string name = "elev";
    std::string separator = "\\";
    std::string left = "L";
    std::string right = "R";

    unsigned index = 0;

    for (unsigned i = 0; i < elevations.size(); i++) {
        std::string elevation = to_string(elevations[i]);
        std::string folder = name + elevation;

        for (unsigned j = 0; j < 360; j += 5) {

            std::stringstream file;
            file << elevation << "e";
            file << std::setfill('0') << std::setw(3) << j << "a.wav";


            std::array<string, 2> paths;

            std::string path = prevPath + separator + folder + separator;

            paths[0] = path + right + file.str(); //Right ear
            paths[1] = path + left  + file.str(); //Left ear

            //std::cout << pathFileR << std::endl << pathFileL << std::endl;

            // Start importing .wav
            for (unsigned k = 0; k < paths.size(); k++) {
                WAVEFORMATEXTENSIBLE wfx = { 0 };
                XAUDIO2_BUFFER buffer = { 0 };

                HRESULT hr = openWav(paths[k].c_str(), wfx, buffer);
                if (FAILED(hr)) {
                    std::cout << "Couldn't open " << paths[k] << "." << std::endl;
                    //assert(0);
                    return hr;
                }

                filters[index] = filter_data {
                    &wfx,
                    &buffer,
                    elevations[i],
                    j,
                    bool(k)
                };
                index++;

            }
           
        }

    }
    return 0;
}

/*
Función principal que carga la librería xAudio2,
carga un fichero .wav e intenta reproducirlo aplicando
el filtro que está especificado en myXapo
*/
int main() { 
    HRESULT hr;

    // Required for xAPO processing
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cout << "CoInitializeEx() failed" << std::endl;
        assert(0);
        return false;
    }

    // Create a instance of xAudio2 Engine
    Microsoft::WRL::ComPtr<IXAudio2> pXAudio2 = NULL;
    hr = XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        std::cout << "XAudio2Create() failed" << std::endl;
        assert(0);
        return false;
    }

    // Create a mastering voice
    IXAudio2MasteringVoice* pMasterVoice = NULL;
    hr = pXAudio2->CreateMasteringVoice(&pMasterVoice);
    if (FAILED(hr)) {
        std::cout << "CreateMasteringVoice() failed" << std::endl;
        assert(0);
        return false;
    }

#ifdef _DEBUG
    XAUDIO2_DEBUG_CONFIGURATION debugConfig = {};
    debugConfig.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
    debugConfig.BreakMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
    pXAudio2->SetDebugConfiguration(&debugConfig);
#endif // _DEBUG

    // Start importing .wav
    WAVEFORMATEXTENSIBLE wfx = { 0 };
    XAUDIO2_BUFFER buffer = { 0 };
    // TODO:  Change 5000 for a more appropiate value
    filter_data* wavData = (filter_data*)malloc(5000 * sizeof(filter_data));

    hr = loadFilters(wavData);
    if (FAILED(hr)) {
        std::cout << "loadFilters() failed" << std::endl;
        assert(0);
        return false;
    }

#ifdef _XBOX
    char* strFileName = "game:\\media\\MusicMono.wav";
#else
    const std::string strFileName = "test_audio\\440hz.wav";
#endif

    openWav(strFileName.c_str(), wfx, buffer); // Output in wfx and buffer

    // Create source voice
    IXAudio2SourceVoice* pSourceVoice;
    hr = pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&wfx);
    if (FAILED(hr)) {
        std::cout << "CreateSourceVoice() failed" << std::endl;
        assert(0);
        return false;
    }

    // Submit buffer to voice
    hr = pSourceVoice->SubmitSourceBuffer(&buffer);
    if (FAILED(hr)) {
        std::cout << "SubmitSourceBuffer() failed" << std::endl;
        assert(0);
        return false;
    }

    // We create the necessary structures to load custom XAPOs
    XAPO_REGISTRATION_PROPERTIES prop = {};
    prop.Flags = XAPOBASE_DEFAULT_FLAG; // | XAPO_FLAG_INPLACE_REQUIRED;
    prop.MinInputBufferCount = 1;
    prop.MaxInputBufferCount = 1;
    prop.MaxOutputBufferCount = 1;
    prop.MinOutputBufferCount = 1;

    // Load custom Xapo instance
    IUnknown* pXAPO = new myXapo(&prop);

    // Structure for custom xAPO
    XAUDIO2_EFFECT_DESCRIPTOR descriptor;
    descriptor.InitialState = true; // Efect applied from start
    descriptor.OutputChannels = 1; // TODO: Change to 2 for HRTF
    descriptor.pEffect = pXAPO; // Custom xAPO to apply

    // Structure indicating how many effects we have (1)
    XAUDIO2_EFFECT_CHAIN chain;
    chain.EffectCount = 1;
    chain.pEffectDescriptors = &descriptor;

    // Apply effect chain
    hr = pSourceVoice->SetEffectChain(&chain);
    if (FAILED(hr)) {
        std::cout << "SetEffectChain() failed" << std::endl;
        assert(0);
        return false;
    }

    // Play the sound (when ready)
    hr = pSourceVoice->Start(0);
    if (FAILED(hr)) {
        std::cout << "Start() failed" << std::endl;
        assert(0);  
        return false;
    }

    std::cout << "Everything loaded!" << std::endl;

    // Pausing so we can hear the sound when loaded
    system("pause");

    // Nothing more to do, exit...
    return 0;
}