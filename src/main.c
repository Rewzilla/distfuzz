#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <mysql/mysql.h>
#include <mpi.h>

#include "config.h"
#include "constants.h"
#include "functions.h"
#include "fuzzdata.h"

int world_size;
int world_rank;
char processor_name[MPI_MAX_PROCESSOR_NAME];
int name_len;

FILE *devnull;

// ./main <input_dir> <output_dir> <target_bin> <args <...>>
int main(int argc, char *argv[]) {

	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	MPI_Get_processor_name(processor_name, &name_len);

	MYSQL conn;
	struct job job;
	FILE *fp;
	char filename[1024];
	int replace_arg;
	int status;
	int c, pos;

	uint8_t tmp_8;
	uint16_t tmp_16;
	uint32_t tmp_32;

	devnull = fopen("/dev/null", "w");

	mysql_init(&conn);

	if(!mysql_real_connect(&conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0)) {
		output("Database error: %s", mysql_error(&conn));
		goto end;
	}

	if(world_rank == 0) {

		output("Initialized %d fuzzing nodes", world_size);

		clean_database(&conn);
		init_samples(&conn, argv[1]);
		init_jobs(&conn);

	}

	// all threads wait on the master process to init the DB
	MPI_Barrier(MPI_COMM_WORLD);

	replace_arg = 0;
	for(c=0; c<argc; c++) {
		if(strcmp(argv[c], "@@") == 0)
			replace_arg = c;
	}

	while(get_job(&conn, world_rank, &job)) {

		output("Fuzzing %s:%d-%d", job.sample, job.start, job.end);

		snprintf(filename, 1024, "%s/%s", TMP_DIR, job.sample);
		store_sample(&conn, job.sample, filename);

		if(replace_arg) {
//			free(argv[replace_arg]);
			argv[replace_arg] = strdup(filename);
		}

		fp = fopen(filename, "w");
		for(pos=job.start; pos<job.end; pos++) {

			fseek(fp, pos, SEEK_SET);
			fread(&tmp_8, 1, 1, fp);

			for(c=0; c<KNOWN_INT8_LEN; c++) {

				fseek(fp, pos, SEEK_SET);
				fwrite(&known_int8s[c], 1, 1, fp);
				fflush(fp);

				status = run_test((argc-3), &argv[3]);
				if(status && status != -1) {
					output("%s in %s:%d using '0x%02x'",
						signal_strings[status], job.sample, pos, known_int8s[c]);
					log_crash(&conn, filename, status);
				}

			}

			fseek(fp, pos, SEEK_SET);
			fwrite(&tmp_8, 1, 1, fp);
			fflush(fp);

			fseek(fp, pos, SEEK_SET);
			fread(&tmp_16, 1, 2, fp);

			for(c=0; c<KNOWN_INT16_LEN; c++) {

				fseek(fp, pos, SEEK_SET);
				fwrite(&known_int16s[c], 1, 2, fp);
				fflush(fp);

				status = run_test((argc-3), &argv[3]);
				if(status) {
					output("%s in %s:%d using '0x%04x'",
						signal_strings[status], job.sample, pos, known_int16s[c]);
					log_crash(&conn, filename, status);
				}

			}

			fseek(fp, pos, SEEK_SET);
			fwrite(&tmp_16, 1, 2, fp);
			fflush(fp);

			fseek(fp, pos, SEEK_SET);
			fread(&tmp_32, 1, 4, fp);

			for(c=0; c<KNOWN_INT32_LEN; c++) {

				fseek(fp, pos, SEEK_SET);
				fwrite(&known_int32s[c], 1, 4, fp);
				fflush(fp);

				status = run_test((argc-3), &argv[3]);
				if(status) {
					output("%s in %s:%d using '0x%08x'",
						signal_strings[status], job.sample, pos, known_int32s[c]);
					log_crash(&conn, filename, status);
				}

			}

			fseek(fp, pos, SEEK_SET);
			fwrite(&tmp_32, 1, 4, fp);
			fflush(fp);

		}

		delete_job(&conn, job.id);

	}

	// wait until all fuzzing is complete to write crash files
	MPI_Barrier(MPI_COMM_WORLD);
	if(world_rank == 0) {

		output("Saving crashing samples");
		write_crash_files(&conn, argv[2]);

	}

end:
	MPI_Finalize();
	return 0;

}
