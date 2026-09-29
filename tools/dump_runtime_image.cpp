#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct HandleCloser {
    void operator()(HANDLE handle) const {
        if (handle && handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
};

using UniqueHandle = std::unique_ptr<void, HandleCloser>;

bool ReadFileBytes(const std::filesystem::path& path, std::vector<std::uint8_t>* bytes) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        return false;
    const auto size = stream.tellg();
    if (size <= 0)
        return false;
    bytes->resize(static_cast<std::size_t>(size));
    stream.seekg(0);
    return static_cast<bool>(stream.read(
        reinterpret_cast<char*>(bytes->data()), static_cast<std::streamsize>(bytes->size())));
}

bool WriteFileBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    return stream && static_cast<bool>(stream.write(
        reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())));
}

bool FindMainModule(const DWORD pid, MODULEENTRY32W* module) {
    UniqueHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
    if (snapshot.get() == INVALID_HANDLE_VALUE)
        return false;
    module->dwSize = sizeof(*module);
    return Module32FirstW(snapshot.get(), module) != FALSE;
}

} // namespace

int wmain(const int argc, wchar_t** argv) {
    if (argc != 3) {
        std::fwprintf(stderr, L"Usage: dump_runtime_image.exe <pid> <output.exe>\n");
        return 2;
    }

    wchar_t* end = nullptr;
    const unsigned long parsedPid = std::wcstoul(argv[1], &end, 10);
    if (!end || *end != L'\0' || parsedPid == 0 || parsedPid > MAXDWORD) {
        std::fwprintf(stderr, L"Invalid process id: %s\n", argv[1]);
        return 3;
    }
    const DWORD pid = static_cast<DWORD>(parsedPid);

    MODULEENTRY32W module = {};
    if (!FindMainModule(pid, &module)) {
        std::fwprintf(stderr, L"Could not identify the process main module (error %lu).\n", GetLastError());
        return 4;
    }

    std::vector<std::uint8_t> image;
    if (!ReadFileBytes(module.szExePath, &image)) {
        std::fwprintf(stderr, L"Could not read the original image: %s\n", module.szExePath);
        return 5;
    }
    if (image.size() < sizeof(IMAGE_DOS_HEADER)) {
        std::fwprintf(stderr, L"Original image is too small.\n");
        return 6;
    }

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        static_cast<std::size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS32) > image.size()) {
        std::fwprintf(stderr, L"Original image has invalid PE headers.\n");
        return 7;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(image.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        std::fwprintf(stderr, L"Original image is not a 32-bit PE file.\n");
        return 8;
    }

    const auto* sections = IMAGE_FIRST_SECTION(nt);
    const auto sectionTableEnd = reinterpret_cast<const std::uint8_t*>(
        sections + nt->FileHeader.NumberOfSections);
    if (sectionTableEnd > image.data() + image.size()) {
        std::fwprintf(stderr, L"Original image has an invalid section table.\n");
        return 9;
    }

    UniqueHandle process(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));
    if (!process) {
        std::fwprintf(stderr, L"Could not open process %lu (error %lu).\n", pid, GetLastError());
        return 10;
    }

    const auto base = reinterpret_cast<std::uintptr_t>(module.modBaseAddr);
    std::wprintf(L"Process %lu main module: %s\n", pid, module.szExePath);
    std::wprintf(L"Runtime base: 0x%08llX\n", static_cast<unsigned long long>(base));

    for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        const IMAGE_SECTION_HEADER& section = sections[index];
        const std::size_t rawOffset = section.PointerToRawData;
        const std::size_t rawSize = section.SizeOfRawData;
        const std::size_t virtualSize = section.Misc.VirtualSize;
        const std::size_t copySize = std::min(rawSize, virtualSize);
        if (copySize == 0 || rawOffset > image.size() || copySize > image.size() - rawOffset)
            continue;

        SIZE_T bytesRead = 0;
        const auto address = reinterpret_cast<const void*>(base + section.VirtualAddress);
        const BOOL read = ReadProcessMemory(
            process.get(), address, image.data() + rawOffset, copySize, &bytesRead);
        char name[IMAGE_SIZEOF_SHORT_NAME + 1] = {};
        std::copy(std::begin(section.Name), std::end(section.Name), name);
        std::printf(
            "%-8s RVA=0x%08lX requested=%zu read=%zu status=%s\n",
            name, section.VirtualAddress, copySize, static_cast<std::size_t>(bytesRead),
            read && bytesRead == copySize ? "complete" : "partial");
        if (!read || bytesRead != copySize) {
            std::fwprintf(stderr, L"Failed to read a complete section (error %lu).\n", GetLastError());
            return 11;
        }
    }

    const std::filesystem::path output(argv[2]);
    if (!WriteFileBytes(output, image)) {
        std::fwprintf(stderr, L"Could not write output image: %s\n", output.c_str());
        return 12;
    }
    std::fwprintf(stdout, L"Runtime image written to: %s\n", output.c_str());
    return 0;
}
