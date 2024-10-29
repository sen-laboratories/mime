/*
 * mime - MIME Import Manipulation & Export
 * a simple MIME type handling tool for SEN.
 *
 * Copyright 2024, Gregor Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <MimeType.h>
#include <Roster.h>
#include <stdio.h>
//#include <RegistrarDefs.h>
#include "include/RosterPrivate.h"

status_t InstallType(const char* mimeType);
status_t DeleteType(const char* mimeType);
status_t GetInstalledTypes(const char* supertype, BMessage* types);
status_t GetMimeType(const char* name, BMimeType* mimeType);
void PrintUsage(const char* name);

#define B_REG_MIME_INSTALL              'rgin'
#define B_REG_MIME_DELETE               'rgdl'
#define B_REG_MIME_GET_INSTALLED_TYPES  'rgit'
#define B_REG_RESULT                    'rgrz'

using namespace BPrivate;
using namespace BPrivate::Storage::Mime;

int
main(int argc, char** argv)
{
    if (argc == 1) {
        PrintUsage(argv[0]);
        return 1;
    }
    status_t result;
    const char* command = argv[1];
    if (strncmp(command, "install", strlen("install")) == 0) {
        BMimeType mimeType;
        result = GetMimeType(argv[2], &mimeType);
        result = InstallType(mimeType.Type());
        if (result != B_OK) {
            fprintf(stderr, "failed to install MIME type %s: %s\n", argv[1], strerror(result));
        }
    }
    else if (strncmp(command, "delete", strlen("delete")) == 0) {
        BMimeType mimeType;
        result = GetMimeType(argv[2], &mimeType);
        result = DeleteType(mimeType.Type());
        if (result != B_OK) {
            fprintf(stderr, "failed to delete MIME type %s: %s\n", argv[1], strerror(result));
        }
    }
    else if (strncmp(command, "list", strlen("list")) == 0) {
        BMessage entityTypes;
        result = GetInstalledTypes("entity", &entityTypes);
        if (result != B_OK) {
            fprintf(stderr, "failed to query MIME type DB: %s\n", strerror(result));
        }
        printf("installed entities:\n");
        entityTypes.PrintToStream();
        BMessage relationTypes;
        result = GetInstalledTypes("relation", &relationTypes);
        if (result != B_OK) {
            fprintf(stderr, "failed to query MIME type DB: %s\n", strerror(result));
        }
        printf("installed relations:\n");
        relationTypes.PrintToStream();
    }
    else {
        fprintf(stderr, "unknown command %s\n", command);
        return 1;
    }

	return 0;
}

void PrintUsage(const char* progname) {
    printf("Usage: %s <operation> [mime-type]\n", progname);
    printf("where operation is one of:\n\n");
    printf("install     installs MIME type in MIME db\n");
    printf("uninstall   uninstalls MIME type from MIME db\n");
    return;
}

status_t GetMimeType(const char* name, BMimeType* mimeType) {
    status_t result;
    mimeType->SetTo(name);

    if (! mimeType->IsValid()) {
        fprintf(stderr, "%s is not a valid MIME type!\n", name);
    }
    if ((result = mimeType->InitCheck()) != B_OK) {
        fprintf(stderr, "error initializing MIME type %s: %s\n", name, strerror(result));
    }
    return result;
}

// Adds the MIME type to the MIME database
status_t InstallType(const char* mimeType)
{
	BMessage message(B_REG_MIME_INSTALL);
	BMessage reply;
	status_t err, result;

	// Build and send the message, read the reply
	if (err == B_OK)
		err = message.AddString("type", mimeType);

	if (err == B_OK)
		err = BRoster::Private().SendTo(&message, &reply, true);

	if (err == B_OK)
		err = (status_t)(reply.what == B_REG_RESULT ? B_OK : B_BAD_REPLY);

	if (err == B_OK)
		err = reply.FindInt32("result", &result);

	if (err == B_OK)
		err = result;

	return err;
}


// Removes the MIME type from the MIME database
status_t DeleteType(const char* mimeType)
{
	BMessage message(B_REG_MIME_DELETE);
	BMessage reply;
	status_t err, result;

	// Build and send the message, read the reply
	if (err == B_OK)
		err = message.AddString("type", mimeType);

	if (err == B_OK)
		err = BRoster::Private().SendTo(&message, &reply, true);

	if (err == B_OK)
		err = (status_t)(reply.what == B_REG_RESULT ? B_OK : B_BAD_REPLY);

	if (err == B_OK)
		err = reply.FindInt32("result", &result);

	if (err == B_OK)
		err = result;

	return err;
}

// Fetches a BMessage listing all the MIME subtypes of the given
// supertype currently installed in the MIME database.
/*static*/ status_t
GetInstalledTypes(const char* supertype, BMessage* types)
{
	if (types == NULL)
		return B_BAD_VALUE;

	status_t result;

	// Build and send the message, read the reply
	BMessage message(B_REG_MIME_GET_INSTALLED_TYPES);
	status_t err = B_OK;

	if (supertype != NULL)
		err = message.AddString("supertype", supertype);
	if (err == B_OK)
		err = BRoster::Private().SendTo(&message, types, true);
	if (err == B_OK)
		err = (status_t)(types->what == B_REG_RESULT ? B_OK : B_BAD_REPLY);
	if (err == B_OK)
		err = types->FindInt32("result", &result);
	if (err == B_OK)
		err = result;

	return err;
}
