#pragma once

#include <string>
#include <string_view>


//*****************************************************************************
// Class BinaryReader
//*****************************************************************************


namespace Neuron
{
  class BinaryReader
  {
    public:
      bool m_eof;
      // std::string since strings-modernised T9. Was char[256], written by an
      // strncpy bounded to 255 — which leaves byte 255 UNWRITTEN, so a filename
      // of exactly 255 characters or more was never terminated.
      std::string m_filename;

      BinaryReader();
      virtual ~BinaryReader();

      virtual bool IsOpen() = 0;
      // Empty rather than nullptr when the name has no extension, which is
      // what the pointer-to-the-terminator this replaces amounted to.
      virtual std::string_view GetFileType() = 0;

      virtual signed char ReadS8() = 0;
      virtual short ReadS16() = 0;
      virtual int ReadS32() = 0;
      virtual unsigned char ReadU8() = 0;
      //	virtual unsigned short	ReadU16	() = 0;
      //	virtual unsigned int	ReadU32	() = 0;

      virtual unsigned int ReadBytes(unsigned int _count, unsigned char* _buffer) = 0;

      virtual int Seek(int _offset, int _origin) = 0;
      virtual int Tell() = 0;

      int GetSize();
  };


  //*****************************************************************************
  // Class BinaryFileReader
  //*****************************************************************************

  class BinaryFileReader : public BinaryReader
  {
    protected:
      FILE* m_file;

    public:
      BinaryFileReader(char const* _filename);
      ~BinaryFileReader();

      bool IsOpen();
      std::string_view GetFileType() override;

      signed char ReadS8();
      short ReadS16();
      int ReadS32();
      unsigned char ReadU8();
      //	unsigned short	ReadU16		();
      //	unsigned int	ReadU32		();

      unsigned int ReadBytes(unsigned int _count, unsigned char* _buffer);

      int Seek(int _offset, int _origin);
      int Tell();
  };


  //*****************************************************************************
  // Class BinaryDataReader
  //*****************************************************************************

  class BinaryDataReader : public BinaryReader
  {
    protected:
      unsigned int m_offset;

    public:
      unsigned char const* m_data;
      unsigned int m_dataSize;

      BinaryDataReader(unsigned char const* _data, unsigned int _dataSize, char const* _filename);
      ~BinaryDataReader();

      bool IsOpen();
      std::string_view GetFileType() override;

      signed char ReadS8();
      short ReadS16();
      int ReadS32();
      unsigned char ReadU8();
      //	unsigned short	ReadU16		();
      //	unsigned int	ReadU32		();

      unsigned int ReadBytes(unsigned int _count, unsigned char* _buffer);

      int Seek(int _offset, int _origin);
      int Tell();
  };
} // namespace Neuron
