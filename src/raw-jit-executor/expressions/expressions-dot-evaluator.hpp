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

#ifndef EXPRESSIONS_DOT_VISITOR_HPP_
#define EXPRESSIONS_DOT_VISITOR_HPP_

#include "common/common.hpp"
#include "plugins/plugins.hpp"
#include "util/raw-functions.hpp"
#include "expressions/expressions-generator.hpp"
#include "expressions/expressions-hasher.hpp"
//#include "values/expressionTypes.hpp"

//#ifdef DEBUG
#define DEBUG_DOT
//#endif

//===---------------------------------------------------------------------------===//
// "Visitor(s)" responsible for evaluating dot equality
//===---------------------------------------------------------------------------===//
class ExpressionDotVisitor : public ExprTandemVisitor
{
public:
	ExpressionDotVisitor(RawContext* const context,
			const OperatorState& currStateLeft,
			const OperatorState& currStateRight) :
			context(context), currStateLeft(currStateLeft),
			currStateRight(currStateRight) {}
	RawValue visit(expressions::IntConstant *e1,
			expressions::IntConstant *e2);
	RawValue visit(expressions::FloatConstant *e1,
			expressions::FloatConstant *e2);
	RawValue visit(expressions::BoolConstant *e1,
			expressions::BoolConstant *e2);
	RawValue visit(expressions::StringConstant *e1,
			expressions::StringConstant *e2);
	RawValue visit(expressions::InputArgument *e1,
			expressions::InputArgument *e2);
	RawValue visit(expressions::RecordProjection *e1,
			expressions::RecordProjection *e2);
	RawValue visit(expressions::IfThenElse *e1, expressions::IfThenElse *e2);
	RawValue visit(expressions::EqExpression *e1,
			expressions::EqExpression *e2);
	RawValue visit(expressions::NeExpression *e1,
			expressions::NeExpression *e2);
	RawValue visit(expressions::GeExpression *e1,
			expressions::GeExpression *e2);
	RawValue visit(expressions::GtExpression *e1,
			expressions::GtExpression *e2);
	RawValue visit(expressions::LeExpression *e1,
			expressions::LeExpression *e2);
	RawValue visit(expressions::LtExpression *e1,
			expressions::LtExpression *e2);
	RawValue visit(expressions::AddExpression *e1,
			expressions::AddExpression *e2);
	RawValue visit(expressions::SubExpression *e1,
			expressions::SubExpression *e2);
	RawValue visit(expressions::MultExpression *e1,
			expressions::MultExpression *e2);
	RawValue visit(expressions::DivExpression *e1,
			expressions::DivExpression *e2);
	RawValue visit(expressions::AndExpression *e1,
			expressions::AndExpression *e2);
	RawValue visit(expressions::OrExpression *e1,
			expressions::OrExpression *e2);
	RawValue visit(expressions::RecordConstruction *e1,
			expressions::RecordConstruction *e2);

private:
	RawContext* const context;
	const OperatorState& currStateLeft;
	const OperatorState& currStateRight;
};

#endif /* EXPRESSIONS_DOT_VISITOR_HPP_ */
