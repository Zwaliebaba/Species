#pragma once

namespace Neuron
{
  using byte_buffer_t = std::vector<uint8_t>;

  class FileSys
  {
    public:
      static void SetHomeDirectory(const std::wstring& _path) { sm_homeDir = _path + L"\\GameData\\"; }
      [[nodiscard]] static std::wstring GetHomeDirectory() { return sm_homeDir; }

      [[nodiscard]] static std::string GetHomeDirectoryA()
      {
#pragma warning(suppress:4244) // Intentional wchar_t→char narrowing (path is ASCII-only)
        return std::string(sm_homeDir.begin(), sm_homeDir.end());
      }

      static std::string GetFullPathA(std::string_view _fileName) { return GetHomeDirectoryA() + std::string(_fileName); }

    protected:
      inline static std::wstring sm_homeDir;
  };

  class BinaryFile : public FileSys
  {
    public:
      [[nodiscard]] static byte_buffer_t ReadFile(std::wstring_view _fileName);
  };

  class TextFile : public FileSys
  {
    public:
      [[nodiscard]] static std::wstring ReadFile(std::wstring_view _fileName);
  };
}
