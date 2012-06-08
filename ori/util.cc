/*
 * Copyright (c) 2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>

#include <openssl/sha.h>

#ifdef OPENSSL_NO_SHA256
#error "SHA256 not supported!"
#endif

#include "util.h"

using namespace std;

/*
 * Check if a file exists.
 */
bool
Util_FileExists(const string &path)
{
    struct stat sb;

    if (stat(path.c_str(), &sb) == 0)
	return true;

    return false;
}

/*
 * Check if a file is a directory.
 */
bool
Util_IsDirectory(const string &path)
{
    struct stat sb;

    if (stat(path.c_str(), &sb) < 0) {
	perror("stat file does not exist");
	return false;
    }

    if (sb.st_mode & S_IFDIR)
	return true;
    else
	return false;
}

/*
 * Read a file containing a text string into memory. There may not be any null 
 * characters in the file.
 */
char *
Util_ReadFile(const string &path, size_t *flen)
{
    FILE *f;
    char *buf;
    size_t len;

    f = fopen(path.c_str(), "rb");
    if (f == NULL) {
	return NULL;
    }

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = new char[len + 1];

    fread(buf, 1, len, f);
    fclose(f);

    // Just for simplicity we add a null charecter at the end to allow us to 
    // read and write strings easily.
    buf[len] = '\0';

    if (flen != NULL)
	*flen = len;

    return buf;
}

/*
 * Write an in memory blob to a file.
 */
bool
Util_WriteFile(const char *blob, size_t len, const string &path)
{
    FILE *f;
    size_t bytesWritten;

    f = fopen(path.c_str(), "w+");
    if (f == NULL) {
	return false;
    }

    bytesWritten = fwrite(blob, len, 1, f);
    fclose(f);

    return (bytesWritten == len);
}

#define COPYFILE_BUFSZ	4096

/*
 * Copy a file.
 */
int
Util_CopyFile(const string &origPath, const string &newPath)
{
    int srcFd, dstFd;
    char buf[COPYFILE_BUFSZ];
    struct stat sb;
    int64_t bytesLeft;
    int64_t bytesRead, bytesWritten;

    srcFd = open(origPath.c_str(), O_RDONLY);
    if (srcFd < 0)
	return -errno;

    dstFd = open(newPath.c_str(), O_WRONLY | O_CREAT,
	         S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (dstFd < 0) {
	close(srcFd);
	return -errno;
    }

    if (fstat(srcFd, &sb) < 0) {
	close(srcFd);
	unlink(newPath.c_str());
	close(dstFd);
	return -errno;
    }

    bytesLeft = sb.st_size;
    while(bytesLeft > 0) {
	bytesRead = read(srcFd, buf, MIN(bytesLeft, COPYFILE_BUFSZ));
	if (bytesRead < 0) {
	    if (errno == EINTR)
		continue;
	    goto error;
	}

retryWrite:
	bytesWritten = write(dstFd, buf, bytesRead);
	if (bytesWritten < 0) {
	    if (errno == EINTR)
		goto retryWrite;
	    goto error;
	}

	// XXX: Need to handle this case!
	assert(bytesRead == bytesWritten);

	bytesLeft -= bytesRead;
    }

    close(srcFd);
    close(dstFd);
    return sb.st_size;

error:
    close(srcFd);
    unlink(newPath.c_str());
    close(dstFd);
    return -errno;
}

/*
 * Safely move a file possibly between file systems.
 */
int
Util_MoveFile(const string &origPath, const string &newPath)
{
    int status = 0;

    if (rename(origPath.c_str(), newPath.c_str()) < 0)
	status = -errno;

    // If the file is on seperate file systems copy it and delete the original.
    if (errno == EXDEV) {
	status = Util_CopyFile(origPath, newPath);
	if (status < 0)
	    return status;

	if (unlink(origPath.c_str()) < 0) {
	    status = -errno;
	    assert(false);
	}
    }

    return status;
}

/*
 * Compute SHA 256 hash for a string.
 */
string
Util_HashString(const string &str)
{
    SHA256_CTX state;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    stringstream rval;

    SHA256_Init(&state);
    SHA256_Update(&state, str.c_str(), str.size());
    SHA256_Final(hash, &state);

    // Convert into string.
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
	rval << hex << setw(2) << setfill('0') << (int)hash[i];
    }

    return rval.str();
}

#define HASHFILE_BUFSZ	4096

/*
 * Compute SHA 256 hash for a file.
 */
string
Util_HashFile(const string &path)
{
    int fd;
    char buf[COPYFILE_BUFSZ];
    struct stat sb;
    int64_t bytesLeft;
    int64_t bytesRead;
    SHA256_CTX state;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    stringstream rval;

    SHA256_Init(&state);

    fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
	return "";
    }

    if (fstat(fd, &sb) < 0) {
	close(fd);
	return "";
    }

    bytesLeft = sb.st_size;
    while(bytesLeft > 0) {
	bytesRead = read(fd, buf, MIN(bytesLeft, COPYFILE_BUFSZ));
	if (bytesRead < 0) {
	    if (errno == EINTR)
		continue;
	    close(fd);
	    return "";
	}

	SHA256_Update(&state, buf, bytesRead);
	bytesLeft -= bytesRead;
    }

    SHA256_Final(hash, &state);

    // Convert into string.
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
	rval << hex << setw(2) << setfill('0') << (int)hash[i];
    }

    close(fd);
    return rval.str();
}

vector<string>
Util_PathToVector(const string &path)
{
    int pos = 0;
    vector<string> rval;

    if (path[0] == '/')
        pos = 1;

    while (pos < path.length()) {
        int end = path.find('/', pos);
        if (end == path.npos) {
            rval.push_back(path.substr(pos));
            break;
        }

        rval.push_back(path.substr(pos, end - 1));
        pos = end + 1;
    }

    return rval;
}

// XXX: Debug Only

#define TESTFILE_SIZE (HASHFILE_BUFSZ * 3 / 2)

int
util_selftest(void)
{
    int status;
    char *buf = new char[TESTFILE_SIZE + 1];
    string origHash;
    string newHash;

    for (int i = 0; i < TESTFILE_SIZE; i++) {
	buf[i] = '0' + (i % 10);
    }
    buf[TESTFILE_SIZE] = '\0';
    origHash = Util_HashString(buf);

    Util_WriteFile(buf, TESTFILE_SIZE, "test.orig");

    status = Util_CopyFile("test.orig", "test.a");
    if (status < 0) {
	printf("Util_CopyFile: %s\n", strerror(-status));
	assert(false);
    }
    status = Util_CopyFile("test.orig", "test.b");
    if (status < 0) {
	printf("Util_CopyFile: %s\n", strerror(-status));
	assert(false);
    }
    status = Util_MoveFile("test.b", "test.c");
    if (status < 0) {
	printf("Util_CopyFile: %s\n", strerror(-status));
	assert(false);
    }
    char *fbuf = Util_ReadFile("test.c", NULL);
    assert(fbuf);
    newHash = Util_HashString(fbuf);
    // XXX: Check that 'test.b' does not exist

    if (newHash != origHash) {
	printf("Hash mismatch!\n");
	assert(false);
    }

    newHash = Util_HashFile("test.a");

    if (newHash != origHash) {
	printf("Hash mismatch!\n");
	assert(false);
    }

    vector<string> tv;
    tv = Util_PathToVector("/abc/def");
    assert(tv.size() == 2);
    assert(tv[0] == "abc");
    assert(tv[1] == "def");

    printf("Test Succeeded!\n");
    return 0;
}

