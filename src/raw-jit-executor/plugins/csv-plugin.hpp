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

#include "plugins/plugins.hpp"
#include "util/atois.hpp"

class CSVPlugin	: public Plugin {
public:

	/**
	 * Fully eager CSV Plugin. Will probably include another one that only outputs 'tuple metadata' (starting and ending positions).
	 *
	 * Compared to the GO executor, we need more info to provide to the 'CSV helper' that will be responsible for parsing etc.
	 * This information will end up being attached in the query plan
	 */
	CSVPlugin(RawContext* const context, string& fname, RecordType& rec_, vector<RecordAttribute*>& whichFields);
	~CSVPlugin();
	virtual string& getName() { return fname; }
	void init();
	void generate(const RawOperator& producer);
	void finish();
	virtual RawValueMemory readPath(string activeRelation, Bindings bindings, const char* pathVar, RecordAttribute attr);
	virtual RawValueMemory readValue(RawValueMemory mem_value, const ExpressionType* type);
	virtual RawValue readCachedValue(CacheInfo info, const OperatorState& currState) {
		string error_msg = "[CSVPlugin: ] No caching support yet";
		LOG(ERROR) << error_msg;
		throw runtime_error(error_msg);
	}
	virtual RawValue readCachedValue(CacheInfo info,
			const map<RecordAttribute, RawValueMemory>& bindings) {
		string error_msg = "[CSVPlugin: ] No caching support yet";
		LOG(ERROR)<< error_msg;
		throw runtime_error(error_msg);
	}

	virtual RawValue hashValue(RawValueMemory mem_value, const ExpressionType* type);
	virtual RawValue hashValueEager(RawValue value, const ExpressionType* type);

	virtual void flushTuple(RawValueMemory mem_value, Value* fileName)	{
		string error_msg = "[CSVPlugin: ] Functionality not supported yet";
		LOG(ERROR)<< error_msg;
		throw runtime_error(error_msg);
	}

	virtual void flushValue(RawValueMemory mem_value, const ExpressionType *type, Value* fileName);
	virtual void flushValueEager(RawValue value, const ExpressionType *type, Value* fileName);

	virtual RawValueMemory initCollectionUnnest(RawValue val_parentObject) {
		string error_msg = "[CSVPlugin: ] CSV files do not contain collections";
		LOG(ERROR)<< error_msg;
		throw runtime_error(error_msg);
	}
	virtual RawValue collectionHasNext(RawValue val_parentObject,
			RawValueMemory mem_currentChild) {
		string error_msg = "[CSVPlugin: ] CSV files do not contain collections";
		LOG(ERROR)<< error_msg;
		throw runtime_error(error_msg);
	}
	virtual RawValueMemory collectionGetNext(RawValueMemory mem_currentChild) {
		string error_msg = "[CSVPlugin: ] CSV files do not contain collections";
		LOG(ERROR)<< error_msg;
		throw runtime_error(error_msg);
	}

	virtual Value* getValueSize(RawValueMemory mem_value, const ExpressionType* type);

//	virtual typeID getOIDSize() { return INT64; }

	virtual ExpressionType *getOIDType() {
		return new Int64Type();
	}

	virtual PluginType getPluginType() { return PGCSV; }

private:
	string& fname;
	off_t fsize;
	int fd;
	char *buf;
	//XXX Will remove as soon as skip(), skipDelim() etc become deprecated
	off_t pos;

	//Schema info provided
	RecordType& rec;
	vector<RecordAttribute*>& wantedFields;

	/**
	 * Code-generation-related
	 */
	//Used to store memory positions of offset, buf and filesize in the generated code
	map<string, AllocaInst*> NamedValuesCSV;
	RawContext* const context;

	const char* posVar;// = "offset";
	const char* bufVar;// = "fileBuffer";
	const char* fsizeVar;// = "fileSize";

	void skip();
	inline size_t skipDelim(size_t pos, char* buf, char delim);
	int readAsInt();
	int eof();

	//Used to generate code
	void skipDelimLLVM(Value* delim,Function* debugChar, Function* debugInt);
	void skipLLVM();
	void skipToEndLLVM();
	void getFieldEndLLVM();
	void readAsIntLLVM(RecordAttribute attName, map<RecordAttribute, RawValueMemory>& variables, Function* atoi_,Function* debugChar,Function* debugInt);
	void readAsIntLLVM(RecordAttribute attName, map<RecordAttribute, RawValueMemory>& variables);
	void readAsFloatLLVM(RecordAttribute attName, map<RecordAttribute, RawValueMemory>& variables, Function* atof_,Function* debugChar,Function* debugFloat);
	void readAsBooleanLLVM(RecordAttribute attName, map<RecordAttribute, RawValueMemory>& variables);

	//Generates a for loop that performs the file scan
	//No assumption on auxiliary structures yet
	void scanCSV(const RawOperator& producer, Function* debug);

};
