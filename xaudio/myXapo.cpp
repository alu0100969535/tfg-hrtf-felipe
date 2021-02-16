#pragma once
#include "myXapo.h"

myXapo::myXapo(XAPO_REGISTRATION_PROPERTIES* prop) : CXAPOBase(prop) {

    m_uChannels = 1;
    m_uBytesPerSample = 0;

    fft_size = 512; // Change this when appropiate

    //int hrtf_samples = 512;
    //int i_kemar_samples = 512
    
}

myXapo::~myXapo() {
}