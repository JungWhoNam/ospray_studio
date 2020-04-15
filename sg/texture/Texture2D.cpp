// ======================================================================== //
// Copyright 2009-2019 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "stb_image.h"

#ifdef USE_OPENIMAGEIO
#include <OpenImageIO/imageio.h>
OIIO_NAMESPACE_USING
#endif

#include "Texture2D.h"

#include "ospcommon/memory/malloc.h"

namespace ospray::sg {

  // static helper functions //////////////////////////////////////////////////

  OSPTextureFormat osprayTextureFormat(int depth,
                                       int channels,
                                       bool preferLinear = false)
  {
    if (depth == 1) {
      if (channels == 1)
        return preferLinear ? OSP_TEXTURE_R8 : OSP_TEXTURE_L8;
      if (channels == 2)
        return preferLinear ? OSP_TEXTURE_RA8 : OSP_TEXTURE_LA8;
      if (channels == 3)
        return preferLinear ? OSP_TEXTURE_RGB8 : OSP_TEXTURE_SRGB;
      if (channels == 4)
        return preferLinear ? OSP_TEXTURE_RGBA8 : OSP_TEXTURE_SRGBA;
    } else if (depth == 4) {
      if (channels == 1)
        return OSP_TEXTURE_R32F;
      if (channels == 3)
        return OSP_TEXTURE_RGB32F;
      if (channels == 4)
        return OSP_TEXTURE_RGBA32F;
    }

    std::cerr << "#osp:sg: INVALID FORMAT " << depth << ":" << channels
              << std::endl;
    return OSP_TEXTURE_FORMAT_INVALID;
  }

  // Texture2D definitions ////////////////////////////////////////////////////

  Texture2D::Texture2D() : Texture("texture2d") {
  }

  void Texture2D::load(const FileName &fileNameAbs,
                                             const bool preferLinear,
                                             const bool nearestFilter)
  {
    FileName fileName        = fileNameAbs;
    std::string fileNameBase = fileNameAbs;

    // if (textureCache.find(fileName.str()) != textureCache.end())
      // return textureCache[fileName.str()];

#ifdef USE_OPENIMAGEIO
    auto in = ImageInput::open(fileName.str().c_str());
    if (!in) {
      std::cerr << "#osp:sg: failed to load texture '" + fileName.str() + "'"
                << std::endl;
      // reset();
    } else {
      const ImageSpec &spec = in->spec();

      size.x         = spec.width;
      size.y         = spec.height;
      channels       = spec.nchannels;
      const bool hdr      = spec.format.size() > 1;
      depth          = hdr ? 4 : 1;
      preferLinear   = preferLinear;
      nearestFilter  = nearestFilter;
      const size_t stride = size.x * channels * depth;
      data = memory::alignedMalloc(sizeof(unsigned char) * size.y * stride);

      in->read_image(hdr ? TypeDesc::FLOAT : TypeDesc::UINT8, data);
      in->close();
#if OIIO_VERSION < 10903 && OIIO_VERSION > 10603
      ImageInput::destroy(in);
#endif

      // flip image (because OSPRay's textures have the origin at the lower
      // left corner)
      unsigned char *data = (unsigned char *)data;
      for (int y = 0; y < size.y / 2; y++) {
        unsigned char *src  = &data[y * stride];
        unsigned char *dest = &data[(size.y - 1 - y) * stride];
        for (size_t x = 0; x < stride; x++)
          std::swap(src[x], dest[x]);
      }
    }
#else
    const std::string ext = fileName.ext();

    if (ext == "ppm") {
      try {
        int rc, peekchar;

        // open file
        FILE *file = fopen(fileName.str().c_str(), "rb");
        if (!file) {
          if (fileNameBase.size() > 0) {
            if (fileNameBase.substr(0, 1) == "/") {  // Absolute path.
              fileName = fileNameBase;
            }
          }
        }
        const int LINESZ = 10000;
        char lineBuf[LINESZ + 1];

        if (!file) {
          throw std::runtime_error("#ospray_sg: could not open texture file '" +
                                   fileName.str() + "'.");
        }

        // read format specifier:
        int format = 0;
        rc         = fscanf(file, "P%i\n", &format);
        if (format != 6) {
          throw std::runtime_error(
              "#ospray_sg: can currently load only binary P6 subformats for "
              "PPM texture files. "
              "Please report this bug at ospray.github.io.");
        }

        // skip all comment lines
        peekchar = getc(file);
        while (peekchar == '#') {
          auto tmp = fgets(lineBuf, LINESZ, file);
          (void)tmp;
          peekchar = getc(file);
        }
        ungetc(peekchar, file);

        // read width and height from first non-comment line
        int width = -1, height = -1;
        rc = fscanf(file, "%i %i\n", &width, &height);
        if (rc != 2) {
          throw std::runtime_error(
              "#ospray_sg: could not parse width and height in P6 PPM file "
              "'" +
              fileName.str() +
              "'. "
              "Please report this bug at ospray.github.io, and include named "
              "file to reproduce the error.");
        }

        // skip all comment lines
        peekchar = getc(file);
        while (peekchar == '#') {
          auto tmp = fgets(lineBuf, LINESZ, file);
          (void)tmp;
          peekchar = getc(file);
        }
        ungetc(peekchar, file);

        // read maxval
        int maxVal = -1;
        rc         = fscanf(file, "%i", &maxVal);
        peekchar   = getc(file);

        if (rc != 1) {
          throw std::runtime_error(
              "#ospray_sg: could not parse maxval in P6 PPM file '" +
              fileName.str() +
              "'. "
              "Please report this bug at ospray.github.io, and include named "
              "file to reproduce the error.");
        }

        if (maxVal != 255) {
          throw std::runtime_error(
              "#ospray_sg: could not parse P6 PPM file '" + fileName.str() +
              "': currently supporting only maxVal=255 formats."
              "Please report this bug at ospray.github.io, and include named "
              "file to reproduce the error.");
        }

        int channels = 3;
        int depth    = 1;

        unsigned int dataSize = width * height * channels * depth;

        std::vector<vec3uc> data(sizeof(unsigned char) * dataSize);

        rc = fread(data.data(), dataSize, 1, file);

        // flip in y, because OSPRay's textures have the origin at the lower
        // left corner

         unsigned char *texels = (unsigned char *)data.data();
          for (int y = 0; y < height/2; y++)
            for (int x = 0; x < width*3; x++) {
              unsigned int a = (y * width * 3) + x;
              unsigned int b = ((height - 1 - y) * width * 3) + x;
              if (a >= dataSize || b >= dataSize) {
                throw std::runtime_error("#osp:minisg: could not parse P6 PPM file '" + fileName.str() + "': buffer overflow.");
              }
              std::swap(texels[a], texels[b]);
            }

        auto texFormat = (int)(osprayTextureFormat(depth, channels, preferLinear));
        auto texFilter = (int)(nearestFilter ? OSP_TEXTURE_FILTER_NEAREST
                                             : OSP_TEXTURE_FILTER_BILINEAR);
        createChildData("data", data, vec2ul(width, height));
        createChild("format", "int", texFormat);
        createChild("filter", "int", texFilter);

      } catch (const std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        // reset();
      }

    } else if (ext == "pfm") {
      try {
        // Note: the PFM file specification does not support comments thus we
        // don't skip any http://netpbm.sourceforge.net/doc/pfm.html
        int rc     = 0;
        FILE *file = fopen(fileName.str().c_str(), "rb");
        if (!file) {
          throw std::runtime_error("#ospray_sg: could not open texture file '" +
                                   fileName.str() + "'.");
        }
        // read format specifier:
        // PF: color floating point image
        // Pf: grayscale floating point image
        char format[2] = {0};
        if (fscanf(file, "%c%c\n", &format[0], &format[1]) != 2)
          throw std::runtime_error("could not fscanf");

        if (format[0] != 'P' || (format[1] != 'F' && format[1] != 'f')) {
          throw std::runtime_error(
              "#ospray_sg: invalid pfm texture file, header is not PF or "
              "Pf");
        }

        int numChannels = 3;
        if (format[1] == 'f') {
          numChannels = 1;
        }

        // read width and height
        int width  = -1;
        int height = -1;
        rc         = fscanf(file, "%i %i\n", &width, &height);
        if (rc != 2) {
          throw std::runtime_error(
              "#ospray_sg: could not parse width and height in PF PFM file "
              "'" +
              fileName.str() +
              "'. "
              "Please report this bug at ospray.github.io, and include named "
              "file to reproduce the error.");
        }

        // read scale factor/endiannes
        float scaleEndian = 0.0;
        rc                = fscanf(file, "%f\n", &scaleEndian);

        if (rc != 1) {
          throw std::runtime_error(
              "#ospray_sg: could not parse scale factor/endianness in PF "
              "PFM file '" +
              fileName.str() +
              "'. "
              "Please report this bug at ospray.github.io, and include named "
              "file to reproduce the error.");
        }
        if (scaleEndian == 0.0) {
          throw std::runtime_error(
              "#ospray_sg: scale factor/endianness in PF PFM file can not "
              "be 0");
        }
        if (scaleEndian > 0.0) {
          throw std::runtime_error(
              "#ospray_sg: could not parse PF PFM file '" + fileName.str() +
              "': currently supporting only little endian formats"
              "Please report this bug at ospray.github.io, and include named "
              "file to reproduce the error.");
        }
        float scaleFactor = std::abs(scaleEndian);

        size.x        = width;
        size.y        = height;
        channels      = numChannels;
        depth         = sizeof(float);

        unsigned int dataSize = size.x * size.y * channels * depth;

        std::vector<vec3f> data(sizeof(float) * dataSize);

        rc = fread(data.data(), dataSize, 1, file);
        
        // flip in y, because OSPRay's textures have the origin at the lower
        // left corner
        float *texels = (float *)data.data();
        for (int y = 0; y < height / 2; ++y) {
          for (int x = 0; x < width * numChannels; ++x) {
            // Scale the pixels by the scale factor
            texels[y * width * numChannels + x] =
                texels[y * width * numChannels + x] * scaleFactor;
            texels[(height - 1 - y) * width * numChannels + x] =
                texels[(height - 1 - y) * width * numChannels + x] *
                scaleFactor;
            std::swap(texels[y * width * numChannels + x],
                      texels[(height - 1 - y) * width * numChannels + x]);
          }
        }

        auto texFormat = (int)(osprayTextureFormat(depth, channels, preferLinear));
        auto texFilter = (int)(nearestFilter ? OSP_TEXTURE_FILTER_NEAREST
                                             : OSP_TEXTURE_FILTER_BILINEAR);
        createChildData("data", data, vec2ul(width, height));
        createChild("format", "int", texFormat);
        createChild("filter", "int", texFilter);
      } catch (const std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        // reset();
      }

    } else {
      int width, height, n;
      const bool hdr        = stbi_is_hdr(fileName.str().c_str());
      unsigned char *pixels = nullptr;
      if (hdr)
        pixels = (unsigned char *)stbi_loadf(
            fileName.str().c_str(), &width, &height, &n, 0);
      else
        pixels = stbi_load(fileName.str().c_str(), &width, &height, &n, 0);

      size.x        = width;
      size.y        = height;
      channels      = n;
      depth         = hdr ? 4 : 1;

      if (!pixels) {
        std::cerr << "#osp:sg: failed to load texture '" + fileName.str() + "'"
                  << std::endl;
        // reset();
      } else {
        unsigned int dataSize = size.x * size.y * channels * depth;

        std::vector<vec3uc> data(sizeof(unsigned char) * dataSize);
        // convert pixels and flip image (because OSPRay's textures
        // have the origin at the lower left corner)
        for (int y = 0; y < size.y; y++) {
          for (int x = 0; x < size.x; x++) {
            if (hdr) {
              const float *pixel =
                  &((float *)pixels)[(y * size.x + x) * channels];
              float *dst = &(
                  (float *)
                      data.data())[(x + (size.y - 1 - y) * size.x) * channels];
              for (int i = 0; i < channels; i++)
                *dst++ = pixel[i];
            } else {
              const unsigned char *pixel = &pixels[(y * size.x + x) * channels];
              unsigned char *dst         = &(
                  (unsigned char *)
                      data.data())[((size.y - 1 - y) * size.x + x) * channels];
              for (int i = 0; i < channels; i++)
                *dst++ = pixel[i];
            }
          }
        }

        auto texFormat =
            (int)(osprayTextureFormat(depth, channels, preferLinear));
        auto texFilter = (int)(nearestFilter ? OSP_TEXTURE_FILTER_NEAREST
                                             : OSP_TEXTURE_FILTER_BILINEAR);
        createChildData("data", data, vec2ul(width, height));
        createChild("format", "int", texFormat);
        createChild("filter", "int", texFilter);
      }
    }
#endif

    // if (get() != nullptr)
    //   textureCache[fileName.str()] = tex;

    // setValue(tex);
    // return tex;
  }

  void Texture2D::clearTextureCache()
  {
    textureCache.clear();
  }

  OSP_REGISTER_SG_NODE_NAME(Texture2D, texture_2d);

  std::map<std::string, std::shared_ptr<Texture2D>> Texture2D::textureCache;

}  // namespace ospray::sg
