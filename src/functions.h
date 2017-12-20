#ifndef _FUNCTIONS_H
#define _FUNCTIONS_H

#include <mpi.h>
#include <openssl/md5.h>

extern int world_size;
extern int world_rank;
extern char processor_name[MPI_MAX_PROCESSOR_NAME];
extern int name_len;
extern FILE *devnull;

struct job {
	int id;
	char sample[MD5_DIGEST_LENGTH * 2 + 1];
	int	start;
	int end;
	int node;
};

void output(const char *fmt, ...);

void clean_database(MYSQL *conn);

void init_samples(MYSQL *conn, const char *input_dir);

void init_jobs(MYSQL *conn);

int get_job(MYSQL *conn, int node, struct job *job);

void delete_job(MYSQL *conn, int id);

void store_sample(MYSQL *conn, char *sample, char *filename);

void write_crash_files(MYSQL *conn, char *output_dir);

void log_crash(MYSQL *conn, char *path, int sig);

int file_size(const char *filename);

const char *md5_file(const char *filename);

void remove_nl(char *str);

void normalize_path(char *str);

int run_test(int argc, char *argv[]);

#endif