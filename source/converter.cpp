#include "pch.hpp"
#include "converter.hpp"

#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

extern std::string exec_path;

void GRConvert::ConvertImage_ARM(const std::string& Roughness, const std::string& Metallic, const std::string& Ambient, const std::string& Target)
{
	assert(Target != "");

	int wr = 0, hr = 0, wm = 0, hm = 0, wa = 0, ha = 0, c = 0;
	unsigned char* rm = nullptr;
	unsigned char* mm = nullptr;
	unsigned char* am = nullptr;

	bool use_r = Roughness != "";
	bool use_m = Metallic != "";
	bool use_a = Ambient != "";

	if (!use_r && !use_m && !use_a)
		return;

	if (use_r)
		rm = stbi_load((exec_path + Roughness).c_str(), &wr, &hr, &c, 1);
	if (use_m)
		mm = stbi_load((exec_path + Metallic).c_str(), &wm, &hm, &c, 1);
	if (use_a)
		am = stbi_load((exec_path + Ambient).c_str(), &wa, &ha, &c, 1);

	if (!use_r)
	{
		wr = use_m ? wm : wa;
		hr = use_m ? hm : ha;
	}

	if (!use_m)
	{
		wm = use_r ? wr : wa;
		hm = use_r ? hr : ha;
	}

	if (!use_a)
	{
		wa = use_r ? wr : wm;
		ha = use_r ? hr : hm;
	}

	assert(wr == wm && wm == wa && hr == hm && hm == ha);

	if (!(wr == wm && wm == wa && hr == hm && hm == ha) || use_r && !rm || use_m && !mm || use_a && !am)
	{
		if (rm)
			free(rm);
		if (mm)
			free(mm);
		if (am)
			free(am);

		return;
	}

	size_t j = 0;
	size_t is = wr * hr * 3;
	uint8_t* pixels = (uint8_t*)malloc(is);
	for (size_t i = 0; i < is; i += 3, ++j)
	{
		pixels[i] = use_a ? am[j] : uint8_t(255);
		pixels[i + 1] = use_r ? rm[j] : uint8_t(255);
		pixels[i + 2] = use_m ? mm[j] : uint8_t(0);
	}

	stbi_write_jpg((exec_path + Target).c_str(), wr, hr, 3, pixels, wr * 3);
	free(pixels);
}

void GRConvert::ConvertImage_NormalHeight(const std::string& Normal, const std::string& Height, const std::string& Target)
{
	assert(Target != "");

	int wn = 0, hn = 0, wh = 0, hh = 0, c = 0;
	unsigned char* nm = nullptr;
	unsigned char* hm = nullptr;

	bool use_n = Normal != "";
	bool use_h = Height != "";

	if (!use_n && !use_h)
		return;

	if (use_n)
		nm = stbi_load((exec_path + Normal).c_str(), &wn, &hn, &c, 3);
	if (use_h)
		hm = stbi_load((exec_path + Height).c_str(), &wh, &hh, &c, 1);

	if (!use_n)
	{
		wn = wh;
		hn = hh;
	}

	if (!use_h)
	{
		wh = wn;
		hh = hn;
	}

	assert(wn == wh && hn == hh);

	if (!(wn == wh && hn == hh) || use_n && !nm || use_h && !hm)
	{
		if (nm)
			free(nm);
		if (hm)
			free(hm);

		return;
	}

	size_t j = 0;
	size_t k = 0;
	size_t is = wn * hn * 4;
	uint8_t* pixels = (uint8_t*)malloc(is);
	for (size_t i = 0; i < is; i += 4, j += 3, ++k)
	{
		pixels[i] = use_n ? nm[j] : uint8_t(127);
		pixels[i + 1] = use_n ? nm[j + 1] : uint8_t(127);
		pixels[i + 2] = use_n ? nm[j + 2] : uint8_t(255);
		pixels[i + 3] = use_h ? hm[k] : uint8_t(255);
	}

	stbi_write_png((exec_path + Target).c_str(), wn, hn, 4, pixels, wn * 4);
	free(pixels);
}