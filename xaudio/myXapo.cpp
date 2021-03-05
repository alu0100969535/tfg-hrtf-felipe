#pragma once
#include "myXapo.h"
#include "filter_data.h"

myXapo::myXapo(XAPO_REGISTRATION_PROPERTIES* prop, filter_data* filters, size_t filters_size) : CXAPOBase(prop) {

    m_uChannels = 1;
    m_uBytesPerSample = 0;

    new_samples.pdata = new CArray(512);
    new_samples.size = 0;

    //UINT32 fir_size = filters[0].data.buffer->AudioBytes / 2; // We have 2 interleaved channels, so half samples for each filter 
    fir_size = 128;                     // M
    fft_n = 8 * pow(2, ceil(log2(fir_size)));  // N
    step_size = fft_n - (fir_size - 1);        // L


    for (unsigned i = 0; i < filters_size; i++) {

        CArray hrtf_temp = CArray();

        UINT32 size = filters[i].data.buffer->AudioBytes / 2;
        
        transformData(hrtf_temp, (int16_t*)filters[i].data.buffer->pAudioData, size, size);

        CArray left  = hrtf_temp[std::slice(0, hrtf_temp.size() / 2, 2)];
        CArray right = hrtf_temp[std::slice(1, hrtf_temp.size() / 2, 2)];

        //std::cout << "i: " << i << std::endl;

        filters[i].fir.left = new CArray(fft_n);
        filters[i].fir.right = new CArray(fft_n);

        for (unsigned j = 0; j < left.size(); j++) {
            (*filters[i].fir.left)[j] = left[j];
        }

        for (unsigned j = left.size(); j < fft_n; j++) {
            (*filters[i].fir.left)[j] = Complex(0, 0);
        }

        for (unsigned j = 0; j < right.size(); j++) {
            (*filters[i].fir.right)[j] = right[j];
        }

        for (unsigned j = right.size(); j < fft_n; j++) {
            (*filters[i].fir.right)[j] = Complex(0, 0);
        }

        fft(*filters[i].fir.left);
        fft(*filters[i].fir.right);

        //outputToFile(*filters[i].fir.left, 1, true);
        //outputToFile(*filters[i].fir.right, 2, true);

    }

    hrtf_database = filters;


    saved_samples.pdata = new CArray(step_size + new_samples.size);
    saved_samples.size = 0;

    process_samples.pdata = new CArray(fft_n);
    process_samples.size = 0;

    processed_samples.left.pdata = new CArray(fft_n);
    processed_samples.left.size = 0;
    processed_samples.right.pdata = new CArray(fft_n);
    processed_samples.right.size = 0;

    to_sum_samples.left.pdata = new CArray(fir_size);
    to_sum_samples.left.size = 0;
    to_sum_samples.right.pdata = new CArray(fir_size);
    to_sum_samples.right.size = 0;

    ready_samples.left.pdata = new CArray(fft_n * 200);
    ready_samples.left.size = 0;
    ready_samples.right.pdata = new CArray(fft_n * 200);
    ready_samples.right.size = 0;

}

myXapo::~myXapo() {
    delete saved_samples.pdata;
    delete process_samples.pdata;
    delete processed_samples.left.pdata;
    delete processed_samples.right.pdata;
    delete to_sum_samples.left.pdata;
    delete to_sum_samples.right.pdata;
    delete ready_samples.left.pdata;
    delete ready_samples.right.pdata;
}