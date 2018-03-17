
# distfuzz

A distributed file-format fuzzer built with OpenMPI

Screenshot
=================
![distfuzz screenshot](https://i.imgur.com/HleYr4S.png)

Setup
=================

Master Node:
 - Install MPI and related libraries
 - Install MySQL server
 - Install MySQL client
 - Install MySQL development libraries
 - Install GLOB library (libghc-glob-dev)
 - Install SSL library (libssl-dev)
 - Create MySQL user/pass/db
 - Create MySQL schema with scema.sql
 - Set MySQL Server to listen on 0.0.0.0
 - Set MySQL credentials in src/config.h
 - List all slave nodes in ./nodes.lst

Slave Node:
 - Install MPI and related libraries
 - Install SSH server
 - Install MySQL client
 - Give master node access via SSH pubkey

Running
=================

To test:
 - make && make runtest

To fuzz:
 - make
 - ./bin/distfuzz <sample_dir> <crash_dir> ./target -cmd line @@
 - note: '@@' will be replaced with the input fuzz sample file

Acknowledgments
=================
This product includes software developed by the Apache Group
for use in the Apache HTTP server project (http://www.apache.org/).
