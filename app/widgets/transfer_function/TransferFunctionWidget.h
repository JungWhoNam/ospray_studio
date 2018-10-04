// ======================================================================== //
// Copyright 2009-2018 Intel Corporation                                    //
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

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#ifdef _WIN32
#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3native.h>
#endif

#include "tfn_lib/tfn_lib.h"

#include "ospray/sg/transferFunction/TransferFunction.h"

namespace tfn {
  namespace tfn_widget {
    using namespace ospray;

    class TransferFunctionWidget
    {
     public:
      TransferFunctionWidget(std::shared_ptr<sg::TransferFunction> tfn);
      ~TransferFunctionWidget();
      TransferFunctionWidget(const TransferFunctionWidget &);
      TransferFunctionWidget &operator=(const TransferFunctionWidget &);
      /* Control Widget Values
       */
      void setDefaultRange(const float &a, const float &b)
      {
        defaultRange[0] = a;
        defaultRange[1] = b;
        valueRange[0]   = a;
        valueRange[1]   = b;
        tfn_changed     = true;
      }
      /* Draw the transfer function editor widget, returns true if the
       * transfer function changed
       */
      bool drawUI();
      /* Render the transfer function to a 1D texture that can
       * be applied to volume data
       */
      void render();
      // Load the transfer function in the file passed and set it active
      void load(const std::string &fileName);
      // Save the current transfer function out to the file
      void save(const std::string &fileName) const;

     private:
      // The indices of the transfer function color presets available
      enum ColorMap
      {
        JET,
        ICE_FIRE,
        COOL_WARM,
        BLUE_RED,
        GRAYSCALE,
      };

      // This defines the control point class
      struct ColorPoint
      {
        float p;  // location of the control point [0, 1]
        float r, g, b;
        ColorPoint(){};
        ColorPoint(const float cp,
                   const float cr,
                   const float cg,
                   const float cb)
            : p(cp), r(cr), g(cg), b(cb)
        {
        }
        ColorPoint(const ColorPoint &c) : p(c.p), r(c.r), g(c.g), b(c.b) {}
        ColorPoint &operator=(const ColorPoint &c)
        {
          if (this == &c) {
            return *this;
          }
          p = c.p;
          r = c.r;
          g = c.g;
          b = c.b;
          return *this;
        }
        // This function gives Hex color for ImGui
        unsigned long GetHex()
        {
          return (0xff << 24) +
                 ((static_cast<uint8_t>(b * 255.f) & 0xff) << 16) +
                 ((static_cast<uint8_t>(g * 255.f) & 0xff) << 8) +
                 ((static_cast<uint8_t>(r * 255.f) & 0xff));
        }
      };
      struct OpacityPoint
      {
        OpacityPoint(){};
        OpacityPoint(const float cp, const float ca) : p(cp), a(ca) {}
        OpacityPoint(const OpacityPoint &c) : p(c.p), a(c.a) {}
        OpacityPoint &operator=(const OpacityPoint &c)
        {
          if (this == &c) {
            return *this;
          }
          p = c.p;
          a = c.a;
          return *this;
        }
        float p;  // location of the control point [0, 1]
        float a;
      };

      // TODO
      // This MAYBE the correct way of doing this
      struct TFN
      {
        std::vector<ColorPoint> colors;
        std::vector<OpacityPoint> opacity;
        tfn::TransferFunction reader;
      };

      // Core Handler
      std::function<void(const std::vector<float> &,
                         const std::vector<float> &,
                         const std::array<float, 2> &)>
          tfn_sample_set;
      std::vector<tfn::TransferFunction> tfn_readers;
      std::array<float, 2> valueRange;
      std::array<float, 2> defaultRange;

      // TFNs information:
      std::vector<bool> tfn_editable;
      std::vector<std::vector<ColorPoint>> tfn_c_list;
      std::vector<std::vector<OpacityPoint>> tfn_o_list;
      std::vector<ColorPoint> *tfn_c;
      std::vector<OpacityPoint> *tfn_o;
      std::vector<std::string> tfn_names;  // name of TFNs in the dropdown menu

      // State Variables:
      // TODO: those variables might be unnecessary
      bool tfn_edit;
      std::vector<char> tfn_text_buffer;  // The filename input text buffer

      // Flags:
      // tfn_changed:
      //     Track if the function changed and must be re-uploaded.
      //     We start by marking it changed to upload the initial palette
      bool tfn_changed{true};

      // Meta Data
      // * Interpolated trasnfer function size
      // int tfn_w = 256;
      // int tfn_h = 1; // TODO: right now we onlu support 1D TFN
      int numSamples {1};

      // * The selected transfer function being shown
      int tfn_selection{JET};
      // * The 2d palette texture on the GPU for displaying the color map in the
      // UI.
      GLuint tfn_palette{0};
      // Local functions
      void LoadDefaultMap();
      void SetTFNSelection(int);
    };
  }  // namespace tfn_widget
}  // namespace tfn