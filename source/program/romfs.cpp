#include "romfs.hpp"

#include <nn.hpp>
#include "lib.hpp"
#include "cJSON.h"
#include "game_functions.hpp"
#include "offsets.hpp"

using odr::game::crc64;

namespace odr::romfs {

    struct Offsets {
        /** Constructor for `CFilePathStrId` class used when the game loads files */
        ptrdiff_t CFilePathStrIdCtor;
        /** Finds a StringInstance or creates one */
        ptrdiff_t FindOrCreateStringInstance;
        /** A large array of common CStrId in alpha order */
        ptrdiff_t StaticStringBank;
    };

    static constexpr offsets::Versioned<Offsets> OFFSETS = {
        .v1_0_0 = {
            .CFilePathStrIdCtor = 0x16624,
            .FindOrCreateStringInstance = 0x4063c,
            .StaticStringBank = 0x1cfc2d0,
        },
        .v2_1_0 = {
            .CFilePathStrIdCtor = 0x166C8,
            .FindOrCreateStringInstance = 0x406dc,
            .StaticStringBank = 0x1d552d0,
        },
    };

};

struct StringList
{
    u64 crc;
    char const *replacement;
};

StringList *g_stringList = nullptr;
size_t g_stringListSize = 0;

CStrId* g_stringBank = nullptr;

/** Create a string instance */
using CreateStringInstanceFn = void (*)(CStrId* value, const char* str, uint len, u64 crc, bool storeInPool);
CreateStringInstanceFn create_string_instance = nullptr;

/* Takes in a pointer to string and if found in the list, is replaced with the desired string. */
void replaceString(const char **str)
{
    /* Hash the string for quicker comparison. */
    u64 crc = crc64(*str, strlen(*str));

    /* Attempt to find matching hash in our list. */
    for(size_t i = 0; i < g_stringListSize; i++)
    {
        /* If the string matches, replace the string. */
        if(crc == g_stringList[i].crc)
            *str = g_stringList[i].replacement;
    }
}

/* Allocates a buffer and reads from the specified file.
 * If the path is not a valid file entry, immediately returns nullptr.
 * Caller is expected to free any non-null return value. */
void* odr::romfs::OpenAndReadFile(const char *path)
{
    nn::fs::DirectoryEntryType entryType;
    nn::fs::GetEntryType(&entryType, path);
    if (entryType != nn::fs::DirectoryEntryType_File) {
        return nullptr;
    }

    long int size;
    nn::fs::FileHandle fileHandle;
    nn::fs::OpenFile(&fileHandle, path, nn::fs::OpenMode_Read);
    nn::fs::GetFileSize(&size, fileHandle);
    u8 *fileBuf = (u8 *)malloc(size + 1);
    nn::fs::ReadFile(fileHandle, 0, fileBuf, size);
    nn::fs::CloseFile(fileHandle);
    fileBuf[size] = '\0';
    return fileBuf;
}

/* Parses rom:/replacements.json and populates g_stringList.
 * If replacements.json does not exist, logs a warning and returns without modifying values. */
void populateStringReplacementList()
{
    /* Read contents of replacements.json in romfs. Allocates a heap buffer for the file contents and returns it. */
    char *fileBuf = (char *)odr::romfs::OpenAndReadFile("rom:/replacements.json");
    if (fileBuf == nullptr) {
        const char* msg = "[LogWarn/0] rom:/replacements.json does not exist!";
        svcOutputDebugString(msg, strlen(msg));
        return;
    }

    /* Parse json from file buffer. */
    cJSON *json = cJSON_Parse(fileBuf);
    if(json == nullptr)
        return;

    /* Get the "replacements" object within the json. */
    const cJSON *replacementList = cJSON_GetObjectItem(json, "replacements");

    /* Get number of elements in the array, for later use. */
    g_stringListSize = cJSON_GetArraySize(replacementList);

    /* Allocate space for the list of strings to replace. */
    g_stringList = (StringList *)malloc(g_stringListSize * sizeof(StringList));

    size_t i = 0;
    const cJSON *itemObject;

    /* Iterate over array contents and extract strings. */
    cJSON_ArrayForEach(itemObject, replacementList)
    {
        /* Extract the strings using the relevant method and add them to the list. */
        if(cJSON_IsString(itemObject))
        {
            char *fileStr = cJSON_GetStringValue(itemObject);
            char *replacementFileStr = (char *)malloc(strlen(fileStr) + strlen("rom:/") + 1);
            strcpy(replacementFileStr, "rom:/");
            replacementFileStr = strcat(replacementFileStr, fileStr);
            g_stringList[i].crc = crc64(fileStr, strlen(fileStr));
            g_stringList[i].replacement = replacementFileStr;
        }
        else if(cJSON_IsObject(itemObject))
        {
            char const *str = cJSON_GetItemName(itemObject);
            g_stringList[i].crc = crc64(str, strlen(str));
            g_stringList[i].replacement = cJSON_GetStringValue(itemObject->child);
        }
        i++;
    }

    /* Free the buffer allocated by OpenAndReadFile, since we're done with it. */
    free(fileBuf);
}

/* Sets the save slot names based on the string in "rom:/RDVHASH". 
 * The file must contain at most a 253-character string (called "hash"). 
 * If rom:/RDVHASH is not a valid file, writes a warning to console and returns without modifying strings.
 * If rom:/RDVHASH has a string that is too long, writes a warning to console and returns without modifying strings.
 *
 * The following strings are changed:
 *
 *  - "profile0" -> "hash_0"
 *  - "profile1" -> "hash_1"
 *  - "profile2" -> "hash_2" */
void setSeedSaveProfile()
{
    char *seedHash = (char*)odr::romfs::OpenAndReadFile("rom:/RDVHASH");
    if (seedHash == nullptr) {
        const char* msg = "[LogWarn/0] rom:/RDVHASH does not exist!";
        svcOutputDebugString(msg, strlen(msg));
        return;
    }

    int len = strlen(seedHash);
    if (len > 253)
    {
        const char* msg = "[LogWarn/0] requested slot name in rom:/RDVHASH must be shorter than 254 characters!";
        svcOutputDebugString(msg, strlen(msg));
        return;
    }

    if (len > 0)
    {
        char slotName[256];
        u64 crc;
        len += 2;

        sprintf(slotName, "%s_0", seedHash);
        crc = crc64(slotName, len);
        create_string_instance(&g_stringBank[odr::romfs::STRINGBANK_PROFILE0], slotName, len, crc, true);

        slotName[len-1] = '1';
        crc = crc64(slotName, len);
        create_string_instance(&g_stringBank[odr::romfs::STRINGBANK_PROFILE0+1], slotName, len, crc, true);

        slotName[len-1] = '2';
        crc = crc64(slotName, len);
        create_string_instance(&g_stringBank[odr::romfs::STRINGBANK_PROFILE0+2], slotName, len, crc, true);
    }
    free(seedHash);
    return;
}

HOOK_DEFINE_TRAMPOLINE(ForceRomfs) {
    /* Define the callback for when the function is called. Don't forget to make it static and name it Callback. */
    static void Callback(void *CFilePathStrIdOut, const char *path, u8 flags) {

        /* Just in case the path is nullptr, pass it down to the real implementation, since we don't support replacing null paths anyway. */
        if(path == nullptr)
        {
            Orig(CFilePathStrIdOut, path, flags);
            return;
        }

        /* Replace string if we have it in our list before passing to the real implementation. */
        replaceString(&path);
        Orig(CFilePathStrIdOut, path, flags);
    }
};

/* Hook romfs mounting. Pass the arguments down to the real implementation so romfs is mounted as normal. */
/* Once romfs is mounted, we can read files from it in order to populate our string replacement list. */
HOOK_DEFINE_TRAMPOLINE(RomMounted) {
    static Result Callback(char const *path, void *romCache, unsigned long cacheSize) {
        Result res = Orig(path, romCache, cacheSize);
        populateStringReplacementList();
        setSeedSaveProfile();
        return res;
    }
};

void odr::romfs::InstallHooks() {
    const Offsets& offsets = OFFSETS.Get();
    RomMounted::InstallAtFuncPtr(nn::fs::MountRom);
    ForceRomfs::InstallAtOffset(offsets.CFilePathStrIdCtor);
    create_string_instance = odr::game::Resolve<CreateStringInstanceFn>(offsets.FindOrCreateStringInstance);
    g_stringBank = odr::game::Resolve<CStrId*>(offsets.StaticStringBank);
}