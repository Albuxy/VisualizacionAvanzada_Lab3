#include <iostream>
#include <fstream>
#include <cmath>
#include <cassert>
#include <algorithm>

#include "hdre.h"

std::map<std::string, HDRE*> HDRE::sHDRELoaded;

HDRE::HDRE()
{
	
}

HDRE::~HDRE()
{
	clean();
}

sHDRELevel HDRE::getLevel(int n)
{
	sHDRELevel level;

	float size = (int)(this->width / pow(2.0, n));

	level.width = size;
	level.height = size; // cubemap sizes!
	level.data = this->faces_array[n];
	level.faces = this->getFaces(n);
	
	return level;
}

float* HDRE::getData()
{
	return this->data;
}

float* HDRE::getFace(int level, int face)
{
	return this->pixels[level][face];
}

float** HDRE::getFaces(int level)
{
	return this->pixels[level];
}

HDRE* HDRE::Get(const char* filename)
{
	assert(filename);

	auto it = sHDRELoaded.find(filename);
	if (it != sHDRELoaded.end())
		return it->second;

	HDRE* hdre = new HDRE();
	if (!hdre->load(filename))
	{
		delete hdre;
		return NULL;
	}

	return hdre;
}

void flipYsides(float ** data, unsigned int size, short num_channels)
{
	// std::cout << "Flipping Y sides" << std::endl;

	size_t face_size = size * size * num_channels * 4; // Bytes!!
	float* temp = new float[face_size];
	memcpy(temp, data[2], face_size);
	memcpy(data[2], data[3], face_size);
	memcpy(data[3], temp, face_size);

	delete[] temp;
}

void flipY(float** data, unsigned int size, short num_channels, bool flip_sides)
{
	assert(data);
	// std::cout << "Flipping Y" << std::endl;

	// bytes
	size_t l = floor(size*0.5);

	int pos = 0;
	int lastpos = size * (size - 1) * num_channels;
	float* temp = new float[size * num_channels];
	size_t row_size = size * num_channels * 4;

	for (unsigned int f = 0; f < 6; ++f)
		for (size_t i = 0; i < l; ++i)
		{
			float* fdata = data[f];

			memcpy(temp, fdata + pos, row_size);
			memcpy(fdata + pos, fdata + lastpos, row_size);
			memcpy(fdata + lastpos, temp, row_size);

			pos += size * num_channels;
			lastpos -= size * num_channels;
			if (pos > lastpos)
			{
				pos = 0;
				lastpos = size * (size - 1) * num_channels;
				continue;
			}
		}

	delete[] temp;

	if (flip_sides)
		flipYsides(data, size, num_channels);
}

bool HDRE::load(const char* filename)
{
	FILE *f;
	assert(filename);

	fopen_s(&f, filename, "rb");
	if (f == NULL)
		return false;

	sHDREHeader HDREHeader;

	fread(&HDREHeader, sizeof(sHDREHeader), 1, f);

	if (HDREHeader.type != 3)
	{
		std::cout << "ArrayType not supported. Please export in Float32Array" << std::endl;
		return false; 
	}

	this->header = HDREHeader;
	this->version = HDREHeader.version;

	if (this->version < 2.0)
	{
		std::cout << "Versions below 2.0 are no longer supported. Please, reexport the environment" << std::endl;
		return false;
	}

	this->numChannels = HDREHeader.numChannels;
	this->bitsPerChannel = HDREHeader.bitsPerChannel;
	this->maxLuminance = HDREHeader.maxLuminance;
	this->type = HDREHeader.type;

	if (HDREHeader.includesSH)
	{
		this->numCoeffs = HDREHeader.numCoeffs;
		this->coeffs = HDREHeader.coeffs;
	}

	int width = this->width = HDREHeader.width;
	int height = this->height = HDREHeader.height;

	int dataSize = 0;
	int w = width;
	int h = height;

	// Get number of floats inside the HDRE
	// Per channel & Per face
	for (int i = 0; i < N_LEVELS; i++)
	{
		int mip_level = i + 1;
		dataSize += w * w * N_FACES * HDREHeader.numChannels;
		w = (int)(width / pow(2.0, mip_level));
	}

	this->data = new float[dataSize];

	fseek(f, HDREHeader.headerSize, SEEK_SET);

	fread(this->data, sizeof(float) * dataSize, 1, f);
	fclose(f);

	// get separated levels

	w = width;
	int mapOffset = 0;
	
	for (int i = 0; i < N_LEVELS; i++)
	{
		int mip_level = i + 1;
		int faceSize = w * w * HDREHeader.numChannels;
		int mapSize = faceSize * N_FACES;
		
		int faceOffset = 0;
		int facePixel = 0;

		this->faces_array[i] = new float[mapSize];

		for (int j = 0; j < N_FACES; j++)
		{
			// allocate memory
			this->pixels[i][j] = new float[faceSize];

			// set data
			for (int k = 0; k < faceSize; k++) {

				float value = this->data[mapOffset + faceOffset + k];

				this->pixels[i][j][k] = value;
				this->faces_array[i][facePixel] = value;
				facePixel++;
			}

			// update face offset
			faceOffset += faceSize;
		}	

		// update level offset
		mapOffset += mapSize;

		// refactored code for writing HDRE 
		// removing Y flipping for webGl at Firefox
		if (this->version < 3.0)
		{
			flipY(this->pixels[i], w, this->numChannels, true);
		}
		else
		{
			if (i != 0) // original is already flipped
				flipY(this->pixels[i], w, this->numChannels, false);
		}

		// reassign width for next level
		w = (int)(width / pow(2.0, mip_level));
	}

	std::cout << std::endl << " + '" << filename << "' (v" << this->version << ") loaded successfully" << std::endl;
	return true;
}

bool HDRE::clean()
{
	if (!data)
		return false;

	try
	{
		delete data;

		for (int i = 0; i < N_LEVELS; i++)
		{
			delete faces_array[i];

			for (int j = 0; j < N_FACES; j++)
			{
				delete pixels[i][j];
			}
		}

		return true;
	}
	catch (const std::exception&)
	{
		std::cout << std::endl << "Error cleaning" << std::endl;
		return false;
	}

	return false;
}