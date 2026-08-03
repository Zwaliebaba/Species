#include "pch.h"

#include "BinaryStreamReaders.h"
#include "Bitmap.h"
#include "Debug.h"
#include "FilesysUtils.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "RgbColour.h"
#include "Preferences.h"


#define BMP_RGB				0
#define OS2INFOHEADERSIZE  12
#define WININFOHEADERSIZE  40

class BitmapFileHeader
{
  public:
    unsigned short bfType;
    unsigned long bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned long bfOffBits;
};

// Used for both OS/2 and Windows BMP.
// Contains only the parameters needed to load the image
class BitmapInfoHeader
{
  public:
    unsigned long biWidth;
    unsigned long biHeight;
    unsigned short biBitCount;
    unsigned long biCompression;
};

class WinBmpInfoHeader /* size: 40 */
{
  public:
    unsigned long biWidth;
    unsigned long biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned long biCompression;
    unsigned long biSizeImage;
    unsigned long biXPelsPerMeter;
    unsigned long biYPelsPerMeter;
    unsigned long biClrUsed;
    unsigned long biClrImportant;
};

class OS2BmpInfoHeader /* size: 12 */
{
  public:
    unsigned short biWidth;
    unsigned short biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
};

// ****************************************************************************
// Private Functions
// ****************************************************************************

// Reads a BMP file header and check that it has the BMP magic number.
void BitmapRGBA::ReadBMPFileHeader(BinaryReader* f, BitmapFileHeader* fileheader)
{
  fileheader->bfType = f->ReadS16();
  fileheader->bfSize = f->ReadS32();
  fileheader->bfReserved1 = f->ReadS16();
  fileheader->bfReserved2 = f->ReadS16();
  fileheader->bfOffBits = f->ReadS32();

  DEBUG_ASSERT(fileheader->bfType == 19778);
}

// Reads information from a BMP file header.
void BitmapRGBA::ReadWinBMPInfoHeader(BinaryReader* f, BitmapInfoHeader* infoheader)
{
  WinBmpInfoHeader win_infoheader;

  win_infoheader.biWidth = f->ReadS32();
  win_infoheader.biHeight = f->ReadS32();
  win_infoheader.biPlanes = f->ReadS16();
  win_infoheader.biBitCount = f->ReadS16();
  win_infoheader.biCompression = f->ReadS32();
  win_infoheader.biSizeImage = f->ReadS32();
  win_infoheader.biXPelsPerMeter = f->ReadS32();
  win_infoheader.biYPelsPerMeter = f->ReadS32();
  win_infoheader.biClrUsed = f->ReadS32();
  win_infoheader.biClrImportant = f->ReadS32();

  infoheader->biWidth = win_infoheader.biWidth;
  infoheader->biHeight = win_infoheader.biHeight;
  infoheader->biBitCount = win_infoheader.biBitCount;
  infoheader->biCompression = win_infoheader.biCompression;
}

// Reads information from an OS/2 format BMP file header.
void BitmapRGBA::ReadOS2BMPInfoHeader(BinaryReader* f, BitmapInfoHeader* infoheader)
{
  OS2BmpInfoHeader os2_infoheader;

  os2_infoheader.biWidth = f->ReadS16();
  os2_infoheader.biHeight = f->ReadS16();
  os2_infoheader.biPlanes = f->ReadS16();
  os2_infoheader.biBitCount = f->ReadS16();

  infoheader->biWidth = os2_infoheader.biWidth;
  infoheader->biHeight = os2_infoheader.biHeight;
  infoheader->biBitCount = os2_infoheader.biBitCount;
  infoheader->biCompression = 0;
}

void BitmapRGBA::ReadBMPPalette(int ncols, RGBAColour pal[256], BinaryReader* f, int win_flag)
{
  for (int i = 0; i < ncols; i++)
  {
    pal[i].b = f->ReadU8();
    pal[i].g = f->ReadU8();
    pal[i].r = f->ReadU8();
    pal[i].a = 255;
    if (win_flag)
      f->ReadU8();
  }
}

// Support function for reading the 4 bit bitmap file format.
void BitmapRGBA::Read4BitLine(int length, BinaryReader* f, RGBAColour pal[256], int y)
{
  for (int x = 0; x < length; x += 2)
  {
    unsigned char i = f->ReadU8();
    unsigned idx1 = i & 15;
    unsigned idx2 = (i >> 4) & 15;
    PutPixel(x + 1, y, pal[idx1]);
    PutPixel(x, y, pal[idx2]);
  }
}

// Support function for reading the 8 bit bitmap file format.
void BitmapRGBA::Read8BitLine(int length, BinaryReader* f, RGBAColour pal[256], int y)
{
  //	y = m_height - line - 1;
  for (int x = 0; x < length; ++x)
  {
    unsigned char i = f->ReadU8();
    PutPixel(x, y, pal[i]);
  }
}

// Support function for reading the 24 bit bitmap file format
void BitmapRGBA::Read24BitLine(int length, BinaryReader* f, int y)
{
  RGBAColour c;
  int nbytes = 0;
  //	y = m_height - y - 1;
  c.a = 255;

  for (int i = 0; i < length; i++)
  {
    c.b = f->ReadU8();
    c.g = f->ReadU8();
    c.r = f->ReadU8();
    PutPixel(i, y, c);
    nbytes += 3;
  }

  for (int padding = (4 - nbytes) & 3; padding; --padding)
    f->ReadU8();
}

void BitmapRGBA::LoadBmp(BinaryReader* _in)
{
  BitmapFileHeader fileheader;
  BitmapInfoHeader infoheader;
  RGBAColour palette[256];

  ReadBMPFileHeader(_in, &fileheader);

  unsigned long biSize = _in->ReadS32();

  if (biSize == WININFOHEADERSIZE)
  {
    ReadWinBMPInfoHeader(_in, &infoheader);

    int ncol = (fileheader.bfOffBits - 54) / 4; // compute number of colors recorded
    ReadBMPPalette(ncol, palette, _in, 1);
  }
  else if (biSize == OS2INFOHEADERSIZE)
  {
    ReadOS2BMPInfoHeader(_in, &infoheader);

    int ncol = (fileheader.bfOffBits - 26) / 3; // compute number of colors recorded
    ReadBMPPalette(ncol, palette, _in, 0);
  }
  else
    DEBUG_ASSERT(0);

  Initialise(infoheader.biWidth, infoheader.biHeight);
  DEBUG_ASSERT(infoheader.biCompression == BMP_RGB);
  DEBUG_ASSERT(!_in->m_eof);

  // Read the image
  for (int i = 0; i < static_cast<int>(infoheader.biHeight); ++i)
  {
    switch (infoheader.biBitCount)
    {
    case 4:
      Read4BitLine(infoheader.biWidth, _in, palette, i);
      break;
    case 8:
      Read8BitLine(infoheader.biWidth, _in, palette, i);
      break;
    case 24:
      Read24BitLine(infoheader.biWidth, _in, i);
      break;
    default: DEBUG_ASSERT(0);
      break;
    }
  }
}

// Little endian output functions
static void writeShort(unsigned short _x, FILE* _out)
{
  fputc(_x & 0xFF, _out);
  _x >>= 8;
  fputc(_x & 0xFF, _out);
}

static void writeInt(unsigned _x, FILE* _out)
{
  fputc(_x & 0xFF, _out);
  _x >>= 8;
  fputc(_x & 0xFF, _out);
  _x >>= 8;
  fputc(_x & 0xFF, _out);
  _x >>= 8;
  fputc(_x & 0xFF, _out);
}

void BitmapRGBA::WriteBMPFileHeader(FILE* _out)
{
  BitmapFileHeader fileheader;
  fileheader.bfType = 19778;
  fileheader.bfSize = m_width * m_height * 3 + 54;
  fileheader.bfReserved1 = 0;
  fileheader.bfReserved2 = 0;
  fileheader.bfOffBits = 54;

  writeShort(fileheader.bfType, _out);
  writeInt(fileheader.bfSize, _out);
  writeShort(fileheader.bfReserved1, _out);
  writeShort(fileheader.bfReserved2, _out);
  writeInt(fileheader.bfOffBits, _out);
}

void BitmapRGBA::WriteWinBMPInfoHeader(FILE* _out)
{
  WinBmpInfoHeader win_infoheader;

  win_infoheader.biWidth = m_width;
  win_infoheader.biHeight = m_height;
  win_infoheader.biPlanes = 1;
  win_infoheader.biBitCount = 24;
  win_infoheader.biCompression = 0;
  win_infoheader.biSizeImage = m_width * m_height * 3;
  win_infoheader.biXPelsPerMeter = 2835;
  win_infoheader.biYPelsPerMeter = 2835;
  win_infoheader.biClrUsed = 0;
  win_infoheader.biClrImportant = 0;

  writeInt(win_infoheader.biWidth, _out);
  writeInt(win_infoheader.biHeight, _out);
  writeShort(win_infoheader.biPlanes, _out);
  writeShort(win_infoheader.biBitCount, _out);
  writeInt(win_infoheader.biCompression, _out);
  writeInt(win_infoheader.biSizeImage, _out);
  writeInt(win_infoheader.biXPelsPerMeter, _out);
  writeInt(win_infoheader.biYPelsPerMeter, _out);
  writeInt(win_infoheader.biClrUsed, _out);
  writeInt(win_infoheader.biClrImportant, _out);
}

void BitmapRGBA::Write24BitLine(FILE* _out, int _y)
{
  int nbytes = 0;

  for (int x = 0; x < m_width; ++x)
  {
    const RGBAColour& c = GetPixel(x, _y);
    fputc(c.b, _out);
    fputc(c.g, _out);
    fputc(c.r, _out);
    nbytes += 3;
  }

  for (int padding = (4 - nbytes) & 3; padding; --padding)
    fputc(0, _out);
}

void BitmapRGBA::SaveBmp(const char* _filename)
{
  FILE* _out = fopen(FileSys::GetFullPathA(_filename).c_str(), "wb");
  ASSERT_TEXT(_out, "Couldn't create image file %s", _filename);
  WriteBmp(_out);
  fclose(_out);
}

void BitmapRGBA::WriteBmp(FILE* _out)
{
  ASSERT_TEXT(_out, "Couldn't write image file");

  WriteBMPFileHeader(_out);

  unsigned long nextHeaderSize = 40;
  writeInt(nextHeaderSize, _out);

  WriteWinBMPInfoHeader(_out);

  for (int y = 0; y < m_height; ++y)
    Write24BitLine(_out, y);
}

// ****************************************************************************
// Public Functions
// ****************************************************************************

BitmapRGBA::BitmapRGBA()
  : m_width(-1),
    m_height(-1),
    m_pixels(nullptr),
    m_lines(nullptr) {}

BitmapRGBA::BitmapRGBA(const BitmapRGBA& _other)
{
  Initialise(_other.m_width, _other.m_height);
  memcpy(m_pixels, _other.m_pixels, m_width * m_height * sizeof(RGBAColour));
}

BitmapRGBA::BitmapRGBA(int width, int height)
  : m_width(-1),
    m_height(-1),
    m_pixels(nullptr),
    m_lines(nullptr) { Initialise(width, height); }

// *** Constructor: Load from file
BitmapRGBA::BitmapRGBA(const char* _filename)
  : m_width(-1),
    m_height(-1),
    m_pixels(nullptr),
    m_lines(nullptr) { Initialise(_filename); }

// *** Constructor: Load from a BinaryReader
BitmapRGBA::BitmapRGBA(BinaryReader* _reader, const char* _type)
  : m_width(-1),
    m_height(-1),
    m_pixels(nullptr),
    m_lines(nullptr) { Initialise(_reader, _type); }

// *** Destructor
BitmapRGBA::~BitmapRGBA()
{
  m_width = -1;
  m_height = -1;
  delete [] m_pixels;
  delete [] m_lines;
  m_pixels = nullptr;
  m_lines = nullptr;
}

// *** Initialise
void BitmapRGBA::Initialise(int width, int height)
{
  m_width = width;
  m_height = height;
  m_pixels = new RGBAColour[width * height];
  m_lines = new RGBAColour*[height];

  DEBUG_ASSERT(m_pixels && m_lines);

  for (int y = 0; y < height; ++y)
    m_lines[y] = &m_pixels[y * width];
}

void BitmapRGBA::Initialise(const char* _filename)
{
  BinaryFileReader in(_filename);

  const std::string extension = GetExtensionPart(_filename);
  DEBUG_ASSERT(stricmp(extension.c_str(), "bmp") == 0);
  LoadBmp(&in);
}

void BitmapRGBA::Initialise(BinaryReader* _reader, const char* _type)
{
  DEBUG_ASSERT(stricmp(_type, "bmp") == 0);
  LoadBmp(_reader);
}

// *** PutPixel
void BitmapRGBA::PutPixel(int x, int y, const RGBAColour& colour) { m_lines[y][x] = colour; }

// *** GetPixel
const RGBAColour& BitmapRGBA::GetPixel(int x, int y) const { return m_lines[y][x]; }

// *** PutPixelClipped
void BitmapRGBA::PutPixelClipped(int x, int y, const RGBAColour& colour)
{
  if (x < 0 || x >= m_width || y < 0 || y >= m_height)
    return;

  m_lines[y][x] = colour;
}

// *** GetPixelClipped
const RGBAColour& BitmapRGBA::GetPixelClipped(int x, int y) const
{
  if (x < 0 || x >= m_width || y < 0 || y >= m_height)
    return g_colourBlack;

  return m_lines[y][x];
}

void BitmapRGBA::DrawLine(int x1, int y1, int x2, int y2, const RGBAColour& colour)
{
  int numSteps = sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
  float dx = static_cast<float>(x2 - x1) / static_cast<float>(numSteps);
  float dy = static_cast<float>(y2 - y1) / static_cast<float>(numSteps);

  float currentX = x1;
  float currentY = y1;

  for (int i = 0; i < numSteps; ++i)
  {
    PutPixelClipped(currentX, currentY, colour);

    currentX += dx;
    currentY += dy;
  }
}

// *** GetInterpolatedPixel
RGBAColour BitmapRGBA::GetInterpolatedPixel(float _x, float _y) const
{
  int x1 = floorf(_x);
  int y1 = floorf(_y);
  int x2 = ceilf(_x);
  int y2 = ceilf(_y);

  if (x2 == m_width)
    x2 = m_width - 1;

  if (y2 == m_height)
    y2 = m_height - 1;

  float fractionalX = _x - x1;
  float fractionalY = _y - y1;

  float weight11 = (1.0f - fractionalX) * (1.0f - fractionalY);
  float weight12 = (1.0f - fractionalX) * (fractionalY);
  float weight21 = (fractionalX) * (1.0f - fractionalY);
  float weight22 = (fractionalX) * (fractionalY);

  const RGBAColour& c11 = GetPixelClipped(x1, y1);
  const RGBAColour& c12 = GetPixelClipped(x1, y2);
  const RGBAColour& c21 = GetPixelClipped(x2, y1);
  const RGBAColour& c22 = GetPixelClipped(x2, y2);

  RGBAColour returnVal(
    static_cast<float>(c11.r) * weight11 + static_cast<float>(c12.r) * weight12 + static_cast<float>(c21.r) * weight21 + static_cast<float>(
      c22.r) * weight22,
    static_cast<float>(c11.g) * weight11 + static_cast<float>(c12.g) * weight12 + static_cast<float>(c21.g) * weight21 + static_cast<float>(
      c22.g) * weight22,
    static_cast<float>(c11.b) * weight11 + static_cast<float>(c12.b) * weight12 + static_cast<float>(c21.b) * weight21 + static_cast<float>(
      c22.b) * weight22,
    static_cast<float>(c11.a) * weight11 + static_cast<float>(c12.a) * weight12 + static_cast<float>(c21.a) * weight21 + static_cast<float>(
      c22.a) * weight22);

  return returnVal;
}

void BitmapRGBA::ConvertToGreyScale()
{
  RGBAColour* c = m_pixels;
  unsigned numPixels = m_width * m_height;

  while (numPixels--)
  {
    int average = (c->r + c->b + c->g) / 3;
    c->r = average;
    c->b = average;
    c->g = average;
    c++;
  }
}

// *** Blit
void BitmapRGBA::Blit(int _srcX, int _srcY, int _srcW, int _srcH, const BitmapRGBA* _srcBmp, int _destX, int _destY, int _destW, int _destH,
                      bool _bilinear)
{
  DEBUG_ASSERT(_srcX + _srcW <= _srcBmp->m_width);
  DEBUG_ASSERT(_srcY + _srcH <= _srcBmp->m_height);
  DEBUG_ASSERT(_destX + _destW <= m_width);
  DEBUG_ASSERT(_destY + _destH <= m_height);

  float sxPitch = static_cast<float>(_srcW) / static_cast<float>(_destW);
  float syPitch = static_cast<float>(_srcH) / static_cast<float>(_destH);

  float sy = _srcY;
  for (int dy = _destY; dy < _destY + _destH; ++dy)
  {
    float sx = _srcX;
    if (_bilinear)
    {
      for (int dx = _destX; dx < _destX + _destW; ++dx)
      {
        const RGBAColour& c = _srcBmp->GetInterpolatedPixel(sx, sy);
        PutPixel(dx, dy, c);
        sx += sxPitch;
      }
    }
    else
    {
      for (int dx = _destX; dx < _destX + _destW; ++dx)
      {
        const RGBAColour& c = _srcBmp->GetPixelClipped(sx, sy);
        PutPixel(dx, dy, c);
        sx += sxPitch;
      }
    }
    sy += syPitch;
  }
}

void BitmapRGBA::ApplyDilateFilter()
{
  auto temp = new RGBAColour[m_width * m_height];
  memcpy(temp, m_pixels, sizeof(RGBAColour) * m_width * m_height);

  for (int x = 1; x < m_width - 1; ++x)
  {
    for (int y = 1; y < m_height - 1; ++y)
    {
      float adjacentRed = 0.0f;
      float adjacentGreen = 0.0f;
      float adjacentBlue = 0.0f;
      for (int i = -1; i <= 1; ++i)
      {
        for (int j = -1; j <= 1; ++j)
        {
          if (i != 0 || j != 0)
          {
            RGBAColour col = temp[(y + j) * m_width + (x + i)];
            adjacentRed += col.r;
            adjacentGreen += col.g;
            adjacentBlue += col.b;
          }
        }
      }
      adjacentRed /= 8.0f;
      adjacentGreen /= 8.0f;
      adjacentBlue /= 8.0f;

      m_pixels[y * m_width + x].Set(adjacentRed, adjacentGreen, adjacentBlue);
    }
  }
  delete[] temp;
}

void BitmapRGBA::ApplyBlurFilter(float _scale)
{
  START_PROFILE(g_profiler, "ApplyBlur");

  DEBUG_ASSERT(m_width > 0 && m_width <= 1024);
  DEBUG_ASSERT(m_height > 0 && m_height <= 1024);

  auto temp = new RGBAColour[m_width * m_height];
  memset(temp, 0, sizeof(RGBAColour) * m_width * m_height);

  constexpr int blurSize = 5;
  constexpr int halfBlurSize = 2;
  float m[blurSize] = {2, 4, 7, 4, 2};
  for (int i = 0; i < blurSize; ++i)
    m[i] *= _scale * 0.0526f;

  //
  // Horizontal blur

  for (int y = 0; y < m_height; ++y)
  {
    for (int x = 0; x < m_width; ++x)
    {
      const RGBAColour& src = m_lines[y][x];
      if (src.r == 0 && src.g == 0 && src.b == 0)
        continue;

      for (int i = 0; i < blurSize; ++i)
      {
        int a = x + i - halfBlurSize;
        if (a < 0 || a >= m_width)
          continue;

        const RGBAColour& dest = temp[y * m_width + a];
        int r = dest.r + Round(static_cast<float>(src.r) * m[i]);
        int g = dest.g + Round(static_cast<float>(src.g) * m[i]);
        int b = dest.b + Round(static_cast<float>(src.b) * m[i]);

        if (r > 255)
          r = 255;
        if (g > 255)
          g = 255;
        if (b > 255)
          b = 255;

        temp[y * m_width + a].Set(r, g, b);
      }
    }
  }

  for (int i = 0; i < blurSize; ++i)
    m[i] *= 2.0f;

  //
  // Vertical blur

  Clear(RGBAColour(0, 0, 0, 0));
  for (int y = 0; y < m_height; ++y)
  {
    for (int x = 0; x < m_width; ++x)
    {
      const RGBAColour& src = temp[y * m_width + x];
      if (src.r == 0 && src.g == 0 && src.b == 0)
        continue;

      for (int i = 0; i < blurSize; ++i)
      {
        int a = y + i - halfBlurSize;
        if (a < 0 || a >= m_height)
          continue;

        const RGBAColour& dest = m_lines[a][x];
        int r = dest.r + Round(static_cast<float>(src.r) * m[i]);
        int g = dest.g + Round(static_cast<float>(src.g) * m[i]);
        int b = dest.b + Round(static_cast<float>(src.b) * m[i]);

        if (r > 255)
          r = 255;
        if (g > 255)
          g = 255;
        if (b > 255)
          b = 255;

        m_lines[a][x].Set(r, g, b);
      }
    }
  }

  delete[] temp;

  END_PROFILE(g_profiler, "ApplyBlur");
}

void BitmapRGBA::ConvertColourToAlpha()
{
  RGBAColour col;
  RGBAColour newCol(255, 255, 255, 0);

  for (int y = 0; y < m_height; ++y)
  {
    for (int x = 0; x < m_width; ++x)
    {
      col = GetPixel(x, y);
      newCol.a = col.g;
      PutPixel(x, y, newCol);
    }
  }
}

void BitmapRGBA::ConvertPinkToTransparent()
{
  const RGBAColour pink(255, 0, 255);
  const RGBAColour trans(128, 128, 128, 0);

  for (int y = 0; y < m_height; ++y)
  {
    for (int x = 0; x < m_width; ++x)
    {
      const RGBAColour c(GetPixel(x, y));
      if (c == pink)
        PutPixel(x, y, trans);
    }
  }
}

void BitmapRGBA::Clear(const RGBAColour& colour)
{
  for (int x = 0; x < m_width; ++x)
    PutPixel(x, 0, colour);

  const int size = sizeof(RGBAColour) * m_width;
  for (int y = 0; y < m_height; ++y)
    memcpy(m_lines[y], m_lines[0], size);
}

int BitmapRGBA::ConvertToTexture(bool _mipmapping) const
{
  unsigned int texId;

  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_2D, texId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  //    bool texturingWasEnabled = glIsEnabled(GL_TEXTURE_2D);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

  if (_mipmapping)
  {
    int newWidth = 1 << static_cast<int>(ceil(log((float)m_width) / log(2.0f)));
    int newHeight = 1 << static_cast<int>(ceil(log((float)m_height) / log(2.0f)));

    if (newHeight < newWidth)
      newHeight = newWidth;
    else if (newWidth < newHeight)
      newWidth = newHeight;

    int result;

    // Our OpenGL implementation in Direct3D mandates power of 2 texture sizes
    bool sameDimensions = (m_width == newWidth && m_height == newHeight);
    bool scale = static_cast<bool>(g_prefsManager->GetInt("ManuallyScaleTextures", 0));

    if (sameDimensions || !scale)
    {
      result = gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels);
    }
    else
    {
      // Scale the bitmap ourselves

      BitmapRGBA scaled(newWidth, newHeight);
      scaled.Blit(0, 0, m_width, m_height, this, 0, 0, newWidth, newHeight, true);

      result = gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, scaled.m_width, scaled.m_height, GL_RGBA, GL_UNSIGNED_BYTE, scaled.m_pixels);
    }

    ASSERT_TEXT(result == 0, "ConvertToTexture failed with error : %s", reinterpret_cast<char const *>(gluErrorString(result)));
  }
  else
    glTexImage2D(GL_TEXTURE_2D, 0, 4, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels);

  //    if (!texturingWasEnabled)
  //    {
  //        glDisable(GL_TEXTURE_2D);
  //    }

  return static_cast<int>(texId);
}
