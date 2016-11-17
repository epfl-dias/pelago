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

#ifndef PATH_HPP_
#define PATH_HPP_

#include "common/common.hpp"
#include "util/raw-catalog.hpp"
#include "plugins/plugins.hpp"

class Path	{
public:
	Path(string nestedName, expressions::RecordProjection* desugarizedPath) :
			desugarizedPath(desugarizedPath), nestedName(nestedName), mem_currentChild(NULL), val_parentColl(
					NULL) {
		RawCatalog& catalog = RawCatalog::getInstance();
		string originalRelation = desugarizedPath->getOriginalRelationName();
		pg = catalog.getPlugin(originalRelation);
	}

	void setParentCollection(Value* collection)							{ val_parentColl = collection; }
	expressions::RecordProjection* get() 								{ return desugarizedPath; }
	Plugin* getRelevantPlugin() 				const					{ return pg; }
	string getNestedName()						const					{ return nestedName; }
	string toString();
private:
	expressions::RecordProjection* const desugarizedPath;
	const AllocaInst* mem_currentChild;
	const Value* val_parentColl;
	string nestedName;
	Plugin* pg;
};

#endif /* PATH_HPP_ */
