#include <stdio.h>
#include <glob.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <openssl/md5.h>
#include <mysql/mysql.h>
#include <mpi.h>
#include <sys/stat.h>

#include "base64.h"
#include "config.h"
#include "functions.h"

void output(const char *fmt, ...) {

	va_list args;

	va_start(args, fmt);

	printf("\e[38;5;%d;1m[%s]\e[0m ", (world_rank + 2), processor_name);
	vprintf(fmt, args);
	printf("\n");

	va_end(args);

}

void clean_database(MYSQL *conn) {

	output("Cleaning up database");

	mysql_query(conn, "DELETE FROM jobs WHERE 1;");
	mysql_query(conn, "DELETE FROM samples WHERE 1;");
	mysql_query(conn, "DELETE FROM crashes WHERE 1;");

}

void init_samples(MYSQL *conn, const char *input_dir) {

	int c;
	glob_t g;
	char glob_str[64];
	char query_str[0x10000];
	FILE *fp;
	int len;
	char *file_data, *base64_data;

	snprintf(glob_str, 64, "%s/*", input_dir);

	switch(glob(glob_str, 0, NULL, &g)) {
		case GLOB_NOSPACE:
		case GLOB_ABORTED:
		case GLOB_NOMATCH:
			return; // opps :(
		default:
			break; // ok :)
	}

	output("Storing samples in database");

	for(c=0; c<g.gl_pathc; c++) {

		normalize_path(g.gl_pathv[c]);

		fp = fopen(g.gl_pathv[c], "r");
		if(!fp)
			continue; // opps :(

		len = file_size(g.gl_pathv[c]);
		file_data = malloc(len);
		base64_data = malloc(Base64encode_len(len));
		if(!base64_data || !file_data)
			continue; // opps :(

		fread(file_data, sizeof(char), len, fp);
		fclose(fp);

		Base64encode(base64_data, file_data, len);

		output(" > %s", g.gl_pathv[c]);

		snprintf(query_str, 0x10000,
			"INSERT INTO samples(hash, name, size, data) VALUES('%s', '%s', %d, '%s');",
			md5_file(g.gl_pathv[c]), g.gl_pathv[c], len, base64_data);

		mysql_query(conn, query_str);

		free(file_data);
		free(base64_data);

	}

}

void init_jobs(MYSQL *conn) {

	int c, s;
	int job_count;
	int assign;
	int start, end, len;
	struct job *jobs;
	MYSQL_RES *res;
	MYSQL_ROW row;
	char query_str[1024];

	mysql_query(conn, "SELECT hash, size FROM samples;");
	res = mysql_use_result(conn);
	jobs = NULL;
	job_count = 0;

	assign = 0;
	while((row = mysql_fetch_row(res)) != NULL) {

		len = atoi(row[1]);
		s = job_count;
		job_count += (len % JOB_SIZE == 0) ? len / JOB_SIZE : len / JOB_SIZE + 1;
		jobs = realloc(jobs, sizeof(struct job) * job_count);

		for(c=0; (c*JOB_SIZE)<len; c++) {

			jobs[s + c].start = c * JOB_SIZE;
			jobs[s + c].end = ((c * JOB_SIZE) + JOB_SIZE > len) ? len : ((c * JOB_SIZE) + JOB_SIZE);
			jobs[s + c].node = assign;
			strcpy(jobs[s + c].sample, row[0]);

			assign = (assign + 1) % world_size;

		}

	}

	mysql_free_result(res);

	for(c=0; c<job_count; c++) {

		snprintf(query_str, 1024,
			"INSERT INTO jobs(start, end, node, sample) VALUES(%d, %d, %d, '%s');",
			jobs[c].start, jobs[c].end, jobs[c].node, jobs[c].sample);

		mysql_query(conn, query_str);

	}

	free(jobs);

}

int get_job(MYSQL *conn, int node, struct job *job) {

	MYSQL_RES *res;
	MYSQL_ROW row;
	char query_str[1024];

	snprintf(query_str, 1024,
		"SELECT id, sample, start, end, node FROM jobs WHERE node=%d LIMIT 1;",
		node);

	mysql_query(conn, query_str);
	res = mysql_store_result(conn);

	if(mysql_num_rows(res) == 0)
		return 0;

	row = mysql_fetch_row(res);

	job->id = atoi(row[0]);
	strncpy(job->sample, row[1], 32);
	job->start = atoi(row[2]);
	job->end = atoi(row[3]);
	job->node = atoi(row[4]);

	mysql_free_result(res);

	return 1;

}

void delete_job(MYSQL *conn, int id) {

	char query_str[1024];

    snprintf(query_str, 1024,
		"DELETE FROM jobs WHERE id=%d;", id);

	mysql_query(conn, query_str);

}

void write_crash_files(MYSQL *conn, char *output_dir) {

	MYSQL_RES *res;
	MYSQL_ROW row;
	FILE *fp;
	char output_path[1024];
	char *file_data;

	mysql_query(conn, "SELECT hash, size, data FROM crashes;");
	res = mysql_use_result(conn);

	if(mysql_num_rows(res) == 0)
		return;

	while((row = mysql_fetch_row(res)) != NULL) {

		file_data = malloc(atoi(row[1]));
		Base64decode(file_data, row[2]);

		snprintf(output_path, 1024, "%s/%s", output_dir, row[0]);

		fp = fopen(output_path, "w");
		fwrite(file_data, 1, atoi(row[1]), fp);
		fclose(fp);

	}

	mysql_free_result(res);

}

int file_size(const char *filename) {

	FILE *fp;
	int len;

	fp = fopen(filename, "r");
	if(!fp)
		return -1;

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fclose(fp);

	return len;

}

const char *md5_file(const char *filename) {

	int c, len;
	FILE *fp;
	MD5_CTX ctx;
	char *buffer;
	unsigned char digest[MD5_DIGEST_LENGTH];
	static char out[MD5_DIGEST_LENGTH * 2 + 1];

	len = file_size(filename);

	fp = fopen(filename, "r");
	if(!fp)
		return NULL;

	buffer = malloc(len);
	if(!buffer)
		return NULL;

	fread(buffer, sizeof(char), len, fp);

	fclose(fp);

	MD5_Init(&ctx);
	MD5_Update(&ctx, buffer, len);
	MD5_Final(digest, &ctx);

	for(c=0; c<MD5_DIGEST_LENGTH; c++)
		sprintf(&out[c*2], "%02x", digest[c]);

	free(buffer);

	return out;

}

void remove_nl(char *str) {

	int c;

	for(c=0; str[c]!='\0'; c++) {

		if(str[c] == '\n')
			str[c] = '\0';

	}

}

void normalize_path(char *path) {

	int c;

	for(c=0; path[c]!='\0'; c++) {

		if(path[c] == '/' && path[c+1] == '/')
			strcpy(&path[c], &path[c+1]);

	}

}

void store_sample(MYSQL *conn, char *sample, char *filename) {

	MYSQL_RES *res;
	MYSQL_ROW row;
	FILE *fp;
	char query_str[1024];
	char *data;

	snprintf(query_str, 2014, "SELECT data, size FROM samples WHERE hash='%s';", sample);

	mysql_query(conn, query_str);
	res = mysql_use_result(conn);
	row = mysql_fetch_row(res);

	fp = fopen(filename, "w");

	data = malloc(Base64decode_len(row[0]));
	Base64decode(data, row[0]);
	fwrite(data, atoi(row[1]), 1, fp);

	fclose(fp);
	free(data);
	mysql_free_result(res);

}

void log_crash(MYSQL *conn, char *path, int sig) {

	FILE *fp;
	char query_str[0x10000];
	char *data;
	char *base64_data;
	int len;

	len = file_size(path);

	data = malloc(len);
	base64_data = malloc(Base64encode_len(len));
	if(!data || !base64_data)
		return; // opps :(

	fp = fopen(path, "r");
	fread(data, len, 1, fp);
	fclose(fp);

	Base64encode(base64_data, data, len);

	snprintf(query_str, 1024,
		"INSERT INTO crashes(hash, data, size, sig) VALUES('%s', '%s', %d, %d);",
		basename(path), base64_data, len, sig);

	mysql_query(conn, query_str);

	free(data);
	free(base64_data);

}

int run_test(int argc, char *argv[]) {

	int status;
	pid_t pid;
	int c;

	pid = vfork();

	if(pid) {

		waitpid(pid, &status, WUNTRACED);
		return status;

	} else {

		dup2(fileno(devnull), 1);
		dup2(fileno(devnull), 2);
		execv(argv[0], &argv[0]);
		exit(0);

	}

}

/*
int run_test(int argc, char *argv[]) {

	FILE *fp;
	char script[1024];
	char args[1024] = {0};
	int c, ret;

	// buffer overflow, meh
	c = 0;
	while(argv[c] != 0) {
		strcat(args, argv[c++]);
		strcat(args, " ");
	}

	snprintf(script, 1024,
		"#!/bin/sh\n"
		"%s\n"
		"echo $? > distfuzz.sig\n",
		args);

	fp = fopen("./distfuzz.tmp", "w");
	fwrite(script, 1, strlen(script), fp);
	fclose(fp);

	chmod("./distfuzz.tmp", 0755);
	system("./distfuzz.tmp");

	fp = fopen("./distfuzz.sig", "r");
	fscanf(fp, "%d", &ret);
	fclose(fp);
	return ret;

}
*/