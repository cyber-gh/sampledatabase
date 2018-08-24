# sampledatabase

Sample database build in C.
Implemented using B+ trees, optimized.

Command line arguments:
1. name of the database

database layout:

id -> uint_32

username -> char[] set to 37 currently

email -> char[] set to 256 currently

mind the null byte at the end of char 


Usage:

.exit -> exits the program

.constants -> displays the constants used in building the table

.btree -> displays the b+ tree of the database

insert id name email -> inserts record with the given id name and email, or returns error if line isn't formatted as follows

select -> displays all records in the database

