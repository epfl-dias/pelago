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

#include "experiments/realworld-vldb/spam-csv-cached-columnar.hpp"

//RawContext prepareContext(string moduleName)	{
//	RawContext ctx = RawContext(moduleName);
//	registerFunctions(ctx);
//	return ctx;
//}

void symantecCSV1Caching(map<string,dataset> datasetCatalog)	{

	int idLow  = 100000000;
	int idHigh = 200000000;
	RawContext ctx = prepareContext("symantec-csv-1");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
			symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';

	RecordAttribute *id = argsSymantecCSV["id"];
	RecordAttribute *classa = argsSymantecCSV["classa"];
	RecordAttribute *classb = argsSymantecCSV["classb"];
	RecordAttribute *size = argsSymantecCSV["size"];

	vector<RecordAttribute*> projections;
	projections.push_back(id);
	projections.push_back(classa);
	projections.push_back(classb);
	projections.push_back(size);


	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
				delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/*
	 * Materialize expression(s) here:
	 * id, classa, classb, size cached
	 */
	list<RecordAttribute> argProjections;
		argProjections.push_back(*id);
		argProjections.push_back(*classa);
		argProjections.push_back(*classb);
	expressions::Expression* arg 			=
					new expressions::InputArgument(&rec,0,argProjections);

	expressions::Expression* toMat1 = new expressions::RecordProjection(
			id->getOriginalType(), arg, *id);
	char matLabel1[] = "idMaterializer";
	ExprMaterializer *mat1 = new ExprMaterializer(toMat1, linehint, scan, &ctx,
			matLabel1);
	scan->setParent(mat1);

	expressions::Expression* toMat2 = new expressions::RecordProjection(
			classa->getOriginalType(), arg, *classa);
	char matLabel2[] = "classaMaterializer";
	ExprMaterializer *mat2 = new ExprMaterializer(toMat2, linehint, mat1, &ctx,
			matLabel2);
	mat1->setParent(mat2);

	expressions::Expression* toMat3 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	char matLabel3[] = "classbMaterializer";
	ExprMaterializer *mat3 = new ExprMaterializer(toMat3, linehint, mat2, &ctx,
			matLabel3);
	mat2->setParent(mat3);

	/* XXX Not strictly needed for this query */
	expressions::Expression* toMat4 = new expressions::RecordProjection(
			size->getOriginalType(), arg, *size);
	char matLabel4[] = "sizeMaterializer";
	ExprMaterializer *mat4 = new ExprMaterializer(toMat4, linehint, mat3, &ctx,
			matLabel4);
	mat3->setParent(mat4);

	ExprMaterializer *lastMat = mat4;

	/**
	 * REDUCE
	 */


	/* Output: */
	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;

	accs.push_back(MAX);
	expressions::Expression* outputExpr1 = new expressions::RecordProjection(
			id->getOriginalType(), arg, *id);
	outputExprs.push_back(outputExpr1);

	accs.push_back(MAX);
		expressions::Expression* outputExpr2 = new expressions::RecordProjection(
				classa->getOriginalType(), arg, *classa);
	outputExprs.push_back(outputExpr2);

	accs.push_back(MAX);
	expressions::Expression* outputExpr3 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	outputExprs.push_back(outputExpr3);

	/* Pred: */
	expressions::Expression* selID  	=
			new expressions::RecordProjection(id->getOriginalType(),arg,*id);
	expressions::Expression* predExpr1 = new expressions::IntConstant(idLow);
	expressions::Expression* predExpr2 = new expressions::IntConstant(idHigh);
	expressions::Expression* predicate1 = new expressions::GtExpression(
				new BoolType(), selID, predExpr1);
	expressions::Expression* predicate2 = new expressions::LtExpression(
					new BoolType(), selID, predExpr2);
	expressions::Expression* predicate = new expressions::AndExpression(
			new BoolType(), predicate1, predicate2);

	opt::Reduce *reduce = new opt::Reduce(accs, outputExprs, predicate, lastMat, &ctx);
	lastMat->setParent(reduce);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	reduce->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

/* Not caching size */
void symantecCSV1CachingB(map<string,dataset> datasetCatalog)	{

	int idLow  = 100000000;
	int idHigh = 200000000;
	RawContext ctx = prepareContext("symantec-csv-1");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
			symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';

	RecordAttribute *id = argsSymantecCSV["id"];
	RecordAttribute *classa = argsSymantecCSV["classa"];
	RecordAttribute *classb = argsSymantecCSV["classb"];
	RecordAttribute *size = argsSymantecCSV["size"];

	vector<RecordAttribute*> projections;
	projections.push_back(id);
	projections.push_back(classa);
	projections.push_back(classb);
	projections.push_back(size);


	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
				delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/*
	 * Materialize expression(s) here:
	 * id, classa, classb, size cached
	 */
	list<RecordAttribute> argProjections;
		argProjections.push_back(*id);
		argProjections.push_back(*classa);
		argProjections.push_back(*classb);
	expressions::Expression* arg 			=
					new expressions::InputArgument(&rec,0,argProjections);

	expressions::Expression* toMat1 = new expressions::RecordProjection(
			id->getOriginalType(), arg, *id);
	char matLabel1[] = "idMaterializer";
	ExprMaterializer *mat1 = new ExprMaterializer(toMat1, linehint, scan, &ctx,
			matLabel1);
	scan->setParent(mat1);

	expressions::Expression* toMat2 = new expressions::RecordProjection(
			classa->getOriginalType(), arg, *classa);
	char matLabel2[] = "classaMaterializer";
	ExprMaterializer *mat2 = new ExprMaterializer(toMat2, linehint, mat1, &ctx,
			matLabel2);
	mat1->setParent(mat2);

	expressions::Expression* toMat3 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	char matLabel3[] = "classbMaterializer";
	ExprMaterializer *mat3 = new ExprMaterializer(toMat3, linehint, mat2, &ctx,
			matLabel3);
	mat2->setParent(mat3);

	/* XXX Not strictly needed for this query */
//	expressions::Expression* toMat4 = new expressions::RecordProjection(
//			size->getOriginalType(), arg, *size);
//	char matLabel4[] = "sizeMaterializer";
//	ExprMaterializer *mat4 = new ExprMaterializer(toMat4, linehint, mat3, &ctx,
//			matLabel4);
//	mat3->setParent(mat4);

	ExprMaterializer *lastMat = mat3;

	/**
	 * REDUCE
	 */


	/* Output: */
	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;

	accs.push_back(MAX);
	expressions::Expression* outputExpr1 = new expressions::RecordProjection(
			id->getOriginalType(), arg, *id);
	outputExprs.push_back(outputExpr1);

	accs.push_back(MAX);
		expressions::Expression* outputExpr2 = new expressions::RecordProjection(
				classa->getOriginalType(), arg, *classa);
	outputExprs.push_back(outputExpr2);

	accs.push_back(MAX);
	expressions::Expression* outputExpr3 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	outputExprs.push_back(outputExpr3);

	/* Pred: */
	expressions::Expression* selID  	=
			new expressions::RecordProjection(id->getOriginalType(),arg,*id);
	expressions::Expression* predExpr1 = new expressions::IntConstant(idLow);
	expressions::Expression* predExpr2 = new expressions::IntConstant(idHigh);
	expressions::Expression* predicate1 = new expressions::GtExpression(
				new BoolType(), selID, predExpr1);
	expressions::Expression* predicate2 = new expressions::LtExpression(
					new BoolType(), selID, predExpr2);
	expressions::Expression* predicate = new expressions::AndExpression(
			new BoolType(), predicate1, predicate2);

	opt::Reduce *reduce = new opt::Reduce(accs, outputExprs, predicate, lastMat, &ctx);
	lastMat->setParent(reduce);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	reduce->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

void symantecCSV1(map<string,dataset> datasetCatalog)	{

	int idLow  = 100000000;
	int idHigh = 200000000;
	RawContext ctx = prepareContext("symantec-csv-1");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
			symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';

	RecordAttribute *id = argsSymantecCSV["id"];
	RecordAttribute *classa = argsSymantecCSV["classa"];
	RecordAttribute *classb = argsSymantecCSV["classb"];

	vector<RecordAttribute*> projections;
	projections.push_back(id);
	projections.push_back(classa);
	projections.push_back(classb);


	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
				delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/**
	 * REDUCE
	 */
	list<RecordAttribute> argProjections;
	argProjections.push_back(*id);
	argProjections.push_back(*classa);
	argProjections.push_back(*classb);
	expressions::Expression* arg 			=
				new expressions::InputArgument(&rec,0,argProjections);
	/* Output: */
	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;

	accs.push_back(MAX);
	expressions::Expression* outputExpr1 = new expressions::RecordProjection(
			id->getOriginalType(), arg, *id);
	outputExprs.push_back(outputExpr1);

	accs.push_back(MAX);
		expressions::Expression* outputExpr2 = new expressions::RecordProjection(
				classa->getOriginalType(), arg, *classa);
	outputExprs.push_back(outputExpr2);

	accs.push_back(MAX);
	expressions::Expression* outputExpr3 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	outputExprs.push_back(outputExpr3);

	/* Pred: */
	expressions::Expression* selID  	=
			new expressions::RecordProjection(id->getOriginalType(),arg,*id);
	expressions::Expression* predExpr1 = new expressions::IntConstant(idLow);
	expressions::Expression* predExpr2 = new expressions::IntConstant(idHigh);
	expressions::Expression* predicate1 = new expressions::GtExpression(
				new BoolType(), selID, predExpr1);
	expressions::Expression* predicate2 = new expressions::LtExpression(
					new BoolType(), selID, predExpr2);
	expressions::Expression* predicate = new expressions::AndExpression(
			new BoolType(), predicate1, predicate2);

	opt::Reduce *reduce = new opt::Reduce(accs, outputExprs, predicate, scan, &ctx);
	scan->setParent(reduce);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	reduce->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

void symantecCSV2Caching(map<string,dataset> datasetCatalog)	{

	int classNo  = 195;

	RawContext ctx = prepareContext("symantec-csv-2");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
			symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';

	RecordAttribute *id = argsSymantecCSV["id"];
	RecordAttribute *classa = argsSymantecCSV["classa"];
	RecordAttribute *classb = argsSymantecCSV["classb"];
	RecordAttribute *size = argsSymantecCSV["size"];

	vector<RecordAttribute*> projections;
//	projections.push_back(id);
//	projections.push_back(classa);
//	projections.push_back(classb);
	projections.push_back(size);

	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
				delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/* Materialize size */
	list<RecordAttribute> argProjections;
	argProjections.push_back(*id);
	argProjections.push_back(*classa);
	argProjections.push_back(*classb);
	argProjections.push_back(*size);
	expressions::Expression* arg = new expressions::InputArgument(&rec, 0,
			argProjections);
	expressions::Expression* toMat = new expressions::RecordProjection(
			size->getOriginalType(), arg, *size);
	char matLabel[] = "sizeMaterializer";
	ExprMaterializer *mat = new ExprMaterializer(toMat, linehint, scan, &ctx,
			matLabel);
	scan->setParent(mat);

	/**
	 * REDUCE
	 */

	/* Output: */
	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;

	accs.push_back(MAX);
	expressions::Expression* outputExpr1 = new expressions::RecordProjection(
			id->getOriginalType(), arg, *id);
	outputExprs.push_back(outputExpr1);

	accs.push_back(MAX);
	expressions::Expression* outputExpr2 = new expressions::RecordProjection(
			classa->getOriginalType(), arg, *classa);
	outputExprs.push_back(outputExpr2);

	accs.push_back(MAX);
	expressions::Expression* outputExpr3 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	outputExprs.push_back(outputExpr3);

	accs.push_back(MAX);
	expressions::Expression* outputExpr4 = new expressions::RecordProjection(
			size->getOriginalType(), arg, *size);
	outputExprs.push_back(outputExpr4);

	accs.push_back(SUM);
	expressions::Expression* outputExpr5 = new expressions::IntConstant(1);
	outputExprs.push_back(outputExpr5);

	/* Pred: */
	expressions::Expression* selClass  	=
			new expressions::RecordProjection(classa->getOriginalType(),arg,*classa);
	expressions::Expression* predExpr1 = new expressions::IntConstant(classNo);
	expressions::Expression* predicate = new expressions::EqExpression(
			new BoolType(), selClass, predExpr1);

	opt::Reduce *reduce = new opt::Reduce(accs, outputExprs, predicate, mat, &ctx);
	mat->setParent(reduce);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	reduce->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

/* Can't improve predicate evaluation */
void symantecCSV2(map<string,dataset> datasetCatalog)	{

	int classNo  = 195;

	RawContext ctx = prepareContext("symantec-csv-2");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
			symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';

	RecordAttribute *id = argsSymantecCSV["id"];
	RecordAttribute *classa = argsSymantecCSV["classa"];
	RecordAttribute *classb = argsSymantecCSV["classb"];
	RecordAttribute *size = argsSymantecCSV["size"];

	vector<RecordAttribute*> projections;
//	projections.push_back(id);
//	projections.push_back(classa);
//	projections.push_back(classb);
//	projections.push_back(size);


	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
				delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/**
	 * REDUCE
	 */
	list<RecordAttribute> argProjections;
	argProjections.push_back(*id);
	argProjections.push_back(*classa);
	argProjections.push_back(*classb);
	argProjections.push_back(*size);
	expressions::Expression* arg 			=
				new expressions::InputArgument(&rec,0,argProjections);
	/* Output: */
	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;

	accs.push_back(MAX);
	expressions::Expression* outputExpr1 = new expressions::RecordProjection(
			id->getOriginalType(), arg, *id);
	outputExprs.push_back(outputExpr1);

	accs.push_back(MAX);
	expressions::Expression* outputExpr2 = new expressions::RecordProjection(
			classa->getOriginalType(), arg, *classa);
	outputExprs.push_back(outputExpr2);

	accs.push_back(MAX);
	expressions::Expression* outputExpr3 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	outputExprs.push_back(outputExpr3);

	accs.push_back(MAX);
	expressions::Expression* outputExpr4 = new expressions::RecordProjection(
			size->getOriginalType(), arg, *size);
	outputExprs.push_back(outputExpr4);

	accs.push_back(SUM);
	expressions::Expression* outputExpr5 = new expressions::IntConstant(1);
	outputExprs.push_back(outputExpr5);

	/* Pred: */
	expressions::Expression* selClass  	=
			new expressions::RecordProjection(classa->getOriginalType(),arg,*classa);
	expressions::Expression* predExpr1 = new expressions::IntConstant(classNo);
	expressions::Expression* predicate = new expressions::EqExpression(
			new BoolType(), selClass, predExpr1);

	opt::Reduce *reduce = new opt::Reduce(accs, outputExprs, predicate, scan, &ctx);
	scan->setParent(reduce);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	reduce->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

/* Can't improve predicate evaluation */
void symantecCSV3(map<string,dataset> datasetCatalog)	{

	string botName  = "DARKMAILER3";

	RawContext ctx = prepareContext("symantec-csv-3");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
			symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';

	RecordAttribute *classb = argsSymantecCSV["classb"];
	RecordAttribute *bot = argsSymantecCSV["bot"];

	vector<RecordAttribute*> projections;
	projections.push_back(bot);
//	projections.push_back(classb);


	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
				delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/**
	 * REDUCE
	 */
	list<RecordAttribute> argProjections;
	argProjections.push_back(*classb);
	argProjections.push_back(*bot);
	expressions::Expression* arg 			=
				new expressions::InputArgument(&rec,0,argProjections);
	/* Output: */
	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;

	accs.push_back(MAX);
	expressions::Expression* outputExpr1 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	outputExprs.push_back(outputExpr1);

	accs.push_back(SUM);
	expressions::Expression* outputExpr2 = new expressions::IntConstant(1);
	outputExprs.push_back(outputExpr2);

	/* Pred: */
	expressions::Expression* selBot = new expressions::RecordProjection(
			bot->getOriginalType(), arg, *bot);
	expressions::Expression* predExpr1 = new expressions::StringConstant(
			botName);
	expressions::Expression* predicate = new expressions::EqExpression(
			new BoolType(), selBot, predExpr1);

	opt::Reduce *reduce = new opt::Reduce(accs, outputExprs, predicate, scan, &ctx);
	scan->setParent(reduce);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	reduce->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

void symantecCSV3v1(map<string,dataset> datasetCatalog)	{

	string botName  = "DARKMAILER3";
	int idLow = 100000000;
	int idHigh = 200000000;

	RawContext ctx = prepareContext("symantec-csv-3v1");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
			symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';

	RecordAttribute *classb = argsSymantecCSV["classb"];
	RecordAttribute *bot = argsSymantecCSV["bot"];
	RecordAttribute *id = argsSymantecCSV["id"];

	vector<RecordAttribute*> projections;
	projections.push_back(bot);
//	projections.push_back(classb);


	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
				delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	list<RecordAttribute> argSelections;
	argSelections.push_back(*id);
	expressions::Expression* argSel = new expressions::InputArgument(&rec, 0,
			argSelections);
	expressions::Expression* selID = new expressions::RecordProjection(
			id->getOriginalType(), argSel, *id);
	expressions::Expression* predExprNum1 = new expressions::IntConstant(idLow);
	expressions::Expression* predExprNum2 = new expressions::IntConstant(idHigh);
	expressions::Expression* predicateNum1 = new expressions::GtExpression(
			new BoolType(), selID, predExprNum1);
	expressions::Expression* predicateNum2 = new expressions::LtExpression(
			new BoolType(), selID, predExprNum2);
	expressions::Expression* predicateSel = new expressions::AndExpression(
			new BoolType(), predicateNum1, predicateNum2);

	Select *sel = new Select(predicateSel, scan);
	scan->setParent(sel);


	/**
	 * REDUCE
	 */
	list<RecordAttribute> argProjections;
	argProjections.push_back(*classb);
	argProjections.push_back(*bot);
	expressions::Expression* arg 			=
				new expressions::InputArgument(&rec,0,argProjections);
	/* Output: */
	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;

	accs.push_back(MAX);
	expressions::Expression* outputExpr1 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	outputExprs.push_back(outputExpr1);

	accs.push_back(SUM);
	expressions::Expression* outputExpr2 = new expressions::IntConstant(1);
	outputExprs.push_back(outputExpr2);

	/* Pred: */
	expressions::Expression* selBot = new expressions::RecordProjection(
			bot->getOriginalType(), arg, *bot);
	expressions::Expression* predExpr1 = new expressions::StringConstant(
			botName);
	expressions::Expression* predicate = new expressions::EqExpression(
			new BoolType(), selBot, predExpr1);

	opt::Reduce *reduce = new opt::Reduce(accs, outputExprs, predicate, sel, &ctx);
	sel->setParent(reduce);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	reduce->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

void symantecCSV4(map<string, dataset> datasetCatalog) {

	int idLow  = 40000000;
	int idHigh = 50000000;
	string botName = "Bobax";
	RawContext ctx = prepareContext("symantec-csv-4(agg)");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
				symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';
	RecordAttribute *id = argsSymantecCSV["id"];
	RecordAttribute *classa = argsSymantecCSV["classa"];
	RecordAttribute *classb = argsSymantecCSV["classb"];
	RecordAttribute *bot = argsSymantecCSV["bot"];
	RecordAttribute *country_code = argsSymantecCSV["country_code"];

	vector<RecordAttribute*> projections;
//	projections.push_back(id);
//	projections.push_back(classa);
//	projections.push_back(classb);
	projections.push_back(country_code);
	projections.push_back(bot);


	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
			delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/**
	 * SELECT
	 * id > 40000000 and id < 50000000 and bot = 'Bobax'
	 */
	list<RecordAttribute> argSelections;
	argSelections.push_back(*id);
	argSelections.push_back(*bot);

	expressions::Expression* arg 			=
					new expressions::InputArgument(&rec,0,argSelections);
	expressions::Expression* selID  	=
				new expressions::RecordProjection(id->getOriginalType(),arg,*id);
	expressions::Expression* selBot  	=
					new expressions::RecordProjection(bot->getOriginalType(),arg,*bot);

	expressions::Expression* predExpr1 = new expressions::IntConstant(idLow);
	expressions::Expression* predExpr2 = new expressions::IntConstant(idHigh);
	expressions::Expression* predExpr3 = new expressions::StringConstant(
			botName);


	expressions::Expression* predicate1 = new expressions::GtExpression(
			new BoolType(), selID, predExpr1);
	expressions::Expression* predicate2 = new expressions::LtExpression(
			new BoolType(), selID, predExpr2);
	expressions::Expression* predicateNum = new expressions::AndExpression(
				new BoolType(), predicate1, predicate2);

	expressions::Expression* predicateStr = new expressions::EqExpression(
				new BoolType(), selBot, predExpr3);

	Select *selNum = new Select(predicateNum, scan);
	scan->setParent(selNum);

	//rest of preds
	Select *sel = new Select(predicateStr, selNum);
	selNum->setParent(sel);

	/**
	 * NEST
	 * GroupBy: country_code
	 * Pred: Redundant (true == true)
	 * 		-> I wonder whether it gets statically removed..
	 * Output: max(classa), max(classb), count(*)
	 */
	list<RecordAttribute> nestProjections;
	nestProjections.push_back(*classa);
	nestProjections.push_back(*classb);
	nestProjections.push_back(*country_code);
	expressions::Expression* nestArg = new expressions::InputArgument(&rec, 0,
			nestProjections);

	//f (& g) -> GROUPBY country_code
	expressions::RecordProjection* f = new expressions::RecordProjection(
			country_code->getOriginalType(), nestArg, *country_code);
	//p
	expressions::Expression* lhsNest = new expressions::BoolConstant(true);
	expressions::Expression* rhsNest = new expressions::BoolConstant(true);
	expressions::Expression* predNest = new expressions::EqExpression(
			new BoolType(), lhsNest, rhsNest);

	//mat.
	vector<RecordAttribute*> fields;
	vector<materialization_mode> outputModes;
	fields.push_back(classa);
	outputModes.insert(outputModes.begin(), EAGER);
	fields.push_back(classb);
	outputModes.insert(outputModes.begin(), EAGER);
	fields.push_back(country_code);
	outputModes.insert(outputModes.begin(), EAGER);

	Materializer* mat = new Materializer(fields, outputModes);

	char nestLabel[] = "nest_cluster";
	string aggrLabel = string(nestLabel);


	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;
	vector<string> aggrLabels;
	string aggrField1;
	string aggrField2;
	string aggrField3;

	/* Aggregate 1: MAX(classa) */
	expressions::Expression* aggrClassa = new expressions::RecordProjection(
			classa->getOriginalType(), arg, *classa);
	expressions::Expression* outputExpr1 = aggrClassa;
	aggrField1 = string("_maxClassA");
	accs.push_back(MAX);
	outputExprs.push_back(outputExpr1);
	aggrLabels.push_back(aggrField1);

	/* Aggregate 2: MAX(classb) */
	expressions::Expression* outputExpr2 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	aggrField2 = string("_maxClassB");
	accs.push_back(MAX);
	outputExprs.push_back(outputExpr2);
	aggrLabels.push_back(aggrField2);

	/* Aggregate 3: COUNT(*) */
	expressions::Expression* outputExpr3 = new expressions::IntConstant(1);
	aggrField3 = string("_aggrCount");
	accs.push_back(SUM);
	outputExprs.push_back(outputExpr3);
	aggrLabels.push_back(aggrField3);

	radix::Nest *nestOp = new radix::Nest(&ctx, accs, outputExprs, aggrLabels,
			predNest, f, f, sel, nestLabel, *mat);
	sel->setParent(nestOp);

	Function* debugInt = ctx.getFunction("printi");
	Function* debugFloat = ctx.getFunction("printFloat");
	IntType intType = IntType();
	FloatType floatType = FloatType();

	/* OUTPUT */
	RawOperator *lastPrintOp;
	RecordAttribute *toOutput1 = new RecordAttribute(1, aggrLabel, aggrField1,
			&intType);
	expressions::RecordProjection* nestOutput1 =
			new expressions::RecordProjection(&intType, nestArg, *toOutput1);
	Print *printOp1 = new Print(debugInt, nestOutput1, nestOp);
	nestOp->setParent(printOp1);

	RecordAttribute *toOutput2 = new RecordAttribute(2, aggrLabel, aggrField2,
			&floatType);
	expressions::RecordProjection* nestOutput2 =
			new expressions::RecordProjection(&floatType, nestArg, *toOutput2);
	Print *printOp2 = new Print(debugFloat, nestOutput2, printOp1);
	printOp1->setParent(printOp2);

	RecordAttribute *toOutput3 = new RecordAttribute(3, aggrLabel, aggrField3,
			&intType);
	expressions::RecordProjection* nestOutput3 =
			new expressions::RecordProjection(&intType, nestArg, *toOutput3);
	Print *printOp3 = new Print(debugInt, nestOutput3, printOp2);
	printOp2->setParent(printOp3);
	lastPrintOp = printOp3;


	Root *rootOp = new Root(lastPrintOp);
	lastPrintOp->setParent(rootOp);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	rootOp->produce();
//	reduce->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

//XXX FIXME Crashes
//select max(classa), max(classb), count(*) as COUNTER from spamsclasses400m  where id > 40000000 and id < 50000000 group by country_code;
void symantecCSV4v1(map<string, dataset> datasetCatalog) {

	int idLow  = 40000000;
	int idHigh = 50000000;
//	string botName = "Bobax";
	RawContext ctx = prepareContext("symantec-csv-4(agg)");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
				symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';
	RecordAttribute *id = argsSymantecCSV["id"];
	RecordAttribute *classa = argsSymantecCSV["classa"];
	RecordAttribute *classb = argsSymantecCSV["classb"];
//	RecordAttribute *bot = argsSymantecCSV["bot"];
	RecordAttribute *country_code = argsSymantecCSV["country_code"];

	vector<RecordAttribute*> projections;
//	projections.push_back(id);
//	projections.push_back(classa);
//	projections.push_back(classb);
	projections.push_back(country_code);
//	projections.push_back(bot);


	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
			delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/**
	 * SELECT
	 * id > 40000000 and id < 50000000 and bot = 'Bobax'
	 */
	list<RecordAttribute> argSelections;
	argSelections.push_back(*id);
//	argSelections.push_back(*bot);

	expressions::Expression* arg 			=
					new expressions::InputArgument(&rec,0,argSelections);
	expressions::Expression* selID  	=
				new expressions::RecordProjection(id->getOriginalType(),arg,*id);
//	expressions::Expression* selBot  	=
//					new expressions::RecordProjection(bot->getOriginalType(),arg,*bot);

	expressions::Expression* predExpr1 = new expressions::IntConstant(idLow);
	expressions::Expression* predExpr2 = new expressions::IntConstant(idHigh);
//	expressions::Expression* predExpr3 = new expressions::StringConstant(
//			botName);


	expressions::Expression* predicate1 = new expressions::GtExpression(
			new BoolType(), selID, predExpr1);
	expressions::Expression* predicate2 = new expressions::LtExpression(
			new BoolType(), selID, predExpr2);
	expressions::Expression* predicateNum = new expressions::AndExpression(
				new BoolType(), predicate1, predicate2);

//	expressions::Expression* predicateStr = new expressions::EqExpression(
//				new BoolType(), selBot, predExpr3);

	Select *sel = new Select(predicateNum, scan);
	scan->setParent(sel);

//	//rest of preds
//	Select *sel = new Select(predicateStr, selNum);
//	selNum->setParent(sel);

	/**
	 * NEST
	 * GroupBy: country_code
	 * Pred: Redundant (true == true)
	 * 		-> I wonder whether it gets statically removed..
	 * Output: max(classa), max(classb), count(*)
	 */
	list<RecordAttribute> nestProjections;
	nestProjections.push_back(*classa);
	nestProjections.push_back(*classb);
	nestProjections.push_back(*country_code);
	expressions::Expression* nestArg = new expressions::InputArgument(&rec, 0,
			nestProjections);

	//f (& g) -> GROUPBY country_code
	expressions::RecordProjection* f = new expressions::RecordProjection(
			country_code->getOriginalType(), nestArg, *country_code);
	//p
	expressions::Expression* lhsNest = new expressions::BoolConstant(true);
	expressions::Expression* rhsNest = new expressions::BoolConstant(true);
	expressions::Expression* predNest = new expressions::EqExpression(
			new BoolType(), lhsNest, rhsNest);

	//mat.
	vector<RecordAttribute*> fields;
	vector<materialization_mode> outputModes;
	fields.push_back(classa);
	outputModes.insert(outputModes.begin(), EAGER);
	fields.push_back(classb);
	outputModes.insert(outputModes.begin(), EAGER);
	fields.push_back(country_code);
	outputModes.insert(outputModes.begin(), EAGER);

	Materializer* mat = new Materializer(fields, outputModes);

	char nestLabel[] = "nest_cluster";
	string aggrLabel = string(nestLabel);


	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;
	vector<string> aggrLabels;
	string aggrField1;
	string aggrField2;
	string aggrField3;

	/* Aggregate 1: MAX(classa) */
	expressions::Expression* aggrClassa = new expressions::RecordProjection(
			classa->getOriginalType(), arg, *classa);
	expressions::Expression* outputExpr1 = aggrClassa;
	aggrField1 = string("_maxClassA");
	accs.push_back(MAX);
	outputExprs.push_back(outputExpr1);
	aggrLabels.push_back(aggrField1);

	/* Aggregate 2: MAX(classb) */
	expressions::Expression* outputExpr2 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	aggrField2 = string("_maxClassB");
	accs.push_back(MAX);
	outputExprs.push_back(outputExpr2);
	aggrLabels.push_back(aggrField2);

	/* Aggregate 3: COUNT(*) */
	expressions::Expression* outputExpr3 = new expressions::IntConstant(1);
	aggrField3 = string("_aggrCount");
	accs.push_back(SUM);
	outputExprs.push_back(outputExpr3);
	aggrLabels.push_back(aggrField3);

	radix::Nest *nestOp = new radix::Nest(&ctx, accs, outputExprs, aggrLabels,
			predNest, f, f, sel, nestLabel, *mat);
	sel->setParent(nestOp);

	Function* debugInt = ctx.getFunction("printi");
	Function* debugFloat = ctx.getFunction("printFloat");
	IntType intType = IntType();
	FloatType floatType = FloatType();

	/* OUTPUT */
	RawOperator *lastPrintOp;
	RecordAttribute *toOutput1 = new RecordAttribute(1, aggrLabel, aggrField1,
			&intType);
	expressions::RecordProjection* nestOutput1 =
			new expressions::RecordProjection(&intType, nestArg, *toOutput1);
	Print *printOp1 = new Print(debugInt, nestOutput1, nestOp);
	nestOp->setParent(printOp1);

	RecordAttribute *toOutput2 = new RecordAttribute(2, aggrLabel, aggrField2,
			&floatType);
	expressions::RecordProjection* nestOutput2 =
			new expressions::RecordProjection(&floatType, nestArg, *toOutput2);
	Print *printOp2 = new Print(debugFloat, nestOutput2, printOp1);
	printOp1->setParent(printOp2);

	RecordAttribute *toOutput3 = new RecordAttribute(3, aggrLabel, aggrField3,
			&intType);
	expressions::RecordProjection* nestOutput3 =
			new expressions::RecordProjection(&intType, nestArg, *toOutput3);
	Print *printOp3 = new Print(debugInt, nestOutput3, printOp2);
	printOp2->setParent(printOp3);
	lastPrintOp = printOp3;


	Root *rootOp = new Root(lastPrintOp);
	lastPrintOp->setParent(rootOp);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	rootOp->produce();
//	reduce->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

//select max(classb), count(*) as COUNTER from spamsclasses400m  where id > 40000000 and id < 50000000 and classa > 105 and classa < 125 and bot = 'Lethic' group by classa;
void symantecCSV4v2(map<string, dataset> datasetCatalog) {

	int classaLow = 105;
	int classaHigh = 125;
	int sizeLow = 100;
	string botName = "Lethic";
	RawContext ctx = prepareContext("symantec-csv-4(agg)");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
				symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';
	RecordAttribute *id = argsSymantecCSV["id"];
	RecordAttribute *classa = argsSymantecCSV["classa"];
	RecordAttribute *classb = argsSymantecCSV["classb"];
	RecordAttribute *size = argsSymantecCSV["size"];
	RecordAttribute *bot = argsSymantecCSV["bot"];

//	RecordAttribute *country_code = argsSymantecCSV["country_code"];

	vector<RecordAttribute*> projections;
//	projections.push_back(id);
//	projections.push_back(classa);
//	projections.push_back(classb);
//	projections.push_back(country_code);
	projections.push_back(bot);
//	projections.push_back(size);


	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
			delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/**
	 * SELECT
	 * id > 40000000 and id < 50000000 and bot = 'Bobax'
	 */
	list<RecordAttribute> argSelections;
	argSelections.push_back(*id);
	argSelections.push_back(*bot);
	argSelections.push_back(*classa);
	argSelections.push_back(*size);

	expressions::Expression* arg 			=
					new expressions::InputArgument(&rec,0,argSelections);
//	expressions::Expression* selID  	=
//				new expressions::RecordProjection(id->getOriginalType(),arg,*id);
	expressions::Expression* selSize  	=
					new expressions::RecordProjection(size->getOriginalType(),arg,*size);
	expressions::Expression* selClassa 	=
					new expressions::RecordProjection(classa->getOriginalType(),arg,*classa);
	expressions::Expression* selBot  	=
					new expressions::RecordProjection(bot->getOriginalType(),arg,*bot);

//	expressions::Expression* predExpr1 = new expressions::IntConstant(idLow);
//	expressions::Expression* predExpr2 = new expressions::IntConstant(idHigh);
	expressions::Expression* predExpr1 = new expressions::IntConstant(sizeLow);

	expressions::Expression* predExpr3 = new expressions::IntConstant(classaLow);
	expressions::Expression* predExpr4 = new expressions::IntConstant(classaHigh);

	expressions::Expression* predExpr5 = new expressions::StringConstant(
			botName);

	expressions::Expression* predicateNum1 = new expressions::GtExpression(
			new BoolType(), selSize, predExpr1);

	expressions::Expression* predicateNum3 = new expressions::GtExpression(
				new BoolType(), selClassa, predExpr3);
	expressions::Expression* predicateNum4 = new expressions::LtExpression(
				new BoolType(), selClassa, predExpr4);

	Select *selNum1 = new Select(predicateNum3, scan);
	scan->setParent(selNum1);

	Select *selNum2 = new Select(predicateNum4, selNum1);
	selNum1->setParent(selNum2);

	Select *selNum3 = new Select(predicateNum1, selNum2);
	selNum2->setParent(selNum3);

	expressions::Expression* predicateStr = new expressions::EqExpression(
			new BoolType(), selBot, predExpr5);

	//rest of preds
	Select *sel = new Select(predicateStr, selNum3);
	selNum3->setParent(sel);

	/**
	 * NEST
	 * GroupBy: classa
	 * Pred: Redundant (true == true)
	 * 		-> I wonder whether it gets statically removed..
	 * Output: max(classa), max(classb), count(*)
	 */
	list<RecordAttribute> nestProjections;
	nestProjections.push_back(*classa);
	nestProjections.push_back(*classb);
//	nestProjections.push_back(*country_code);
	expressions::Expression* nestArg = new expressions::InputArgument(&rec, 0,
			nestProjections);

	//f (& g) -> GROUPBY country_code
	expressions::RecordProjection* f = new expressions::RecordProjection(
			classa->getOriginalType(), nestArg, *classa);
	//p
	expressions::Expression* lhsNest = new expressions::BoolConstant(true);
	expressions::Expression* rhsNest = new expressions::BoolConstant(true);
	expressions::Expression* predNest = new expressions::EqExpression(
			new BoolType(), lhsNest, rhsNest);

	//mat.
	vector<RecordAttribute*> fields;
	vector<materialization_mode> outputModes;
//	fields.push_back(classa);
//	outputModes.insert(outputModes.begin(), EAGER);
	fields.push_back(classb);
	outputModes.insert(outputModes.begin(), EAGER);
//	fields.push_back(country_code);
//	outputModes.insert(outputModes.begin(), EAGER);

	Materializer* mat = new Materializer(fields, outputModes);

	char nestLabel[] = "nest_cluster";
	string aggrLabel = string(nestLabel);


	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;
	vector<string> aggrLabels;
	string aggrField1;
	string aggrField2;
	string aggrField3;

	/* Aggregate 1: MAX(classa) */
//	expressions::Expression* aggrClassa = new expressions::RecordProjection(
//			classa->getOriginalType(), arg, *classa);
//	expressions::Expression* outputExpr1 = aggrClassa;
//	aggrField1 = string("_maxClassA");
//	accs.push_back(MAX);
//	outputExprs.push_back(outputExpr1);
//	aggrLabels.push_back(aggrField1);

	/* Aggregate 2: MAX(classb) */
	expressions::Expression* outputExpr2 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	aggrField2 = string("_maxClassB");
	accs.push_back(MAX);
	outputExprs.push_back(outputExpr2);
	aggrLabels.push_back(aggrField2);

	/* Aggregate 3: COUNT(*) */
	expressions::Expression* outputExpr3 = new expressions::IntConstant(1);
	aggrField3 = string("_aggrCount");
	accs.push_back(SUM);
	outputExprs.push_back(outputExpr3);
	aggrLabels.push_back(aggrField3);

	radix::Nest *nestOp = new radix::Nest(&ctx, accs, outputExprs, aggrLabels,
			predNest, f, f, sel, nestLabel, *mat);
	sel->setParent(nestOp);

	Function* debugInt = ctx.getFunction("printi");
	Function* debugFloat = ctx.getFunction("printFloat");
	IntType intType = IntType();
	FloatType floatType = FloatType();

	/* OUTPUT */
	RawOperator *lastPrintOp;
//	RecordAttribute *toOutput1 = new RecordAttribute(1, aggrLabel, aggrField1,
//			&intType);
//	expressions::RecordProjection* nestOutput1 =
//			new expressions::RecordProjection(&intType, nestArg, *toOutput1);
//	Print *printOp1 = new Print(debugInt, nestOutput1, nestOp);
//	nestOp->setParent(printOp1);

	RecordAttribute *toOutput2 = new RecordAttribute(2, aggrLabel, aggrField2,
			&floatType);
	expressions::RecordProjection* nestOutput2 =
			new expressions::RecordProjection(&floatType, nestArg, *toOutput2);
	Print *printOp2 = new Print(debugFloat, nestOutput2, nestOp);
	nestOp->setParent(printOp2);

	RecordAttribute *toOutput3 = new RecordAttribute(3, aggrLabel, aggrField3,
			&intType);
	expressions::RecordProjection* nestOutput3 =
			new expressions::RecordProjection(&intType, nestArg, *toOutput3);
	Print *printOp3 = new Print(debugInt, nestOutput3, printOp2);
	printOp2->setParent(printOp3);
	lastPrintOp = printOp3;


	Root *rootOp = new Root(lastPrintOp);
	lastPrintOp->setParent(rootOp);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	rootOp->produce();
//	reduce->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

/*
 * Since caches are columnar, intuitively so should the operators
 * for better locality
 */
void symantecCSV5(map<string, dataset> datasetCatalog) {

	int idLow = 90000000;
	int idHigh = 130000000;
	string botName = "FESTI";
	int sizeHigh = 1500;
	string countryCode1 = "US";
	string countryCode2 = "RU";
	string countryCode3 = "IN";

	RawContext ctx = prepareContext("symantec-csv-5(agg)");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
				symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';
	RecordAttribute *id = argsSymantecCSV["id"];
	RecordAttribute *classa = argsSymantecCSV["classa"];
	RecordAttribute *classb = argsSymantecCSV["classb"];
	RecordAttribute *country_code = argsSymantecCSV["country_code"];
	RecordAttribute *size = argsSymantecCSV["size"];
	RecordAttribute *bot = argsSymantecCSV["bot"];

	vector<RecordAttribute*> projections;
//	projections.push_back(id);
//	projections.push_back(classa);
//	projections.push_back(classb);
	projections.push_back(country_code);
//	projections.push_back(size);
	projections.push_back(bot);



	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
			delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/**
	 * SELECT
	 * id > 90000000 and id < 130000000 and bot = 'FESTI' and size < 1500 and (country_code = 'US' OR country_code = 'RU' OR country_code = 'IN')
	 */
	list<RecordAttribute> argSelections;
	argSelections.push_back(*id);
	argSelections.push_back(*bot);

	expressions::Expression* arg 			=
					new expressions::InputArgument(&rec,0,argSelections);
	expressions::Expression* selID  	=
				new expressions::RecordProjection(id->getOriginalType(),arg,*id);
	expressions::Expression* selSize  	=
						new expressions::RecordProjection(size->getOriginalType(),arg,*size);
	expressions::Expression* selBot  	=
					new expressions::RecordProjection(bot->getOriginalType(),arg,*bot);
	expressions::Expression* selCode  	=
						new expressions::RecordProjection(country_code->getOriginalType(),arg,*country_code);

	expressions::Expression* predExpr1 = new expressions::IntConstant(idLow);
	expressions::Expression* predExpr2 = new expressions::IntConstant(idHigh);
	expressions::Expression* predExpr3 = new expressions::IntConstant(sizeHigh);
	expressions::Expression* predExpr4 = new expressions::StringConstant(
			botName);
	expressions::Expression* predExpr5a = new expressions::StringConstant(
			countryCode1);
	expressions::Expression* predExpr5b = new expressions::StringConstant(
			countryCode2);
	expressions::Expression* predExpr5c = new expressions::StringConstant(
			countryCode3);

	//id > 90000000 and id < 130000000
	expressions::Expression* predicate1 = new expressions::GtExpression(
			new BoolType(), selID, predExpr1);
	expressions::Expression* predicate2 = new expressions::LtExpression(
			new BoolType(), selID, predExpr2);
	expressions::Expression* predicateNum1 = new expressions::AndExpression(
				new BoolType(), predicate1, predicate2);
	Select *selNum1 = new Select(predicateNum1, scan);
	scan->setParent(selNum1);

	// size < 1500
	expressions::Expression* predicateNum2 = new expressions::LtExpression(
			new BoolType(), selSize, predExpr3);

	Select *selNum2 = new Select(predicateNum2, selNum1);
	selNum1->setParent(selNum2);

	//bot = 'FESTI'
	expressions::Expression* predicateStr1 = new expressions::EqExpression(
			new BoolType(), selBot, predExpr4);
	Select *selStr1 = new Select(predicateStr1, selNum2);
		selNum2->setParent(selStr1);

	//(country_code = 'US' OR country_code = 'RU' OR country_code = 'IN')
	expressions::Expression* predicateCodeStrA = new expressions::EqExpression(
				new BoolType(), selCode, predExpr5a);
	expressions::Expression* predicateCodeStrB = new expressions::EqExpression(
					new BoolType(), selCode, predExpr5b);
	expressions::Expression* predicateCodeStrC = new expressions::EqExpression(
					new BoolType(), selCode, predExpr5c);

	expressions::Expression* predicateOr_ = new expressions::OrExpression(
				new BoolType(), predicateCodeStrA, predicateCodeStrB);
	expressions::Expression* predicateStr2 = new expressions::OrExpression(
					new BoolType(), predicateOr_, predicateCodeStrC);

	//rest of preds
	Select *sel = new Select(predicateStr2, selStr1);
	selStr1->setParent(sel);

	/**
	 * NEST
	 * GroupBy: country_code
	 * Pred: Redundant (true == true)
	 * 		-> I wonder whether it gets statically removed..
	 * Output: max(classa), max(classb), count(*)
	 */
	list<RecordAttribute> nestProjections;
	nestProjections.push_back(*classa);
	nestProjections.push_back(*classb);
	nestProjections.push_back(*country_code);
	expressions::Expression* nestArg = new expressions::InputArgument(&rec, 0,
			nestProjections);

	//f (& g) -> GROUPBY country_code
	expressions::RecordProjection* f = new expressions::RecordProjection(
			country_code->getOriginalType(), nestArg, *country_code);
	//p
	expressions::Expression* lhsNest = new expressions::BoolConstant(true);
	expressions::Expression* rhsNest = new expressions::BoolConstant(true);
	expressions::Expression* predNest = new expressions::EqExpression(
			new BoolType(), lhsNest, rhsNest);

	//mat.
	vector<RecordAttribute*> fields;
	vector<materialization_mode> outputModes;
	fields.push_back(classa);
	outputModes.insert(outputModes.begin(), EAGER);
	fields.push_back(classb);
	outputModes.insert(outputModes.begin(), EAGER);
	fields.push_back(country_code);
	outputModes.insert(outputModes.begin(), EAGER);

	Materializer* mat = new Materializer(fields, outputModes);

	char nestLabel[] = "nest_cluster";
	string aggrLabel = string(nestLabel);


	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;
	vector<string> aggrLabels;
	string aggrField1;
	string aggrField2;
	string aggrField3;

	/* Aggregate 1: MAX(classa) */
	expressions::Expression* aggrClassa = new expressions::RecordProjection(
			classa->getOriginalType(), arg, *classa);
	expressions::Expression* outputExpr1 = aggrClassa;
	aggrField1 = string("_maxClassA");
	accs.push_back(MAX);
	outputExprs.push_back(outputExpr1);
	aggrLabels.push_back(aggrField1);

	/* Aggregate 2: MAX(classb) */
	expressions::Expression* outputExpr2 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	aggrField2 = string("_maxClassB");
	accs.push_back(MAX);
	outputExprs.push_back(outputExpr2);
	aggrLabels.push_back(aggrField2);

	/* Aggregate 2: COUNT(*) */
	expressions::Expression* outputExpr3 = new expressions::IntConstant(1);
	aggrField3 = string("_aggrCount");
	accs.push_back(SUM);
	outputExprs.push_back(outputExpr3);
	aggrLabels.push_back(aggrField3);

	radix::Nest *nestOp = new radix::Nest(&ctx, accs, outputExprs, aggrLabels,
			predNest, f, f, sel, nestLabel, *mat);
	sel->setParent(nestOp);

	Function* debugInt = ctx.getFunction("printi");
	Function* debugFloat = ctx.getFunction("printFloat");
	IntType intType = IntType();
	FloatType floatType = FloatType();

	/* OUTPUT */
	RawOperator *lastPrintOp;
	RecordAttribute *toOutput1 = new RecordAttribute(1, aggrLabel, aggrField1,
			&intType);
	expressions::RecordProjection* nestOutput1 =
			new expressions::RecordProjection(&intType, nestArg, *toOutput1);
	Print *printOp1 = new Print(debugInt, nestOutput1, nestOp);
	nestOp->setParent(printOp1);
	lastPrintOp = printOp1;

	RecordAttribute *toOutput2 = new RecordAttribute(2, aggrLabel, aggrField2,
			&intType);
	expressions::RecordProjection* nestOutput2 =
			new expressions::RecordProjection(&floatType, nestArg, *toOutput2);
	Print *printOp2 = new Print(debugFloat, nestOutput2, printOp1);
	printOp1->setParent(printOp2);
	lastPrintOp = printOp2;

	RecordAttribute *toOutput3 = new RecordAttribute(3, aggrLabel, aggrField3,
			&intType);
	expressions::RecordProjection* nestOutput3 =
			new expressions::RecordProjection(&intType, nestArg, *toOutput3);
	Print *printOp3 = new Print(debugInt, nestOutput3, printOp2);
	printOp2->setParent(printOp3);
	lastPrintOp = printOp3;


	Root *rootOp = new Root(lastPrintOp);
	lastPrintOp->setParent(rootOp);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	rootOp->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

//Very good caching candidate (id,classa,classb)
/*
 * Since caches are columnar, intuitively so should the operators
 * for better locality
 */
void symantecCSV6(map<string, dataset> datasetCatalog) {

	int idLow  = 100000000;
	int idHigh = 200000000;
	int classHigh = 10;
	RawContext ctx = prepareContext("symantec-csv-6(agg)");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
				symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';
	RecordAttribute *id = argsSymantecCSV["id"];
	RecordAttribute *classa = argsSymantecCSV["classa"];
	RecordAttribute *classb = argsSymantecCSV["classb"];


	vector<RecordAttribute*> projections;
//	projections.push_back(id);
//	projections.push_back(classa);
//	projections.push_back(classb);


	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
			delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/**
	 * SELECT
	 * id < 200000000 and id > 100000000 and classa < 10
	 */
	list<RecordAttribute> argSelections;
	argSelections.push_back(*id);
	argSelections.push_back(*classa);

	expressions::Expression* arg 			=
					new expressions::InputArgument(&rec,0,argSelections);
	expressions::Expression* selID  	=
				new expressions::RecordProjection(id->getOriginalType(),arg,*id);
	expressions::Expression* selClass  	=
					new expressions::RecordProjection(classa->getOriginalType(),arg,*classa);

	expressions::Expression* predExpr1 = new expressions::IntConstant(idLow);
	expressions::Expression* predExpr2 = new expressions::IntConstant(idHigh);
	expressions::Expression* predExpr3 = new expressions::IntConstant(classHigh);


	expressions::Expression* predicate1 = new expressions::GtExpression(
			new BoolType(), selID, predExpr1);
	expressions::Expression* predicate2 = new expressions::LtExpression(
			new BoolType(), selID, predExpr2);
	expressions::Expression* predicateAnd = new expressions::AndExpression(
				new BoolType(), predicate1, predicate2);

	Select *selNum = new Select(predicateAnd, scan);
	scan->setParent(selNum);

	expressions::Expression* predicate3 = new expressions::LtExpression(
				new BoolType(), selClass, predExpr3);

	Select *sel = new Select(predicate3, selNum);
	selNum->setParent(sel);

	/**
	 * NEST
	 * GroupBy: classa
	 * Pred: Redundant (true == true)
	 * 		-> I wonder whether it gets statically removed..
	 * Output: max(id), max(classb)
	 */
	list<RecordAttribute> nestProjections;
	nestProjections.push_back(*id);
	nestProjections.push_back(*classa);
	nestProjections.push_back(*classb);
	expressions::Expression* nestArg = new expressions::InputArgument(&rec, 0,
			nestProjections);

	//f (& g) -> GROUPBY country_code
	expressions::RecordProjection* f = new expressions::RecordProjection(
			classa->getOriginalType(), nestArg, *classa);
	//p
	expressions::Expression* lhsNest = new expressions::BoolConstant(true);
	expressions::Expression* rhsNest = new expressions::BoolConstant(true);
	expressions::Expression* predNest = new expressions::EqExpression(
			new BoolType(), lhsNest, rhsNest);

	//mat.
	vector<RecordAttribute*> fields;
	vector<materialization_mode> outputModes;
	fields.push_back(id);
	outputModes.insert(outputModes.begin(), EAGER);
	fields.push_back(classa);
	outputModes.insert(outputModes.begin(), EAGER);
	fields.push_back(classb);
	outputModes.insert(outputModes.begin(), EAGER);

	Materializer* mat = new Materializer(fields, outputModes);

	char nestLabel[] = "nest_cluster";
	string aggrLabel = string(nestLabel);


	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;
	vector<string> aggrLabels;
	string aggrField1;
	string aggrField2;
	string aggrField3;

	/* Aggregate 1: MAX(id) */
	expressions::Expression* aggrID = new expressions::RecordProjection(
			id->getOriginalType(), arg, *id);
	expressions::Expression* outputExpr1 = aggrID;
	aggrField1 = string("_maxID");
	accs.push_back(MAX);
	outputExprs.push_back(outputExpr1);
	aggrLabels.push_back(aggrField1);

	/* Aggregate 2: MAX(classb) */
	expressions::Expression* outputExpr2 = new expressions::RecordProjection(
			classb->getOriginalType(), arg, *classb);
	aggrField2 = string("_maxClassB");
	accs.push_back(MAX);
	outputExprs.push_back(outputExpr2);
	aggrLabels.push_back(aggrField2);

	radix::Nest *nestOp = new radix::Nest(&ctx, accs, outputExprs, aggrLabels,
			predNest, f, f, sel, nestLabel, *mat);
	sel->setParent(nestOp);

	Function* debugInt = ctx.getFunction("printi");
	Function* debugFloat = ctx.getFunction("printFloat");
	IntType intType = IntType();
	FloatType floatType = FloatType();

	/* OUTPUT */
	RawOperator *lastPrintOp;
	RecordAttribute *toOutput1 = new RecordAttribute(1, aggrLabel, aggrField1,
			&intType);
	expressions::RecordProjection* nestOutput1 =
			new expressions::RecordProjection(&intType, nestArg, *toOutput1);
	Print *printOp1 = new Print(debugInt, nestOutput1, nestOp);
	nestOp->setParent(printOp1);

	RecordAttribute *toOutput2 = new RecordAttribute(2, aggrLabel, aggrField2,
			&floatType);
	expressions::RecordProjection* nestOutput2 =
			new expressions::RecordProjection(&floatType, nestArg, *toOutput2);
	Print *printOp2 = new Print(debugFloat, nestOutput2, printOp1);
	printOp1->setParent(printOp2);
	lastPrintOp = printOp2;

	Root *rootOp = new Root(lastPrintOp);
	lastPrintOp->setParent(rootOp);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	rootOp->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

//select max(id), max(size) from spamsclasses400m
//where id > 90000000 and id < 130000000 and size < 1500 and classa > 50 and classa < 60 group by classa;
/*
 * Since caches are columnar, intuitively so should the operators
 * for better locality
 */
void symantecCSV7(map<string, dataset> datasetCatalog) {

	int idLow  = 90000000;
	int idHigh = 130000000;
	int classLow = 50;
	int classHigh = 60;
	int sizeHigh = 1500;
	RawContext ctx = prepareContext("symantec-csv-7(agg)");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantecCSV = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantecCSV 	=
				symantecCSV.recType.getArgsMap();

	/**
	 * SCAN CSV FILE
	 */
	string fname = symantecCSV.path;
	RecordType rec = symantecCSV.recType;
	int linehint = symantecCSV.linehint;
	int policy = 5;
	char delimInner = ';';
	RecordAttribute *id = argsSymantecCSV["id"];
	RecordAttribute *classa = argsSymantecCSV["classa"];
	RecordAttribute *size = argsSymantecCSV["size"];

	vector<RecordAttribute*> projections;
//	projections.push_back(id);
//	projections.push_back(classa);
//	projections.push_back(size);


	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
			delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/**
	 * SELECT
	 * id > 90000000 and id < 130000000 and size < 1500 and classa > 50 and classa < 60
	 */
	list<RecordAttribute> argSelections;
	argSelections.push_back(*id);
	argSelections.push_back(*classa);
	argSelections.push_back(*size);

	expressions::Expression* arg 			=
					new expressions::InputArgument(&rec,0,argSelections);
	expressions::Expression* selID  	=
				new expressions::RecordProjection(id->getOriginalType(),arg,*id);
	expressions::Expression* selClass  	=
					new expressions::RecordProjection(classa->getOriginalType(),arg,*classa);
	expressions::Expression* selSize  	=
						new expressions::RecordProjection(size->getOriginalType(),arg,*size);

	expressions::Expression* predExpr1 = new expressions::IntConstant(idLow);
	expressions::Expression* predExpr2 = new expressions::IntConstant(idHigh);
	expressions::Expression* predExpr3 = new expressions::IntConstant(sizeHigh);
	expressions::Expression* predExpr4 = new expressions::IntConstant(classLow);
	expressions::Expression* predExpr5 = new expressions::IntConstant(classHigh);

	expressions::Expression* predicate1 = new expressions::GtExpression(
			new BoolType(), selID, predExpr1);
	expressions::Expression* predicate2 = new expressions::LtExpression(
			new BoolType(), selID, predExpr2);
	expressions::Expression* predicateAnd1 = new expressions::AndExpression(
			new BoolType(), predicate1, predicate2);

	Select *selNum1 = new Select(predicateAnd1, scan);
	scan->setParent(selNum1);

	expressions::Expression* predicate3 = new expressions::LtExpression(
			new BoolType(), selSize, predExpr3);
	Select *selNum2 = new Select(predicate3, selNum1);
	selNum1->setParent(selNum2);

	expressions::Expression* predicate4 = new expressions::GtExpression(
			new BoolType(), selClass, predExpr4);
	expressions::Expression* predicate5 = new expressions::LtExpression(
			new BoolType(), selClass, predExpr5);
	expressions::Expression* predicateAnd2 = new expressions::AndExpression(
			new BoolType(), predicate4, predicate5);
	Select *selNum3 = new Select(predicateAnd2, selNum2);
	selNum2->setParent(selNum3);

	Select *sel = selNum3;

	/**
	 * NEST
	 * GroupBy: classa
	 * Pred: Redundant (true == true)
	 * 		-> I wonder whether it gets statically removed..
	 * Output: max(id), max(size)
	 */
	list<RecordAttribute> nestProjections;
	nestProjections.push_back(*id);
	nestProjections.push_back(*classa);
	nestProjections.push_back(*size);

	nestProjections.push_back(*classa);
	expressions::Expression* nestArg = new expressions::InputArgument(&rec, 0,
			nestProjections);

	//f (& g) -> GROUPBY country_code
	expressions::RecordProjection* f = new expressions::RecordProjection(
			classa->getOriginalType(), nestArg, *classa);
	//p
	expressions::Expression* lhsNest = new expressions::BoolConstant(true);
	expressions::Expression* rhsNest = new expressions::BoolConstant(true);
	expressions::Expression* predNest = new expressions::EqExpression(
			new BoolType(), lhsNest, rhsNest);

	//mat.
	vector<RecordAttribute*> fields;
	vector<materialization_mode> outputModes;
	fields.push_back(id);
	outputModes.insert(outputModes.begin(), EAGER);
	fields.push_back(classa);
	outputModes.insert(outputModes.begin(), EAGER);
	fields.push_back(size);
	outputModes.insert(outputModes.begin(), EAGER);

	Materializer* mat = new Materializer(fields, outputModes);

	char nestLabel[] = "nest_cluster";
	string aggrLabel = string(nestLabel);


	vector<Monoid> accs;
	vector<expressions::Expression*> outputExprs;
	vector<string> aggrLabels;
	string aggrField1;
	string aggrField2;
	string aggrField3;

	/* Aggregate 1: MAX(id) */
	expressions::Expression* aggrID = new expressions::RecordProjection(
			id->getOriginalType(), arg, *id);
	expressions::Expression* outputExpr1 = aggrID;
	aggrField1 = string("_maxID");
	accs.push_back(MAX);
	outputExprs.push_back(outputExpr1);
	aggrLabels.push_back(aggrField1);

	/* Aggregate 2: MAX(size) */
	expressions::Expression* outputExpr2 = new expressions::RecordProjection(
			size->getOriginalType(), arg, *size);
	aggrField2 = string("_maxSize");
	accs.push_back(MAX);
	outputExprs.push_back(outputExpr2);
	aggrLabels.push_back(aggrField2);

	radix::Nest *nestOp = new radix::Nest(&ctx, accs, outputExprs, aggrLabels,
			predNest, f, f, sel, nestLabel, *mat);
	sel->setParent(nestOp);

	Function* debugInt = ctx.getFunction("printi");
	IntType intType = IntType();

	/* OUTPUT */
	RawOperator *lastPrintOp;
	RecordAttribute *toOutput1 = new RecordAttribute(1, aggrLabel, aggrField1,
			&intType);
	expressions::RecordProjection* nestOutput1 =
			new expressions::RecordProjection(&intType, nestArg, *toOutput1);
	Print *printOp1 = new Print(debugInt, nestOutput1, nestOp);
	nestOp->setParent(printOp1);

	RecordAttribute *toOutput2 = new RecordAttribute(2, aggrLabel, aggrField2,
			&intType);
	expressions::RecordProjection* nestOutput2 =
			new expressions::RecordProjection(&intType, nestArg, *toOutput2);
	Print *printOp2 = new Print(debugInt, nestOutput2, printOp1);
	printOp1->setParent(printOp2);

	lastPrintOp = printOp2;

	Root *rootOp = new Root(lastPrintOp);
	lastPrintOp->setParent(rootOp);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	rootOp->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

void symantecCSVWarmup(map<string,dataset> datasetCatalog)	{

	RawContext ctx = prepareContext("symantec-csv-warmup");
	RawCatalog& rawCatalog = RawCatalog::getInstance();

	string nameSymantec = string("symantecCSV");
	dataset symantec = datasetCatalog[nameSymantec];
	map<string, RecordAttribute*> argsSymantec 	=
			symantec.recType.getArgsMap();

	/**
	 * SCAN
	 */
	string fname = symantec.path;
	RecordType rec = symantec.recType;
	int linehint = symantec.linehint;
	int policy = 5;
	char delimInner = ';';

	vector<RecordAttribute*> projections;

	pm::CSVPlugin* pg = new pm::CSVPlugin(&ctx, fname, rec, projections,
			delimInner, linehint, policy, false);
	rawCatalog.registerPlugin(fname, pg);
	Scan *scan = new Scan(&ctx, *pg);

	/**
	 * REDUCE
	 * (SUM 1)
	 */
	list<RecordAttribute> argProjections;

	expressions::Expression* arg 			=
				new expressions::InputArgument(&rec,0,argProjections);
	/* Output: */
	expressions::Expression* outputExpr =
			new expressions::IntConstant(1);

	ReduceNoPred *reduce = new ReduceNoPred(SUM, outputExpr, scan, &ctx);
	scan->setParent(reduce);

	//Run function
	struct timespec t0, t1;
	clock_gettime(CLOCK_REALTIME, &t0);
	reduce->produce();
	ctx.prepareFunction(ctx.getGlobalFunction());
	clock_gettime(CLOCK_REALTIME, &t1);
	printf("Execution took %f seconds\n", diff(t0, t1));

	//Close all open files & clear
	pg->finish();
	rawCatalog.clear();
}

//int main()	{
//	cout << "Execution" << endl;
//	map<string,dataset> datasetCatalog;
//	symantecCSVSchema(datasetCatalog);
//
//	cout << "SYMANTEC CSV 1 (+Caching)" << endl;
//	symantecCSV1Caching(datasetCatalog);
//	cout << "**************" << endl;
//	cout << "SYMANTEC CSV 2 (+Caching)" << endl;
//	symantecCSV2Caching(datasetCatalog);
//	cout << "**************" << endl;
//	cout << "SYMANTEC CSV 3v1" << endl;
//	symantecCSV3v1(datasetCatalog);
//	cout << "**************" << endl;
//	cout << "SYMANTEC CSV 4v1" << endl;
//	symantecCSV4v2(datasetCatalog);
//	cout << "**************" << endl;
//	cout << "SYMANTEC CSV 5" << endl;
//	symantecCSV5(datasetCatalog);
//	cout << "**************" << endl;
//	cout << "SYMANTEC CSV 6" << endl;
//	symantecCSV6(datasetCatalog);
//	cout << "**************" << endl;
//	cout << "SYMANTEC CSV 7" << endl;
//	symantecCSV7(datasetCatalog);
//	cout << "**************" << endl;
//}
