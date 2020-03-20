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
 */

/** Default HTML Pages **/
/**
 * This file is copied to the web files directory when no other root
 * index.html was found. The copying of this file can be disabled by
 * specifying the '-u' option.
 */
const char *HTMLDefaultTestPage = "<!doctype html>\n<head>\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>pws Test Page</title>\n</head>\n<body>\n<h1>pws</h1>\n<p>\nIt works!\nThis page was automatically generated, because you didn't have an 'index.html' file.\nFeel free to overwrite this file when needed.\nTo disable the creation of this file, add the '-u' option to the server instance.\n</p>\n</body>\n</html>\n";
/**
 * The 404-message body. A 404 is an error which occurs when an user tries
 * to access a file that doesn't exist.
 */
const char *HTMLNotFoundPage = "<!doctype html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>File Not Found</title></head><body><h1>404 Not Found</h1><p>The file at this location wasn't found. If you are the server administrator, please refer to the logs and make sure you're not using the '-w' (disable all warnings) option when resolving this problem.</p></body></html>";


/** Options **/
/**
 * Should we print warning messages? This boolean can be set to 1 for
 * debugging purposes, but can clog the log when it isn't needed.
 */
int logWarnings = 0;
/**
 * The default port of the server. When using plain (non-TLS) connections,
 * the 80 port is expected and should be used. When using TLS, for example
 * when you're using the LibreSSL patch, choose port 443.
 *
 * Please note that this is the default port and that this value can be
 * changed by specifying the '-p' option.
 *
 * See: RFC 7230 Section 2.7 <https://www.rfc-editor.org/rfc/rfc7230.html#section-2.7>
 */
uint16_t port = 80;
/**
 * How many threads may be used? Having a lot of available threads can be
 * advantageous because you can serve a lot of users at once, but low-budget
 * hardware can suffer under a lot of threads.
 */
uint16_t threadCount = 20;


/** Miscellaneous **/
/**
 * The 'name' of the server. This is the value of the 'Server' header.
 */
const char *serverName = "pws";


