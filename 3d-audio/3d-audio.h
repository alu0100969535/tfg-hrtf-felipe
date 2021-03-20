#pragma once
#include "framework.h"

#include <Windows.h>   // GetModuleHandleW
#include <wrl/client.h> // ComPtr
#include <xaudio2.h>
#include <xaudio2fx.h>
#include <x3daudio.h>
#include <xapo.h>
#include <xapobase.h>

#include <string>
#include <sstream>
#include <iomanip>
#include <array>

#include "filter_data.h"
#include "myXapo.h"
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

namespace a3d {

	namespace utils {
		/*
		* Función auxiliar que encuentra un trozo (chunk) de datos específico
		* de un fichero .wav.
		*
		* Devuelve los datos en dwChunkSize y dwChunkDataPosition para leer
		*/
		HRESULT FindChunk(HANDLE hFile, DWORD fourcc, DWORD& dwChunkSize, DWORD& dwChunkDataPosition);

		/*
		* Función auxiliar para leer los datos que se encontraron en un fichero .wav
		* Nos devuelve el buffer de datos en buffer.
		*/
		HRESULT ReadChunkData(HANDLE hFile, void* buffer, DWORD buffersize, DWORD bufferoffset);

		/*
		* Función auxiliar que nos carga un fichero .wav en el programa y en la libreria xaudio2
		* Devuelve una estrucutra de datos y el buffer de xaudio2
		*/
		HRESULT openWav(const LPCSTR name, WAVEFORMATEXTENSIBLE& wfx, XAUDIO2_BUFFER& buffer);

		/*
		* Función que carga los filtros HRTF recogidos por el MIT Media Lab en 1994.
		* https://sound.media.mit.edu/resources/KEMAR.html
		*/
		HRESULT loadFilters(filter_data* filters, size_t* size);
	}

	/* 
	* Function that sets a new position to the sound engine, chaning the
	* aparent sound source localization.
	*/
	void setPosition(IXAPOParameters* effect, spherical_coordinates position);

	/* Function that creates a new XAPO instance, 
	 * load all filters and returns a pointer to use
	 * with xAudio2
	 */ 
	IXAPOParameters* getXAPO(filter_data* filters, size_t filters_size);

	/* Returns a structure suitable for this 
	* XAPO to use in xAudio2, must use in 
	* effect chain structure
	*/
	XAUDIO2_EFFECT_DESCRIPTOR getEffectDescriptor(IXAPOParameters* pEffect);
}