/*
 * This file contains software that has been made available under
 * The Mozilla Public License 1.1. Use and distribution hereof are
 * subject to the restrictions set forth therein.
 *
 * Copyright (c) 2003 TADA AB - Taby Sweden
 * All Rights Reserved
 */
#include <postgres.h>
#include <funcapi.h>
#include <utils/memutils.h>
#include <utils/numeric.h>

#include "pljava/type/Type_priv.h"
#include "pljava/type/TupleDesc.h"
#include "pljava/type/SingleRowWriter.h"

/*
 * void primitive type.
 */
static jclass s_SingleRowWriter_class;
static jmethodID s_SingleRowWriter_init;
static jmethodID s_SingleRowWriter_getTupleAndClear;
static TypeClass s_SingleRowWriterClass;
static HashMap s_cache;

/*
 * This function is a bit special in that it adds an additional parameter
 * to the parameter list (a java.sql.ResultSet implemented as a
 * SingleRowWriter) and calls a boolean method. It's assumed that the
 * SingleRowWriter has been initialized with values if the method returns
 * true. If so, the values are obtained in the form of a HeapTuple which in
 * turn is returned (as a Datum) from this method.
 * 
 * NOTE! It's an absolute prerequisite that the args argument has room for
 * one extra parameter.
 */
static Datum _SingleRowWriter_invoke(Type self, JNIEnv* env, jclass cls, jmethodID method, jvalue* args, PG_FUNCTION_ARGS)
{
	TupleDesc tupleDesc = TupleDesc_forOid(Type_getOid(self));
	jobject singleRowWriter = SingleRowWriter_create(env, tupleDesc);
	int numArgs = fcinfo->nargs;

	/* It's guaranteed that the args array has room for one more
	 * argument.
	 */
	args[numArgs].l = singleRowWriter;

	bool saveIcj = isCallingJava;
	isCallingJava = true;
	bool hasRow = ((*env)->CallStaticBooleanMethodA(env, cls, method, args) == JNI_TRUE);
	isCallingJava = saveIcj;

	Datum result = 0;
	if(hasRow)
	{
		/* Obtain tuple and return it as a Datum.
		 */
		HeapTuple tuple = SingleRowWriter_getTupleAndClear(env, singleRowWriter);
		TupleTableSlot* slot = TupleDescGetSlot(tupleDesc);
	    result = TupleGetDatum(slot, tuple);
	}
	else
		fcinfo->isnull = true;
	
	(*env)->DeleteLocalRef(env, singleRowWriter);
	return result;
}

jobject SingleRowWriter_create(JNIEnv* env, TupleDesc tupleDesc)
{
	if(tupleDesc == 0)
		return 0;

	jobject jtd = TupleDesc_create(env, tupleDesc);
	jobject result = PgObject_newJavaObject(env, s_SingleRowWriter_class, s_SingleRowWriter_init, jtd);
	(*env)->DeleteLocalRef(env, jtd);
	return result;
}

HeapTuple SingleRowWriter_getTupleAndClear(JNIEnv* env, jobject jrps)
{
	if(jrps == 0)
		return 0;

	bool saveIcj = isCallingJava;
	isCallingJava = true;
	jobject tuple = (*env)->CallObjectMethod(env, jrps, s_SingleRowWriter_getTupleAndClear);
	isCallingJava = saveIcj;
	if(tuple == 0)
		return 0;

	HeapTuple result = (HeapTuple)NativeStruct_getStruct(env, tuple);
	(*env)->DeleteLocalRef(env, tuple);
	return result;
}

static jvalue _SingleRowWriter_coerceDatum(Type self, JNIEnv* env, Datum nothing)
{
	jvalue result;
	result.j = 0L;
	return result;
}

static Datum _SingleRowWriter_coerceObject(Type self, JNIEnv* env, jobject nothing)
{
	return 0;
}

static Type SingleRowWriter_obtain(Oid typeId)
{
	/* Check to see if we have a cached version for this
	 * postgres type
	 */
	Type infant = (Type)HashMap_getByOid(s_cache, typeId);
	if(infant == 0)
	{
		infant = TypeClass_allocInstance(s_SingleRowWriterClass, typeId);
		HashMap_putByOid(s_cache, typeId, infant);
	}
	return infant;
}

/* Make this datatype available to the postgres system.
 */
extern Datum SingleRowWriter_initialize(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(SingleRowWriter_initialize);
Datum SingleRowWriter_initialize(PG_FUNCTION_ARGS)
{
	JNIEnv* env = (JNIEnv*)PG_GETARG_POINTER(0);

	s_SingleRowWriter_class = (*env)->NewGlobalRef(
				env, PgObject_getJavaClass(env, "org/postgresql/pljava/jdbc/SingleRowWriter"));

	s_SingleRowWriter_init = PgObject_getJavaMethod(
				env, s_SingleRowWriter_class, "<init>", "(Lorg/postgresql/pljava/internal/TupleDesc;)V");

	s_SingleRowWriter_getTupleAndClear = PgObject_getJavaMethod(
				env, s_SingleRowWriter_class, "getTupleAndClear", "()Lorg/postgresql/pljava/internal/Tuple;");

	s_cache = HashMap_create(13, TopMemoryContext);

	s_SingleRowWriterClass = TypeClass_alloc("type.SingleRowWriter");
	s_SingleRowWriterClass->JNISignature = "Ljava/sql/ResultSet;";
	s_SingleRowWriterClass->javaTypeName = "java.lang.ResultSet";
	s_SingleRowWriterClass->coerceDatum  = _SingleRowWriter_coerceDatum;
	s_SingleRowWriterClass->coerceObject = _SingleRowWriter_coerceObject;
	s_SingleRowWriterClass->invoke       = _SingleRowWriter_invoke;

	Type_registerJavaType("org.postgresql.pljava.jdbc.SingleRowWriter", SingleRowWriter_obtain);
	PG_RETURN_VOID();
}