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

#include "filter_data.h"
#include "coordinates.h"


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


void setPosition(IXAPOParameters* effect, spherical_coordinates position) {
    effect->SetParameters(&position, sizeof(position));
}

int loadFilters(filter_data* filters, size_t* size) {
    
    const array<int, 14> elevations = {-40, -30, -20, -10, 0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
    const array<double, 14> azimuth_inc = {6.43, 6.00, 5.00, 5.00, 5.00, 5.00, 5.00, 6.00, 6.43, 8.00, 10.00, 15.00, 30.00, 91};
    
    std::string prevPath = "filters";
    std::string name = "elev";
    std::string separator = "\\";
    std::string start = "H";

    unsigned index = 0;

    for (unsigned i = 0; i < elevations.size(); i++) {
        std::string elevation = to_string(elevations[i]);
        std::string folder = name + elevation;

        unsigned j = 0;
        while((j + 1) * azimuth_inc[i] <= 180) {

            unsigned azimuth = round(azimuth_inc[i] * j);

            std::stringstream file;
            file << elevation << "e";
            file << std::setfill('0') << std::setw(3) <<(unsigned) azimuth << "a.wav";

            std::string path = prevPath + separator + folder + separator + start + file.str();
            // Start importing .wav
            WAVEFORMATEXTENSIBLE *wfx = new WAVEFORMATEXTENSIBLE({ 0 });
            XAUDIO2_BUFFER *buffer = new XAUDIO2_BUFFER({ 0 });

            HRESULT hr = openWav(path.c_str(), *wfx, *buffer);
            if (FAILED(hr)) {
                std::cout << "Couldn't open " << path << "." << std::endl;
                return hr;
            }

            filters[index] = filter_data {
                raw_data {
                    wfx,
                    buffer,
                },
                fir {
                    NULL,
                    NULL,
                },
                elevations[i],
                azimuth,
            };
            index++;
            j++;
            
        }

    }
    *size = index - 1;
    return 0;
}

/*
Función principal que carga la librería xAudio2,
carga un fichero .wav e intenta reproducirlo aplicando
el filtro que está especificado en myXapo
*/
int main() { 
    HRESULT hr;

    // Required for xAudio2
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
    hr = pXAudio2->CreateMasteringVoice(&pMasterVoice, 2);
    if (FAILED(hr)) {
        std::cout << "CreateMasteringVoice() failed" << std::endl;
        assert(0);
        return false;
    }

    // Load filters for xAPO class
    filter_data* filters = (filter_data*)malloc(710 * sizeof(filter_data));
    size_t* filters_size = new size_t();

    hr = loadFilters(filters, filters_size);
    if (FAILED(hr)) {
        std::cout << "loadFilters() failed" << std::endl;
        assert(0);
        return false;
    }

    // We create the necessary structures to load custom XAPOs
    XAPO_REGISTRATION_PROPERTIES prop = {};
    prop.Flags = XAPO_FLAG_FRAMERATE_MUST_MATCH
        | XAPO_FLAG_BITSPERSAMPLE_MUST_MATCH
        | XAPO_FLAG_BUFFERCOUNT_MUST_MATCH;
    prop.MinInputBufferCount = 1;
    prop.MaxInputBufferCount = 1;
    prop.MaxOutputBufferCount = 1;
    prop.MinOutputBufferCount = 1;

    spherical_coordinates* sync = static_cast<spherical_coordinates*>(malloc(sizeof(spherical_coordinates) * 3));

    for (unsigned i = 0; i < 3; i++)
        sync[i] = { 0, 0, 0 };


    // Start importing .wav
    WAVEFORMATEXTENSIBLE wfx = { 0 };
    XAUDIO2_BUFFER buffer = { 0 };

#ifdef _XBOX
    char* strFileName = "game:\\media\\MusicMono.wav";
#else
    //const std::string strFileName = "test_audio\\440hz.wav";
    const std::string strFileName = "test_audio\\motor.wav";
    //const std::string strFileName = "test_audio\\guitar-harmonics.wav";
#endif

    openWav(strFileName.c_str(), wfx, buffer); // Output in wfx and buffer

    // Load custom Xapo instance
    IXAPOParameters* pXAPO = new myXapo(&prop, (BYTE*) sync,(UINT32) sizeof(cartesian_coordinates3d), filters, *filters_size, buffer.AudioBytes / 4);

    // Structure for custom xAPO
    XAUDIO2_EFFECT_DESCRIPTOR descriptor;
    descriptor.InitialState = true; // Efect applied from start
    descriptor.OutputChannels = 2; // TODO: Change to 2 for HRTF
    descriptor.pEffect = pXAPO; // Custom xAPO to apply

    // Structure indicating how many effects we have (1)
    XAUDIO2_EFFECT_CHAIN chain;
    chain.EffectCount = 1;
    chain.pEffectDescriptors = &descriptor;

    // Create a Submix Voice
    IXAudio2SubmixVoice* pSFXSubmixVoice = NULL;
    hr = pXAudio2->CreateSubmixVoice(&pSFXSubmixVoice, 1, 44100, 0, 0, 0, &chain);
    if (FAILED(hr)) {
        std::cout << "CreateSubmixVoice() failed" << std::endl;
        assert(0);
        return false;
    }

#ifdef _DEBUG
    XAUDIO2_DEBUG_CONFIGURATION debugConfig = {};
    debugConfig.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
    debugConfig.BreakMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
    pXAudio2->SetDebugConfiguration(&debugConfig);
#endif // _DEBUG


    // Populate structures for xaudio2, so it knows where to send data
    XAUDIO2_SEND_DESCRIPTOR voice;
    voice.Flags = 0;
    voice.pOutputVoice = pSFXSubmixVoice;

    XAUDIO2_VOICE_SENDS sendlist;
    sendlist.SendCount = 1;
    sendlist.pSends = &voice;

    // Create source voice
    IXAudio2SourceVoice* pSourceVoice;
    hr = pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&wfx, 0, 2.0, NULL, &sendlist, NULL);
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

    //setPosition(pXAPO, cartesian3d2spherical({ 1, 1, 1.75 }));
    setPosition(pXAPO, { 1.4, 0, 0 });

    // Play the sound (when ready)
    hr = pSourceVoice->Start(0);
    if (FAILED(hr)) {
        std::cout << "Start() failed" << std::endl;
        assert(0);  
        return false;
    }

    std::cout << "Everything loaded!" << std::endl;

    //cartesian_coordinates3d sound = {1.4, 5, 1.75};
    cartesian_coordinates3d player = {0, 0, 1.75};    //Player ears

    spherical_coordinates sound = { 1.4, 0, 0 };

    bool forward = true;

    // TODO: Change from spherical to cartesian
    while (true) {
        // Pausing so we can hear the sound
        system("pause");

        sound.azimuth = sound.azimuth + 5;

        if (abs(sound.azimuth) == 180) {
            sound.azimuth = -sound.azimuth;
        }
        std::cout << "changing to azimuth: " << sound.azimuth << " elevation: " << sound.elevation << " radius: " << sound.radius << std::endl;

        setPosition(pXAPO, sound);
    }

    /*while (true) {
        // Pausing so we can hear the sound when loaded
        system("pause");
        if (forward) {
            sound.y = sound.y + 1;
        }
        else {
            sound.y = sound.y - 1;
        }
        if (abs(sound.y) == 5) forward = !forward;

        cartesian_coordinates3d travel;
        travel.x = sound.x - player.x;
        travel.y = sound.y - player.y;
        travel.z = sound.z - player.z;

        setPosition(pXAPO, cartesian3d2spherical(travel));
        std::cout << "changing to x: " << travel.x  << " y: "<< travel.y << " z: " << travel.z << std::endl;
        std::cout << "changing to azimuth: " << rad2degree(cartesian3d2spherical(travel).azimuth) << " elevation: " << rad2degree(cartesian3d2spherical(travel).elevation) << " radius: " << cartesian3d2spherical(travel).radius << std::endl;
    }*/

    // Close COM Library
    CoUninitialize();

    // Nothing more to do, exit...
    return 0;
}