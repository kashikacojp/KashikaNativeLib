#define _CRT_SECURE_NO_WARNINGS
#include "HasAlphaChannel.h"

#define STB_IMAGE_IMPLEMENTATION 1
#define STB_IMAGE_RESIZE_IMPLEMENTATION 1
#define STB_IMAGE_WRITE_IMPLEMENTATION 1

#include <stb/stb_image.h>
#include <stb/stb_image_resize.h>
#include <stb/stb_image_write.h>

namespace kil
{
	bool HasAlphaChannel(const std::string& src_path)
    {
        int width = 0;
        int height = 0;
        int channels = 1;
        stbi_uc* buffer = stbi_load(src_path.c_str(), &width, &height, &channels, 0);
        stbi_image_free(buffer);
        if (buffer == NULL)
        {
            return false;
        }
        if(channels <= 3)
        {
            return false;
        }
        return true;
    }
}
