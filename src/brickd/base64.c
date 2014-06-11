/*
 * This code originally came from here
 *
 * http://base64.sourceforge.net/b64.c
 *
 * with the following license:
 *
 * LICENSE:       Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.
 *
 *                Permission is hereby granted, free of charge, to any person
 *                obtaining a copy of this software and associated
 *                documentation files (the "Software"), to deal in the
 *                Software without restriction, including without limitation
 *                the rights to use, copy, modify, merge, publish, distribute,
 *                sublicense, and/or sell copies of the Software, and to
 *                permit persons to whom the Software is furnished to do so,
 *                subject to the following conditions:
 *
 *                The above copyright notice and this permission notice shall
 *                be included in all copies or substantial portions of the
 *                Software.
 *
 *                THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 *                KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 *                WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 *                PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 *                OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 *                OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 *                OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *                SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "base64.h"

static const char encode[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int32_t base64_encode(const char *in, int32_t in_len, char *out, int32_t out_len) {
	unsigned char triple[3];
	int32_t i;
	int32_t len;
	int32_t line = 0;
	int32_t done = 0;

	while (in_len) {
		len = 0;

		for (i = 0; i < 3; i++) {
			if (in_len) {
				triple[i] = *in++;
				len++;
				in_len--;
			} else {
				triple[i] = 0;
			}
		}

		if (!len) {
			continue;
		}

		if (done + 4 >= out_len) {
			return -1;
		}

		*out++ = encode[triple[0] >> 2];
		*out++ = encode[((triple[0] & 0x03) << 4) |  ((triple[1] & 0xF0) >> 4)];
		*out++ = (len > 1 ? encode[((triple[1] & 0x0F) << 2) | ((triple[2] & 0xC0) >> 6)] : '=');
		*out++ = (len > 2 ? encode[triple[2] & 0x3F] : '=');

		done += 4;
		line += 4;
	}

	if (done + 1 >= out_len) {
		return -1;
	}

	*out++ = '\0';

	return done;
}