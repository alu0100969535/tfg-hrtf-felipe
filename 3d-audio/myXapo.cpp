#pragma once
#include "myXapo.h"
#include "filter_data.h"

myXapo::myXapo(XAPO_REGISTRATION_PROPERTIES* prop, BYTE* params, UINT32 params_size ,filter_data* filters, size_t filters_size) : CXAPOBase(prop), CXAPOParametersBase(prop, params, params_size, false) {

    m_uChannels = 1;
    m_uBytesPerSample = 0;

    index_hrtf = 0;
    flip_filters = false;

    new_samples.pdata = new CArray(512);
    new_samples.size = 0;

    //UINT32 fir_size = filters[0].data.buffer->AudioBytes / 2; // We have 2 interleaved channels, so half samples for each filter 
    fir_size = 128;                            // M
    fft_n = 8 * pow(2, ceil(log2(fir_size)));  // N
    step_size = fft_n - (fir_size - 1);        // L

    // Start preparing filters
    for (unsigned i = 0; i < filters_size; i++) {

        CArray hrtf_temp = CArray();

        UINT32 size = filters[i].data.buffer->AudioBytes / 2;
        
        // Convert data into CArray
        transformData(hrtf_temp, (int16_t*)filters[i].data.buffer->pAudioData, size, size);

        // De-interleave so we have channels separated
        CArray left  = hrtf_temp[std::slice(0, hrtf_temp.size() / 2, 2)];
        CArray right = hrtf_temp[std::slice(1, hrtf_temp.size() / 2, 2)];
        assert(left.size() == right.size());

        // Copy data to fir structure, so we can have its FFT
        filters[i].fir.left = new CArray(fft_n);
        filters[i].fir.right = new CArray(fft_n);

        for (unsigned j = 0; j < left.size(); j++) {
            (*filters[i].fir.left)[j] = left[j];
            (*filters[i].fir.right)[j] = right[j];
        }
        // Pad with 0's to match N
        for (unsigned j = left.size(); j < fft_n; j++) {
            (*filters[i].fir.left)[j] = Complex(0, 0);
            (*filters[i].fir.right)[j] = Complex(0, 0);
        }

        // Convert to frequency domain
        fft(*filters[i].fir.left);
        fft(*filters[i].fir.right);

        //outputToFile(*filters[i].fir.left, 1, true);
        //outputToFile(*filters[i].fir.right, 2, true);

    }

    hrtf_database = filters;
    n_filters = filters_size;


    saved_samples.pdata = new CArray(step_size + new_samples.size);
    saved_samples.size = 0;

    process_samples.pdata = new CArray(fft_n);
    process_samples.size = 0;

    processed_samples.left.pdata = new CArray(fft_n);
    processed_samples.left.size = 0;
    processed_samples.right.pdata = new CArray(fft_n);
    processed_samples.right.size = 0;

    to_sum_samples.left.pdata = new CArray(fir_size - 1);
    to_sum_samples.left.size = 0;
    to_sum_samples.right.pdata = new CArray(fir_size - 1);
    to_sum_samples.right.size = 0;

    ready_samples.left.pdata = new CArray(fft_n * 2);
    ready_samples.left.size = 0;
    ready_samples.right.pdata = new CArray(fft_n * 2);
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

void myXapo::changeFilter(spherical_coordinates input) {
    
    unsigned index = -1;

    spherical_coordinates coords = input;

    this->flip_filters = false;
    if (coords.azimuth < 0) {
        flip_filters = true;
        coords.azimuth = -coords.azimuth;
    }

    double minDiffE = 500000;
    int min_Elevation = -1;
    // Search for good elevation
    for (int i = 0; i < this->n_filters; i++) {
        double diffE = abs(this->hrtf_database[i].elevation - input.elevation);
        if (diffE < minDiffE) {
            minDiffE = diffE;
            min_Elevation = this->hrtf_database[i].elevation;
        }
    }

    //Search for an azimuth with best elevation found
    double minDiffA = 500000;
    int min_Azimuth = -1;
    for (int i = 0; i < this->n_filters; i++) {
        if (this->hrtf_database[i].elevation == min_Elevation) {

            double diffA = abs(this->hrtf_database[i].angle - coords.azimuth);

            if (diffA < minDiffA) {
                minDiffA = diffA;
                min_Azimuth = this->hrtf_database[i].angle;
            }
        }
    }
    // Get the best filter we found
    for (int i = 0; i < n_filters; i++) {
        if (this->hrtf_database[i].angle == min_Azimuth && this->hrtf_database[i].elevation == min_Elevation) {
            index = i;
            break;
        }
    }

    this->index_hrtf = index;
}

unsigned myXapo::getSampleGap(spherical_coordinates input) {
    const double gapPerMeter = 128.57; // Result of 180 samples per 1.4m

    return (unsigned) round(gapPerMeter * input.radius);
}

double myXapo::getGain(spherical_coordinates input) {
    // Using 1.4m as reference
    double distance = 1.4;
    
    if (input.radius / distance < 1) {
        return 0;
    }

    return (input.radius / distance) * -6; // 6dB for every doubling in distance
}
