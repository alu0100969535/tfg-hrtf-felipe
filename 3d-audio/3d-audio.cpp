// 3d-audio.cpp : Defines the functions for the static library.
//
#include "3d-audio.h"

namespace a3d {

    namespace utils {

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
            return S_OK;
        }

        HRESULT loadFilters(filter_data* filters, size_t* size) {

            const std::array<int, 14> elevations = { -40, -30, -20, -10, 0, 10, 20, 30, 40, 50, 60, 70, 80, 90 };
            const std::array<double, 14> azimuth_inc = { 6.43, 6.00, 5.00, 5.00, 5.00, 5.00, 5.00, 6.00, 6.43, 8.00, 10.00, 15.00, 30.00, 91 };

            WCHAR wexe[MAX_PATH];
            GetModuleFileNameW(NULL, wexe, MAX_PATH);
            std::wstring::size_type pos = std::wstring(wexe).find_last_of(L"\\/");
            std::wstring exe = std::wstring(wexe).substr(0, pos);

            std::string prevPath = "filters";
            std::string name = "elev";
            std::string separator = "\\";
            std::string start = "H";

            unsigned index = 0;

            for (unsigned i = 0; i < elevations.size(); i++) {
                std::string elevation = std::to_string(elevations[i]);
                std::string folder = name + elevation;

                unsigned j = 0;
                while ((j + 1) * azimuth_inc[i] <= 180) {

                    unsigned azimuth = round(azimuth_inc[i] * j);

                    std::stringstream file;
                    file << elevation << "e";
                    file << std::setfill('0') << std::setw(3) << (unsigned)azimuth << "a.wav";

                    std::string path = std::string(exe.begin(), exe.end()) + separator + prevPath + separator + folder + separator + start + file.str();
                    // Start importing .wav
                    WAVEFORMATEXTENSIBLE* wfx = new WAVEFORMATEXTENSIBLE({ 0 });
                    XAUDIO2_BUFFER* buffer = new XAUDIO2_BUFFER({ 0 });

                    HRESULT hr = openWav(path.c_str(), *wfx, *buffer);
                    if (FAILED(hr)) {
                        std::cout << "Couldn't open " << path << "." << std::endl;
                        return hr;
                    }

                    filters[index] = filter_data{
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
            return S_OK;
        }

    }

    void setPosition(IXAPOParameters* effect, spherical_coordinates position) {
        effect->SetParameters(&position, sizeof(position));
    }

    IXAPOParameters* getXAPO(filter_data* filters, size_t filters_size) {
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


        return new myXapo(&prop, (BYTE*)sync, (UINT32)sizeof(cartesian_coordinates3d), filters, filters_size);
    }

    XAUDIO2_EFFECT_DESCRIPTOR getEffectDescriptor(IXAPOParameters* pEffect) {
        XAUDIO2_EFFECT_DESCRIPTOR descriptor;
        descriptor.InitialState = true; // Efect applied from start
        descriptor.OutputChannels = 2;
        descriptor.pEffect = pEffect; // Custom xAPO to apply

        return descriptor;
    }
}
