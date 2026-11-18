#include <filesystem>
#include <cstdlib>
#include <optional>
#include <string>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace {
namespace fs = std::filesystem;
struct Resource { const char* data; DWORD size; };

Resource load(HRSRC info) {
  if (!info) throw std::runtime_error("Missing bundled game file.");
  HGLOBAL resource = LoadResource(nullptr, info);
  const auto* bytes = static_cast<const char*>(LockResource(resource));
  if (!bytes) throw std::runtime_error("Cannot read bundled game file.");
  return {bytes, SizeofResource(nullptr, info)};
}

bool matches(const fs::path& target, Resource bytes) {
  std::ifstream file(target, std::ios::binary | std::ios::ate);
  if (file && file.tellg() == std::streamoff(bytes.size)) {
    file.seekg(0);
    std::vector<char> old(bytes.size);
    return file.read(old.data(), bytes.size) && std::equal(old.begin(), old.end(), bytes.data);
  }
  return false;
}

void install(const fs::path& target, Resource bytes) {
  if (matches(target, bytes)) return;
  fs::create_directories(target.parent_path());
  auto temporary = target;
  temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId());
  try {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.write(bytes.data, bytes.size);
    output.close();
    if (!output) throw std::runtime_error("Cannot write game data in LocalAppData.");
    if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING)) {
      if (!matches(target, bytes)) throw std::runtime_error("Cannot prepare game data in LocalAppData.");
      fs::remove(temporary);
    }
  } catch (...) {
    std::error_code ignored;
    fs::remove(temporary, ignored);
    throw;
  }
}
}
#endif

// The development binary has no bundle index and keeps using its checkout.
std::optional<std::filesystem::path> prepareStandaloneGame() {
#ifdef _WIN32
  auto index = FindResourceW(nullptr, MAKEINTRESOURCEW(900), MAKEINTRESOURCEW(10));
  if (!index) return {};
  auto manifest = load(index);
  std::istringstream lines(std::string(manifest.data, manifest.size));
  std::string id, name;
  std::getline(lines, id);
  if (id.size()!=64 || id.find_first_not_of("0123456789abcdef")!=std::string::npos)
    throw std::runtime_error("Invalid KT resource index.");
  const wchar_t* local = _wgetenv(L"LOCALAPPDATA");
  if (!local || !*local) throw std::runtime_error("Windows LocalAppData is unavailable.");
  const fs::path home = fs::path(local) / L"KT";
  const fs::path cache = home / L"assets" / id;
  int number=1000;
  while (std::getline(lines, name)) {
    fs::path path = fs::u8path(name);
    if (name.empty() || path.has_root_path() || path.lexically_normal()!=path ||
        *path.begin()=="..") throw std::runtime_error("Invalid bundled resource path.");
    install(cache/path, load(FindResourceW(nullptr, MAKEINTRESOURCEW(number++), MAKEINTRESOURCEW(10))));
  }
  fs::current_path(cache);
  std::wstring module(32768, L'\0');
  DWORD length=GetModuleFileNameW(nullptr,module.data(),static_cast<DWORD>(module.size()));
  if (!length || length>=module.size()) throw std::runtime_error("Cannot locate KT.exe.");
  module.resize(length);
  const fs::path legacy=fs::path(module).parent_path()/L"sdp100.db";
  return fs::is_regular_file(legacy)?legacy:home/L"sdp100.db";
#else
  return {};
#endif
}
