#pragma once
#include "myXapo.h"

myXapo::myXapo(XAPO_REGISTRATION_PROPERTIES* prop) : CXAPOBase(prop) {

    m_uChannels = 1;
    m_uBytesPerSample = 0;

    fft_size = 512; // Change this when appropiate

    //TODO: Move filter management to xaudio.cpp
    // OPEN FILTERS AND FFT THEM
    int hrtf_samples = 512;
    int i_kemar_samples = 512;

    // Load HRTF library
    const TCHAR* strFileName = TEXT("D:\\GitHub\\audio3d\\src\\audio3d\\kemar\\full\\elev50\\L50e232a.wav");

    WAVEFORMATEXTENSIBLE wfx = { 0 };
    XAUDIO2_BUFFER buffer = { 0 };

    openWav2(strFileName, wfx, buffer);

    const int16_t* pvSrcf = reinterpret_cast<const int16_t*>(buffer.pAudioData);

    std::cout << buffer.AudioBytes << std::endl;

    // Prepare array to perform FFT
    CArray arr;
    transformData(arr, pvSrcf, hrtf_samples); // 512 samples for HRTF

    outputToFile(arr, 1);

    fft(arr);
    // Set DC to 0
    arr[0] = Complex(0, 0);
    hrtf = arr;

    // Load kemar inverse filter
    const TCHAR* strFileName2 = TEXT("D:\\GitHub\\audio3d\\src\\audio3d\\kemar\\full\\headphones+spkr\\Opti-inverse.wav");

    WAVEFORMATEXTENSIBLE wfx2 = { 0 };
    XAUDIO2_BUFFER buffer2 = { 0 };

    openWav2(strFileName2, wfx2, buffer2);

    const int16_t* kemarf = reinterpret_cast<const int16_t*>(buffer2.pAudioData);

    std::cout << buffer2.AudioBytes << std::endl;

    // Prepare array to perform FFT
    CArray arr2;
    transformData(arr2, kemarf, i_kemar_samples); // 512 samples for HRTF

    //outputToFile(arr2, 1);

    fft(arr2);

    // Set DC to 0
    arr2[0] = Complex(0, 0);

    i_kemar = arr2;

    std::cout << "Loaded HRTF database!" << "hrtf: " << hrtf_samples << "i_kemar: " << i_kemar_samples << std::endl;
    
}

myXapo::~myXapo() {
}