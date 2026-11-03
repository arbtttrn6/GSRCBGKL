#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_FAILURE_STRINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stb_image.h"


#define TGA_SLICE_SIZE 256


bool WriteTgaRle24(const char* pszFileName, const unsigned char* pbyRgbData, int nImgWidth,
				   int nImgHeight, int nSliceX, int nSliceY, int nSliceW, int nSliceH)
{
	FILE* pFile = NULL;
	unsigned char abyHeader[18];
	int nY = 0;
	int nX = 0;

	if (pszFileName  == NULL || pbyRgbData == NULL
		|| nImgWidth <= 0    || nImgHeight <= 0
		|| nSliceX   <  0    || nSliceY    <  0
		|| nSliceW   <= 0    || nSliceH    <= 0)
	{
		return false;
	}

	pFile = fopen(pszFileName, "wb");
	if (pFile == NULL)
	{
		return false;
	}

	memset(abyHeader, 0, sizeof(abyHeader));
	abyHeader[2] = 10;   /* RLE Compressed True-Color */
	abyHeader[12] = (unsigned char)(nSliceW & 0xFF);
	abyHeader[13] = (unsigned char)((nSliceW >> 8) & 0xFF);
	abyHeader[14] = (unsigned char)(nSliceH & 0xFF);
	abyHeader[15] = (unsigned char)((nSliceH >> 8) & 0xFF);
	abyHeader[16] = 24;   /* 24 bits per pixel */
	abyHeader[17] = 0x00; /* Origin: Bottom-Left */

	if (fwrite(abyHeader, 1, sizeof(abyHeader), pFile) != sizeof(abyHeader))
	{
		fclose(pFile);
		return false;
	}

	for (nY = nSliceH - 1; nY >= 0; --nY)
	{
		int nGlobalY = nSliceY + nY;
		unsigned char* abyRowPixels = NULL;
		int nPixelIdx = 0;

		abyRowPixels = (unsigned char*)malloc(nSliceW * 3);
		if (abyRowPixels == NULL)
		{
			fclose(pFile);

			return false;
		}

		for (nX = 0; nX < nSliceW; ++nX)
		{
			int nGlobalX = nSliceX + nX;
			if (nGlobalX < nImgWidth && nGlobalY < nImgHeight)
			{
				int nSrcIdx = (nGlobalY * nImgWidth + nGlobalX) * 3;

				abyRowPixels[nPixelIdx * 3 + 0] = pbyRgbData[nSrcIdx + 2]; /* B */
				abyRowPixels[nPixelIdx * 3 + 1] = pbyRgbData[nSrcIdx + 1]; /* G */
				abyRowPixels[nPixelIdx * 3 + 2] = pbyRgbData[nSrcIdx + 0]; /* R */
			}
			else
			{
				abyRowPixels[nPixelIdx * 3 + 0] = 0;
				abyRowPixels[nPixelIdx * 3 + 1] = 0;
				abyRowPixels[nPixelIdx * 3 + 2] = 0;
			}
			++nPixelIdx;
		}
		{
			int nPos = 0;
			while (nPos < nSliceW)
			{
				int nRunLen = 1;
				bool bIsRaw = true;
				if (nPos + 1 < nSliceW)
				{
					if (abyRowPixels[nPos * 3 + 0] == abyRowPixels[(nPos + 1) * 3 + 0] &&
						abyRowPixels[nPos * 3 + 1] == abyRowPixels[(nPos + 1) * 3 + 1] &&
						abyRowPixels[nPos * 3 + 2] == abyRowPixels[(nPos + 1) * 3 + 2])
					{
						bIsRaw = false;
						while (nPos + nRunLen < nSliceW && nRunLen < 128)
						{
							if (abyRowPixels[nPos * 3 + 0] == abyRowPixels[(nPos + nRunLen) * 3 + 0] &&
								abyRowPixels[nPos * 3 + 1] == abyRowPixels[(nPos + nRunLen) * 3 + 1] &&
								abyRowPixels[nPos * 3 + 2] == abyRowPixels[(nPos + nRunLen) * 3 + 2])
							{
								++nRunLen;
							}
							else
							{
								break;
							}
						}
					}
					else
					{
						while (nPos + nRunLen < nSliceW && nRunLen < 128)
						{
							if (nPos + nRunLen + 1 < nSliceW &&
								abyRowPixels[(nPos + nRunLen) * 3 + 0] == abyRowPixels[(nPos + nRunLen + 1) * 3 + 0] &&
								abyRowPixels[(nPos + nRunLen) * 3 + 1] == abyRowPixels[(nPos + nRunLen + 1) * 3 + 1] &&
								abyRowPixels[(nPos + nRunLen) * 3 + 2] == abyRowPixels[(nPos + nRunLen + 1) * 3 + 2])
							{
								break;
							}
							++nRunLen;
						}
					}
				}
				if (!bIsRaw)
				{
					unsigned char byPacketHeader = (unsigned char)(128 + (nRunLen - 1));
					fputc(byPacketHeader, pFile);
					fwrite(&abyRowPixels[nPos * 3], 1, 3, pFile);
					nPos += nRunLen;
				}
				else
				{
					unsigned char byPacketHeader = (unsigned char)(nRunLen - 1);
					fputc(byPacketHeader, pFile);
					fwrite(&abyRowPixels[nPos * 3], 1, nRunLen * 3, pFile);
					nPos += nRunLen;
				}
			}
		}
		free(abyRowPixels);
	}
	fclose(pFile);

	return true;
}


bool ResizeImageBilinear(const unsigned char* pbySrcData, int nSrcW, int nSrcH, unsigned char* pbyDstData, int nDstW, int nDstH)
{
	int nY = 0;
	int nX = 0;
	int nC = 0;

	if (pbySrcData == NULL || pbyDstData == NULL || nSrcW <= 0 || nSrcH <= 0 || nDstW <= 0 || nDstH <= 0)
	{
		return false;
	}

	for (nY = 0; nY < nDstH; ++nY)
	{
		float fSrcY = ((float)nY + 0.5f) * ((float)nSrcH / (float)nDstH) - 0.5f;
		int nY0 = (int)fSrcY;
		int nY1 = nY0 + 1;
		float fWeightY = fSrcY - (float)nY0;

		if (nY0 < 0)
		{
			nY0 = 0;
			nY1 = 0;
			fWeightY = 0.0f;
		}

		if (nY1 >= nSrcH)
		{
			nY1 = nSrcH - 1;
			nY0 = nY1;
			fWeightY = 0.0f;
		}

		for (nX = 0; nX < nDstW; ++nX)
		{
			float fSrcX = ((float)nX + 0.5f) * ((float)nSrcW / (float)nDstW) - 0.5f;
			int nX0 = (int)fSrcX;
			int nX1 = nX0 + 1;
			float fWeightX = fSrcX - (float)nX0;

			if (nX0 < 0)
			{
				nX0 = 0;
				nX1 = 0;
				fWeightX = 0.0f;
			}

			if (nX1 >= nSrcW)
			{
				nX1 = nSrcW - 1;
				nX0 = nX1;
				fWeightX = 0.0f;
			}

			for (nC = 0; nC < 3; ++nC)
			{
				float fVal00 = (float)pbySrcData[(nY0 * nSrcW + nX0) * 3 + nC];
				float fVal10 = (float)pbySrcData[(nY0 * nSrcW + nX1) * 3 + nC];
				float fVal01 = (float)pbySrcData[(nY1 * nSrcW + nX0) * 3 + nC];
				float fVal11 = (float)pbySrcData[(nY1 * nSrcW + nX1) * 3 + nC];
				float fInterp = fVal00 * (1.0f - fWeightX) * (1.0f - fWeightY) +
								fVal10 * fWeightX * (1.0f - fWeightY) +
								fVal01 * (1.0f - fWeightX) * fWeightY +
								fVal11 * fWeightX * fWeightY;
				pbyDstData[(nY * nDstW + nX) * 3 + nC] = (unsigned char)(fInterp + 0.5f);
			}
		}
	}

	return true;
}


bool GenerateLayoutFiles(int nResWidth, int nResHeight, const char* pszLayoutName, const char* pszPrefix, const unsigned char* pbyImgData, int nImgWidth, int nImgHeight)
{
	FILE* pLayoutFile = NULL;
	unsigned char* pbyResizedBuffer = NULL;
	int nCols = 0;
	int nRows = 0;
	int nR = 0;
	int nC = 0;
	int nLineCounter = 3;
	bool bSuccess = true;

	if (pszLayoutName == NULL || pszPrefix  == NULL || pbyImgData == NULL
		|| nResWidth  <= 0    || nResHeight <= 0    || nImgWidth  <= 0
		|| nImgHeight <= 0)
	{
		return false;
	}

	nCols = (nResWidth + TGA_SLICE_SIZE - 1) / TGA_SLICE_SIZE;
	nRows = (nResHeight + TGA_SLICE_SIZE - 1) / TGA_SLICE_SIZE;
	pbyResizedBuffer = (unsigned char*)malloc(nResWidth * nResHeight * 3);

	if (pbyResizedBuffer == NULL)
	{
		return false;
	}

	if (!ResizeImageBilinear(pbyImgData, nImgWidth, nImgHeight, pbyResizedBuffer, nResWidth, nResHeight))
	{
		free(pbyResizedBuffer);

		return false;
	}

	pLayoutFile = fopen(pszLayoutName, "w");
	if (pLayoutFile == NULL)
	{
		free(pbyResizedBuffer);

		return false;
	}

	fprintf(pLayoutFile, "resolution %d %d\n\n", nResWidth, nResHeight);

	for (nR = 0; nR < nRows; ++nR)
	{
		int nSliceY = nR * TGA_SLICE_SIZE;
		int nCurrentSliceH = TGA_SLICE_SIZE;

		if (nSliceY + nCurrentSliceH > nResHeight)
		{
			nCurrentSliceH = nResHeight - nSliceY;
		}

		for (nC = 0; nC < nCols; ++nC)
		{
			int nSliceX = nC * TGA_SLICE_SIZE;
			int nCurrentSliceW = TGA_SLICE_SIZE;

			if (nSliceX + nCurrentSliceW > nResWidth)
			{
				nCurrentSliceW = nResWidth - nSliceX;
			}

			char szTgaName[256];
			char szTgaPath[512];
			char chLetter = (char)('a' + nC);

			sprintf(szTgaName, "%s_%d_%c_loading.tga", pszPrefix, nR + 1, chLetter);
			sprintf(szTgaPath, "background/%s", szTgaName);

			if (!WriteTgaRle24(szTgaPath, pbyResizedBuffer, nResWidth, nResHeight, nSliceX, nSliceY, nCurrentSliceW, nCurrentSliceH))
			{
				printf("Error write tga: %s\n", szTgaPath);
				bSuccess = false;

				break;
			}
			fprintf(pLayoutFile, "resource/background/%s\t", szTgaName);
			fprintf(pLayoutFile, "fit\t%d\t%d\n", nSliceX, nSliceY);

			nLineCounter += 2;
		}
		if (!bSuccess)
		{
			break;
		}
	}
	fclose(pLayoutFile);
	free(pbyResizedBuffer);

	return bSuccess;
}


const char* get_program_version(void)
{
	static char szFullVersion[17] = "1.0.0.00000000";
	const char* pszD;
	const char* pszM;
	int nMonth;
	int nI;

	if (szFullVersion[6] == '0' && szFullVersion[7] == '0')
	{
		pszD = __DATE__;

		szFullVersion[6] = (pszD[4] == ' ') ? '0' : pszD[4];
		szFullVersion[7] = pszD[5];

		pszM   = "JanFebMarAprMayJunJulAugSepOctNovDec";
		nMonth = 1;
		for (nI = 0; nI < 12; ++nI)
		{
			if (pszD[0] == pszM[nI * 3] && pszD[1] == pszM[nI * 3 + 1])
			{
				nMonth = nI + 1;
				break;
			}
		}
		szFullVersion[8]  = (char)('0' + nMonth / 10);
		szFullVersion[9]  = (char)('0' + nMonth % 10);
		szFullVersion[10] = pszD[7];
		szFullVersion[11] = pszD[8];
		szFullVersion[12] = pszD[9];
		szFullVersion[13] = pszD[10];
	}

	return szFullVersion;
}

void PrintInfo(const char* pszProgramName)
{
	const char* pszProgramVersion = get_program_version();
	fputs("author: Artur Ajvazjan (arbtttrn6)\n", stdout);
	fputs("version: ", stdout);
	fputs(pszProgramVersion, stdout);
	fputs("\nusage:\n", stdout);
	if (pszProgramName != NULL)
	{
		fputs(pszProgramName, stdout);
	}
	fputs(" <path_to_image.png>\n", stdout);
}


int main(int argc, char* argv[])
{
	int nImgWidth = 0;
	int nImgHeight = 0;
	int nChannels = 0;
	unsigned char* pbyImgData = NULL;

	if (argc < 2)
	{
		PrintInfo(argv[0]);

		return 1;
	}

	pbyImgData = stbi_load(argv[1], &nImgWidth, &nImgHeight, &nChannels, 3);
	if (pbyImgData == NULL)
	{
		fputs("Error load or decode PNG.\n", stdout);

		return 1;
	}
	if (nImgWidth <= 0 || nImgHeight <= 0)
	{
		fputs("Invalid image size.\n", stdout);
		stbi_image_free(pbyImgData);

		return 1;
	}

#if defined(_WIN32)
	system("mkdir background 2>nul");
#else
	system("mkdir -p background 2>/dev/null");
#endif

	fputs("Generation of (800x600, TGA 1024x768)... ", stdout);
	if (GenerateLayoutFiles(800, 600, "BackgroundLayout.txt", "800", pbyImgData, nImgWidth, nImgHeight))
	{
		fputs("Success.\n", stdout);
	}
	else
	{
		fputs("Error.\n", stdout);
	}

	fputs("Generation of HD (3840x1600, TGA 3840x1792)... ", stdout);
	if (GenerateLayoutFiles(3840, 1600, "HD_BackgroundLayout.txt", "21_9", pbyImgData, nImgWidth, nImgHeight))
	{
		fputs("Success.\n", stdout);
	}
	else
	{
		fputs("Error.\n", stdout);
	}

	stbi_image_free(pbyImgData);

	return 0;
}
