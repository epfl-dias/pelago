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

#ifndef _NEST_RADIX_HPP_
#define _NEST_RADIX_HPP_

#include "operators/operators.hpp"
#include "operators/monoids.hpp"
#include "expressions/expressions-generator.hpp"
#include "expressions/expressions-hasher.hpp"
#include "expressions/expressions-dot-evaluator.hpp"
#include "expressions/path.hpp"
#include "expressions/expressions.hpp"
#include "plugins/binary-internal-plugin.hpp"
#include "util/radix/aggregations/radix-aggr.hpp"

#define DEBUGRADIX_NEST
/**
 * Indicative query where a nest (..and an outer join) occur:
 * for (d <- Departments) yield set (D := d, E := for ( e <- Employees, e.dno = d.dno) yield set e)
 *
 * NEST requires aliases for the record arguments that are its results.
 *
 * TODO ADD MATERIALIZER / OUTPUT PLUGIN FOR NEST OPERATOR (?)
 * XXX  Hashing keys is not enough - also need to compare the actual keys
 *
 * TODO Different types of (input) collections enforce different dot equality requirements!!
 * Example: OID & 'CollectionID' (i.e., pgID) must participate in dot equality for elems for lists!!
 */
namespace radix {

/* valuePtr is relative to the payloadBuffer! */
typedef struct htEntry {
	size_t key;
	size_t valuePtr;
} htEntry;

struct relationBuf {
	/* Mem layout:
	 * Consecutive <payload> chunks - payload type defined at runtime
	 */
	AllocaInst *mem_relation;
	/* Size in bytes */
	AllocaInst *mem_tuplesNo;
	/* Size in bytes */
	AllocaInst *mem_size;
	/* (Current) Offset in bytes */
	AllocaInst *mem_offset;
};

struct kvBuf {
	/* Mem layout:
	 * Pairs of (size_t key, size_t payloadPtr)
	 */
	AllocaInst *mem_kv;
	/* Size in bytes */
	AllocaInst *mem_tuplesNo;
	/* Size in bytes */
	AllocaInst *mem_size;
	/* (Current) Offset in bytes */
	AllocaInst *mem_offset;
};

class Nest: public UnaryRawOperator {
public:
	Nest(RawContext* const context, vector<Monoid> accs,
			vector<expressions::Expression*> outputExprs,
			vector<string> aggrLabels,
			expressions::Expression *pred,
			expressions::Expression *f_grouping,
			expressions::Expression *g_nullToZero,
			RawOperator* const child, const char* opLabel, Materializer& mat);
	virtual ~Nest() {
		LOG(INFO)<<"Collapsing Nest operator";}
	virtual void produce();
	virtual void consume(RawContext* const context, const OperatorState& childState);
	Materializer& getMaterializer() {return mat;}
	virtual bool isFiltering() const {return true;}
private:
	void generateInsert(RawContext* context, const OperatorState& childState);
	/* Very similar to radix join building phase! */
	void buildHT(RawContext* context, const OperatorState& childState);
	void probeHT() const;
	Value* radix_cluster_nopadding(struct relationBuf rel, struct kvBuf ht) const;
	/**
	 * Once HT has been fully materialized, it is time to resume execution.
	 * Note: generateProbe (should) not require any info reg. the previous op that was called.
	 * Any info needed is (should be) in the HT that will now be probed.
	 */
	void generateProbe(RawContext* const context) const;
	void generateSum(expressions::Expression* outputExpr, RawContext* const context, const OperatorState& state, AllocaInst *mem_accumulating) const;
	void generateMul(expressions::Expression* outputExpr, RawContext* const context, const OperatorState& state, AllocaInst *mem_accumulating) const;
	void generateMax(expressions::Expression* outputExpr, RawContext* const context, const OperatorState& state, AllocaInst *mem_accumulating) const;
	void generateOr(expressions::Expression* outputExpr, RawContext* const context, const OperatorState& state, AllocaInst *mem_accumulating) const;
	void generateAnd(expressions::Expression* outputExpr, RawContext* const context, const OperatorState& state, AllocaInst *mem_accumulating) const;

	map<RecordAttribute, RawValueMemory>* reconstructResults(Value *htBuffer, Value *idx) const;
	/**
	 * We need a new accumulator for every resulting bucket of the HT
	 */
	AllocaInst* resetAccumulator(expressions::Expression* outputExpr, Monoid acc) const;
	void updateRelationPointers() const;

	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;
	expressions::Expression* pred;
	expressions::Expression* f_grouping;
	expressions::Expression* g_nullToZero;

	vector<string> aggregateLabels;

	const char *htName;
	Materializer& mat;
	RawContext* context;

	/**
	 * Relevant to radix-based HT
	 */
	/* What the keyType is */
	/* XXX int32 FOR NOW   */
	/* If it is not int32 to begin with, hash it to make it so */
	StructType *payloadType;
	Type *keyType;
	StructType *htEntryType;
	struct relationBuf relR;
	struct kvBuf htR;
	HT *HT_per_cluster;
	StructType *htClusterType;
	// Raw Buffers
	char *relationR;
	char **ptr_relationR;
	char *kvR;
};

}
#endif /* NEST_OPT_HPP_ */
