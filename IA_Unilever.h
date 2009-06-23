#pragma once

#ifndef __IA_UNILEVER_H__
#define __IA_UNILEVER_H__

#include "CImg.h"

void  bdcalcWrinkle(cimg_library::CImg<float> &image,
			cimg_library::CImg<float> &procImg,
			double& wrinkleScore,
			double threshold,
			unsigned char *alpha);
void bdcalcMHP(cimg_library::CImg<float> &image,
		cimg_library::CImg<float>& rawim,
		double& mhpScore,
		double threshold,
		unsigned char *alpha);

void bdPinkishWhite(cimg_library::CImg<float> &image,
			float perBLT,
			float delta_L,
			float delta_a,
			float delta_b,
			int bltSize);

#endif
