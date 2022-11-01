/* 
 * File:   pgsiftorder.c
 * Author: chmelarp
 *
 * Created on 16 April   2008, 15:36 CET
 * Updated on 26 January 2015, 03:07 CET
 * 
 * See the INFO.txt for reference!
 */

// debugging? uncomment this...
// #define _DEBUG

/* PostgreSQL version as a number */
#define PG_VERSION_NUM 90400

#include <math.h>
#include <postgres.h>           // general Postgres declarations
#include <fmgr.h>               // function manager and function-call interface
#include <catalog/pg_type.h>    // definition of "type" relation (pg_type)
#include <utils/array.h>        // declarations for Postgres arrays.
#include <utils/typcache.h>     // for Type cache definitions
#include <access/tupmacs.h>     // Tuple macros used by both index tuples and heap tuples

#include "abbrevs.h"


// identification... its me :)
#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif


/*
 * The macro PG_ARGISNULL(n) allows a function to test whether each input is null. (Of course, 
 * doing this is only necessary in functions not declared "strict".) 
 * As with the PG_GETARG_xxx() macros, the input arguments are counted beginning at zero.
 * PG_GETARG_xxx_COPY() guarantees to return a copy of the specified argument that is safe for writing into.
 *
 * Never modify the contents of a pass-by-reference input value. If you do so you are likely to corrupt 
 * on-disk data, since the pointer you are given might point directly into a disk buffer. 
 * The sole exception to this rule is explained in Section 34.10 (User-Defined Aggregates). 
 * 
 * When allocating memory, use the PostgreSQL functions palloc and pfree instead of the corresponding 
 * C library functions malloc and free. The memory allocated by palloc will be freed automatically 
 * at the end of each transaction, preventing memory leaks. 
 * Always zero the bytes of your structures using memset. Without this, it's difficult to support 
 * hash indexes or hash joins, as you must pick out only the significant bits of your data structure 
 * to compute a hash. Even if you initialize all fields of your structure, there might be alignment padding 
 * (holes in the structure) that contain garbage values.
 *
 */



/* Create a new pg_type OID array for "num" elements */
ArrayType* array_new(int num, unsigned int oid) {
    ArrayType  *r;
    int nbytes = ARR_OVERHEAD_NONULLS(1) + sizeof(int) * num;

    r = (ArrayType *) palloc0(nbytes);

    SET_VARSIZE(r, nbytes);
    ARR_NDIM(r) = 1;
    r->dataoffset = 0;			/* marker for no null bitmap */
    ARR_ELEMTYPE(r) = oid;
    ARR_DIMS(r)[0] = num;
    ARR_LBOUND(r)[0] = 1;

    return r;
}

ArrayType* array_new_real(int num) {
    return array_new(num, FLOAT4OID);
}


ArrayType* array_resize_real(ArrayType *a, int num)
{
    int sizea  = ArrayGetNItems(ARR_NDIM(a), ARR_DIMS(a));
    int nbytes = ARR_DATA_OFFSET(a) + sizeof(int) * num;
    int i;

    /* if no elements, return a zero-dimensional array */
    if (num == 0)
    {
        ARR_NDIM(a) = 0;
        return a;
    }
    if (num == sizea) return a;

    a = (ArrayType *) repalloc(a, nbytes);

    SET_VARSIZE(a, nbytes);
    /* usually the array should be 1-D already, but just in case ... */
    for (i = 0; i < ARR_NDIM(a); i++)
    {
        ARR_DIMS(a)[i] = num;
        num = 1;
    }
    
    return a;
}

ArrayType * array_copy_real(ArrayType *a)
{
    ArrayType  *r;
    int        n = ArrayGetNItems(ARR_NDIM(a), ARR_DIMS(a));

    r = array_new_real(n);
    memcpy((float4*)ARR_DATA_PTR(r), (float4*)ARR_DATA_PTR(r), n * sizeof(float4));
    return r;
}



PG_FUNCTION_INFO_V1(c_array_greatest_real);
/****************************************************************************************************
 * Addition of vectors by elements - Σimax(Ai, Bi)
 * @param elements0 real[]  // IN OUT?
 * @param elements1 real[]  // IN OUT?
 */
Datum c_array_greatest_real(PG_FUNCTION_ARGS) {
    ArrayType*  vector0 = PG_GETARG_ARRAYTYPE_P_COPY(0); // result
    ArrayType*  vector1 = PG_GETARG_ARRAYTYPE_P(1); // operand
    
    float4*     ptr0 = (float4*) ARR_DATA_PTR(vector0);         // array data pointers
    float4*     ptr1 = (float4*) ARR_DATA_PTR(vector1);
    int         len  = MIN(ArrayGetNItems(ARR_NDIM(vector0), ARR_DIMS(vector0)),   // array lengths
                           ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1)));
    int         pos  = 0;           // array position

    for (pos = 0; pos < len; pos++) {
        ptr0[pos] =  MAX(ptr0[pos], ptr1[pos]);
    }
    
    PG_RETURN_ARRAYTYPE_P(vector0);
}


PG_FUNCTION_INFO_V1(c_array_least_real);
/****************************************************************************************************
 * Addition of vectors by elements - Σimin(Ai, Bi)
 * @param elements0 real[]  // IN OUT?
 * @param elements1 real[]  // IN OUT?
 */
Datum c_array_least_real(PG_FUNCTION_ARGS) {
    ArrayType*  vector0 = PG_GETARG_ARRAYTYPE_P_COPY(0); // result
    ArrayType*  vector1 = PG_GETARG_ARRAYTYPE_P(1); // operand
    
    float4*     ptr0 = (float4*) ARR_DATA_PTR(vector0);         // array data pointers
    float4*     ptr1 = (float4*) ARR_DATA_PTR(vector1);
    int         len  = MIN(ArrayGetNItems(ARR_NDIM(vector0), ARR_DIMS(vector0)),   // array lengths
                           ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1)));
    int         pos  = 0;           // array position

    for (pos = 0; pos < len; pos++) {
        ptr0[pos] =  MIN(ptr0[pos], ptr1[pos]);
    }
    
    PG_RETURN_ARRAYTYPE_P(vector0);
}



PG_FUNCTION_INFO_V1(c_array_add_real);
/****************************************************************************************************
 * Addition of vectors by elements - ΣAi + Bi
 * @param elements0 real[]  // INOUT
 * @param elements1 real[]  // IN
 */
Datum c_array_add_real(PG_FUNCTION_ARGS) {
    ArrayType*  vector0 = PG_GETARG_ARRAYTYPE_P_COPY(0); // result
    ArrayType*  vector1 = PG_GETARG_ARRAYTYPE_P(1); // operand
    
    float4*     ptr0 = (float4*) ARR_DATA_PTR(vector0);         // array data pointers
    float4*     ptr1 = (float4*) ARR_DATA_PTR(vector1);
    int         len  = MIN(ArrayGetNItems(ARR_NDIM(vector0), ARR_DIMS(vector0)),   // array lengths
                           ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1)));
    int         pos  = 0;           // array position

    for (pos = 0; pos < len; pos++) {
        ptr0[pos] +=  ptr1[pos];
    }
    
    PG_RETURN_ARRAYTYPE_P(vector0);
}


PG_FUNCTION_INFO_V1(c_array_sub_real);
/****************************************************************************************************
 * Subtraction of vectors by elements - ΣAi - Bi
 * @param elements0 real[]  // INOUT
 * @param elements1 real[]  // IN
 */
Datum c_array_sub_real(PG_FUNCTION_ARGS) {
    ArrayType*  vector0 = PG_GETARG_ARRAYTYPE_P_COPY(0); // result
    ArrayType*  vector1 = PG_GETARG_ARRAYTYPE_P(1); // operand
    
    float4*     ptr0 = (float4*) ARR_DATA_PTR(vector0);         // array data pointers
    float4*     ptr1 = (float4*) ARR_DATA_PTR(vector1);
    int         len  = MIN(ArrayGetNItems(ARR_NDIM(vector0), ARR_DIMS(vector0)),   // array lengths
                           ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1)));
    int         pos  = 0;           // array position

    for (pos = 0; pos < len; pos++) {
        ptr0[pos] -=  ptr1[pos];
    }
    
    PG_RETURN_ARRAYTYPE_P(vector0);
}


PG_FUNCTION_INFO_V1(c_array_mul_real);
/****************************************************************************************************
 * Multiplication of vectors by elements - ΣAi * Bi
 * @param elements0 real[]  // INOUT
 * @param elements1 real[]  // IN
 */
Datum c_array_mul_real(PG_FUNCTION_ARGS) {
    ArrayType*  vector0 = PG_GETARG_ARRAYTYPE_P_COPY(0); // result
    ArrayType*  vector1 = PG_GETARG_ARRAYTYPE_P(1); // operand
    
    float4*     ptr0 = (float4*) ARR_DATA_PTR(vector0);         // array data pointers
    float4*     ptr1 = (float4*) ARR_DATA_PTR(vector1);
    int         len  = MIN(ArrayGetNItems(ARR_NDIM(vector0), ARR_DIMS(vector0)),   // array lengths
                           ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1)));
    int         pos  = 0;           // array position

    for (pos = 0; pos < len; pos++) {
        ptr0[pos] *=  ptr1[pos];
    }
    
    PG_RETURN_ARRAYTYPE_P(vector0);
}


PG_FUNCTION_INFO_V1(c_array_div_real);
/****************************************************************************************************
 * Division of vectors by elements - ΣAi / Bi
 * WARNING: May return NaN!
 * @param elements0 real[]  // INOUT
 * @param elements1 real[]  // IN
 */
Datum c_array_div_real(PG_FUNCTION_ARGS) {
    ArrayType*  vector0 = PG_GETARG_ARRAYTYPE_P_COPY(0); // result
    ArrayType*  vector1 = PG_GETARG_ARRAYTYPE_P(1); // operand
    
    float4*     ptr0 = (float4*) ARR_DATA_PTR(vector0);         // array data pointers
    float4*     ptr1 = (float4*) ARR_DATA_PTR(vector1);
    int         len  = MIN(ArrayGetNItems(ARR_NDIM(vector0), ARR_DIMS(vector0)),   // array lengths
                           ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1)));
    int         pos  = 0;           // array position

    for (pos = 0; pos < len; pos++) {
        ptr0[pos] /=  ptr1[pos];
    }
    
    PG_RETURN_ARRAYTYPE_P(vector0);
}


PG_FUNCTION_INFO_V1(c_array_sqr_real);
/****************************************************************************************************
 * Square - multiplication of the vector by elements - ΣAi * Ai
 * @param elements0 real[]  // INOUT
 */
Datum c_array_sqr_real(PG_FUNCTION_ARGS) {
    ArrayType*  vector0 = PG_GETARG_ARRAYTYPE_P_COPY(0); // result
    
    float4*     ptr0 = (float4*) ARR_DATA_PTR(vector0);         // array data pointers
    int         len  = ArrayGetNItems(ARR_NDIM(vector0), ARR_DIMS(vector0));
    int         pos  = 0;           // array position

    for (pos = 0; pos < len; pos++) {
        ptr0[pos] *=  ptr0[pos];
    }
    
    PG_RETURN_ARRAYTYPE_P(vector0);
}


PG_FUNCTION_INFO_V1(c_array_sqrt_real);
/****************************************************************************************************
 * Sqare root of the vector by elements - ΣAi * Ai
 * WARNING: May return NaN!
 * @param elements0 real[]  // INOUT
 */
Datum c_array_sqrt_real(PG_FUNCTION_ARGS) {
    ArrayType*  vector0 = PG_GETARG_ARRAYTYPE_P_COPY(0); // result
    
    float4*     ptr0 = (float4*) ARR_DATA_PTR(vector0);         // array data pointers
    int         len  = ArrayGetNItems(ARR_NDIM(vector0), ARR_DIMS(vector0));
    int         pos  = 0;           // array position

    for (pos = 0; pos < len; pos++) {
        ptr0[pos] =  (float4)sqrt(ptr0[pos]);
    }
    
    PG_RETURN_ARRAYTYPE_P(vector0);
}



PG_FUNCTION_INFO_V1(c_array_acc_real);
/****************************************************************************************************
 * Average and standard deviation accumulator - ΣAi, ΣAi^2, Σi, -i (~ length checksum)
 * @param elements0 real[]  // INOUT
 * @param elements1 real[]  // IN
 */
Datum c_array_acc_real(PG_FUNCTION_ARGS) {
    ArrayType*  vector0 = PG_GETARG_ARRAYTYPE_P_COPY(0); // accumulated value
    ArrayType*  vector1 = PG_GETARG_ARRAYTYPE_P(1); // operand
    
    float4*     ptr0 = (float4*) ARR_DATA_PTR(vector0);         // array data pointers
    float4*     ptr1 = (float4*) ARR_DATA_PTR(vector1);
    int         len0 = ArrayGetNItems(ARR_NDIM(vector0), ARR_DIMS(vector0));   // array lengths
    int         len1 = ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1));
    int         pos  = 0;           // array position
    
    //#ifdef _DEBUG
    //    ereport(NOTICE, (111111, errmsg("c_array_avg_std_acc_real() len0: %d; len1: %d", len0, len1)));
    //#endif
        
    // len0 should be 3*len1 +1 ... if not, make it happen :)
    if (len0 == len1) {
        vector0 = array_resize_real(vector0, 3*len0 +1);
        ptr0 = (float4*) ARR_DATA_PTR(vector0);
        len0 = ArrayGetNItems(ARR_NDIM(vector0), ARR_DIMS(vector0));
        
        // create a checksum number
        ptr0[len0 -1] = -len1;
        
        // init the std and counts 
        for(pos = 0; pos < len1; pos++) {
            ptr0[pos +   len1] = ptr0[pos]*ptr0[pos];
            ptr0[pos + 2*len1] = 1;
        }
    }
    
    if (len0 == 3*len1 +1) {
        for (pos = 0; pos < len1; pos++) {
            ptr0[pos         ] += ptr1[pos];
            ptr0[pos +   len1] += ptr1[pos]*ptr1[pos];
            ptr0[pos + 2*len1] ++;
        }
    }
    else {
        #ifdef _DEBUG
            ereport(WARNING, (111111, errmsg("c_array_acc_real() length0: %d != (3*length1 +1): %d", len0, len1))); 
        #endif
    }
    
    PG_RETURN_ARRAYTYPE_P(vector0);
}



PG_FUNCTION_INFO_V1(c_array_avg_final);
/****************************************************************************************************
 * Average of vectors final
 * @param elements0 real[]    // INOUT
  */
Datum 
c_array_avg_final(PG_FUNCTION_ARGS) {
    ArrayType*  vector0 = PG_GETARG_ARRAYTYPE_P(0); 
    ArrayType*  result; // result
    
    float4*   ptr0 = (float4*) ARR_DATA_PTR(vector0);         // array data pointers
    int       len0 = ArrayGetNItems(ARR_NDIM(vector0), ARR_DIMS(vector0));   // array lengths
    int       len1 = -(int)ROUND(ptr0[len0 -1]);
    int       pos;           // array position

    // if there is the proper size
    if ((len0 -1)/3 == len1 && len1 > 0) {
        result = array_new_real(len1);
        float4*   ptr1 = (float4*) ARR_DATA_PTR(result);
        
        for (pos = 0; pos < len1; pos++) {
            ptr1[pos] = ptr0[pos] / ptr0[pos + 2*len1];
        }
        
        PG_RETURN_ARRAYTYPE_P(result);
    }
    // Well, this might happen if there was just a single row in the aggregate
    else {
        #ifdef _DEBUG        
            ereport(WARNING, (111111, errmsg("c_array_avg_final() length0: %d != (3*length1 +1): %d", len0, len1)));
        #endif
        PG_RETURN_ARRAYTYPE_P(vector0);
    }
}


PG_FUNCTION_INFO_V1(c_array_std_final);
/****************************************************************************************************
 * Standard deviation of vectors final
 * @param elements0 real[]    // INOUT
  */
Datum 
c_array_std_final(PG_FUNCTION_ARGS) {
    ArrayType*  vector0 = PG_GETARG_ARRAYTYPE_P(0); 
    ArrayType*  result; // result
    
    float4*   ptr0 = (float4*) ARR_DATA_PTR(vector0);         // array data pointers
    int       len0 = ArrayGetNItems(ARR_NDIM(vector0), ARR_DIMS(vector0));   // array lengths
    int       len1 = -(int)ROUND(ptr0[len0 -1]);
    int       pos;           // array position

    // if there is the proper size
    if ((len0 -1)/3 == len1 && len1 > 0) {
        result = array_new_real(len1);
        float4*   ptr1 = (float4*) ARR_DATA_PTR(result);
        
        for (pos = 0; pos < len1; pos++) {
            ptr1[pos] = ptr0[pos] / ptr0[pos + 2*len1]; // avg
            
            float4 var = ptr0[pos + len1] / ptr0[pos + 2*len1] - (ptr1[pos] * ptr1[pos]); // variance
            if (var > 0) ptr1[pos] = sqrt(var);
            else         ptr1[pos] = 0;
        }
        
        PG_RETURN_ARRAYTYPE_P(result);
    }
    // well, this might happen if there was just a single row in the aggregate
    else {
        #ifdef _DEBUG
                ereport(WARNING, (111111, errmsg("c_array_std_final() length0: %d != 3*(length1 +1): %d", len0, len1)));
        #endif
        result = array_new_real(len0);
        float4*   ptr1 = (float4*) ARR_DATA_PTR(result);
        
        for (pos = 0; pos < len0; pos++) {
            ptr1[pos] = 0;
        }
        
        PG_RETURN_ARRAYTYPE_P(result);
    }
}




/****************************************************************************************************
 * Document Retrieval Functions
 ****************************************************************************************************/

PG_FUNCTION_INFO_V1(c_rating_normalize_vect);
/*
 * Perform the normalization of vector. 
 * @param vector float[]
 */
Datum 
c_rating_normalize_vect(PG_FUNCTION_ARGS) {
    ArrayType*  vector = PG_GETARG_ARRAYTYPE_P(0);

    int         length = ArrayGetNItems(ARR_NDIM(vector), ARR_DIMS(vector));   // array lengths

    float4*     ptr = (float4*)ARR_DATA_PTR(vector);         // array data pointers  
    int         pos = 0;           // array position
    float4      norm = 0;          // norm for the normalization

    #ifdef _DEBUG
        ereport(NOTICE, (111111, errmsg("c_rating_normalize_vect length: %d \r\n", length)));
    #endif
    
    // L2 norm(x) = sqrt( Sum[(xi)^2] )
    //                  i
    // go through the vector
    while (pos < length) {    
        #ifdef _DEBUG
            ereport(NOTICE, (111112, errmsg("c_rating_normalize_vect pos: %d (%f) \r\n", pos, ptr[pos])));
        #endif        
            
        norm += (ptr[pos] * ptr[pos]);
        pos++; 
    } // go through the vector

    // sqrt
    norm = sqrt(norm);

    #ifdef _DEBUG
        ereport(NOTICE, (111115, errmsg("c_rating_normalize_vect return norm: %f \r\n", norm)));
    #endif
            
    PG_RETURN_FLOAT4(norm);
}


PG_FUNCTION_INFO_V1(c_rating_cosine_norm);
/*
 * Counts cosine rating of two vectors using the pre-counted norm (recomended). 
 * @param elements1 int4[]
 * @param weights1 float[]
 * @param norm1 float  
 * @param elements2 int4[]
 * @param weights2 float[]
 * @param norm2 float 
 */
Datum 
c_rating_cosine_norm(PG_FUNCTION_ARGS) {
    ArrayType*  vector1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType*  weight1 = PG_GETARG_ARRAYTYPE_P(0);
    float4      norm1 = PG_GETARG_FLOAT4(2);

    ArrayType*  vector2 = PG_GETARG_ARRAYTYPE_P(3);
    ArrayType*  weight2 = PG_GETARG_ARRAYTYPE_P(4);
    float4      norm2 = PG_GETARG_FLOAT4(5);

    int         length1 = ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1));   // array lengths
    int         length2 = ArrayGetNItems(ARR_NDIM(vector2), ARR_DIMS(vector2));
    
    // check length of weights - for SIGSEGV :)
    if ( ( length1 > ArrayGetNItems(ARR_NDIM(weight1), ARR_DIMS(weight1) )) || ( length2 > ArrayGetNItems(ARR_NDIM(weight2), ARR_DIMS(weight2) )) ) {
        ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION),
                       errmsg("weight arrays must be of the same size as key arrays")));
    }

    int32*      ptr1 = (int32*) ARR_DATA_PTR(vector1);         // array data pointers
    float4*     ptrw1 = (float4*) ARR_DATA_PTR(weight1);
    int32*      ptr2 = (int32*) ARR_DATA_PTR(vector2);
    float4*     ptrw2 = (float4*) ARR_DATA_PTR(weight2);    
    int         pos1 = 0;           // array position
    int         pos2 = 0;
    float4      rating = 0;         // result

    #ifdef _DEBUG
        ereport(NOTICE, (111111, errmsg("c_rating_cosine length1: %d length2: %d \r\n", length1, length2)));
    #endif
        
    //                  |dq.dd|
    //    r(dq, dd) = -----------
    //                 |dq|x|dd|
    //
    // go through the two vectors
    while (pos1 < length1 && pos2 < length2) {    

        #ifdef _DEBUG
            ereport(NOTICE, (111112, errmsg("c_rating_cosine pos1: %d (%d, %f) pos2: %d (%d, %f) \r\n", pos1, ptr1[pos1], ptrw1[pos1], pos2, ptr2[pos2], ptrw2[pos2])));
        #endif        
            
        // this is done for the first time and when values equals	
        if (ptr1[pos1] == ptr2[pos2]) {
            rating += (ptrw1[pos1] * ptrw2[pos2]);
            pos1++;
            pos2++;

	    #ifdef _DEBUG
                ereport(NOTICE, (111113, errmsg("ptr1[pos1] == ptr2[pos2] (pos1++ pos2++) \r\n")));
            #endif        
            continue;
        }
        // value of vector1 is smaller - set the next one
        else if(ptr1[pos1] < ptr2[pos2])
        {
            pos1++;

	    #ifdef _DEBUG
                ereport(NOTICE, (111113, errmsg("ptr1[pos1] < ptr2[pos2] (pos1++) \r\n")));
            #endif        
            continue;            
        }
        // cmpresult > 0 - try next position in vector2
        else
        {
            pos2++;

	    #ifdef _DEBUG
                ereport(NOTICE, (111113, errmsg("ptr1[pos1] > ptr2[pos2] (pos2++) \r\n")));
            #endif        
            continue;            
        }
    } // go through the two vectors

    // if the rating is 0 or it may cause division by 0, return 0
    if (rating == 0 || norm1 == 0 || norm2 == 0) PG_RETURN_FLOAT4(rating);

    // normalize
    rating /= (norm1 * norm2);

    #ifdef _DEBUG
        ereport(NOTICE, (111115, errmsg("c_rating_cosine return rating: %f \r\n", rating)));
    #endif
            
    PG_RETURN_FLOAT4(rating);
}


PG_FUNCTION_INFO_V1(c_rating_cosine);
/*
 * Counts cosine rating of two vectors.
 * @param elements1 int4[]
 * @param weights1 float[]
 * @param elements2 int4[]
 * @param weights2 float[]
 */
Datum 
c_rating_cosine(PG_FUNCTION_ARGS) {
    ArrayType*  vector1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType*  weight1 = PG_GETARG_ARRAYTYPE_P(1);

    ArrayType*  vector2 = PG_GETARG_ARRAYTYPE_P(2);
    ArrayType*  weight2 = PG_GETARG_ARRAYTYPE_P(3);

    int         length1 = ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1));   // array lengths
    int         length2 = ArrayGetNItems(ARR_NDIM(vector2), ARR_DIMS(vector2));
    
    // check length of weights - for SIGSEGV :)
    if ( ( length1 > ArrayGetNItems(ARR_NDIM(weight1), ARR_DIMS(weight1) )) || ( length2 > ArrayGetNItems(ARR_NDIM(weight2), ARR_DIMS(weight2) )) ) {
        ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION),
                       errmsg("weight arrays must be of the same size as key arrays")));
    }

    int32*      ptr1 = (int32*) ARR_DATA_PTR(vector1);         // array data pointers
    float4*     ptrw1 = (float4*) ARR_DATA_PTR(weight1);
    int32*      ptr2 = (int32*) ARR_DATA_PTR(vector2);
    float4*     ptrw2 = (float4*) ARR_DATA_PTR(weight2);    
    int         pos1 = 0;           // array position
    int         pos2 = 0;
    float4      norm1 = 0;          // norms for the normalization
    float4      norm2 = 0;
    float4      rating = 0;         // result

    #ifdef _DEBUG
        ereport(NOTICE, (111111, errmsg("c_rating_cosine length1: %d length2: %d \r\n", length1, length2)));
    #endif
        
    //                  |dq.dd|
    //    r(dq, dd) = -----------
    //                 |dq|x|dd|
    //
    // go through the two vectors
    while (pos1 < length1 && pos2 < length2) {    

        #ifdef _DEBUG
            ereport(NOTICE, (111112, errmsg("c_rating_cosine pos1: %d (%d, %f) pos2: %d (%d, %f) \r\n", pos1, ptr1[pos1], ptrw1[pos1], pos2, ptr2[pos2], ptrw2[pos2])));
        #endif        
            
        // this is done for the first time and when values equals	
        if (ptr1[pos1] == ptr2[pos2]) {
            rating += (ptrw1[pos1] * ptrw2[pos2]);

            norm1 += (ptrw1[pos1] * ptrw1[pos1]);
            norm2 += (ptrw2[pos2] * ptrw2[pos2]);

            pos1++;
            pos2++;

	    #ifdef _DEBUG
                ereport(NOTICE, (111113, errmsg("ptr1[pos1] == ptr2[pos2] (pos1++ pos2++) \r\n")));
            #endif        
            continue;
        }
        // value of vector1 is smaller - set the next one
        else if(ptr1[pos1] < ptr2[pos2])
        {
            norm1 += (ptrw1[pos1] * ptrw1[pos1]);

            pos1++;

	    #ifdef _DEBUG
                ereport(NOTICE, (111113, errmsg("ptr1[pos1] < ptr2[pos2] (pos1++) \r\n")));
            #endif        
            continue;            
        }
        // cmpresult > 0 - try next position in vector2
        else
        {
            norm2 += (ptrw2[pos2] * ptrw2[pos2]);

            pos2++;

	    #ifdef _DEBUG
                ereport(NOTICE, (111113, errmsg("ptr1[pos1] > ptr2[pos2] (pos2++) \r\n")));
            #endif        
            continue;            
        }
    } // go through the two vectors

    // finish the vectors normalization (none if equal or one of them)
    int   i;
    for (i = pos1; i < length1; i++) norm1 += (ptrw1[i] * ptrw1[i]);
    for (i = pos2; i < length2; i++) norm2 += (ptrw2[i] * ptrw2[i]);
    
    #ifdef _DEBUG
        ereport(NOTICE, (111114, errmsg("c_rating_cosine rating: %f norm1: %f norm2: %f (pos1: %d pos2: %d) \r\n", rating, sqrt(norm1), sqrt(norm2), pos1, pos2)));
    #endif

    // if the rating is 0 or it may cause division by 0, return 0
    if (rating == 0 || norm1 == 0 || norm2 == 0) PG_RETURN_FLOAT4(rating);

    // normalize
    rating /= (sqrt(norm1) * sqrt(norm2));

    #ifdef _DEBUG
        ereport(NOTICE, (111115, errmsg("c_rating_cosine return rating: %f \r\n", rating)));
    #endif
            
    PG_RETURN_FLOAT4(rating);
}



PG_FUNCTION_INFO_V1(c_rating_boolean_int);
/****************************************************************************************************
 * Counts boolean rating of two vectors.
 * @param elements1 int4[]
 * @param elements2 int4[]
 */
Datum 
c_rating_boolean_int(PG_FUNCTION_ARGS) {
    ArrayType*   vector1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType*   vector2 = PG_GETARG_ARRAYTYPE_P(1);
    
    int          length1 = ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1));   // array lengths
    int          length2 = ArrayGetNItems(ARR_NDIM(vector2), ARR_DIMS(vector2));
    int32*       ptr1 = (int32*) ARR_DATA_PTR(vector1);         // array data pointers
    int32*       ptr2 = (int32*) ARR_DATA_PTR(vector2);
    int          pos1 = 0;           // array position
    int          pos2 = 0;
    int32        rating = 0;         // result

    #ifdef _DEBUG
        ereport(NOTICE, (111111, errmsg("c_rating_boolean_int length1: %d length2: %d", length1, length2)));
    #endif
    
    //
    // r(dq, dd) = |dq.dd|
    //
    // go through the two vectors
    while (pos1 < length1 && pos2 < length2) {    

        #ifdef _DEBUG
            ereport(NOTICE, (111111, errmsg("c_rating_boolean_impl pos1: %d (%d) pos2: %d (%d)", pos1, ptr1[pos1], pos2, ptr2[pos2])));
        #endif
            
        // this is done for the first time and when values equals	
        if (ptr1[pos1] == ptr2[pos2]) {
            rating++;
            
            pos1++;
            pos2++;
            continue;
        }
        // value of vector1 is smaller - set the next one
        else if(ptr1[pos1] < ptr2[pos2])
        {
            pos1++;
            continue;            
        }
        // cmpresult > 0 - try next position in vector2
        else
        {
            pos2++;
            continue;            
        }
    } // go through the two vectors
    
    PG_RETURN_INT32(rating);
}



PG_FUNCTION_INFO_V1(c_rating_boolean);
/*****************************************************************************************************
 * Counts the boolean rating of two vectors (equals to the number of identical elements).
 * Array must not contain any NULL elements nor be NULL itself.
 * @param anyarray1[]
 * @param anyarray2[]
 */
Datum c_rating_boolean(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT32(c_rating_boolean_anyarray(fcinfo));
}

// just an implementation
// TODO: CHECK!
int32 c_rating_boolean_anyarray(FunctionCallInfo fcinfo) {
    ArrayType*  array1 = PG_GETARG_ARRAYTYPE_P(0);          // arrays to be compared
    ArrayType*  array2 = PG_GETARG_ARRAYTYPE_P(1);
    int32       ndims1 = ARR_NDIM(array1);                  // temporary dimension info
    int32       ndims2 = ARR_NDIM(array2);
    int32*      dims1 = ARR_DIMS(array1);
    int32*      dims2 = ARR_DIMS(array2);
    int32       length1 = ArrayGetNItems(ndims1, dims1);    // array lengths
    int32       length2 = ArrayGetNItems(ndims2, dims2);
    Oid         element_type = ARR_ELEMTYPE(array1);        // type of the array - int4 ~ 23, float4 ~ 800
    TypeCacheEntry* typentry;
    FunctionCallInfoData locfcinfo;
    bool        typbyval;
    int32       typlen;
    char        typalign;
    char*       ptr1 = ARR_DATA_PTR(array1);
    char*       ptr2 = ARR_DATA_PTR(array2);
    Datum       elt1;           // loop temps
    Datum       elt2;
    int32       cmpresult = 0;  // loop temp to hold information about compared info
    int32       pos1 = 0;      // array position (for the size info)
    int32       pos2 = 0;
    int32       rating = 0;        // result

    #ifdef _DEBUG
        ereport(NOTICE, (111111, errmsg("c_rating_boolean_impl length1: %d length2: %d", length1, length2)));
    #endif
        
    // if zero size... zero same elements and return zero :)
    if (length1 == 0 || length2 == 0) PG_RETURN_INT32(0);
    
    // check types of elements
    if (element_type != ARR_ELEMTYPE(array2))
            ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
            errmsg("cannot compare arrays of different element types")));    
    
    /*
     * We arrange to look up the comparison function only once per series of
     * calls, assuming the element type doesn't change underneath us. The
     * typcache is used so that we have no memory leakage when being used as
     * an index support function.
     */
    typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;
    if (typentry == NULL || typentry->type_id != element_type)
    {
        typentry = lookup_type_cache(element_type, TYPECACHE_CMP_PROC_FINFO);
        if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
                ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION),
                errmsg("could not identify a comparison function for type %i", format_type_be(element_type))));
        fcinfo->flinfo->fn_extra = (void *) typentry;
    }
    typlen = typentry->typlen;
    typbyval = typentry->typbyval;
    typalign = typentry->typalign;

    // apply the operator to each pair of array elements.
    InitFunctionCallInfoData(locfcinfo, &typentry->cmp_proc_finfo, 2, NULL, NULL, NULL); // FIXME: LAST NULL!!!!!

    //
    // r(dq, dd) = |dq.dd|
    //
    // go through the two vectors
    while (pos1 < length1 && pos2 < length2) {
        // get the elements
        elt1 = fetch_att(ptr1, typbyval, typlen);
        elt2 = fetch_att(ptr2, typbyval, typlen);
        
        // Compare the pairs of elements
        locfcinfo.arg[0] = elt1;
        locfcinfo.arg[1] = elt2;
        locfcinfo.argnull[0] = false;
        locfcinfo.argnull[1] = false;
        locfcinfo.isnull = false;
        cmpresult = DatumGetInt32(FunctionCallInvoke(&locfcinfo));  // store the cmpresult to the next round
        
        #ifdef _DEBUG
            ereport(NOTICE, (111111, errmsg("c_rating_boolean_impl pos1: %d pos2: %d cmpresult: %d", pos1, pos2, cmpresult)));
        #endif

        // this is done for the first time and when values equals	
        if (cmpresult == 0) {
            pos1++;
            ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
            ptr1 = (char *) att_align_nominal(ptr1, typalign);
            
            pos2++;
            ptr2 = att_addlength_pointer(ptr2, typlen, ptr2);
            ptr2 = (char *) att_align_nominal(ptr2, typalign);            

            rating++;
            continue;
        }
        // value of vector1 is smaller - set the next one
        else if(cmpresult < 0)
        {
            pos1++;
            ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
            ptr1 = (char *) att_align_nominal(ptr1, typalign);            
            
            continue;            
        }
        // cmpresult > 0 - try next position in vector2
        else
        {
            pos2++;
            ptr2 = att_addlength_pointer(ptr2, typlen, ptr2);
            ptr2 = (char *) att_align_nominal(ptr2, typalign);            
            
            continue;            
        }
    } // go through the two vectors
    
    PG_RETURN_INT32(rating);
}


PG_FUNCTION_INFO_V1(c_distance_square_int);
/****************************************************************************************************
 * Counts square distance of two vectors.
 * @param elements1 int4[]
 * @param elements2 int4[]
 */
Datum 
c_distance_square_int(PG_FUNCTION_ARGS) {
    ArrayType*   vector1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType*   vector2 = PG_GETARG_ARRAYTYPE_P(1);
    
    int          length = ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1));   // array lengths
    if (length != ArrayGetNItems(ARR_NDIM(vector2), ARR_DIMS(vector2))) {
        ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION),
                        errmsg("both arrays must be of the same size")));
    }
    
    int32*       ptr1 = (int32*) ARR_DATA_PTR(vector1);         // array data pointers
    int32*       ptr2 = (int32*) ARR_DATA_PTR(vector2);
    int          pos = 0;            // array position
    int64        distance = 0;       // result

    #ifdef _DEBUG
        ereport(NOTICE, (111111, errmsg("c_distance_square_int length: %d (%ld)", length, distance)));
    #endif
    
    // Euclidean distance without sqrt() normalization (square distance):
    // d(x, y) = Sum[ (xi - yi)^2 ]
    //            i
    //
    // go through the two vectors
    for (pos = 0; pos < length; pos++) {
        int64 diff = ptr1[pos] - ptr2[pos];
        distance += (diff * diff);
        #ifdef _DEBUG
            ereport(NOTICE, (111111, errmsg("c_distance_square_int pos: %d (%d, %d), diff: %ld^2=%ld, distance: %ld", length, ptr1[pos], ptr2[pos], diff, (diff*diff), distance)));
        #endif
    } // go through the two vectors

    PG_RETURN_INT64(distance);
}


PG_FUNCTION_INFO_V1(c_distance_square_real);
/****************************************************************************************************
 * Counts square distance of two vectors.
 * @param elements1 int4[]
 * @param elements2 int4[]
 */
Datum 
c_distance_square_real(PG_FUNCTION_ARGS) {
    ArrayType*   vector1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType*   vector2 = PG_GETARG_ARRAYTYPE_P(1);
    
    int          length = ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1));   // array lengths
    if (length != ArrayGetNItems(ARR_NDIM(vector2), ARR_DIMS(vector2))) {
        ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION),
                        errmsg("both arrays must be of the same size")));
    }
    
    float4*     ptr1 = (float4*) ARR_DATA_PTR(vector1);         // array data pointers
    float4*     ptr2 = (float4*) ARR_DATA_PTR(vector2);
    int         pos = 0;            // array position
    float4      distance = 0;       // result

    #ifdef _DEBUG
        ereport(NOTICE, (111111, errmsg("c_distance_square_real length: %d", length)));
    #endif
    
    // Euclidean distance without sqrt() normalization (square distance):
    // d(x, y) = Sum[ (xi - yi)^2 ]
    //            i
    //
    // go through the two vectors
    for (pos = 0; pos < length; pos++) {
        float4 diff = ptr1[pos] - ptr2[pos];
        distance += (diff * diff);
        #ifdef _DEBUG
            ereport(NOTICE, (111111, errmsg("c_distance_square_real pos: %d (%f, %f), diff: %f^2=%f, distance: %f", pos, ptr1[pos], ptr2[pos], diff, (diff*diff), distance)));
        #endif
    } // go through the two vectors

    PG_RETURN_FLOAT8(distance);
}


PG_FUNCTION_INFO_V1(c_distance_manhattan_int);
/****************************************************************************************************
 * Counts Manhattan distance(Minkowski distance - L1) of two vectors.
 * @param elements1 int4[]
 * @param elements2 int4[]
 */
Datum 
c_distance_manhattan_int(PG_FUNCTION_ARGS) {
    ArrayType*   vector1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType*   vector2 = PG_GETARG_ARRAYTYPE_P(1);
    
    int          length = ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1));   // array lengths
    if (length != ArrayGetNItems(ARR_NDIM(vector2), ARR_DIMS(vector2))) {
        ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION),
                        errmsg("both arrays must be of the same size")));
    }
    
    int32*       ptr1 = (int32*) ARR_DATA_PTR(vector1);         // array data pointers
    int32*       ptr2 = (int32*) ARR_DATA_PTR(vector2);
    int          pos = 0;            // array position
    int64        distance = 0;       // result

    #ifdef _DEBUG
        ereport(NOTICE, (111111, errmsg("c_distance_manhattan_int length: %d (%ld)", length, distance)));
    #endif
    
    // Manhattan distance:
    // d(x, y) = Sum ( |xi - yi| )
    //            i
    //
    // go through the two vectors
    for (pos = 0; pos < length; pos++) {
        int64 diff = ABS(ptr1[pos] - ptr2[pos]);
        distance += diff;
        #ifdef _DEBUG
            ereport(NOTICE, (111111, errmsg("c_distance_manhattan_int pos: %d (%d, %d), diff: %ld^2=%ld, distance: %ld", pos, ptr1[pos], ptr2[pos], diff, (diff*diff), distance)));
        #endif
    } // go through the two vectors

    PG_RETURN_INT64(distance);
}


PG_FUNCTION_INFO_V1(c_distance_chessboard_int);
/****************************************************************************************************
 * Counts Chebyshev distance(Minkowski distance - Lmax or called Chessboard distance) of two vectors.
 * @param elements1 int4[]
 * @param elements2 int4[]
 */
Datum 
c_distance_chessboard_int(PG_FUNCTION_ARGS) {
    ArrayType*   vector1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType*   vector2 = PG_GETARG_ARRAYTYPE_P(1);
    
    int          length = ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1));   // array lengths
    if (length != ArrayGetNItems(ARR_NDIM(vector2), ARR_DIMS(vector2))) {
        ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION),
                        errmsg("both arrays must be of the same size")));
    }
    
    int32*       ptr1 = (int32*) ARR_DATA_PTR(vector1);         // array data pointers
    int32*       ptr2 = (int32*) ARR_DATA_PTR(vector2);
    int          pos = 0;            // array position
    int64        distance = 0;       // result

    #ifdef _DEBUG
        ereport(NOTICE, (111111, errmsg("c_distance_chessboard_int length: %d (%ld)", length, distance)));
    #endif
    
    // Chebyshev distance:
    // d(x, y) = max (|xi - yi|)
    //            i
    //
    // go through the two vectors
    for (pos = 0; pos < length; pos++) {
        int64 diff = ABS(ptr1[pos] - ptr2[pos]);
        if (diff > distance) {
          distance = diff;
        }
        #ifdef _DEBUG
            ereport(NOTICE, (111111, errmsg("c_distance_chessboard_int pos: %d (%d, %d), diff: %ld, distance: %ld", pos, ptr1[pos], ptr2[pos], diff, distance)));
        #endif
    } // go through the two vectors

    PG_RETURN_INT64(distance);
}


PG_FUNCTION_INFO_V1(c_distance_mahalanobis_int);
/****************************************************************************************************
 * Counts Mahalanobis distance of two vectors.
 * @param elements1 int4[]
 * @param elements2 int4[]
 * @param StandardDeviation float4[] 
 */
Datum 
c_distance_mahalanobis_int(PG_FUNCTION_ARGS) {
    ArrayType*   vector1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType*   vector2 = PG_GETARG_ARRAYTYPE_P(1);
    ArrayType*   stdev   = PG_GETARG_ARRAYTYPE_P(2);
    
    int          length = ArrayGetNItems(ARR_NDIM(vector1), ARR_DIMS(vector1));   // array lengths
    if (length != ArrayGetNItems(ARR_NDIM(vector2), ARR_DIMS(vector2))) {
        ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION),
                        errmsg("both arrays must be of the same size")));
    }
    
    int32*       ptr1 = (int32*) ARR_DATA_PTR(vector1);         // array data pointers
    int32*       ptr2 = (int32*) ARR_DATA_PTR(vector2);
    float4*      ptr_stdev = (float4*) ARR_DATA_PTR(stdev);
    int          pos = 0;            // array position
    float4       sq_diff = 0;
    float4       variance = 0;
    float4       distance = 0;  

    #ifdef _DEBUG
        ereport(NOTICE, (111111, errmsg("c_distance_mahalanobis_int length: %d (%f)", length, distance)));
    #endif
    
    //  Standard deviation(as argument):
    //  sigma = sqrt( 1/N * Sum (pi - mean)^2 ) = sqrt ( (1/N * Sum (pi^2)) - mean^2 ) 
    //                       i                                  i
    //
    // Mahalanobis distance without sqrt():
    // d(x, y) = Sum [ (xi - yi)^2 ] / sigmai^2
    //            i
    //
    // go through the two vectors
    for (pos = 0; pos < length; pos++) {
          // Sum [ (pi - qi)^2 ]  - for count of Mahalanobis distance
        int64 diff = ptr1[pos] - ptr2[pos];
        sq_diff += ((diff * diff));
        variance = ptr_stdev[pos] * ptr_stdev[pos];
        
        //if (variance == 0) PG_RETURN_FLOAT8(variance);
        
          // Mahalanobis distance:
        distance += sq_diff / variance;
        
        #ifdef _DEBUG
              // TODO: upravit !!
            ereport(NOTICE, (111111, errmsg("c_distance_mahalanobis_int pos: %d (%d, %d), diff: %ld^2=%ld, distance: %f", pos, ptr1[pos], ptr2[pos], diff, (diff*diff), distance)));
        #endif
    } // go through the two vectors

    PG_RETURN_FLOAT8(distance);
}

