/*
 *  Load texture from an in-memory image buffer.
 *
 *  Used by both the native and the wasm builds so the binary owns its
 *  texture data and no runtime filesystem access is required. The image
 *  format is whatever stb_image supports; in this project we feed it PNG.
 *
 *  stbi_set_flip_vertically_on_load(1) is called the first time through
 *  because PNG rows are stored top-to-bottom while GL's default texture
 *  coordinate origin is bottom-left.
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "CSCIx229.h"

unsigned int LoadTexFromMemory(const unsigned char* data, int len)
{
   static int flip_set = 0;
   unsigned int texture;
   int w, h, channels;
   unsigned char* img;

   if (!flip_set)
   {
      stbi_set_flip_vertically_on_load(1);
      flip_set = 1;
   }

   img = stbi_load_from_memory(data, len, &w, &h, &channels, 3);
   if (!img) Fatal("stb_image decode failed: %s\n", stbi_failure_reason());

   ErrCheck("LoadTexFromMemory pre");
   glGenTextures(1, &texture);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexImage2D(GL_TEXTURE_2D, 0, 3, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
   if (glGetError()) Fatal("Error in glTexImage2D %dx%d\n", w, h);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   stbi_image_free(img);
   return texture;
}
