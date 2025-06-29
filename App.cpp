/*
 * mime - MIME Import Manipulation & Export
 * a simple MIME type handling tool for SEN.
 *
 * Copyright 2024, Gregor Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <errno.h>
#include <kernel/fs_index.h>
#include <MimeType.h>
#include <Path.h>
#include <Resources.h>
#include <stdio.h>
#include <Volume.h>
#include <VolumeRoster.h>

status_t InstallMimeTypeFromResource(const char* path);
status_t DeleteMimeType(const char* mimeType);
status_t GetInstalledMimeTypes(const char* supertype, BMessage* types);
void PrintUsage(const char* name);

#define ATTR_INDEX "attr:searchable"

int
main(int argc, char** argv)
{
    if (argc == 1) {
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
    }
    status_t result;
    const char* command = argv[1];
    if (strncmp(command, "install", strlen("install")) == 0) {
        result = InstallMimeTypeFromResource(argv[2]);
        if (result != B_OK) {
            fprintf(stderr, "failed to install MIME type %s: %s\n", argv[2], strerror(result));
        } else {
            printf("successfully installed MIME type %s.\n", argv[2]);
        }
    }
    else if (strncmp(command, "uninstall", strlen("uninstall")) == 0) {
        result = DeleteMimeType(argv[2]);
        if (result != B_OK) {
            fprintf(stderr, "failed to uninstall MIME type %s: %s\n", argv[2], strerror(result));
        } else {
            printf("successfully uninstalled MIME type %s.\n", argv[2]);
        }
    }
    else if (strncmp(command, "list", strlen("list")) == 0) {
        BMessage entityTypes;
        result = GetInstalledMimeTypes("entity", &entityTypes);
        if (result != B_OK) {
            fprintf(stderr, "failed to query MIME type DB: %s\n", strerror(result));
        }
        printf("installed entities:\n");
        entityTypes.PrintToStream();

        BMessage relationTypes;
        result = GetInstalledMimeTypes("relation", &relationTypes);
        if (result != B_OK) {
            fprintf(stderr, "failed to query MIME type DB: %s\n", strerror(result));
        }
        printf("installed relations:\n");
        relationTypes.PrintToStream();
    }
    else {
        fprintf(stderr, "unknown command %s\n", command);
        return EXIT_FAILURE;
    }

	return result == B_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

void PrintUsage(const char* progname) {
    BPath path(progname);

    printf("Usage: %s <operation> [mime-type]\n", path.Leaf());
    printf("where operation is one of:\n\n");
    printf("install     installs MIME type in MIME db\n");
    printf("uninstall   uninstalls MIME type from MIME db\n");
    printf("list        lists entities and relations in MIME db\n");

    return;
}

status_t InstallMimeTypeFromResource(const char* path) {
    BResources resources(path);
    if (! BFile(path, B_READ_ONLY).IsReadable()) {
        fprintf(stderr, "cannot read resources from path %s: check path is valid!\n", path);
        return B_ERROR;
    }

    status_t result = resources.InitCheck();
    if (result != B_OK) {
        fprintf(stderr, "error initializing resources from path %s: %s\n", path, strerror(result));
        return result;
    }

    BMimeType mimeType;
    BMessage message;
    const void *type, *sDesc, *lDesc, *attrInfo, *extens, *snifferRule, *prefApp, *icon;
    size_t* size = new size_t;

    // get Type
    type = resources.LoadResource(B_STRING_TYPE, "META:TYPE", size);
    if (type == NULL) {
        return B_ERROR;
    }

    const char* mime = reinterpret_cast<const char*>(type);
    mimeType.SetTo(mime);
    if (!mimeType.IsValid()) {
        fprintf(stderr, "invalid MIME type '%s' in resource %s: %s\n", mime, path, strerror(result));
        return result;
    }
    if (mimeType.IsInstalled()) {
        printf("MIME type %s is already installed, updating...\n", mime);
    } else {
        // we need to install as first step, since all other MimeType operations act on the MIME DB directly:(
        result = mimeType.Install();
        if (result != B_OK) {
            fprintf(stderr, "error installing MIME type %s from resource %s: %s\n", mime, path, strerror(result));
            return result;
        }
    }

    // get short description (used as type name in prefs)
    sDesc = resources.LoadResource('MSDC', "META:S:DESC", size);
    if (sDesc == NULL) {
        return B_ERROR;
    }
    mimeType.SetShortDescription(reinterpret_cast<const char*>(sDesc));

    // get long description (optional)
    lDesc = resources.LoadResource('MLDC', "META:L:DESC", size);
    if (lDesc != NULL) {
        mimeType.SetLongDescription(reinterpret_cast<const char*>(lDesc));
    }

    // get preferred app
    prefApp = resources.LoadResource('MSIG', "META:PREF_APP", size);
    if (prefApp != NULL) {
        mimeType.SetPreferredApp(reinterpret_cast<const char*>(prefApp));
    }

    // get sniffer rule
    snifferRule = resources.LoadResource(B_STRING_TYPE, "META:SNIFF_RULE", size);
    if (snifferRule != NULL) {
        mimeType.SetSnifferRule(reinterpret_cast<const char*>(snifferRule));
    }

    // get extensions
    extens = resources.LoadResource(B_MESSAGE_TYPE, "META:EXTENS", size);
    if (extens != NULL && message.Unflatten(reinterpret_cast<const char*>(extens)) == B_OK) {
        mimeType.SetFileExtensions(&message);
    }

    // get attribute info
    attrInfo = resources.LoadResource(B_MESSAGE_TYPE, "META:ATTR_INFO", size);
    if (attrInfo != NULL && message.Unflatten(reinterpret_cast<const char*>(attrInfo)) == B_OK) {
        mimeType.SetAttrInfo(&message);

        // check if attribute should be added to index
        int32 indexAttrCount;
        message.GetInfo(ATTR_INDEX, NULL, &indexAttrCount);
        BVolume bootVolume; // FIXME
        BVolumeRoster().GetBootVolume(&bootVolume);

        for (int i = 0; i < indexAttrCount; i++) {
            const char* attrName = message.GetString("attr:name", i, "");
            const char* attrPublicName = message.GetString("attr:public_name", i, "");
            uint32      attrType = message.GetUInt32("attr:type", i, B_STRING_TYPE);

            result = message.FindBool("attr:searchable", i);
            if (result == B_OK) {
                bool addToIndex = message.GetBool("attr:searchable", i);
                int result = 0;
                if (addToIndex) {
                    // add to index
                    printf("* adding attribute %s ['%s'] to index...", attrPublicName, attrName);
                    result = fs_create_index(bootVolume.Device(), attrName, attrType, 0);
                } else {
                    printf("* removing attribute %s ['%s'] from index...", attrPublicName, attrName);
                    result = fs_remove_index(bootVolume.Device(), attrName);
                }

                if (result == B_OK) {
                    printf("OK\n");
                } else {
                    if (errno == B_FILE_EXISTS) {
                        printf("EXISTS, skipping.\n");
                    } else if (errno == B_ENTRY_NOT_FOUND) {
                        printf("NOT FOUND, skipping.\n");
                    } else {
                        printf("ERROR: %s\n", strerror(errno));
                    }
                }
            }
        }
    }

    // get icon
    icon = resources.LoadResource(B_VECTOR_ICON_TYPE, "META:ICON", size);
    if (icon != NULL && *size > 0) {
        mimeType.SetIcon(reinterpret_cast<const uint8*>(icon), *size);
    }

    return B_OK;
}

status_t DeleteMimeType(const char* type) {
    BMimeType mimeType(type);
    status_t result;

    if (!mimeType.IsValid()) {
        fprintf(stderr, "%s is not a valid MIME type.\n", type);
        return result;
    }
    if (!mimeType.IsInstalled()) {
        printf("MIME type %s is not installed, skipping...", type);
    }

    return mimeType.Delete();
}

status_t
GetInstalledMimeTypes(const char* supertype, BMessage* types)
{
	return BMimeType::GetInstalledTypes(supertype, types);
}
