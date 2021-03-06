/*
* BM3D denoising filter - VapourSynth plugin
* Copyright (C) 2015  mawen1250
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "BM3D.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions of struct BM3D_Para


BM3D_Para::BM3D_Para(bool _wiener, std::string _profile)
    : wiener(_wiener), profile(_profile), sigma({ 10.0, 10.0, 10.0 })
{
    BlockSize = 8;
    BMrange = 16;
    BMstep = 1;

    if (!wiener)
    {
        BlockStep = 4;
        GroupSize = 16;
        lambda = 2.7;
    }
    else
    {
        BlockStep = 3;
        GroupSize = 32;
    }

    if (profile == "fast")
    {
        BMrange = 9;
        GroupSize = 8;

        if (!wiener)
        {
            BlockStep = 8;
        }
        else
        {
            BlockStep = 7;
        }
    }
    else if (profile == "lc")
    {
        BMrange = 9;

        if (!wiener)
        {
            BlockStep = 6;
        }
        else
        {
            BlockStep = 5;
            GroupSize = 16;
        }
    }
    else if (profile == "high")
    {
        if (!wiener)
        {
            BlockStep = 3;
        }
        else
        {
            BlockStep = 2;
        }
    }
    else if (profile == "vn")
    {
        if (!wiener)
        {
            BlockStep = 4;
            GroupSize = 32;
            lambda = 2.8;
        }
        else
        {
            BlockSize = 11;
            BlockStep = 6;
        }
    }

    thMSE_Default();
}


void BM3D_Para::thMSE_Default()
{
    if (!wiener)
    {
        thMSE = 400 + sigma[0] * 80;

        if (profile == "vn")
        {
            thMSE = 1000 + sigma[0] * 150;
        }
    }
    else
    {
        thMSE = 200 + sigma[0] * 10;

        if (profile == "vn")
        {
            thMSE = 400 + sigma[0] * 40;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions of struct BM3D_FilterData


BM3D_FilterData::BM3D_FilterData(bool wiener, double sigma, PCType GroupSize, PCType BlockSize, double lambda)
    : fp(GroupSize), bp(GroupSize), finalAMP(GroupSize), thrTable(wiener ? 0 : GroupSize),
    wienerSigmaSqr(wiener ? GroupSize : 0)
{
    const unsigned int flags = FFTW_PATIENT;
    const fftw::r2r_kind fkind = FFTW_REDFT10;
    const fftw::r2r_kind bkind = FFTW_REDFT01;

    FLType *temp = nullptr;

    for (PCType i = 1; i <= GroupSize; ++i)
    {
        AlignedMalloc(temp, i * BlockSize * BlockSize);
        fp[i - 1].r2r_3d(i, BlockSize, BlockSize, temp, temp, fkind, fkind, fkind, flags);
        bp[i - 1].r2r_3d(i, BlockSize, BlockSize, temp, temp, bkind, bkind, bkind, flags);
        AlignedFree(temp);

        finalAMP[i - 1] = 2 * i * 2 * BlockSize * 2 * BlockSize;
        double forwardAMP = sqrt(finalAMP[i - 1]);

        if (wiener)
        {
            wienerSigmaSqr[i - 1] = static_cast<FLType>(sigma * forwardAMP * sigma * forwardAMP);
        }
        else
        {
            double thrBase = sigma * lambda * forwardAMP;
            std::vector<double> thr(4);
            
            thr[0] = thrBase;
            thr[1] = thrBase * sqrt(double(2));
            thr[2] = thrBase * double(2);
            thr[3] = thrBase * sqrt(double(8));

            FLType *thrp = nullptr;
            AlignedMalloc(thrp, i * BlockSize * BlockSize);
            thrTable[i - 1].reset(thrp, [](FLType *memory)
            {
                AlignedFree(memory);
            });

            for (PCType z = 0; z < i; ++z)
            {
                for (PCType y = 0; y < BlockSize; ++y)
                {
                    for (PCType x = 0; x < BlockSize; ++x, ++thrp)
                    {
                        int flag = 0;

                        if (x == 0)
                        {
                            ++flag;
                        }
                        if (y == 0)
                        {
                            ++flag;
                        }
                        if (z == 0)
                        {
                            ++flag;
                        }

                        *thrp = static_cast<FLType>(thr[flag]);
                    }
                }
            }
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
