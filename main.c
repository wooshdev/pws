/**
 * Copyright (C) 2020 Tristan <tristan@thewoosh.me>
 *
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * This software is meant as a very barebones, minimalist HTTP/1.1 webserver,
 * hence the name. Because of this, I wont implement features such as
 * authentication, compression, any form of caching, HTTPS, etc. If you need
 * any of these features, use some other webserver solution, such as thttpd,
 * nginx, etc.
 */

/* Includes: First STL */
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Includes: Then POSIX */
#include <fcntl.h>
#include <fts.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "validator.c"

#define CNRM  "\x1B[0m"
#define CRED  "\x1B[31m"
#define CGRN  "\x1B[32m"
#define CYEL  "\x1B[33m"
#define CBLU  "\x1B[34m"
#define CMAG  "\x1B[35m"
#define CCYN  "\x1B[36m"
#define CWHT  "\x1B[37m"
#define CITA  "\x1B[3m"

#define HTTPDATA_SZMETHOD 32
#define HTTPDATA_SZPATH 8192
#define HTTPDATA_SZVERSION 9

struct HTTPData {
	char *method;
	char *path;
	char *version;
	char *fileName;
};

struct Thread {
	pthread_t thread;
	int socket;
	int alive;
	struct HTTPData httpData;
};

struct Thread *threads;
pthread_mutex_t threadsMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_attr_t threadsAttributes;

/* State */
int sockfd = 0;
int keepRunning = 1;
int logRequests = 0;
int createTestIndexHtmlFile = 1;
char *filesDirectory;

#define TIME_FORMAT "%a, %d %h %Y %T %z"
static char *FormatDate(time_t the_time) {
	char *value = calloc(128, sizeof(char));
	if (!value) return NULL;
	size_t len = strftime(value, 128, TIME_FORMAT, localtime(&the_time));
	return realloc(value, len + 1);
}

void CatchSignal(int signo, siginfo_t *info, void *context) {
	if (signo == SIGINT && sockfd > 0) {
		keepRunning = 0;
		close(sockfd);
		sockfd = 0;
	}
}

char *ConstructFileName(const char *path) {
	size_t filesDirectoryLength = strlen(filesDirectory);
	if (filesDirectoryLength == 0)
		return NULL;

	DIR *dir = opendir(filesDirectory);
	if (!filesDirectory)
		return NULL;
	closedir(dir);

	char removeSlash = filesDirectory[filesDirectoryLength-1] == '/';
	char *fileName = malloc(filesDirectoryLength - (removeSlash ? 1 : 0) + strlen(path) + 1);

	memcpy(fileName, filesDirectory, filesDirectoryLength);
	strcpy(fileName + filesDirectoryLength - removeSlash, path);
	return fileName;
}

char *ConstructIndexHtml(const char *directory) {
	const char *path = "/index.html";

	size_t directoryLength = strlen(directory);
	char removeSlash = directory[directoryLength-1] == '/';
	char *fileName = malloc(directoryLength - (removeSlash ? 1 : 0) + strlen(path) + 1);

	memcpy(fileName, directory, directoryLength);
	strcpy(fileName + directoryLength - removeSlash, path);
	return fileName;
}

void FormatFileSize(size_t size, float *rsize, const char **runit) {
	const char *names[] = { "bytes", "kB", "MB", "GB", "TB", "PT", "EB", "ZB", "YB" };
	size_t pos = rsize == 0 ? 0 : (size_t) (log(size) / log(1000));

	if (pos > sizeof(names)/sizeof(names[0]))
		pos = 8; /* set to last. */

	*rsize = ((size_t)(size / pow(1000, pos) * 10)) / 10.0;
	*runit = names[pos];
}

char *ConstructDirectoryIndex(const char *directory, const char *path) {
	DIR *dir = opendir(directory);
	if (!dir)
		return NULL;
	size_t contentSize = 4096;
	const char *contentPrefix = "<!doctype><html><head><title>Index of %s</title><style>th{text-align:left}</style></head><body><header><h1>Index of %s</h1><em>This directory didn't have an index.html file, so this page is shown.</em></header><table><tbody><tr><th>File Name</th><th>File Size</th></tr>";
	const char *contentSuffix = "</tbody></table></body></html>";
	char *content = malloc(contentSize);
	size_t pos = snprintf(content, 4096, contentPrefix, path, path);
	content = realloc(content, 4096 + pos);

	char filename[512];
	struct dirent *dp;
	struct stat stbuf;
	while ((dp = readdir(dir)) != NULL) {
		snprintf(filename, 512, "%s/%s", directory, dp->d_name);
		if (stat(filename, &stbuf) == -1) {
			printf("UNABLE TO STAT FILE: %s\n", filename);
			continue;
		}

		float fileSize;
		const char *fileSizeUnit;
		FormatFileSize(stbuf.st_size, &fileSize, &fileSizeUnit);
		pos += snprintf(content+pos, 4096, "<tr><td><a href=\"%s%s\">%s</a></td><td>%.1f %s</td></tr>", dp->d_name, S_ISDIR(stbuf.st_mode) ? "/" : "", dp->d_name, fileSize, fileSizeUnit);
		content = realloc(content, 4096 + pos);
	}
	size_t suffixLength = strlen(contentSuffix);
	memcpy(content+pos, contentSuffix, suffixLength+1);
	pos += suffixLength;

	closedir(dir);
	return content;
}

/* Create the index.html file. */
void TryCreateIndexFile(void) {
	char *fileName = ConstructFileName("/index.html");

	FILE *file = fopen(fileName, "r");
	if (file) {
		fclose(file);
		free(fileName);
		return;
	}

	file = fopen(fileName, "wb");
	fputs(HTMLDefaultTestPage, file);
	fclose(file);
	free(fileName);
}

void PrintUsage(int argc, char **argv) {
	fprintf(stderr, CYEL"Usage: %s [-d directory] [-h] [-l] [-p port] [-t threads] [-u] [-w]\n"CNRM, argv[0]);
}

void CleanUpClient(struct Thread *currentThread) {
	if (currentThread->socket != 0) {
		shutdown(currentThread->socket, SHUT_WR);
		close(currentThread->socket);
		currentThread->socket = 0;
	}

	/* Clear HTTP Data (These pointers are malloc'ed) */
	free(currentThread->httpData.method);
	free(currentThread->httpData.path);
	free(currentThread->httpData.version);
	free(currentThread->httpData.fileName);
	currentThread->httpData.method = NULL;
	currentThread->httpData.path = NULL;
	currentThread->httpData.version = NULL;
	currentThread->httpData.fileName = NULL;

	/* Make thread free */
	pthread_mutex_lock(&threadsMutex);
	if (threads) {
		currentThread->alive = 0;
	}
	pthread_mutex_unlock(&threadsMutex);
}

/* Read from the filedescriptor until the 'match' character is found.
 * All the characters read (excluding the 'match' character) will be
 * stored in 'dest'. On success, 1 is returned.
 *
 * This function will NULL-terminate 'dest'.
 *
 * When an I/O error occurs, -1 will be returned. When the amount of
 * characters read plus the NULL-terminator is greater than 'max', -2
 * will be returned.
 *
 * 'dest' should be a valid address and max should be greater than 0.
 */
int ReadUntil(int fd, char *dest, size_t max, char match) {
	size_t pos = 0;

	do {
		int status = read(fd, dest+pos, 1);
		if (status != 1) {
			dest[pos] = 0;
			if (logWarnings)
				printf(CYEL"I/O Warning: (ReadUntil) read() error: errno=%i\n"CNRM, errno);
			return -1;
		}

		if (dest[pos] == match) {
			dest[pos] = 0;
			return 1;
		}
	} while(++pos != max);

	dest[pos-1] = 0;
	return -2;
}

/**
 * Sends HTTP response metadata: response-line and headers.
 * additionalHeaders can be NULL.
 */
void SendResponseMetadata(int fd, const char *status, const char *additionalHeaders, size_t bodyLength) {
	size_t formatChars = 11;
	const char *responseFmt = "HTTP/1.1 %s\r\nContent-Length: %zu\r\nConnection: close\r\nDate: %s\r\nServer: %s\r\n\r\n";
	/* TODO Check for NULL ?*/
	char *date = FormatDate(time(NULL));
	size_t length =
		strlen(responseFmt) - formatChars +
		strlen(status) +
		32 +
		strlen(date) +
		strlen(serverName) +
		(additionalHeaders == NULL ? 0 : strlen(additionalHeaders));
	char *response = malloc(length);
	int writtenOctets = snprintf(response, length, responseFmt, status, bodyLength, date, serverName);
	write(fd, response, writtenOctets);
	free(date);
	free(response);
}

void *HandleClient(void *data) {
	struct Thread *currentThread = (struct Thread *) data;

	currentThread->httpData.method  = malloc(HTTPDATA_SZMETHOD);
	currentThread->httpData.path    = malloc(HTTPDATA_SZPATH);
	currentThread->httpData.version = malloc(HTTPDATA_SZVERSION);

	if (!currentThread->httpData.method ||
		!currentThread->httpData.version ||
		!currentThread->httpData.path
	) {
		if (logWarnings)
			fputs("Warning: Failed to allocate start-line HTTP data.\n", stderr);
		CleanUpClient(currentThread);
		return NULL;
	}

	currentThread->httpData.method[0] = 0;
	currentThread->httpData.path[0] = 0;
	currentThread->httpData.version[0] = 0;

	int status;
	status = ReadUntil(currentThread->socket, currentThread->httpData.method, HTTPDATA_SZMETHOD, ' ');
	if (status < 0) {
		if (logWarnings)
			fprintf(stderr, CYEL"\tWarning: Read method error: %i (\"%s\")\n"CNRM, status, currentThread->httpData.method);
		CleanUpClient(currentThread);
		return NULL;
	}

	const char *message;
	message = ValidateMethod(currentThread->httpData.method);
	if (message != NULL) {
		if (logWarnings)
			printf("Warning: Invalid method: %s (\"%s\")\n", message, currentThread->httpData.method);
		CleanUpClient(currentThread);
		return NULL;
	}

	status = ReadUntil(currentThread->socket, currentThread->httpData.path, HTTPDATA_SZPATH, ' ');
	if (status < 0) {
		if (logWarnings)
			fprintf(stderr, CYEL"Warning: Read path error: %i (\"%s\")\n"CNRM, status, currentThread->httpData.path);
		CleanUpClient(currentThread);
		return NULL;
	}

	status = ReadUntil(currentThread->socket, currentThread->httpData.version, HTTPDATA_SZVERSION, '\r');
	if (status < 0) {
		if (logWarnings)
			fprintf(stderr, CYEL"Warning: Read version error: %i (\"%s\")\n"CNRM, status, currentThread->httpData.method);
		CleanUpClient(currentThread);
		return NULL;
	}

	if (strcmp(currentThread->httpData.version, "HTTP/1.1") != 0) {
		if (logWarnings)
			printf(CYEL"Warning: Invalid/unknown version: \"%s\""CNRM"\n", currentThread->httpData.version);
		size_t formatChars = 7;
		const char *responseFmt = "HTTP/1.1 505 Version Not Supported\r\nContent-Length: %zu\r\nConnection: close\r\nDate: %s\r\nServer: %s\r\n\r\n";
		const char *content = "<h1>HTTP Version not supported</h1>";
		/* TODO Check for NULL ?*/
		char *date = FormatDate(time(NULL));
		size_t length =
			strlen(responseFmt) - formatChars +
			32 +
			strlen(serverName) +
			strlen(content)
			+ 1;
		char *response = malloc(length);
		int writtenOctets = snprintf(response, length, responseFmt, strlen(content), date, serverName);
		if (writtenOctets != length) {
			/* TODO: if snprintf writes more characters than we expect, we get a buffer
			 * overflow error. */
			memcpy(response + writtenOctets, content, strlen(content));
		}
		status = write(currentThread->socket, response, length);
		free(date);
		free(response);
		CleanUpClient(currentThread);
		return NULL;
	}

	if (logRequests)
		printf(CCYN "Request> " CMAG "%s" CNRM "\n", currentThread->httpData.path);

	currentThread->httpData.fileName = ConstructFileName(currentThread->httpData.path);
	int f = open(currentThread->httpData.fileName, O_RDONLY);

	if (f == -1) {
		if (logWarnings) {
			printf("Warning: '%s' wasn't found\n", currentThread->httpData.fileName);
			perror("-> Message");
		}
		size_t size = strlen(HTMLNotFoundPage);
		SendResponseMetadata(currentThread->socket, "404 Not Found", NULL, size);
		write(currentThread->socket, HTMLNotFoundPage, size);
		CleanUpClient(currentThread);
		return NULL;
	}

	struct stat fstat_info;
	repeatLabel:
	if (fstat(f, &fstat_info) == -1) {
		printf("f=%i\n", f);
		perror("Failed to fstat()");
		if (f != -1)
			close(f);
		CleanUpClient(currentThread);
		return NULL;
	}

	if (S_ISDIR(fstat_info.st_mode)) {
		if (f != -1)
			close(f);
		char *index = ConstructIndexHtml(currentThread->httpData.fileName);

		f = open(index, O_RDONLY);
		if (f == -1) {
			free(index);
			char *message = ConstructDirectoryIndex(currentThread->httpData.fileName, currentThread->httpData.path);
			if (!message) {
				if (logWarnings)
					fputs("WARNING: ConstructDirectoryIndex was null!\n", stderr);
				CleanUpClient(currentThread);
				return NULL;
			}

			size_t size = strlen(message);
			SendResponseMetadata(currentThread->socket, "200 OK", NULL, size);
			write(currentThread->socket, message, size);
			free(message);
			CleanUpClient(currentThread);
			return NULL;
		}

		free(currentThread->httpData.fileName);
		currentThread->httpData.fileName = index;
		/* TODO Can this cause an infinite loop when 'index.html' is a folder? */
		goto repeatLabel;
	}

	size_t fileSize = (size_t) fstat_info.st_size;

	SendResponseMetadata(currentThread->socket, "200 OK", NULL, fileSize);

	size_t pos = 0;
	ssize_t octetsJustRead;
	const size_t chunkSize = 4096;
	char *buffer = malloc(chunkSize);

	do {
		octetsJustRead = read(f, buffer, chunkSize);

		if (octetsJustRead == 0) {
			break;
		}

		if (octetsJustRead == -1) {
			break;
		}

		write(currentThread->socket, buffer, octetsJustRead);
	} while (pos += octetsJustRead < fileSize);

	free(buffer);
	if (f != -1)
		close(f);

	/** Cleaning up... **/
	CleanUpClient(currentThread);
	return NULL;
}

int main(int argc, char **argv) {
	/* Option parsing */
	int opt, parsedInteger;
	filesDirectory = strdup("/var/www/html/");
	while ((opt = getopt(argc, argv, "d:hlp:t:uw")) != -1) {
		switch (opt) {
			case 'h':
				PrintUsage(argc, argv);
				puts(CCYN"\nOptions:\n"
					CMAG"    -d "CITA CCYN"<directory>"CNRM"  Set the directory of the files. Default: '/var/www/html/'\n"
					CMAG"    -h              " CNRM "Print this message.\n"
					CMAG"    -l              " CNRM "Enable request logging.\n"
					CMAG"    -p "CITA CCYN"<port>"CNRM"       Set port. Default: 80\n"
					CMAG"    -t "CITA CCYN"<threads>"CNRM"    Maximum child threads.\n"
					CMAG"    -u              " CNRM "Don't create the default index.html test page.\n"
					CMAG"    -w              " CNRM "Disable all warnings. This doesn't disable errors."CNRM);
				return EXIT_SUCCESS;
				break;
			case 'd':
				free(filesDirectory);
				filesDirectory = strdup(optarg);
				break;
			case 'l':
				logRequests = 1;
				break;
			case 'p':
				parsedInteger = strtol(optarg, NULL, 10);
				if (parsedInteger <= 0 || parsedInteger > 65535) {
					fprintf(stderr, CRED"Invalid port: '%s' The value should be greater than 0 and smaller than 65536!\n"CNRM,  optarg);
					PrintUsage(argc, argv);
					return EXIT_FAILURE;
				}
				port = (uint16_t) parsedInteger;
				break;
			case 't':
				parsedInteger = strtol(optarg, NULL, 10);
				if (parsedInteger <= 0 || parsedInteger > 65535) {
					fprintf(stderr, CRED"Invalid maximum thread count: '%s' The value should be greater than 0 and smaller than 65536!\n"CNRM, optarg);
					PrintUsage(argc, argv);
					return EXIT_FAILURE;
				}
				threadCount = (uint16_t) parsedInteger;
				break;
			case 'u':
				createTestIndexHtmlFile = 0;
				break;
			case 'w': /* The -w option disables all warnings.
					   * (It doesn't disable errors though!) */
				logWarnings = 0;
				break;
			default:
				PrintUsage(argc, argv);
				return EXIT_FAILURE;
		}
	}

	/* Send warning if the directory doesn't exists */
	if (logWarnings) {
		DIR *dir = opendir(filesDirectory);
		if (dir)
			closedir(dir);
		else {
			const char *warningMessage = NULL;
			const char *secondWarningMessage = "";

			if (errno == EACCES)
				warningMessage = "is a protected directory";
			else if (errno == ENOENT)
				warningMessage = "doesn't exists";
			else if (errno == ENOTDIR)
				warningMessage = "is not a directory";
			else {
				warningMessage = "failed to open: ";
				secondWarningMessage = strerror(errno);
			}

			fprintf(stderr, CYEL
				"Warning: Files Directory \"%s\" %s%s!\n"
				"To avoid '404 Not Found' messages, change the directory with the '-d' option.\n"
				CITA"For example: "CNRM CITA"%s -d /public_html/\n"CNRM,
				filesDirectory,
				warningMessage,
				secondWarningMessage,
				argv[0]);
			errno = 0;
		}
	}

	/* Create default index.html test page */
	if (createTestIndexHtmlFile)
		TryCreateIndexFile();

	/* Signal handling setup */
	struct sigaction signalAction;
	sigemptyset(&signalAction.sa_mask);
	signalAction.sa_sigaction = CatchSignal;
	signalAction.sa_flags = SA_SIGINFO;

	if (sigaction(SIGINT, &signalAction, NULL) == -1 || sigaction(SIGPIPE, &signalAction, NULL) == -1) {
		perror("Failed to set signal handler");
		free(filesDirectory);
		return EXIT_FAILURE;
	}

	/* Server socket setup */
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("Failed to create socket");
		free(filesDirectory);
		return EXIT_FAILURE;
	}

	int enable = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		perror("Failed to set SO_REUSEADDR option");

	int flags = fcntl(sockfd, F_GETFL);
	if (flags == -1) {
		perror("Failed to get server socket flags");
		free(filesDirectory);
		return EXIT_FAILURE;
	}

	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		perror("Unable to bind to address");
		printf("Port: %hu\n", port);
		free(filesDirectory);
		return EXIT_FAILURE;
	}

	if (listen(sockfd, 1) < 0) {
		perror("Unable to listen");
		printf("Port: %hu\n", port);
		free(filesDirectory);
		return EXIT_FAILURE;
	}

	int status = pthread_mutex_init(&threadsMutex, NULL);
	if (status != 0) {
		close(sockfd);
		fprintf(stderr, CRED"POSIX Mutex error: %s\n"CNRM, strerror(status));
		free(filesDirectory);
		return EXIT_FAILURE;
	}

	if ((status = pthread_attr_init(&threadsAttributes)) != 0 ||
		(status = pthread_attr_setdetachstate(&threadsAttributes, PTHREAD_CREATE_DETACHED)) != 0) {
		pthread_mutex_destroy(&threadsMutex);
		close(sockfd);
		fprintf(stderr, CRED"POSIX Thread Attribute error: %s\n"CNRM, strerror(status));
		free(filesDirectory);
		return EXIT_FAILURE;
	}

	threads = calloc(threadCount, sizeof(struct Thread));
	if (!threads) {
		pthread_attr_destroy(&threadsAttributes);
		pthread_mutex_destroy(&threadsMutex);
		close(sockfd);
		fprintf(stderr, CRED"Allocation error! Thread count %hu may be too much?\n"CNRM, threadCount);
		free(filesDirectory);
		return EXIT_FAILURE;
	}

	puts(CGRN"Ready"CNRM);
	uint32_t len = sizeof(struct sockaddr_in);

	while (keepRunning) {
		int client = accept(sockfd, (struct sockaddr*)&addr, &len);

		if (client < 0) {
			if (errno == EWOULDBLOCK ||
				errno == EAGAIN ||
				errno == ECONNABORTED) {
				continue;
			}
			if (errno == EBADF ||
				errno == EINTR) {
				/* The server socket has been closed (this is most-likely done by the
				 * CatchSignal function). */
				break;
			}

			perror(CRED"Unknown client acceptance failure"CYEL);
			printf("error=%i\n", errno);
			break;
		}

		pthread_mutex_lock(&threadsMutex);
		size_t i;
		for (i = 0; i < threadCount; i++) {
			if (!threads[i].alive) {
				threads[i].alive = 1;
				threads[i].socket = client;
				status = pthread_create(&threads[i].thread, &threadsAttributes, HandleClient, &threads[i]);

				if (status != 0) {
					memset(&threads[i], 0, sizeof(struct Thread));
					close(client);

					if (logWarnings) {
						char errorMessage[256];
						strerror_r(status, errorMessage, sizeof(errorMessage));
						fprintf(stderr, CYEL"Warning: Failed to create thread: %s\n", errorMessage);
					}
				}
				break;
			}
		}
		pthread_mutex_unlock(&threadsMutex);
	}

	putc('\n', stdout);
	if (sockfd > 0)
		close(sockfd);

	pthread_mutex_lock(&threadsMutex);
	size_t i;
	int threadStillAlive = 0;
	for (i = 0; i < threadCount; i++) {
		if (threads[i].alive) {
			threadStillAlive = 1;
			break;
		}
	}
	pthread_mutex_unlock(&threadsMutex);

	/* Wait for clients to stop execution. If they don't stop after 500ms, force-quit the thread.*/
	if (threadStillAlive) {
		struct timespec waitTime = { 0, 500000000 }; /* 500 ms */
		nanosleep(&waitTime, NULL);
		pthread_mutex_lock(&threadsMutex);
		for (i = 0; i < threadCount; i++) {
			if (threads[i].alive) {
				pthread_cancel(threads[i].thread);
			}
		}
		pthread_mutex_unlock(&threadsMutex);
	}

	for (i = 0; i < threadCount; i++) {
		CleanUpClient(&threads[i]);
	}

	free(threads);
	threads = NULL;
	pthread_mutex_destroy(&threadsMutex);
	pthread_attr_destroy(&threadsAttributes);
	pthread_mutex_destroy(&threadsMutex);

	free(filesDirectory);

	puts(CGRN"Stopped succesfully."CNRM);

	return EXIT_SUCCESS;
}

