#include "plan/plan-parser.hpp"

/* too primitive */
struct PlanHandler {
    bool Null() { cout << "Null()" << endl; return true; }
    bool Bool(bool b) { cout << "Bool(" << std::boolalpha << b << ")" << endl; return true; }
    bool Int(int i) { cout << "Int(" << i << ")" << endl; return true; }
    bool Uint(unsigned u) { cout << "Uint(" << u << ")" << endl; return true; }
    bool Int64(int64_t i) { cout << "Int64(" << i << ")" << endl; return true; }
    bool Uint64(uint64_t u) { cout << "Uint64(" << u << ")" << endl; return true; }
    bool Double(double d) { cout << "Double(" << d << ")" << endl; return true; }
    bool String(const char* str, SizeType length, bool copy) {
        cout << "String(" << str << ", " << length << ", " << std::boolalpha << copy << ")" << std::endl;
        return true;
    }
    bool StartObject() { cout << "StartObject()" << endl; return true; }
    bool Key(const char* str, SizeType length, bool copy) {
        cout << "Key(" << str << ", " << length << ", " << std::boolalpha << copy << ")" << std::endl;
        return true;
    }
    bool EndObject(SizeType memberCount) { cout << "EndObject(" << memberCount << ")" << endl; return true; }
    bool StartArray() { cout << "StartArray()" << endl; return true; }
    bool EndArray(SizeType elementCount) { cout << "EndArray(" << elementCount << ")" << endl; return true; }
};

PlanExecutor::PlanExecutor(const char *planPath, CatalogParser& cat, const char *moduleName) :
		planPath(planPath), moduleName(moduleName), ctx(RawContext(moduleName)), catalogParser(cat) {

	/* Init LLVM Context and catalog */
	//ctx = RawContext(this->moduleName);
	registerFunctions(ctx);
	RawCatalog& catalog = RawCatalog::getInstance();

	//Input Path
	const char* nameJSON = planPath;
	//Prepare Input
	struct stat statbuf;
	stat(nameJSON, &statbuf);
	size_t fsize = statbuf.st_size;

	int fd = open(nameJSON, O_RDONLY);
	if (fd == -1) {
		throw runtime_error(string("json.open"));
	}

	const char *bufJSON = (const char*) mmap(NULL, fsize, PROT_READ,
			MAP_PRIVATE, fd, 0);
	if (bufJSON == MAP_FAILED ) {
		const char *err = "json.mmap";
		LOG(ERROR)<< err;
		throw runtime_error(err);
	}

	Document document; // Default template parameter uses UTF8 and MemoryPoolAllocator.
	if (document.Parse(bufJSON).HasParseError()) {
		const char *err = "[PlanExecutor: ] Error parsing physical plan";
		LOG(ERROR)<< err;
		throw runtime_error(err);
	}

	/* Start plan traversal. */
	printf("\nParsing physical plan:\n");
	assert(document.IsObject());

	assert(document.HasMember("operator"));
	assert(document["operator"].IsString());
	printf("operator = %s\n", document["operator"].GetString());

	parsePlan(document);

	vector<Plugin*>::iterator pgIter = activePlugins.begin();

	/* Cleanup */
	for(; pgIter != activePlugins.end(); pgIter++)	{
		Plugin *currPg = *pgIter;
		currPg->finish();
	}

	return;
}

void PlanExecutor::parsePlan(const rapidjson::Document& doc)	{
	RawOperator* planRootOp = parseOperator(doc);
	planRootOp->produce();

	//Run function
	ctx.prepareFunction(ctx.getGlobalFunction());

	RawCatalog& catalog = RawCatalog::getInstance();
	/* XXX Remove when testing caches (?) */
	catalog.clear();
}

RawOperator* PlanExecutor::parseOperator(const rapidjson::Value& val)	{

	const char *keyPg = "plugin";
	const char *keyOp = "operator";

	assert(val.HasMember(keyOp));
	assert(val[keyOp].IsString());
	const char *opName = val["operator"].GetString();

	RawOperator *newOp = NULL;

	if (strcmp(opName, "reduce") == 0) {
		/* "Multi - reduce"! */

		/* get monoid(s) */
		assert(val.HasMember("accumulator"));
		assert(val["accumulator"].IsArray());
		vector<Monoid> accs;
		const rapidjson::Value& accsJSON = val["accumulator"];
		for (SizeType i = 0; i < accsJSON.Size(); i++) // rapidjson uses SizeType instead of size_t.
		{
			assert(accsJSON[i].IsString());
			Monoid acc = parseAccumulator(accsJSON[i].GetString());
			accs.push_back(acc);
		}

		/*
		 * parse output expressions
		 * XXX Careful: Assuming numerous output expressions!
		 */
		assert(val.HasMember("e"));
		assert(val["e"].IsArray());
		vector<expressions::Expression*> e;
		const rapidjson::Value& exprsJSON = val["e"];
		for (SizeType i = 0; i < exprsJSON.Size(); i++)
		{
			expressions::Expression *outExpr = parseExpression(exprsJSON[i]);
			e.push_back(outExpr);
		}

		/* parse filtering expression */
		assert(val.HasMember("p"));
		assert(val["p"].IsObject());
		expressions::Expression *p = parseExpression(val["p"]);

		/* parse operator input */
		RawOperator* childOp = parseOperator(val["input"]);
		/* 'Multi-reduce' used */
		newOp = new opt::Reduce(accs, e, p, childOp, &(this->ctx),true,moduleName);
		childOp->setParent(newOp);
	} else if (strcmp(opName, "unnest") == 0) {

		/* parse operator input */
		RawOperator* childOp = parseOperator(val["input"]);

		/* parse filtering expression */
		assert(val.HasMember("p"));
		assert(val["p"].IsObject());
		expressions::Expression *p = parseExpression(val["p"]);

		/* parse path expression */
		assert(val.HasMember("path"));
		assert(val["path"].IsObject());

		assert(val["path"]["name"].IsString());
		string pathAlias = val["path"]["name"].GetString();

		assert(val["path"]["e"].IsObject());
		expressions::Expression *exprToUnnest = parseExpression(
				val["path"]["e"]);
		expressions::RecordProjection *proj =
				dynamic_cast<expressions::RecordProjection*>(exprToUnnest);
		if (proj == NULL) {
			string error_msg = string(
					"[Unnest: ] Cannot cast to record projection");
			LOG(ERROR)<< error_msg;
			throw runtime_error(string(error_msg));
		}

		Path *projPath = new Path(pathAlias,proj);

		newOp = new Unnest(p, *projPath, childOp);
		childOp->setParent(newOp);
	} else if (strcmp(opName, "outer_unnest") == 0) {

		/* parse operator input */
		RawOperator* childOp = parseOperator(val["input"]);

		/* parse filtering expression */
		assert(val.HasMember("p"));
		assert(val["p"].IsObject());
		expressions::Expression *p = parseExpression(val["p"]);

		/* parse path expression */
		assert(val.HasMember("path"));
		assert(val["path"].IsObject());

		assert(val["path"]["name"].IsString());
		string pathAlias = val["path"]["name"].GetString();

		assert(val["path"]["e"].IsObject());
		expressions::Expression *exprToUnnest = parseExpression(
				val["path"]["e"]);
		expressions::RecordProjection *proj =
				dynamic_cast<expressions::RecordProjection*>(exprToUnnest);
		if (proj == NULL) {
			string error_msg = string(
					"[Unnest: ] Cannot cast to record projection");
			LOG(ERROR)<< error_msg;
			throw runtime_error(string(error_msg));
		}

		Path *projPath = new Path(pathAlias, proj);

		newOp = new OuterUnnest(p, *projPath, childOp);
		childOp->setParent(newOp);
	}
	else if(strcmp(opName, "join") == 0)	{

		const char *keyMatLeft = "leftFields";
		const char *keyMatRight = "rightFields";
		const char *keyPred = "p";

		/* parse operator input */
		RawOperator* leftOp = parseOperator(val["leftInput"]);
		RawOperator* rightOp = parseOperator(val["rightInput"]);

		//Predicate
		assert(val.HasMember(keyPred));
		assert(val[keyPred].IsObject());

		expressions::Expression *predExpr = parseExpression(val[keyPred]);
		expressions::BinaryExpression *pred =
				dynamic_cast<expressions::BinaryExpression*>(predExpr);
		if (predExpr == NULL) {
			string error_msg = string(
					"[JOIN: ] Cannot cast to binary predicate. Original: ")
					+ predExpr->getExpressionType()->getType();
			LOG(ERROR)<< error_msg;
			throw runtime_error(string(error_msg));
		}

		/*
		 * *** WHAT TO MATERIALIZE ***
		 * XXX v0: JSON file contains a list of **RecordProjections**
		 * EXPLICIT OIDs injected by PARSER (not in json file by default)
		 * XXX Eager materialization atm
		 *
		 * XXX Why am I not using minimal constructor for materializer yet, as use cases do?
		 * 	-> Because then I would have to encode the OID type in JSON -> can be messy
		 */

		//LEFT SIDE
		assert(val.HasMember(keyMatLeft));
		assert(val[keyMatLeft].IsArray());
		vector<expressions::Expression*> exprsLeft = vector<expressions::Expression*>();
		map<string,RecordAttribute*> mapOidsLeft = map<string,RecordAttribute*>();
		vector<RecordAttribute*> fieldsLeft = vector<RecordAttribute*>();
		vector<materialization_mode> outputModesLeft;
		for (SizeType i = 0; i < val[keyMatLeft].Size(); i++) {
			expressions::Expression *exprL = parseExpression(val[keyMatLeft][i]);

			exprsLeft.push_back(exprL);
			outputModesLeft.insert(outputModesLeft.begin(), EAGER);

			//XXX STRONG ASSUMPTION: Expression is actually a record projection!
			expressions::RecordProjection *projL =
					dynamic_cast<expressions::RecordProjection *>(exprL);
			if(projL == NULL)
			{
				string error_msg = string(
						"[Join: ] Cannot cast to rec projection. Original: ")
						+ exprL->getExpressionType()->getType();
				LOG(ERROR)<< error_msg;
				throw runtime_error(string(error_msg));
			}
			//Added in 'wanted fields'
			RecordAttribute *recAttr = new RecordAttribute(projL->getAttribute());
			fieldsLeft.push_back(recAttr);

			string relName = recAttr->getRelationName();
			if (mapOidsLeft.find(relName) == mapOidsLeft.end()) {
				InputInfo *datasetInfo = (this->catalogParser).getInputInfo(
						relName);
				RecordAttribute *oid = new RecordAttribute(
						recAttr->getRelationName(), activeLoop,
						datasetInfo->oidType);
				mapOidsLeft[relName] = oid;
				expressions::RecordProjection *oidL =
						new expressions::RecordProjection(datasetInfo->oidType,
								projL->getExpr(), *oid);
				//Added in 'wanted expressions'
				cout << "Injecting left OID for " << relName << endl;
				exprsLeft.insert(exprsLeft.begin(),oidL);
				outputModesLeft.insert(outputModesLeft.begin(), EAGER);
			}
		}
		vector<RecordAttribute*> oidsLeft = vector<RecordAttribute*>();
		MapToVec(mapOidsLeft,oidsLeft);
		Materializer* matLeft = new Materializer(fieldsLeft, exprsLeft,
					oidsLeft, outputModesLeft);

		//RIGHT SIDE
		assert(val.HasMember(keyMatRight));
		assert(val[keyMatRight].IsArray());
		vector<expressions::Expression*> exprsRight = vector<
				expressions::Expression*>();
		map<string, RecordAttribute*> mapOidsRight = map<string,
				RecordAttribute*>();
		vector<RecordAttribute*> fieldsRight = vector<RecordAttribute*>();
		vector<materialization_mode> outputModesRight;
		for (SizeType i = 0; i < val[keyMatRight].Size(); i++) {
			expressions::Expression *exprR = parseExpression(
					val[keyMatRight][i]);

			exprsRight.push_back(exprR);
			outputModesRight.insert(outputModesRight.begin(), EAGER);

			//XXX STRONG ASSUMPTION: Expression is actually a record projection!
			expressions::RecordProjection *projR =
					dynamic_cast<expressions::RecordProjection *>(exprR);
			if (projR == NULL) {
				string error_msg = string(
						"[Join: ] Cannot cast to rec projection. Original: ")
						+ exprR->getExpressionType()->getType();
				LOG(ERROR)<< error_msg;
				throw runtime_error(string(error_msg));
			}

			//Added in 'wanted fields'
			RecordAttribute *recAttr = new RecordAttribute(
					projR->getAttribute());
			fieldsRight.push_back(recAttr);

			string relName = recAttr->getRelationName();
			if (mapOidsRight.find(relName) == mapOidsRight.end()) {
				InputInfo *datasetInfo = (this->catalogParser).getInputInfo(
						relName);
				RecordAttribute *oid = new RecordAttribute(
						recAttr->getRelationName(), activeLoop,
						datasetInfo->oidType);
				mapOidsRight[relName] = oid;
				expressions::RecordProjection *oidR =
						new expressions::RecordProjection(datasetInfo->oidType,
								projR->getExpr(), *oid);
				//Added in 'wanted expressions'
				exprsRight.insert(exprsRight.begin(),oidR);
				cout << "Injecting right OID for " << relName << endl;
				outputModesRight.insert(outputModesRight.begin(), EAGER);
			}
		}
		vector<RecordAttribute*> oidsRight = vector<RecordAttribute*>();
		MapToVec(mapOidsRight, oidsRight);
		Materializer* matRight = new Materializer(fieldsRight, exprsRight,
				oidsRight, outputModesRight);

		newOp = new RadixJoin(pred,leftOp,rightOp,&(this->ctx),"radixHashJoin",*matLeft,*matRight);
		leftOp->setParent(newOp);
		rightOp->setParent(newOp);
	}
	else if (strcmp(opName, "nest") == 0) {

		const char *keyGroup = "f";
		const char *keyNull  = "g";
		const char *keyPred  = "p";
		const char *keyExprs = "e";
		const char *keyAccum = "accumulator";
		/* Physical Level Info */
		const char *keyAggrNames = "aggrLabels";
		//Materializer
		const char *keyMat = "fields";

		/* parse operator input */
		RawOperator* childOp = parseOperator(val["input"]);

		/* get monoid(s) */
		assert(val.HasMember(keyAccum));
		assert(val[keyAccum].IsArray());
		vector<Monoid> accs;
		const rapidjson::Value& accsJSON = val[keyAccum];
		for (SizeType i = 0; i < accsJSON.Size(); i++)
		{
			assert(accsJSON[i].IsString());
			Monoid acc = parseAccumulator(accsJSON[i].GetString());
			accs.push_back(acc);
		}
		/* get label for each of the aggregate values */
		vector<string> aggrLabels;
		assert(val.HasMember(keyAggrNames));
		assert(val[keyAggrNames].IsArray());
		const rapidjson::Value& labelsJSON = val[keyAggrNames];
		for (SizeType i = 0; i < labelsJSON.Size(); i++) {
			assert(labelsJSON[i].IsString());
			aggrLabels.push_back(labelsJSON[i].GetString());
		}

		/* Predicate */
		assert(val.HasMember(keyPred));
		assert(val[keyPred].IsObject());
		expressions::Expression *predExpr = parseExpression(val[keyPred]);

		/* Group By */
		assert(val.HasMember(keyGroup));
		assert(val[keyGroup].IsObject());
		expressions::Expression *groupByExpr = parseExpression(val[keyGroup]);

		/* Null-to-zero Checks */
		//FIXME not used in radix nest yet!
		assert(val.HasMember(keyNull));
		assert(val[keyNull].IsObject());
		expressions::Expression *nullsToZerosExpr = parseExpression(val[keyNull]);

		/* Output aggregate expression(s) */
		assert(val.HasMember(keyExprs));
		assert(val[keyExprs].IsArray());
		vector<expressions::Expression*> outputExprs =
				vector<expressions::Expression*>();
		for (SizeType i = 0; i < val[keyExprs].Size(); i++) {
			expressions::Expression *expr = parseExpression(val[keyExprs][i]);
			outputExprs.push_back(expr);
		}

		/*
		 * *** WHAT TO MATERIALIZE ***
		 * XXX v0: JSON file contains a list of **RecordProjections**
		 * EXPLICIT OIDs injected by PARSER (not in json file by default)
		 * XXX Eager materialization atm
		 *
		 * XXX Why am I not using minimal constructor for materializer yet, as use cases do?
		 * 	-> Because then I would have to encode the OID type in JSON -> can be messy
		 */

		assert(val.HasMember(keyMat));
		assert(val[keyMat].IsArray());
		vector<expressions::Expression*> exprsToMat =
				vector<expressions::Expression*>();
		map<string, RecordAttribute*> mapOids = map<string, RecordAttribute*>();
		vector<RecordAttribute*> fieldsToMat = vector<RecordAttribute*>();
		vector<materialization_mode> outputModes;
		for (SizeType i = 0; i < val[keyMat].Size(); i++) {
			expressions::Expression *expr = parseExpression(val[keyMat][i]);
			exprsToMat.push_back(expr);
			outputModes.insert(outputModes.begin(), EAGER);

			//XXX STRONG ASSUMPTION: Expression is actually a record projection!
			expressions::RecordProjection *proj =
					dynamic_cast<expressions::RecordProjection *>(expr);
			if (proj == NULL) {
				string error_msg = string(
						"[Nest: ] Cannot cast to rec projection. Original: ")
						+ expr->getExpressionType()->getType();
				LOG(ERROR)<< error_msg;
				throw runtime_error(string(error_msg));
			}
			//Added in 'wanted fields'
			RecordAttribute *recAttr =
					new RecordAttribute(proj->getAttribute());
			fieldsToMat.push_back(recAttr);

			string relName = recAttr->getRelationName();
			if (mapOids.find(relName) == mapOids.end()) {
				InputInfo *datasetInfo =
						(this->catalogParser).getInputInfo(relName);
				RecordAttribute *oid =
						new RecordAttribute(recAttr->getRelationName(), activeLoop,
						datasetInfo->oidType);
				mapOids[relName] = oid;
				expressions::RecordProjection *oidProj =
						new expressions::RecordProjection(datasetInfo->oidType,
								proj->getExpr(), *oid);
				//Added in 'wanted expressions'
				LOG(INFO)<< "[Plan Parser: ] Injecting OID for " << relName;
//				std::cout << "[Plan Parser: ] Injecting OID for " << relName << std::endl;
				/* ORDER OF expression fields matters!! OIDs need to be placed first! */
				exprsToMat.insert(exprsToMat.begin(), oidProj);
				outputModes.insert(outputModes.begin(), EAGER);
			}
		}
		vector<RecordAttribute*> oids = vector<RecordAttribute*>();
		MapToVec(mapOids, oids);
		/* FIXME This constructor breaks nest use cases that trigger caching */
		/* Check similar hook in radix-nest.cpp */
//		Materializer *mat =
//				new Materializer(fieldsToMat, exprsToMat, oids, outputModes);
		Materializer* matCoarse = new Materializer(exprsToMat);

		//Put operator together
		const char *opLabel = "radixNest";
		newOp = new radix::Nest(&(this->ctx), accs, outputExprs, aggrLabels, predExpr,
				groupByExpr, nullsToZerosExpr, childOp, opLabel, *matCoarse);
		childOp->setParent(newOp);
	}
	else if (strcmp(opName, "select") == 0) {
		/* parse filtering expression */
		assert(val.HasMember("p"));
		assert(val["p"].IsObject());
		expressions::Expression *p = parseExpression(val["p"]);

		/* parse operator input */
		RawOperator* childOp = parseOperator(val["input"]);
		newOp = new Select(p, childOp);
		childOp->setParent(newOp);
	}
	else if(strcmp(opName,"scan") == 0)	{
		assert(val.HasMember(keyPg));
		assert(val[keyPg].IsObject());
		Plugin *pg = this->parsePlugin(val[keyPg]);
		newOp =  new Scan(&(this->ctx),*pg);
	}
	else	{
		string err = string("Unknown Operator: ") + opName;
		LOG(ERROR) << err;
		throw runtime_error(err);
	}

	return newOp;
}

/*
 *	enum ExpressionId	{ CONSTANT, ARGUMENT, RECORD_PROJECTION, RECORD_CONSTRUCTION, IF_THEN_ELSE, BINARY, MERGE };
 *	FIXME / TODO No Merge yet!! Will be needed for parallelism!
 *	TODO Add NotExpression ?
 */

expressions::Expression* ExpressionParser::parseExpression(const rapidjson::Value& val) {

	const char *keyExpression = "expression";
	const char *keyArgNo = "argNo";
	const char *keyExprType = "type";

	/* Input Argument specifics */
	const char *keyAtts = "attributes";

	/* Record Projection specifics */
	const char *keyInnerExpr = "e";
	const char *keyProjectedAttr = "attribute";

	/* Record Construction specifics */
	const char *keyAttsConstruction = "attributes";
	const char *keyAttrName = "name";
	const char *keyAttrExpr = "e";

	/* If-else specifics */
	const char *keyCond = "cond";
	const char *keyThen = "then";
	const char *keyElse = "else";

	/*Binary operator(s) specifics */
	const char *leftArg = "left";
	const char *rightArg = "right";

	assert(val.HasMember(keyExpression));
	assert(val[keyExpression].IsString());
	const char *valExpression = val[keyExpression].GetString();

	if (strcmp(valExpression, "bool") == 0) {
		assert(val.HasMember("v"));
		assert(val["v"].IsBool());
		return new expressions::BoolConstant(val["v"].GetBool());
	} else if (strcmp(valExpression, "int") == 0) {
		assert(val.HasMember("v"));
		assert(val["v"].IsInt());
		return new expressions::IntConstant(val["v"].GetInt());
	} else if (strcmp(valExpression, "float") == 0) {
		assert(val.HasMember("v"));
		assert(val["v"].IsDouble());
		return new expressions::FloatConstant(val["v"].GetDouble());
	} else if (strcmp(valExpression, "string") == 0) {
		assert(val.HasMember("v"));
		assert(val["v"].IsString());
		string *stringVal = new string(val["v"].GetString());
		return new expressions::StringConstant(*stringVal);
	} else if (strcmp(valExpression, "argument") == 0) {

		/* exprType */
		assert(val.HasMember(keyExprType));
		assert(val[keyExprType].IsObject());
		ExpressionType *exprType = parseExpressionType(val[keyExprType]);

		/* argNo */
		assert(val.HasMember(keyArgNo));
		assert(val[keyArgNo].IsInt());
		int argNo = val[keyArgNo].GetInt();

		/* 'projections' / attributes */
		assert(val.HasMember(keyAtts));
		assert(val[keyAtts].IsArray());

		list<RecordAttribute> atts = list<RecordAttribute>();
		const rapidjson::Value& attributes = val[keyAtts]; // Using a reference for consecutive access is handy and faster.
		for (SizeType i = 0; i < attributes.Size(); i++) // rapidjson uses SizeType instead of size_t.
		{
			RecordAttribute *recAttr = parseRecordAttr(attributes[i]);
			atts.push_back(*recAttr);
		}
		return new expressions::InputArgument(exprType, argNo, atts);

	} else if (strcmp(valExpression, "recordProjection") == 0) {

		/* exprType */
		assert(val.HasMember(keyExprType));
		assert(val[keyExprType].IsObject());
		ExpressionType *exprType = parseExpressionType(val[keyExprType]);

		/* e: expression over which projection is calculated */
		assert(val.HasMember(keyInnerExpr));
		assert(val[keyInnerExpr].IsObject());
		expressions::Expression *expr = parseExpression(val[keyInnerExpr]);

		/* projected attribute */
		assert(val.HasMember(keyProjectedAttr));
		assert(val[keyProjectedAttr].IsObject());
		RecordAttribute *recAttr = parseRecordAttr(val[keyProjectedAttr]);

		return new expressions::RecordProjection(exprType,expr,*recAttr);
	} else if (strcmp(valExpression, "recordConstruction") == 0) {
		/* exprType */
		assert(val.HasMember(keyExprType));
		assert(val[keyExprType].IsObject());
		ExpressionType *exprType = parseExpressionType(val[keyExprType]);

		/* attribute construction(s) */
		assert(val.HasMember(keyAttsConstruction));
		assert(val[keyAttsConstruction].IsArray());

		list<expressions::AttributeConstruction> *newAtts = new list<expressions::AttributeConstruction>();
		const rapidjson::Value& attributeConstructs = val[keyAttsConstruction]; // Using a reference for consecutive access is handy and faster.
		for (SizeType i = 0; i < attributeConstructs.Size(); i++) // rapidjson uses SizeType instead of size_t.
		{
			assert(attributeConstructs[i].HasMember(keyAttrName));
			assert(attributeConstructs[i][keyAttrName].IsString());
			string newAttrName = attributeConstructs[i][keyAttrName].GetString();

			assert(attributeConstructs[i].HasMember(keyAttrExpr));
			assert(attributeConstructs[i][keyAttrExpr].IsObject());
			expressions::Expression *newAttrExpr = parseExpression(attributeConstructs[i][keyAttrExpr]);

			expressions::AttributeConstruction *newAttr =
					new expressions::AttributeConstruction(newAttrName,newAttrExpr);
			newAtts->push_back(*newAttr);
		}
		return new expressions::RecordConstruction(exprType,*newAtts);

	} else if (strcmp(valExpression,"if") == 0)	{
		/* exprType */
		assert(val.HasMember(keyExprType));
		assert(val[keyExprType].IsObject());
		ExpressionType *exprType = parseExpressionType(val[keyExprType]);

		/* if cond */
		assert(val.HasMember(keyCond));
		assert(val[keyCond].IsObject());
		expressions::Expression *condExpr = parseExpression(val[keyCond]);

		/* then expression */
		assert(val.HasMember(keyThen));
		assert(val[keyThen].IsObject());
		expressions::Expression *thenExpr = parseExpression(val[keyThen]);

		/* else expression */
		assert(val.HasMember(keyElse));
		assert(val[keyElse].IsObject());
		expressions::Expression *elseExpr = parseExpression(val[keyElse]);

		return new expressions::IfThenElse(exprType,condExpr,thenExpr,elseExpr);
	}
	/*
	 * BINARY EXPRESSIONS
	 */
	else if (strcmp(valExpression, "eq") == 0) {
		/* left child */
		assert(val.HasMember(leftArg));
		assert(val[leftArg].IsObject());
		expressions::Expression *leftExpr = parseExpression(val[leftArg]);

		/* right child */
		assert(val.HasMember(rightArg));
		assert(val[rightArg].IsObject());
		expressions::Expression *rightExpr = parseExpression(val[rightArg]);

		return new expressions::EqExpression(new BoolType(),leftExpr,rightExpr);
	} else if (strcmp(valExpression, "neq") == 0) {
		/* left child */
				assert(val.HasMember(leftArg));
				assert(val[leftArg].IsObject());
				expressions::Expression *leftExpr = parseExpression(val[leftArg]);

				/* right child */
				assert(val.HasMember(rightArg));
				assert(val[rightArg].IsObject());
				expressions::Expression *rightExpr = parseExpression(val[rightArg]);

				return new expressions::NeExpression(new BoolType(),leftExpr,rightExpr);
	} else if (strcmp(valExpression, "lt") == 0) {
		/* left child */
				assert(val.HasMember(leftArg));
				assert(val[leftArg].IsObject());
				expressions::Expression *leftExpr = parseExpression(val[leftArg]);

				/* right child */
				assert(val.HasMember(rightArg));
				assert(val[rightArg].IsObject());
				expressions::Expression *rightExpr = parseExpression(val[rightArg]);

				return new expressions::LtExpression(new BoolType(),leftExpr,rightExpr);
	} else if (strcmp(valExpression, "le") == 0) {
		/* left child */
		assert(val.HasMember(leftArg));
		assert(val[leftArg].IsObject());
		expressions::Expression *leftExpr = parseExpression(val[leftArg]);

		/* right child */
		assert(val.HasMember(rightArg));
		assert(val[rightArg].IsObject());
		expressions::Expression *rightExpr = parseExpression(val[rightArg]);

		return new expressions::LeExpression(new BoolType(),leftExpr,rightExpr);
	} else if (strcmp(valExpression, "gt") == 0) {
		/* left child */
		assert(val.HasMember(leftArg));
		assert(val[leftArg].IsObject());
		expressions::Expression *leftExpr = parseExpression(val[leftArg]);

		/* right child */
		assert(val.HasMember(rightArg));
		assert(val[rightArg].IsObject());
		expressions::Expression *rightExpr = parseExpression(val[rightArg]);

		return new expressions::GtExpression(new BoolType(),leftExpr,rightExpr);
	} else if (strcmp(valExpression, "ge") == 0) {
		/* left child */
		assert(val.HasMember(leftArg));
		assert(val[leftArg].IsObject());
		expressions::Expression *leftExpr = parseExpression(val[leftArg]);

		/* right child */
		assert(val.HasMember(rightArg));
		assert(val[rightArg].IsObject());
		expressions::Expression *rightExpr = parseExpression(val[rightArg]);

		return new expressions::GeExpression(new BoolType(),leftExpr,rightExpr);
	} else if (strcmp(valExpression, "and") == 0) {
		/* left child */
		assert(val.HasMember(leftArg));
		assert(val[leftArg].IsObject());
		expressions::Expression *leftExpr = parseExpression(val[leftArg]);

		/* right child */
		assert(val.HasMember(rightArg));
		assert(val[rightArg].IsObject());
		expressions::Expression *rightExpr = parseExpression(val[rightArg]);

		return new expressions::AndExpression(new BoolType(),leftExpr,rightExpr);
	} else if (strcmp(valExpression, "or") == 0) {
		/* left child */
		assert(val.HasMember(leftArg));
		assert(val[leftArg].IsObject());
		expressions::Expression *leftExpr = parseExpression(val[leftArg]);

		/* right child */
		assert(val.HasMember(rightArg));
		assert(val[rightArg].IsObject());
		expressions::Expression *rightExpr = parseExpression(val[rightArg]);

		return new expressions::OrExpression(new BoolType(),leftExpr,rightExpr);
	} else if (strcmp(valExpression, "add") == 0) {
		/* left child */
		assert(val.HasMember(leftArg));
		assert(val[leftArg].IsObject());
		expressions::Expression *leftExpr = parseExpression(val[leftArg]);

		/* right child */
		assert(val.HasMember(rightArg));
		assert(val[rightArg].IsObject());
		expressions::Expression *rightExpr = parseExpression(val[rightArg]);

		ExpressionType *exprType = const_cast<ExpressionType*>(leftExpr->getExpressionType());
		return new expressions::AddExpression(exprType,leftExpr,rightExpr);
	} else if (strcmp(valExpression, "sub") == 0) {
		/* left child */
		assert(val.HasMember(leftArg));
		assert(val[leftArg].IsObject());
		expressions::Expression *leftExpr = parseExpression(val[leftArg]);

		/* right child */
		assert(val.HasMember(rightArg));
		assert(val[rightArg].IsObject());
		expressions::Expression *rightExpr = parseExpression(val[rightArg]);

		ExpressionType *exprType = const_cast<ExpressionType*>(leftExpr->getExpressionType());
		return new expressions::SubExpression(exprType,leftExpr,rightExpr);
	} else if (strcmp(valExpression, "multiply") == 0) {
		/* left child */
		assert(val.HasMember(leftArg));
		assert(val[leftArg].IsObject());
		expressions::Expression *leftExpr = parseExpression(val[leftArg]);

		/* right child */
		assert(val.HasMember(rightArg));
		assert(val[rightArg].IsObject());
		expressions::Expression *rightExpr = parseExpression(val[rightArg]);

		ExpressionType *exprType = const_cast<ExpressionType*>(leftExpr->getExpressionType());
		return new expressions::MultExpression(exprType,leftExpr,rightExpr);
	} else if (strcmp(valExpression, "div") == 0) {
		/* left child */
		assert(val.HasMember(leftArg));
		assert(val[leftArg].IsObject());
		expressions::Expression *leftExpr = parseExpression(val[leftArg]);

		/* right child */
		assert(val.HasMember(rightArg));
		assert(val[rightArg].IsObject());
		expressions::Expression *rightExpr = parseExpression(val[rightArg]);

		ExpressionType *exprType = const_cast<ExpressionType*>(leftExpr->getExpressionType());
		return new expressions::DivExpression(exprType,leftExpr,rightExpr);
	} else if (strcmp(valExpression, "merge") == 0) {
		string err = string("(Still) unsupported expression: ") + valExpression;
		LOG(ERROR)<< err;
		throw runtime_error(err);
	} else {
		string err = string("Unknown expression: ") + valExpression;
		LOG(ERROR)<< err;
		throw runtime_error(err);
	}

	return NULL;
}


/*
 * enum typeID	{ BOOL, STRING, FLOAT, INT, RECORD, LIST, BAG, SET, INT64, COMPOSITE };
 * FIXME / TODO: Do I need to cater for 'composite' types?
 * IIRC, they only occur as OIDs / complex caches
*/
ExpressionType* ExpressionParser::parseExpressionType(const rapidjson::Value& val) {

	/* upper-level keys */
	const char *keyExprType = "type";
	const char *keyCollectionType = "inner";

	/* Related to record types */
	const char *keyRecordAttrs = "attributes";

	assert(val.HasMember(keyExprType));
	assert(val[keyExprType].IsString());
	const char *valExprType = val[keyExprType].GetString();

	if (strcmp(valExprType, "bool") == 0) {
		return new BoolType();
	} else if (strcmp(valExprType, "int") == 0) {
		return new IntType();
	} else if (strcmp(valExprType, "int64") == 0) {
		return new Int64Type();
	} else if (strcmp(valExprType, "float") == 0) {
		return new FloatType();
	} else if (strcmp(valExprType, "string") == 0) {
		return new StringType();
	} else if (strcmp(valExprType, "set") == 0) {
		assert(val.HasMember("inner"));
		assert(val["inner"].IsObject());
		ExpressionType *innerType = parseExpressionType(val["inner"]);
		return new SetType(*innerType);
	} else if (strcmp(valExprType, "bag") == 0) {
		assert(val.HasMember("inner"));
		assert(val["inner"].IsObject());
		ExpressionType *innerType = parseExpressionType(val["inner"]);
		cout << "BAG" << endl;
		return new BagType(*innerType);
	} else if (strcmp(valExprType, "list") == 0) {
		assert(val.HasMember("inner"));
		assert(val["inner"].IsObject());
		ExpressionType *innerType = parseExpressionType(val["inner"]);
		return new ListType(*innerType);
	} else if (strcmp(valExprType, "record") == 0) {

		assert(val.HasMember("attributes"));
		assert(val["attributes"].IsArray());

		list<RecordAttribute*> atts = list<RecordAttribute*>();
		const rapidjson::Value& attributes = val["attributes"]; // Using a reference for consecutive access is handy and faster.
		for (SizeType i = 0; i < attributes.Size(); i++) // rapidjson uses SizeType instead of size_t.
		{
			//Starting from 1
			RecordAttribute *recAttr = parseRecordAttr(attributes[i]);
			atts.push_back(recAttr);
		}
		return new RecordType(atts);
	} else if (strcmp(valExprType, "composite") == 0) {
		string err = string("(Still) Unsupported expression type: ")
				+ valExprType;
		LOG(ERROR)<< err;
		throw runtime_error(err);
	} else {
		string err = string("Unknown expression type: ") + valExprType;
		LOG(ERROR)<< err;
		throw runtime_error(err);
	}

}

RecordAttribute* ExpressionParser::parseRecordAttr(const rapidjson::Value& val) {

	const char *keyRecAttrType = "type";
	const char *keyRelName = "relName";
	const char *keyAttrName = "attrName";
	const char *keyAttrNo = "attrNo";

	assert(val.HasMember(keyRecAttrType));
	assert(val[keyRecAttrType].IsObject());
	ExpressionType* recArgType = parseExpressionType(val[keyRecAttrType]);

	assert(val.HasMember(keyRelName));
	assert(val[keyRelName].IsString());
	string relName = val[keyRelName].GetString();

	assert(val.HasMember(keyAttrName));
	assert(val[keyAttrName].IsString());
	string attrName = val[keyAttrName].GetString();

	assert(val.HasMember(keyAttrNo));
	assert(val[keyAttrNo].IsInt());
	int attrNo = val[keyAttrNo].GetInt();

	return new RecordAttribute(attrNo, relName, attrName, recArgType);
}

Monoid ExpressionParser::parseAccumulator(const char *acc) {

	if (strcmp(acc, "sum") == 0) {
		return SUM;
	} else if (strcmp(acc, "max") == 0) {
		return MAX;
	} else if (strcmp(acc, "multiply") == 0) {
		return MULTIPLY;
	} else if (strcmp(acc, "or") == 0) {
		return OR;
	} else if (strcmp(acc, "and") == 0) {
		return AND;
	} else if (strcmp(acc, "union") == 0) {
		return UNION;
	} else if (strcmp(acc, "bagunion") == 0) {
		return BAGUNION;
	} else if (strcmp(acc, "append") == 0) {
		return APPEND;
	} else {
		string err = string("Unknown Monoid: ") + acc;
		LOG(ERROR)<< err;
		throw runtime_error(err);
	}
}

/**
 * {"name": "foo", "type": "csv", ... }
 * FIXME / TODO If we introduce more plugins, this code must be extended
 */
Plugin* PlanExecutor::parsePlugin(const rapidjson::Value& val)	{

	Plugin *newPg = NULL;

	const char *keyInputName = "name";
	const char *keyPgType = "type";

	/*
	 * CSV-specific
	 */
	//which fields to project
	const char *keyProjectionsCSV = "projections";
	//pm policy
	const char *keyPolicy = "policy";
	//line hint
	const char *keyLineHint = "lines";
	//OPTIONAL: which delimiter to use
	const char *keyDelimiter = "delimiter";
	//OPTIONAL: are string values wrapped in brackets?
	const char *keyBrackets = "brackets";

	/*
	 * BinRow
	 */
	const char *keyProjectionsBinRow = "projections";

	/*
	 * BinCol
	 */
	const char *keyProjectionsBinCol = "projections";

	assert(val.HasMember(keyInputName));
	assert(val[keyInputName].IsString());
	string datasetName = val[keyInputName].GetString();

	assert(val.HasMember(keyPgType));
	assert(val[keyPgType].IsString());
	const char *pgType = val[keyPgType].GetString();

	//Lookup in catalog based on name
	InputInfo *datasetInfo = (this->catalogParser).getInputInfo(datasetName);
	//Dynamic allocation because I have to pass reference later on
	string *pathDynamicCopy = new string(datasetInfo->path);

	/* Retrieve RecordType */
	/* Extract inner type of collection */
	CollectionType *collType = dynamic_cast<CollectionType*>(datasetInfo->exprType);
	if(collType == NULL)	{
		string error_msg = string("[Plugin Parser: ] Cannot cast to collection type. Original intended type: ") + datasetInfo->exprType->getType();
		LOG(ERROR)<< error_msg;
		throw runtime_error(string(error_msg));
	}
	/* For the current plugins, the expression type is unambiguously RecordType */
	const ExpressionType& nestedType = collType->getNestedType();
	const RecordType& recType_ = dynamic_cast<const RecordType&>(nestedType);
	//circumventing the presence of const
	RecordType *recType = new RecordType(recType_.getArgs());


	if (strcmp(pgType, "csv") == 0) {
//		cout<<"Original intended type: " << datasetInfo.exprType->getType()<<endl;
//		cout<<"File path: " << datasetInfo.path<<endl;

		/* Projections come in an array of Record Attributes */
		assert(val.HasMember(keyProjectionsCSV));
		assert(val[keyProjectionsCSV].IsArray());

		vector<RecordAttribute*> projections;
		for (SizeType i = 0; i < val[keyProjectionsCSV].Size(); i++)
		{
			assert(val[keyProjectionsCSV][i].IsObject());
			RecordAttribute *recAttr = this->parseRecordAttr(val[keyProjectionsCSV][i]);
			projections.push_back(recAttr);
		}

		assert(val.HasMember(keyLineHint));
		assert(val[keyLineHint].IsInt());
		int linehint = val[keyLineHint].GetInt();

		assert(val.HasMember(keyPolicy));
		assert(val[keyPolicy].IsInt());
		int policy = val[keyPolicy].GetInt();

		char delim = ',';
		if (val.HasMember(keyDelimiter)) {
			assert(val[keyDelimiter].IsString());
			delim = (val[keyDelimiter].GetString())[0];
		}
		else
		{
			string err = string("WARNING - NO DELIMITER SPECIFIED. FALLING BACK TO DEFAULT");
			LOG(WARNING)<< err;
			cout << err << endl;
		}

		bool stringBrackets = true;
		if (val.HasMember(keyBrackets)) {
			assert(val[keyBrackets].IsBool());
			stringBrackets = val[keyBrackets].GetBool();
		}

		newPg = new pm::CSVPlugin(&(this->ctx), *pathDynamicCopy, *recType,
				projections, delim, linehint, policy, stringBrackets);
	} else if (strcmp(pgType, "json") == 0) {
		assert(val.HasMember(keyLineHint));
		assert(val[keyLineHint].IsInt());
		int linehint = val[keyLineHint].GetInt();

		newPg = new jsonPipelined::JSONPlugin(&(this->ctx), *pathDynamicCopy, datasetInfo->exprType);
	} else if (strcmp(pgType, "binrow") == 0) {
		assert(val.HasMember(keyProjectionsBinRow));
		assert(val[keyProjectionsBinRow].IsArray());

		vector<RecordAttribute*> *projections = new vector<RecordAttribute*>();
		for (SizeType i = 0; i < val[keyProjectionsBinRow].Size(); i++) {
			assert(val[keyProjectionsBinRow][i].IsObject());
			RecordAttribute *recAttr = this->parseRecordAttr(
					val[keyProjectionsBinRow][i]);
			projections->push_back(recAttr);
		}

		newPg = new BinaryRowPlugin(&(this->ctx), *pathDynamicCopy, *recType,
				*projections);
	} else if (strcmp(pgType, "bincol") == 0) {
		assert(val.HasMember(keyProjectionsBinCol));
		assert(val[keyProjectionsBinCol].IsArray());

		vector<RecordAttribute*> *projections = new vector<RecordAttribute*>();
		for (SizeType i = 0; i < val[keyProjectionsBinCol].Size(); i++) {
			assert(val[keyProjectionsBinCol][i].IsObject());
			RecordAttribute *recAttr = this->parseRecordAttr(
					val[keyProjectionsBinCol][i]);
			projections->push_back(recAttr);
		}

		newPg = new BinaryColPlugin(&(this->ctx), *pathDynamicCopy, *recType,
				*projections);
	} else {
		string err = string("Unknown Plugin Type: ") + pgType;
		LOG(ERROR)<< err;
		throw runtime_error(err);
	}

	activePlugins.push_back(newPg);
	RawCatalog& catalog = RawCatalog::getInstance();
	catalog.registerPlugin(*pathDynamicCopy,newPg);
	datasetInfo->oidType = newPg->getOIDType();
	(this->catalogParser).setInputInfo(datasetName,datasetInfo);
	return newPg;
}

/**
 * {"datasetname": {"path": "foo", "type": { ... } }
 */
CatalogParser::CatalogParser(const char *catalogPath) {
	//Input Path
	const char *nameJSON = catalogPath;

	//key aliases
	const char *keyInputPath = "path";
	const char *keyExprType =  "type";

	//Prepare Input
	struct stat statbuf;
	stat(nameJSON, &statbuf);
	size_t fsize = statbuf.st_size;

	int fd = open(nameJSON, O_RDONLY);
	if (fd == -1) {
		throw runtime_error(string("json.open"));
	}

	const char *bufJSON = (const char*) mmap(NULL, fsize, PROT_READ,
			MAP_PRIVATE, fd, 0);
	if (bufJSON == MAP_FAILED ) {
		const char *err = "json.mmap";
		LOG(ERROR)<< err;
		throw runtime_error(err);
	}

	Document document; // Default template parameter uses UTF8 and MemoryPoolAllocator.
	if (document.Parse(bufJSON).HasParseError()) {
		const char *err = "[CatalogParser: ] Error parsing physical plan";
		LOG(ERROR)<< err;
		throw runtime_error(err);
	}

	//Start plan traversal.
	printf("\nParsing catalog information:\n");
	assert(document.IsObject());

	for (rapidjson::Value::ConstMemberIterator itr = document.MemberBegin();
			itr != document.MemberEnd(); ++itr) {
		printf("Key of member is %s\n", itr->name.GetString());

		assert(itr->value.IsObject());
		assert((itr->value)[keyInputPath].IsString());
		string inputPath = ((itr->value)[keyInputPath]).GetString();
		assert((itr->value)[keyExprType].IsObject());
		ExpressionType *exprType = exprParser.parseExpressionType((itr->value)[keyExprType]);
		InputInfo *info = new InputInfo();
		info->exprType = exprType;
		info->path = inputPath;
		//Initialized by parsePlugin() later on
		info->oidType = NULL;
//		(this->inputs)[itr->name.GetString()] = info;
		(this->inputs)[info->path] = info;
	}
}
