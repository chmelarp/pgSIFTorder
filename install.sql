
-- LOAD '$libdir/plugins/pgsiftorder.so';
-- LOAD 'pgsiftorder.so';


-- DEBUG
CREATE OR REPLACE FUNCTION array_debug(
    a real[],
    b real[])
  RETURNS real[] AS
$BODY$ BEGIN
    RAISE NOTICE 'array_debug():\n  a: %\n  b: %', a::text[], b::text[];
    RETURN b;
END; $BODY$
  LANGUAGE plpgsql IMMUTABLE STRICT;
ALTER FUNCTION array_debug(real[], real[])
  OWNER TO tidb;  


-- Real vector (array) funs
------------------------------

-- Addition of vectors by elements - ΣAi + Bi
DROP FUNCTION IF EXISTS array_add(real[], real[]) CASCADE;
CREATE OR REPLACE FUNCTION array_add(real[], real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_array_add_real'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION array_add(real[], real[]) IS 'Addition of vectors by elements - ΣAi + Bi
@param elements0 real[]  // INOUT
@param elements1 real[]  // IN';

CREATE AGGREGATE array_add_agg(real[]) (
  SFUNC=array_add,
  STYPE=real[]
);
COMMENT ON FUNCTION array_add_agg(real[]) IS 'Addition of vectors by elements - ΣAi';

CREATE AGGREGATE array_sum(real[]) (
  SFUNC=array_add,
  STYPE=real[]
);
COMMENT ON FUNCTION array_sum(real[]) IS 'Addition of vectors by elements - ΣAi';

-- Subtraction of vectors by elements - ΣAi - Bi
DROP FUNCTION IF EXISTS array_sub(real[], real[]) CASCADE;
CREATE OR REPLACE FUNCTION array_sub(real[], real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_array_sub_real'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION array_sub(real[], real[]) IS 'Subtraction of vectors by elements - ΣAi - Bi
@param elements0 real[]  // INOUT
@param elements1 real[]  // IN';

CREATE AGGREGATE array_sub_agg(real[]) (
  SFUNC=array_sub,
  STYPE=real[]
);
COMMENT ON FUNCTION array_sub_agg(real[]) IS 'Subtraction of vectors by elements - Σ-Ai';

-- Multiplication of vectors by elements - ΣAi * Bi
DROP FUNCTION IF EXISTS array_mul(real[], real[]) CASCADE;
CREATE OR REPLACE FUNCTION array_mul(real[], real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_array_mul_real'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION array_mul(real[], real[]) IS 'Multiplication of vectors by elements - ΣAi * Bi
@param elements0 real[]  // INOUT
@param elements1 real[]  // IN';

CREATE AGGREGATE array_mul_agg(real[]) (
  SFUNC=model_avg_real,
  STYPE=real[]
);
COMMENT ON FUNCTION array_mul_agg(real[]) IS 'Multiplication of vectors by elements - ∏Ai';

CREATE AGGREGATE array_product(real[]) (
  SFUNC=array_mul,
  STYPE=real[]
);
COMMENT ON FUNCTION array_product(real[]) IS 'Multiplication of vectors by elements - ∏Ai';

-- Division of vectors by elements - ΣAi * Bi
-- WARNING: May return NaN!
DROP FUNCTION IF EXISTS array_div(real[], real[]) CASCADE;
CREATE OR REPLACE FUNCTION array_div(real[], real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_array_div_real'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION array_div(real[], real[]) IS 'Division of vectors by elements - ΣAi / Bi
WARNING: May return NaN!
@param elements0 real[]  // INOUT
@param elements1 real[]  // IN';

CREATE AGGREGATE array_div_agg(real[]) (
  SFUNC=array_div,
  STYPE=real[]
);
COMMENT ON FUNCTION array_div_agg(real[]) IS 'Division of vectors by elements - ∏/Ai';

-- Square - multiplication of the vector by elements - ΣAi * Ai
DROP FUNCTION IF EXISTS array_sqr(real[]) CASCADE;
CREATE OR REPLACE FUNCTION array_sqr(real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_array_sqr_real'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION array_sqr(real[]) IS 'Square - multiplication of the vector by elements - ΣAi * Ai
@param elements0 real[]  // INOUT';


-- Sqare root of the vector by elements - ΣAi * Ai
-- WARNING: May return NaN!
DROP FUNCTION IF EXISTS array_sqrt(real[], real[]) CASCADE;
CREATE OR REPLACE FUNCTION array_sqrt(real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_array_sqrt_real'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION array_sqrt(real[]) IS 'Sqare root of the vector by elements - ΣAi * Ai
WARNING: May return NaN!
@param elements0 real[]  // INOUT';

--------------------------------
-- TODO: ARRAY min && ARRAY MAX
--------------------------------

-- Average and standard deviation accumulator - ΣAi, ΣAi^2, Σi
DROP FUNCTION IF EXISTS array_acc_real(real[], real[]) CASCADE;
CREATE OR REPLACE FUNCTION array_acc_real(real[], real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_array_acc_real'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION array_acc_real(real[], real[]) IS 'Average and standard deviation accumulator - ΣAi, ΣAi^2, Σi, -i (~ length checksum)
@param elements0 real[]  // INOUT
@param elements1 real[]  // IN';

CREATE AGGREGATE array_accumulate(real[]) (
  SFUNC=array_acc_real,
  STYPE=real[]
);
COMMENT ON FUNCTION array_accumulate(real[]) IS 'Average and standard deviation accumulator - ΣAi, ΣAi^2, Σi, -i (~ length checksum)';

-- Average and standard deviation accumulator - ΣAi, ΣAi^2, Σi
DROP FUNCTION IF EXISTS array_avg_final(real[]) CASCADE;
CREATE OR REPLACE FUNCTION array_avg_final(real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_array_avg_final'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION array_avg_final(real[]) IS 'Average of vectors final
@param elements0 real[]    // INOUT';

CREATE AGGREGATE array_avg(real[]) (
  SFUNC=array_acc_real,
  STYPE=real[],
  FINALFUNC=array_avg_final
);
COMMENT ON FUNCTION array_avg(real[]) IS 'Average of vectors ΣAi';

-- Average and standard deviation accumulator - ΣAi, ΣAi^2, Σi
DROP FUNCTION IF EXISTS array_std_final(real[]) CASCADE;
CREATE OR REPLACE FUNCTION array_std_final(real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_array_std_final'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION array_std_final(real[]) IS 'Standard deviation of vectors final
@param elements0 real[]    // INOUT';

CREATE AGGREGATE array_std(real[]) (
  SFUNC=array_acc_real,
  STYPE=real[],
  FINALFUNC=array_std_final
);
COMMENT ON FUNCTION array_std(real[]) IS 'Standard deviation of vectors ΣAi';



-- ASNM Funs
---------------

-- DROP FUNCTION IF EXISTS model_sum_real(real[], real[]) CASCADE;
DROP FUNCTION IF EXISTS model_sum_real(real[], real[]) CASCADE;
DROP FUNCTION IF EXISTS model_sum_real(double precision[], real[]) CASCADE;
CREATE OR REPLACE FUNCTION model_sum_real(real[], real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_model_sum_real'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION model_sum_real(real[], real[]) IS 'Sum of model vectors - 5*add [5*sqr]
@param elements0 real[]  // INOUT
@param elements1 real[]  // IN';

-- DROP FUNCTION IF EXISTS model_sum_final(real[]) CASCADE;
DROP FUNCTION IF EXISTS model_sum_final(real[]) CASCADE;
DROP FUNCTION IF EXISTS model_sum_final(double precision[]) CASCADE;
CREATE OR REPLACE FUNCTION model_sum_final(real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_model_sum_final'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION model_sum_final(real[]) IS 'Sum of model vectors final - 5*void [5*sqrt]
 * @param elements0 real[]    // INOUT';

-- DROP AGGREGATE IF EXISTS model_sum(real[]);
CREATE AGGREGATE model_sum(real[]) (
  SFUNC=model_sum_real, --array_debug, --
  STYPE=real[],
  FINALFUNC=model_sum_final,
  initcond = '{0,0,0,0,0,0,0,0,0,0}'
);
COMMENT ON FUNCTION model_sum(real[]) IS 'Sum of model vectors - (5*add [sqrt(5*sqr)]) / count';



-- DROP FUNCTION IF EXISTS model_avg_real(real[], real[]) CASCADE;
DROP FUNCTION IF EXISTS model_avg_real(real[], real[]) CASCADE;
DROP FUNCTION IF EXISTS model_avg_real(double precision[], real[]) CASCADE;
CREATE OR REPLACE FUNCTION model_avg_real(real[], real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_model_avg_real'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION model_avg_real(real[], real[]) IS 'Sum of model vectors - 5*add; [5*sqr;] 5*++
@param elements0 real[15]    // INOUT
@param elements1 real[5-10]  // IN';

-- DROP FUNCTION IF EXISTS model_avg_std_real(real[], real[], real[]) CASCADE;
DROP FUNCTION IF EXISTS model_avg_std_real(real[], real[], real[]) CASCADE;
DROP FUNCTION IF EXISTS model_avg_std_real(double precision[], real[], real[]) CASCADE;
CREATE OR REPLACE FUNCTION model_avg_std_real(real[], real[], real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_model_avg_std_real'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION model_avg_std_real(real[], real[], real[]) IS 'Sum of model vectors - 5*add; 5*sqr; 5*++
@param elements0 real[15]    // INOUT
@param elements1 real[5-10]  // IN
@param elements1 real[5+]    // IN';

-- DROP FUNCTION IF EXISTS model_avg_final(real[]) CASCADE;
DROP FUNCTION IF EXISTS model_avg_final(real[]) CASCADE;
DROP FUNCTION IF EXISTS model_avg_final(double precision[]) CASCADE;
CREATE OR REPLACE FUNCTION model_avg_final(real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_model_avg_final'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION model_avg_final(real[]) IS 'Compute average and standard deviation for the training vector:

real std_dev2(real a[], int n) {
   if(n == 0)
        return 0.0;
    real sum = 0;
    real sq_sum = 0;
    for(int i = 0; i < n; ++i) {
       sum += a[i];
       sq_sum += a[i] * a[i];
    }
    real mean = sum / n;
    real variance = sq_sum / n - mean * mean; // > 0!
    return sqrt(variance);
}

@param elements0 real[]    // INOUT';

-- DROP FUNCTION IF EXISTS model_avg_std_final(real[]) CASCADE;
DROP FUNCTION IF EXISTS model_avg_std_final(real[]) CASCADE;
DROP FUNCTION IF EXISTS model_avg_std_final(double precision[]) CASCADE;
CREATE OR REPLACE FUNCTION model_avg_std_final(real[]) RETURNS real[]
AS 'pgsiftorder.so', 'c_model_avg_std_final'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION model_avg_std_final(real[]) IS 'Compute average and standard deviation for the model vector where stds were given
5* div n; sqrt(5*sqr); 5*==
@param elements0 real[]    // INOUT';


-- DROP AGGREGATE model_avg(real[]);
CREATE AGGREGATE model_avg(real[]) (
  SFUNC=model_avg_real,
  STYPE=real[],
  FINALFUNC=model_avg_final,
  initcond = '{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}'
);
COMMENT ON FUNCTION model_avg(real[]) IS 'Compute average and standard deviation for the training vector[15]';

-- DROP AGGREGATE model_avg_std(real[]);
CREATE AGGREGATE model_avg_std(real[]) (
  SFUNC=model_avg_real,
  STYPE=real[],
  FINALFUNC=model_avg_std_final,
  initcond = '{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}'
);
COMMENT ON FUNCTION model_avg_std(real[]) IS 'Compute average and standard deviation for the training vector[15] where stds were given';

-- DROP AGGREGATE model_avg(real[], real[]);
CREATE AGGREGATE model_avg(real[], real[]) (
  SFUNC=model_avg_std_real,
  STYPE=real[],
  FINALFUNC=model_avg_final,
  initcond = '{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}'
);
COMMENT ON FUNCTION model_avg(real[], real[]) IS 'Compute WEIGHTED average and standard deviation for the training vector[15]';

-- DROP AGGREGATE model_avg_std(real[], real[]);
CREATE AGGREGATE model_avg_std(real[], real[]) (
  SFUNC=model_avg_std_real,
  STYPE=real[],
  FINALFUNC=model_avg_std_final,
  initcond = '{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}'
);
COMMENT ON FUNCTION model_avg_std(real[], real[]) IS 'Compute WEIGHTED average and standard deviation for the training vector[15] where stds were given';




-- Comparison of event (host, subnet {services}) to model vectors (event[5], model[10], emin, emax, emul, sigma):
-- (event > EMAX AND event > EMUL*model.avg) 
-- OR (model.avg > 0 AND model.std > 0 AND event > EMIN 
--     AND event > EMUL*model.avg      AND event > model.avg + SIGMA*model.std)
DROP FUNCTION IF EXISTS model_compare(real[], real[], real, real, real, real) CASCADE;
CREATE OR REPLACE FUNCTION model_compare(real[], real[], real, real, real, real) RETURNS real[] -- real[11]
AS 'pgsiftorder.so', 'c_model_compare'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION model_compare(real[], real[], real, real, real, real) IS 'Comparison of event (host, subnet {services}) to model vectors:
(event > EMAX AND event > EMUL*model.avg) 
OR (model.avg > 0 AND model.std > 0 AND event > EMIN 
    AND event > EMUL*model.avg      AND event > model.avg + SIGMA*model.std)';












-- DROP FUNCTION rating_cosine_norm(int[], real[], real, int[], real[], real);
CREATE OR REPLACE FUNCTION rating_cosine_norm(int[], real[], real, int[], real[], real) RETURNS real
AS 'pgsiftorder.so', 'c_rating_cosine_norm'
LANGUAGE C STRICT;

-- DROP FUNCTION rating_normalize_vect(real[]);
CREATE OR REPLACE FUNCTION rating_normalize_vect(real[]) RETURNS real
AS 'pgsiftorder.so', 'c_rating_normalize_vect'
LANGUAGE C STRICT;

-- DROP FUNCTION rating_cosine(int[], real[], int[], real[]);
CREATE OR REPLACE FUNCTION rating_cosine(int[], real[], int[], real[]) RETURNS real
AS 'pgsiftorder.so', 'c_rating_cosine'
LANGUAGE C STRICT;

-- DROP FUNCTION rating_boolean_int(int[], int[]);
CREATE OR REPLACE FUNCTION rating_boolean_int(int[], int[]) RETURNS int
AS 'pgsiftorder.so', 'c_rating_boolean_int'   -- the second parameter might be omited in case of the same name
LANGUAGE C STRICT;

-- DROP FUNCTION rating_boolean(anyarray, anyarray);
CREATE OR REPLACE FUNCTION rating_boolean(anyarray, anyarray) RETURNS int
AS 'pgsiftorder.so', 'c_rating_boolean'
LANGUAGE C STRICT;

-- DROP FUNCTION distance_square_int(int[], int[]);
CREATE OR REPLACE FUNCTION distance_square_int(int[], int[]) RETURNS int8
AS 'pgsiftorder.so', 'c_distance_square_int'
LANGUAGE C STRICT;

-- DROP FUNCTION distance_square_real(int[], int[]);
DROP FUNCTION IF EXISTS distance_square_real(real[], real[]) CASCADE;
CREATE OR REPLACE FUNCTION distance_square_real(real[], real[]) RETURNS real
AS 'pgsiftorder.so', 'c_distance_square_real'
LANGUAGE C STRICT;

-- DROP FUNCTION distance_mahalanobis_int(int[], int[], real[]);
DROP FUNCTION IF EXISTS distance_mahalanobis_int(int[], int[], real[]) CASCADE;
CREATE OR REPLACE FUNCTION distance_mahalanobis_int(int[], int[], real[]) RETURNS real
AS 'pgsiftorder.so', 'c_distance_mahalanobis_int'
LANGUAGE C STRICT;


