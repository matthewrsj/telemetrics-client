/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms and conditions of the GNU Lesser General Public License, as
 * published by the Free Software Foundation; either version 2.1 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 */

#define _GNU_SOURCE
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PAYLOAD_SIZE 8192

char *last_read_fname = "/var/log/mcelog-latest";
char *mcelog_fname = "/var/log/mcelog";

/*
 * returns 0 if no file or unable to read contents. This insures all records
 * will be read from mcelog if the file doesn't exist
 */
static long get_last_read(void)
{
        FILE *fp = NULL;
        char *last_read_s = NULL;
        size_t fsize;
        size_t bytes_in = 0;
        long ret = 0;

        fp = fopen(last_read_fname, "r");
        if (!fp) {
                ret = 0;
                goto out;
        }

        fseek(fp, 0, SEEK_END);
        fsize = (size_t)ftell(fp);
        rewind(fp);

        last_read_s = malloc(fsize + 1);

        bytes_in = fread(last_read_s, fsize, 1, fp);
        if (bytes_in == 0) {
                /* read all */
                ret = 0;
                goto out;
        }

        ret = atol(last_read_s);

out:
        if (fp) {
                fclose(fp);
        }

        if (last_read_s) {
                free(last_read_s);
        }

        return ret;
}

static void write_last_read(long new_last_read)
{
        FILE *fp = NULL;

        if (getuid() != 0) {
                fprintf(stderr, "Must be root to update %s\n", last_read_fname);
                goto out;
        }

        fp = fopen(last_read_fname, "w");
        if (!fp) {
                fprintf(stderr, "Error writing new last-read time to %s\n", last_read_fname);
                goto out;
        }

        if (fprintf(fp, "%lu", new_last_read) <= 0) {
                fprintf(stderr, "Error writing new last-read time to %s\n", last_read_fname);
                goto out;
        }

out:
        if (fp) {
                fclose(fp);
        }
}

int get_mce_payload(GString *backtrace)
{
        FILE   *fp = NULL;
        char   *mcelog_out = NULL, *record_start = NULL,
               *line_end = NULL, *line = NULL;
        long   record_time = 0, last_read = 0, new_last_read = 0;
        size_t bytes_in = 0, fsize = 0;
        int    f_err, ret;

        last_read = get_last_read();

        fp = fopen(mcelog_fname, "r");

        if (!fp) {
                fprintf(stderr, "Error: mce log could not be opened for reading\n");
                ret = 1;
                goto out;
        }

        if (fseek(fp, 0, SEEK_END) < 0) {
                fprintf(stderr, "Error seeking end of mce log file\n");
                ret = 1;
                goto out;
        }

        fsize = (size_t)ftell(fp);
        if (fsize <= 0) {
                fprintf(stderr, "Error reading mce log file size\n");
                ret = 1;
                goto out;
        }

        rewind(fp);

        mcelog_out = malloc(fsize + 1);

        bytes_in = fread(mcelog_out, fsize, 1, fp);
        f_err = ferror(fp);
        if (bytes_in == 0) {
                if (f_err == 0) {
                        fprintf(stderr, "No information in mce log\n");
                        g_string_append(backtrace, "no information in mce log\n");
                        ret = 0;
                        goto out;
                } else {
                        fprintf(stderr, "fread failed on %s\n", mcelog_fname);
                        ret = 1;
                        goto out;
                }
        }

        line = mcelog_out;
        record_start = mcelog_out;
        while (line) {
                line_end = memchr(line, '\n', fsize);
                if (!line_end) {
                        break;
                }

                *line_end = '\0';

                if (!record_start && strstr(line, "Hardware event.")) {
                        record_start = line;
                } else if (strstr(line, "TIME ")) {
                        record_time = atol(line + strlen("TIME "));
                        if (record_time <= last_read) {
                                record_start = NULL;
                        }

                        new_last_read = record_time;
                }
                *line_end = '\n';
                line = line_end + 1;
        }

        if (record_start) {
                g_string_append(backtrace, record_start);
        }

        write_last_read(new_last_read);

        ret = 0;

out:
        if (mcelog_out) {
                free(mcelog_out);
        }
        if (fp) {
                pclose(fp);
        }
        return ret;
}
/*
int main(void)
{
        GString *backtrace;
        backtrace = g_string_new(NULL);
        if (get_mce_payload(backtrace)) {
                fprintf(stderr, "Error reading mcelog\n");
                return 1;
        }

        printf("%s", backtrace->str);
        return 0;
}
*/
/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */

