/*
	RAW -- High-performance querying over raw, never-seen-before data.

							Copyright (c) 2014
		Data Intensive Applications and Systems Labaratory (DIAS)
				École Polytechnique Fédérale de Lausanne

							All Rights Reserved.

	Permission to use, copy, modify and distribute this software and
	its documentation is hereby granted, provided that both the
	copyright notice and this permission notice appear in all copies of
	the software, derivative works or modified versions, and any
	portions thereof, and that both notices appear in supporting
	documentation.

	This code is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
	DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
	RESULTING FROM THE USE OF THIS SOFTWARE.
*/

#ifndef PLUGINS_LLVM_HPP_
#define PLUGINS_LLVM_HPP_

#include "common/common.hpp"
#include "util/raw-context.hpp"
#include "util/raw-catalog.hpp"
#include "util/raw-caching.hpp"
#include "operators/operators.hpp"
#include "expressions/expressions.hpp"
#include "values/expressionTypes.hpp"

/* Leads to incomplete type */
class OperatorState;
class RawOperator;
//class RawValueMemory;
//class RawValue;

//Used by all plugins
static const string activeLoop = "activeTuple";

/**
 * In principle, every readPath() method should deal with a record.
 * For some formats/plugins, however, (e.g. CSV) projection pushdown makes significant
 * difference in performance.
 */
typedef struct Bindings	{
	const OperatorState* state;
	const RawValue record;
} Bindings;

enum PluginType	{ PGCSV, PGJSON, PGBINARY };
/**********************************/
/*  The abstract part of plug-ins */
/**********************************/
class Plugin {
public:
	virtual 			~Plugin() 														{ LOG(INFO) << "[PLUGIN: ] Collapsing plug-in"; }
	virtual 			string& getName() 												= 0;
	virtual void 		init() 															= 0;
	virtual void 		finish() 														= 0;
	virtual void 		generate(const RawOperator& producer) 							= 0;
	/**
	 * @param activeRelation Which relation's activeTuple is to be processed.
	 * 						 Does not have to be a native one
	 * 						 Relevant example:
	 * 						 for ( e <- employees, w <- employees.children ) yield ...
	 * 						 Active tuple at some point will be the one of "employees.children"
	 */
	virtual RawValueMemory readPath(string activeRelation,
			Bindings wrappedBindings, const char* pathVar, RecordAttribute attr)			= 0;
	virtual RawValueMemory readValue(RawValueMemory mem_value, const ExpressionType* type) 	= 0;
	virtual RawValue readCachedValue(CacheInfo info, const OperatorState& currState)		= 0;
	virtual RawValue readCachedValue(CacheInfo info, const map<RecordAttribute, RawValueMemory>& bindings) = 0;

	//Relevant for hashing visitors
	virtual RawValue hashValue(RawValueMemory mem_value, const ExpressionType* type)		= 0;
	/* Hash without re-interpreting. Mostly relevant if some value is cached
	 * Meant for primitive values! */
	virtual RawValue hashValueEager(RawValue value, const ExpressionType* type)	= 0;

	/**
	 * Not entirely sure which is the correct granularity for 'stuff to flush' here.
	 * XXX Atm we only have one (JSON) serializer, later on it'll have to be an argument
	 * XXX We probably also need sth for more isolated values
	 */
	/**
	 * We need an intermediate internal representation to deserialize to
	 * before serializing back.
	 *
	 * Example: How would one convert from CSV to JSON?
	 * Can't just crop CSV entries and flush as JSON
	 *
	 * 'Eager' variation: Caching related. Separates eager from lazy plugins (i.e., all vs. JSON atm)
	 * XXX 'Eager' flushing not tested
	 */
	virtual void flushTuple(RawValueMemory mem_value, Value* fileName) = 0;
	virtual void flushValue(RawValueMemory mem_value, const ExpressionType *type, Value* fileName) = 0;
	virtual void flushValueEager(RawValue value, const ExpressionType *type, Value* fileName) = 0;

	/**
	 * Relevant for collections' unnesting
	 */
	virtual RawValueMemory initCollectionUnnest(RawValue val_parentObject) = 0;
	virtual RawValue collectionHasNext(RawValue val_parentObject, RawValueMemory mem_currentChild) = 0;
	virtual RawValueMemory collectionGetNext(RawValueMemory mem_currentChild) = 0;

	/**
	 * Relevant when needed to materialize EAGERLY.
	 * Otherwise, allocated type info suffices
	 *
	 * (i.e., not used that often)
	 */
	virtual Value* getValueSize(RawValueMemory mem_value, const ExpressionType* type) = 0;

//	virtual typeID getOIDSize() = 0;
	virtual ExpressionType *getOIDType() = 0;
	virtual PluginType getPluginType() = 0;
};
#endif /* PLUGINS_LLVM_HPP_ */
