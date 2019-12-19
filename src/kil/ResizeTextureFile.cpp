#include "ResizeTextureFile.h"
#include "CopyTextureFile.h"

#include <algorithm>
#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

//#define STB_IMAGE_IMPLEMENTATION 1
//#define STB_IMAGE_RESIZE_IMPLEMENTATION 1
//#define STB_IMAGE_WRITE_IMPLEMENTATION 1

#include <stb/stb_image.h>
#include <stb/stb_image_resize.h>
#include <stb/stb_image_write.h>

namespace kil
{
    static std::string GetExt(const std::string& filepath)
    {
        if (filepath.find_last_of(".") != std::string::npos)
            return filepath.substr(filepath.find_last_of("."));
        return "";
    }

    static bool IsPowerOfTwo(int x)
    {
        if (x <= 0)
        {
            return false;
        }
        else
        {
            return (x & (x - 1)) == 0;
        }
    }

    static bool IsNeededToResize(int width, int height, int maximum_size, bool is_poweroftwo = false, bool is_squared = false)
    {
        bool bRet = false;
        if (maximum_size <= width || maximum_size <= height)
        {
            bRet = true;
        }
        if (is_poweroftwo)
        {
            if (!IsPowerOfTwo(width) || !IsPowerOfTwo(height))
            {
                bRet = true;
            }
        }
        if (is_squared)
        {
            if (width != height)
            {
                bRet = true;
            }
        }
        return bRet;
    }

    static int GetPowerOfTwo(int x)
    {
        x--;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        x++;
        return x;
    }

    bool ResizeTextureFile_STB(const std::string& orgPath, const std::string& dstPath, int maximum_size, int resize_size, bool is_poweroftwo, bool is_squared, float quality)
    {
        int width = 0;
        int height = 0;
        int channels = 1;
        stbi_uc* buffer = stbi_load(orgPath.c_str(), &width, &height, &channels, 0);
        if (buffer == NULL)
        {
            return false;
        }

        bool resizeSize = false;
        bool resizePowerOfTwo = false;
        bool resizeSquared = false;
        if(maximum_size != 0)
        {
            if (maximum_size <= width || maximum_size <= height)
            {
                resizeSize = true;
            }
        }
        if (is_poweroftwo)
        {
            if (!IsPowerOfTwo(width) || !IsPowerOfTwo(height))
            {
                resizePowerOfTwo = true;
            }
        }
        if (is_squared)
        {
            if (width != height)
            {
                resizeSquared = true;
            }
        }

        bool is_needed_to_resize = resizeSize | resizePowerOfTwo | resizeSquared;

        if (!is_needed_to_resize)
        {
            if (buffer)
            {
                stbi_image_free(buffer);
            }
            return CopyTextureFile(orgPath, dstPath, quality);
        }

        int nw = width;
        int nh = height;
        stbi_uc* nbuffer = NULL;
        if (resizeSize && !is_poweroftwo && !is_squared)
        {
            float factor = resize_size / ((float)std::max<int>(width, height));
            nw = (int)floor(width * factor);
            nh = (int)floor(height * factor);
            nbuffer = (unsigned char*)malloc(sizeof(unsigned char) * nw * nh * channels);
        }
        else
        {
            if(resizeSize)
            {
                float factor = resize_size / ((float)std::max<int>(width, height));
                nw = (int)floor(width * factor);
                nh = (int)floor(height * factor);
            }
            if(is_poweroftwo)
            {
                nw = GetPowerOfTwo(nw);
                nh = GetPowerOfTwo(nh);
            }
            if(is_squared)
            {
                int max_width = std::max<int>(nw, nh);
                nw = max_width;
                nh = max_width;
            }
            nbuffer = (unsigned char*)malloc(sizeof(unsigned char) * nw * nh * channels);
        }

        if (!stbir_resize_uint8(buffer, width, height, 0, nbuffer, nw, nh, 0, channels))
        {
            if (nbuffer)
            {
                free(nbuffer);
            }
            if (buffer)
            {
                stbi_image_free(buffer);
            }
            return false;
        }

        std::string ext = GetExt(dstPath);
        if (ext == ".jpg" || ext == ".jpeg")
        {
            int q = std::max<int>(0, std::min<int>(int(quality * 100), 100));
            stbi_write_jpg(dstPath.c_str(), nw, nh, channels, nbuffer, q); //quality = 90
        }
        else if (ext == ".png")
        {
            stbi_write_png(dstPath.c_str(), nw, nh, channels, nbuffer, 0);
        }
        else if (ext == ".bmp")
        {
            stbi_write_bmp(dstPath.c_str(), nw, nh, channels, nbuffer);
        }
        else if (ext == ".gif")
        {
            if (nbuffer)
            {
                free(nbuffer);
            }

            if (buffer)
            {
                stbi_image_free(buffer);
            }
            return false;
        }

        if (nbuffer)
        {
            free(nbuffer);
        }

        if (buffer)
        {
            stbi_image_free(buffer);
        }
        return true;
    }

    static std::string GetPngTempPath()
    {
#ifdef _WIN32
        char bufDir[_MAX_PATH] = {};
        DWORD nsz = ::GetTempPathA((DWORD)_MAX_PATH, bufDir);
        bufDir[nsz] = 0;
        return std::string(bufDir) + std::string("tmp.png");
#else // Linux and macOS
        const char* tmpdir = getenv("TMPDIR");
        return std::string(tmpdir) + std::string("tmp.png");
#endif
    }

    static void RemoveFile(const std::string& path)
    {
#ifdef _WIN32
        ::DeleteFileA(path.c_str());
#else // Linux and macOS
        remove(path.c_str());
#endif
    }

    bool ResizeTextureFile(const std::string& orgPath, const std::string& dstPath, int maximum_size, int resize_size, bool is_poweroftwo, bool is_squared, float quality)
    {
        std::string ext = GetExt(orgPath);
        if (ext == ".tiff" || ext == ".tif")
        {
            std::string tmpPath = GetPngTempPath();
            bool bRet = true;
            bRet = CopyTextureFile(orgPath, tmpPath, quality);
            if (!bRet)
                return bRet;
            bRet = ResizeTextureFile_STB(tmpPath, dstPath, maximum_size, resize_size, is_poweroftwo, is_squared, quality);
            RemoveFile(tmpPath);
            return bRet;
        }
        else
        {
            return ResizeTextureFile_STB(orgPath, dstPath, maximum_size, resize_size, is_poweroftwo, is_squared, quality);
        }
    }
} // namespace kil
