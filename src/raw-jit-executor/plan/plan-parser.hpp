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

#ifndef PLAN_PARSER_HPP_
#define PLAN_PARSER_HPP_

#include "common/common.hpp"
#include "util/raw-context.hpp"
#include "util/raw-functions.hpp"
#include "util/raw-caching.hpp"
#include "operators/operators.hpp"
#include "operators/scan.hpp"
#include "operators/select.hpp"
#include "operators/radix-join.hpp"
#include "operators/unnest.hpp"
#include "operators/outer-unnest.hpp"
#include "operators/print.hpp"
#include "operators/root.hpp"
#include "operators/reduce-opt.hpp"
#include "operators/radix-nest.hpp"
#include "operators/materializer-expr.hpp"
#include "plugins/csv-plugin-pm.hpp"
#include "plugins/binary-row-plugin.hpp"
#include "plugins/binary-col-plugin.hpp"
#include "plugins/json-plugin.hpp"
#include "values/expressionTypes.hpp"
#include "expressions/binary-operators.hpp"
#include "expressions/expressions.hpp"
#include "expressions/expressions-hasher.hpp"


using namespace rapidjson;
using namespace std;

typedef struct InputInfo	{
	string path;
	ExpressionType *exprType;
	//Used by materializing operations
	ExpressionType *oidType;
} InputInfo;

class ExpressionParser {
public:
	ExpressionParser() {};
	expressions::Expression* parseExpression(const rapidjson::Value& val);
	ExpressionType* 		 parseExpressionType(const rapidjson::Value& val);
	RecordAttribute* 		 parseRecordAttr(const rapidjson::Value& val);
	Monoid parseAccumulator(const char *acc);
};

class CatalogParser {
public:
	CatalogParser(const char *catalogPath);
	InputInfo *getInputInfo(string inputName)	{

		map<string,InputInfo*>::iterator it;
		it = inputs.find(inputName);
		if(it == inputs.end())	{
			string err = string("Unknown Input: ") + inputName;
			LOG(ERROR)<< err;
			throw runtime_error(err);
		}
		return it->second;
	}

	void setInputInfo(string inputName, InputInfo *info) {
		inputs[inputName] = info;
	}
private:
	ExpressionParser exprParser;
	map<string,InputInfo*> inputs;
};

class PlanExecutor {
public:
	PlanExecutor(const char *planPath, CatalogParser& cat, const char *moduleName = "llvmModule");
private:
	ExpressionParser exprParser;
	const char *planPath;
	const char *moduleName;
	CatalogParser& catalogParser;
	vector<Plugin*> activePlugins;

	RawContext ctx;
	void		 			 parsePlan(const rapidjson::Document& doc);
	/* When processing tree root, parent will be NULL */
	RawOperator* 			 parseOperator(const rapidjson::Value& val);
	expressions::Expression* parseExpression(const rapidjson::Value& val) {
		return exprParser.parseExpression(val);
	}
	ExpressionType* parseExpressionType(const rapidjson::Value& val) {
		return exprParser.parseExpressionType(val);
	}
	RecordAttribute* parseRecordAttr(const rapidjson::Value& val) {
		return exprParser.parseRecordAttr(val);
	}
	Monoid parseAccumulator(const char * acc) {
		return exprParser.parseAccumulator(acc);
	}

	Plugin* parsePlugin(const rapidjson::Value& val);
};




#endif /* PLAN_PARSER_HPP_ */
