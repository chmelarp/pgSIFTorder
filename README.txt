# pgSIFTorder
﻿
    PostgreSQL Document Retrieval Functions
"""""""""""""""""""""""""""""""""""""""""""""
There are two kind of functions - ranking of sparse arrays (e.g. for information retrieval) and Euclidean distance for dense vectors (e.g. for image similarity search).

User-defined functions for sparse vector ranking. Arrays (vectors) must be ordered (and normalized) as follows.

    The tf-idf weighting weight(w) is a product of two terms. The word (term) frequency: 
        tf(w) = |d(w)| / |d|,
    where d is a document, |d| number of words in d and |d(w)| is the number of occurrences of word w in d
    and the inverse document frequency:
        idf(w) = log(|D| / |D(w)|), 
    where D is a database of all documents and D(w) are documents containing word w.

Then the ranking (or relevance) has two posibilities.

    The cosine rating (relevance) is a normalized dot product of two vectors:
                       dq.dd
        r(dq, dd) = -----------
                     |dq|x|dd|
    where where dq is a document query (vectors), dd document in the database and dq.dd a dot product of weights and |dg|, |dd| is the L2 norm (or the sqrt of dot product); x is multiplication.

    The boolean rating doesn't need normalization, it is a dot product of two vectors:
        r(dq, dd) = dq.dd
    where dq and dd document vectors, dq.dd just a dot product of ones where words w in vectors (and zeros respectively).

User-defined functions for dense vector distance count.
    
    The Euclidean distance is the distance between two points (given as vectors) or a measure, which can be proven by repeated application of the Pythagorean theorem. 
        dE(v1, v2) = Sqrt[ (v11 - v21)^2 + ... + (v1n - v2n)^2 ],
    where v1 and v2 are vectors of a dimension n.
    
    However the "real" Euclidean distance uses square root of the sum, which is a time consuming operation, we don't need. Thus we compute:
        d2(v1, v2) = Sum[ (v1i - v2i)^2 ],
                      i
    in this way, the Eucleidean distance is given as dE(v1, v2) = Sqrt(d2(v1, v2)).

For related literature see:
    [1] Keith <http://www.dcs.gla.ac.uk/Keith/Preface.html>
    [2] Baeza-Yates, Ricardo. Modern information retrieval. New York: ACM Press, 1999. 513 p. ISBN 0-201-39829-X.

    
    PostgreSQL 8.3 - 9.4+ C-Language Library pgSiftOrder
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
User-defined functions can be written in C (or a language that can be made compatible with C, such as C++). Such functions are compiled into dynamically loadable objects (also called shared libraries) and are loaded by the server on demand.

For programming see: 
    [3] PostgreSQL Documentation: C-Language Functions <http://www.postgresql.org/docs/current/static/xfunc-c.html>


    Compilation (Linux, gcc)
''''''''''''''''''''''''''''''
Just type "make". The makefile for (cross) compilation at x86-64 @ FIT is also included - type "make Makefile64.mak".
Note, you must have installed development files for PostgreSQL 8.3 server-side programming (postgresql-server-dev* package).

The compiler flag to create PIC is -fpic. On some platforms in some situations -fPIC must be used if -fpic does not work. Refer to the GCC manual for more information. The compiler flag to create a shared library is -shared. A complete example looks like this:
  gcc -fpic -c foo.c
  gcc -shared -o foo.so foo.o
    // in more detail (or use pg_config to find the library location -I\'pg_config\ --includedir-server\' -I\'pg_config\ --includedir\'): 
    gcc -c -g -I/usr/include -I/usr/include/postgresql -I/usr/include/postgresql/8.3/server -fPIC -o pgsiftorder.o pgsiftorder.c 
    gcc -shared -o pgsiftorder.so -fPIC pgsiftorder.o 


    Library location
''''''''''''''''''''''''
pg_config --pkglibdir (run in a shell to find the directory) and create the plugins directory (/usr/lib/postgresql/8.3/lib/plugins).

The following algorithm is used to locate the shared object file based on the name given in the CREATE FUNCTION command:
 1. If the name is an absolute path, the given file is loaded.
 2. If the name starts with the string $libdir, that part is replaced by the PostgreSQL package library directory name, which is determined at build time.
 3. If the name does not contain a directory part, the file is searched for in the path specified by the configuration variable dynamic_library_path.


    LOAD
'''''''''''
LOAD '$libdir/plugins/pgsiftorder.so';

This command loads a shared library file into the PostgreSQL server's address space. If the file had been loaded previously, it is first unloaded. This command is primarily useful to unload and reload a shared library file that has been changed since the server first loaded it. To make use of the shared library, function(s) in it need to be declared using the CREATE FUNCTION command.
The file name is specified in the same way as for shared library names in CREATE FUNCTION; in particular, one can rely on a search path and automatic addition of the system's standard shared library file name extension. See Section 34.9 for more information on this topic.
Non-superusers can only apply LOAD to library files located in $libdir/plugins/ — the specified filename must begin with exactly that string. (It is the database administrator's responsibility to ensure that only "safe" libraries are installed there.)

The superuser (postgres) is also responsible to make the language trusted (ie. anyone can create functions) and add sufficient privileges to the users:
UPDATE pg_language  SET lanpltrusted = true WHERE lanname = 'c';
GRANT USAGE ON LANGUAGE C TO chmelarp, trecvid;

-- UPDATE pg_language  SET lanpltrusted = true WHERE lanname = 'c';
-- GRANT USAGE ON LANGUAGE C TO tidb;
-- LOAD 'pgsiftorder.so';


    CREATE FUNCTIONs
'''''''''''''''''''''''''''''''''''''''''
    ...  see install.sql >>


    TEST examples
''''''''''''''''''''
-- E.g.
SELECT array_add('{}', NULL);
SELECT array_add('{}', '{}');
SELECT array_add('{}', '{1,2,3}');
SELECT array_add('{1,2,3}', '{}');
SELECT array_add('{1,2,3}', '{1,2}');
SELECT array_add('{1,2,3}', '{1,2,3}');
SELECT array_add('{1,2,3}', '{1,2,3,4}');

SELECT array_sub('{1,2,3}', '{1,2}');
SELECT array_mul('{1,2,3}', '{1,2,3}');
SELECT array_div('{1,2,3}', '{1,2,3,4}');
SELECT array_div('{1,2,3}', '{0}');

SELECT array_sqrt(array_sqr('{1,2,3}'));
SELECT array_sqrt('{-1,2,-3}');

-- // TODO: ARRAY min + MAX!!!!!!

    ASNM Tests + Examples
'''''''''''''''''''''''''''
SELECT array_sum(sum_flow[1:5]), sum(sum_flow[1]), sum(sum_flow[2]), sum(sum_flow[3]), sum(sum_flow[4]), sum(sum_flow[5])
FROM ti.hosts;

SELECT array_accumulate(sum_flow), count(*), avg(sum_flow[1]), avg(sum_flow[2]), avg(sum_flow[3]), avg(sum_flow[4]), avg(sum_flow[5])
  FROM nb.subnets30
 WHERE sn_addr = '0.0.0.0/0';

SELECT array_avg(sum_flow), count(*), avg(sum_flow[1]), avg(sum_flow[2]), avg(sum_flow[3]), avg(sum_flow[4]), avg(sum_flow[5])
  FROM nb.subnets30
 WHERE sn_addr = '0.0.0.0/0';

SELECT array_std(sum_flow), count(*), stddev(sum_flow[1]), stddev(sum_flow[2]), stddev(sum_flow[3]), stddev(sum_flow[4]), stddev(sum_flow[5])
  FROM nb.subnets30
 WHERE sn_addr = '0.0.0.0/0';


-- SELECT array_avg(vlan_ids::real[])
--   FROM ui.tab4h;
-- 
-- SELECT array_std(vlan_ids::real[])
--   FROM ui.tab4h;
-- 
-- ALTER TABLE ui.tab4h ADD COLUMN test real[];
-- UPDATE ui.tab4h SET test = vlan_ids::real[];
-- 
-- SELECT array_avg(test)
--   FROM ui.tab4h;
-- 
-- SELECT array_std(test)
--   FROM ui.tab4h;
-- 
-- UPDATE ui.tab4h SET test = '{}';
-- 
-- SELECT array_avg(test)
--   FROM ui.tab4h;
-- 
-- SELECT array_std(test)
--   FROM ui.tab4h;
-- 
-- ALTER TABLE ui.tab4h DROP COLUMN test;


SELECT * FROM model_sum_real(ARRAY[]::real[], ARRAY[]::real[]);
SELECT * FROM model_sum_real(ARRAY[1]::real[], ARRAY[]::real[]);
SELECT * FROM model_sum_real(ARRAY[]::real[], ARRAY[2]::real[]);
SELECT * FROM model_sum_real(ARRAY[1,2,3,4,5,6,7,8,9,10]::real[], ARRAY[1,2,3,4,5,6,7,8,9,10]::real[]);
SELECT * FROM model_sum_final(model_sum_real(ARRAY[1,2,3,4,5,6,7,8,9,10]::real[], ARRAY[1,2,3,4,5,6,7,8,9,10]::real[]));

SELECT sum_flow
  FROM nb.subnets30
 WHERE sn_addr = '0.0.0.0/0';

SELECT model_sum(sum_flow), sum(sum_flow[1]), sum(sum_flow[2]), sum(sum_flow[3]), sum(sum_flow[4]), sum(sum_flow[5])
  FROM nb.subnets30
 WHERE sn_addr = '0.0.0.0/0';

SELECT model_avg(sum_flow), avg(sum_flow[1]), avg(sum_flow[2]), avg(sum_flow[3]), avg(sum_flow[4]), avg(sum_flow[5])
  FROM nb.subnets30
 WHERE sn_addr = '0.0.0.0/0';

SELECT model_avg(sum_flow), 
       avg(sum_flow[1]), avg(sum_flow[2]), avg(sum_flow[3]), avg(sum_flow[4]), avg(sum_flow[5]),
       stddev(sum_flow[1]), stddev(sum_flow[2]), stddev(sum_flow[3]), stddev(sum_flow[4]), stddev(sum_flow[5])
  FROM nb.subnets30
 WHERE sn_addr = '0.0.0.0/0';

SELECT model_avg_std(sum_flow), 
       avg(sum_flow[1]), avg(sum_flow[2]), avg(sum_flow[3]), avg(sum_flow[4]), avg(sum_flow[5])
  FROM nb.subnets30
 WHERE sn_addr = '0.0.0.0/0';


SELECT model_avg_std(sum_port, sum_flow), 
       avg(sum_port[1]), avg(sum_port[2]), avg(sum_port[3]), avg(sum_port[4]), avg(sum_port[5])
  FROM nb.subnets30
 WHERE sn_addr = '0.0.0.0/0';

SELECT array_accumulate(sum_flow), avg(sum_flow[1]), avg(sum_flow[2]), avg(sum_flow[3]), avg(sum_flow[4]), avg(sum_flow[5])
  FROM nb.subnets30
 WHERE sn_addr = '0.0.0.0/0';

    Document Retrieval Functions
''''''''''''''''''''''''''''''''''

-- EXPLAIN ANALYZE
SELECT * FROM rating_cosine( ARRAY[2,5], ARRAY[0.7,0.7]::float4[], ARRAY[1,5], ARRAY[0.7,0.7]::float4[] );
-- in case of floats - Postgres supposes real numbers to be numeric, so explicit cast is necessary
SELECT * FROM rating_boolean(ARRAY[1,5,9], ARRAY[5,6,7]);   -- 1
SELECT * FROM rating_boolean(ARRAY['cat','dog','mouse'], ARRAY['cat','eat','mouse']);   -- 2 :)

SELECT *, rating_boolean(sift, ARRAY[11,12,16,20,10,182,237,359,380,408,559]) as score FROM tv2_sift_norm 
WHERE sift && ARRAY[11,12,16,20,10,182,237,359,380,408,559]
ORDER BY score DESC
LIMIT 200;

SELECT * FROM distance_square_int4(ARRAY[1,5,9], ARRAY[5,6,7]);   -- 21
SELECT * FROM distance_square_float4(ARRAY[0.7,0.8]::float4[], ARRAY[0.4,1.1]::float4[] ); -- 0.18 (0.179999992251396 on Intel machines :)

SELECT *, sqrt(distance_square_int4(features, ARRAY[166,157,196,196,153,193,197,164,165,164,157,163,161,171,165,113,146,109,157,170,152,113,97,113,142,198,154,83,64,80,143])) as distance
FROM tv2_gabor 
ORDER BY distance, video, frame ASC
LIMIT 1000;



    Notes
'''''''''''
Notice we have used STRICT so that we did not have to check whether the input arguments were NULL.

Note that sparse arrays must be ordered. And without element repetition (in the case, there wont be a really precise result).
In JAVA, use map or cern.colt (Sparse1DMatrix or map) instead of jama for vector computation.

How to work with arrays: http://doxygen.postgresql.org/array_8h.html
                         http://doxygen.postgresql.org/array__userfuncs_8c.html
For examples, see eg.:   contrib/intarray/_int_op.c

