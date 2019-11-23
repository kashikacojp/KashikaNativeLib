#define _CRT_SECURE_NO_WARNINGS
#include "CopyTextureFile_STB.h"

#define STB_IMAGE_IMPLEMENTATION 1
#define STB_IMAGE_RESIZE_IMPLEMENTATION 1
#define STB_IMAGE_WRITE_IMPLEMENTATION 1

#include <stb/stb_image.h>
#include <stb/stb_image_resize.h>
#include <stb/stb_image_write.h>

#include <algorithm>

namespace kil
{
    static std::string GetExt(const std::string& filepath)
    {
        if (filepath.find_last_of(".") != std::string::npos)
            return filepath.substr(filepath.find_last_of("."));
        return "";
    }

    bool CopyTextureFile_STB(const std::string& orgPath, const std::string& dstPath, float quality)
    {
        int width = 0;
        int height = 0;
        int channels = 1;
        stbi_uc* buffer = stbi_load(orgPath.c_str(), &width, &height, &channels, 0);
        if (buffer == NULL)
        {
            return false;
        }

        std::string ext = GetExt(dstPath);
        if (ext == ".jpg" || ext == ".jpeg")
        {
            int q = std::max<int>(0, std::min<int>(int(quality * 100), 100));
            stbi_write_jpg(dstPath.c_str(), width, height, channels, buffer, q);
            stbi_image_free(buffer);
        }
        else if (ext == ".png")
        {
            stbi_write_png(dstPath.c_str(), width, height, channels, buffer, 0);
            stbi_image_free(buffer);
        }
        else if (ext == ".bmp")
        {
            stbi_write_bmp(dstPath.c_str(), width, height, channels, buffer);
            stbi_image_free(buffer);
        }
        else if (ext == ".gif")
        {
            stbi_image_free(buffer);
            return false;
        }
        return true;
    }
} // namespace kil
