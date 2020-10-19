#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "GLTF2GLB.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>

#include <stdio.h>

#include <picojson/picojson.h>

namespace kml
{
    typedef std::uint32_t uint32;
    typedef std::uint8_t uchar;

    using namespace picojson;

    namespace
    {
        struct GLBHeader
        {
            uint32 magic;
            uint32 version;
            uint32 length;
        };

        struct GLBChunk
        {
            uint32 chunkLength;
            uint32 chunkType;
        };

        struct BufferProperty
        {
            std::string path;
            uint32 length;
        };

        static int Get4BytesAlign(int x)
        {
            if ((x % 4) == 0)
            {
                return x;
            }
            else
            {
                return ((x / 4) + 1) * 4;
            }
        }

        struct ImageData
        {
            std::string path;
            std::vector<uchar> buffer;
        };
    } // namespace

    static std::string GetDirectoryPath(const std::string& path)
    {
#ifdef _WIN32
        char szDrive[_MAX_DRIVE];
        char szDir[_MAX_DIR];
        _splitpath(path.c_str(), szDrive, szDir, NULL, NULL);
        std::string strRet1;
        strRet1 += szDrive;
        strRet1 += szDir;
        return strRet1;
#else
        return path.substr(0, path.find_last_of('/')) + "/";
#endif
    }

    static std::string GetExt(const std::string& filepath)
    {
        if (filepath.find_last_of(".") != std::string::npos)
            return filepath.substr(filepath.find_last_of("."));
        return "";
    }

    static std::string ReplaceExt(const std::string& filepath, const std::string& ext)
    {
        if (filepath.find_last_of(".") != std::string::npos)
        {
            return filepath.substr(0, filepath.find_last_of(".")) + ext;
        }
        return filepath + ext;
    }

    static bool SplitBuffer(picojson::value& root_val, std::vector<uchar>& bin_buffer, std::vector<ImageData>& images)
    {

        return true;
    }

    bool GLB2GLTF(const std::string& src, const std::string& dst)
    {
        FILE* fp = fopen(src.c_str(), "rb");
        if (!fp)
        {
            return false;
        }

        GLBHeader header = {};
        //header
        fread(&header, sizeof(GLBHeader), 1, fp);
        if (header.magic != (uint32)0x46546C67) //gltf
        {
            if (fp != NULL)
            {
                fclose(fp);
            }
            return false;
        }

        GLBChunk chunk0 = {};
        fread(&chunk0, sizeof(GLBChunk), 1, fp);
        if (chunk0.chunkType != (uint32)0x4E4F534A) //ASCII
        {
            if (fp != NULL)
            {
                fclose(fp);
            }
            return false;
        }
        std::vector<uchar> json_buffer(chunk0.chunkLength + 1);
        json_buffer[chunk0.chunkLength] = 0;
        fread(&json_buffer[0], sizeof(uchar) * chunk0.chunkLength, 1, fp);

        GLBChunk chunk1 = {};
        fread(&chunk1, sizeof(GLBChunk), 1, fp);
        if (chunk1.chunkType != (uint32)0x004E4942) //BIN
        {
            if (fp != NULL)
            {
                fclose(fp);
            }
            return false;
        }

        if (fp != NULL)
        {
            fclose(fp);
        }

        std::vector<uchar> bin_buffer(chunk1.chunkLength);
        fread(&bin_buffer[0], sizeof(uchar) * chunk1.chunkLength, 1, fp);

        picojson::value root_val;
        std::stringstream ss((const char*)&json_buffer[0]);
        std::string err = picojson::parse(root_val, ss);
        if (!err.empty())
        {
            return false;
        }

        std::vector<ImageData> images;
        bool bSplit = SplitBuffer(root_val, bin_buffer, images);
        if(!bSplit)
        {
            return false;
        }

        std::string dir_path = GetDirectoryPath(dst);
        for(size_t i = 0; i < images.size(); i++)
        {
            std::string path = dir_path + "/" + images[i].path;
            FILE* fp = fopen(path.c_str(), "wb");
            if(fp != NULL)
            {
                fwrite(&(images[i].buffer[0]), images[i].buffer.size() * sizeof(uchar), 1, fp);
                fclose(fp);
            }
            else
            {
                return false;
            }
        }

        {
            std::string bin_path = ReplaceExt(dst, ".bin");
            FILE* fp = fopen(bin_path.c_str(), "wb");
            if(fp != NULL)
            {
                fwrite(&(bin_buffer[0]), bin_buffer.size() * sizeof(uchar), 1, fp);
                fclose(fp);
            }
        }

        {
            std::ofstream ofs(dst.c_str());
            if(!ofs)
            {
                ofs << root_val;
                std::flush(ofs);
            }
        }

        return true;
    }
} // namespace kml
