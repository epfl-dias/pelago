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

#include "expressions/expressions.hpp"

namespace expressions {

BinaryExpression::~BinaryExpression() {}

RawValue IntConstant::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue FloatConstant::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue BoolConstant::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue StringConstant::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue InputArgument::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue RecordProjection::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue IfThenElse::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue RecordConstruction::accept(ExprVisitor &v) {
	return v.visit(this);
}

/**
 * The binary expressions
 */
RawValue EqExpression::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue NeExpression::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue GeExpression::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue GtExpression::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue LeExpression::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue LtExpression::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue AddExpression::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue SubExpression::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue MultExpression::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue DivExpression::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue AndExpression::accept(ExprVisitor &v) {
	return v.visit(this);
}

RawValue OrExpression::accept(ExprVisitor &v) {
	return v.visit(this);
}

/*XXX My responsibility to provide appropriate (i.e., compatible) input */
RawValue IntConstant::acceptTandem(ExprTandemVisitor &v, expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		Constant *rConst = dynamic_cast<Constant*>(expr);
		if (rConst->getConstantType() == INT) {
			IntConstant* rInt = dynamic_cast<IntConstant*>(expr);
			return v.visit(this, rInt);
		}
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue FloatConstant::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		Constant *rConst = dynamic_cast<Constant*>(expr);
		if (rConst->getConstantType() == FLOAT) {
			FloatConstant* rFloat = dynamic_cast<FloatConstant*>(expr);
			return v.visit(this, rFloat);
		}
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue BoolConstant::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		Constant *rConst = dynamic_cast<Constant*>(expr);
		if (rConst->getConstantType() == BOOL) {
			BoolConstant* rBool = dynamic_cast<BoolConstant*>(expr);
			return v.visit(this, rBool);
		}
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue StringConstant::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		Constant *rConst = dynamic_cast<Constant*>(expr);
		if (rConst->getConstantType() == STRING) {
			StringConstant* rString = dynamic_cast<StringConstant*>(expr);
			return v.visit(this, rString);
		}
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue InputArgument::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		InputArgument *rInputArg = dynamic_cast<InputArgument*>(expr);
		return v.visit(this, rInputArg);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue RecordProjection::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		RecordProjection *rRecordProjection =
				dynamic_cast<RecordProjection*>(expr);
		return v.visit(this, rRecordProjection);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue IfThenElse::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		IfThenElse *rIfThenElse = dynamic_cast<IfThenElse*>(expr);
		return v.visit(this, rIfThenElse);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue RecordConstruction::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		RecordConstruction *rRecordConstruction =
				dynamic_cast<RecordConstruction*>(expr);
		return v.visit(this, rRecordConstruction);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

/**
 * The binary expressions
 */

RawValue EqExpression::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		EqExpression *rEqExpression =
				dynamic_cast<EqExpression*>(expr);
		return v.visit(this, rEqExpression);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue NeExpression::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		NeExpression *rNeExpression =
				dynamic_cast<NeExpression*>(expr);
		return v.visit(this, rNeExpression);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue GtExpression::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		GtExpression *rGtExpression =
				dynamic_cast<GtExpression*>(expr);
		return v.visit(this, rGtExpression);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue GeExpression::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		GeExpression *rGeExpression =
				dynamic_cast<GeExpression*>(expr);
		return v.visit(this, rGeExpression);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue LeExpression::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		LeExpression *rLeExpression =
				dynamic_cast<LeExpression*>(expr);
		return v.visit(this, rLeExpression);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue LtExpression::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		LtExpression *rLtExpression =
				dynamic_cast<LtExpression*>(expr);
		return v.visit(this, rLtExpression);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue AddExpression::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		AddExpression *rAddExpression =
				dynamic_cast<AddExpression*>(expr);
		return v.visit(this, rAddExpression);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue SubExpression::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		SubExpression *rSubExpression =
				dynamic_cast<SubExpression*>(expr);
		return v.visit(this, rSubExpression);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue MultExpression::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		MultExpression *rMultExpression =
				dynamic_cast<MultExpression*>(expr);
		return v.visit(this, rMultExpression);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue DivExpression::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		DivExpression *rDivExpression =
				dynamic_cast<DivExpression*>(expr);
		return v.visit(this, rDivExpression);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue AndExpression::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		AndExpression *rAndExpression =
				dynamic_cast<AndExpression*>(expr);
		return v.visit(this, rAndExpression);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

RawValue OrExpression::acceptTandem(ExprTandemVisitor &v,
		expressions::Expression* expr) {
	if (this->getTypeID() == expr->getTypeID()) {
		OrExpression *rOrExpression =
				dynamic_cast<OrExpression*>(expr);
		return v.visit(this, rOrExpression);
	}
	string error_msg = string("[Tandem Visitor: ] Incompatible Pair");
	LOG(ERROR)<< error_msg;
	throw runtime_error(string(error_msg));
}

}
