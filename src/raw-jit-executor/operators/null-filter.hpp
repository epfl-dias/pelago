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

#include "operators/operators.hpp"
#include "expressions/expressions-generator.hpp"

/*
 * Minimal operator. Performs subset of Nest's functionality
 * Used to filter out NULL bindings w/o going through a heavy nesting operation */
class NullFilter : public UnaryRawOperator {
public:
	NullFilter(expressions::InputArgument* expr, RawOperator* const child) :
		  UnaryRawOperator(child), expr(expr) 							{}
	virtual ~NullFilter() 												{ LOG(INFO) << "Collapsing null_filter operator"; }

	virtual void produce();
	virtual void consume(RawContext* const context, const OperatorState& childState);
	virtual bool isFiltering() const {return true;}
private:
	expressions::InputArgument* expr;
	void generate(RawContext* const context, const OperatorState& childState) const;
};
