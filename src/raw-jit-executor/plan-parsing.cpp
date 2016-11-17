//============================================================================
// Name        : planparsing-rapidjson.cpp
// Author      : Manos Karpathiotakis
// Version     :
// Copyright   : 
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "common/common.hpp"
#include "plan/plan-parser.hpp"

//using namespace rapidjson;
//using namespace std;

//struct MyHandler {
//    bool Null() { cout << "Null()" << endl; return true; }
//    bool Bool(bool b) { cout << "Bool(" << boolalpha << b << ")" << endl; return true; }
//    bool Int(int i) { cout << "Int(" << i << ")" << endl; return true; }
//    bool Uint(unsigned u) { cout << "Uint(" << u << ")" << endl; return true; }
//    bool Int64(int64_t i) { cout << "Int64(" << i << ")" << endl; return true; }
//    bool Uint64(uint64_t u) { cout << "Uint64(" << u << ")" << endl; return true; }
//    bool Double(double d) { cout << "Double(" << d << ")" << endl; return true; }
//    bool String(const char* str, SizeType length, bool copy) {
//        cout << "String(" << str << ", " << length << ", " << boolalpha << copy << ")" << endl;
//        return true;
//    }
//    bool StartObject() { cout << "StartObject()" << endl; return true; }
//    bool Key(const char* str, SizeType length, bool copy) {
//        cout << "Key(" << str << ", " << length << ", " << boolalpha << copy << ")" << endl;
//        return true;
//    }
//    bool EndObject(SizeType memberCount) { cout << "EndObject(" << memberCount << ")" << endl; return true; }
//    bool StartArray() { cout << "StartArray()" << endl; return true; }
//    bool EndArray(SizeType elementCount) { cout << "EndArray(" << elementCount << ")" << endl; return true; }
//};

//int main() {
//    const char json[] = " { \"hello\" : \"world\", \"t\" : true , \"f\" : false, \"n\": null, \"i\":123, \"pi\": 3.1416, \"a\":[1, 2, 3, 4] } ";
//
//    //Input Path
//    const char* nameJSON = "inputs/plans/reduce-scan.json";
//    //Prepare Input
//    struct stat statbuf;
//    stat(nameJSON, &statbuf);
//    size_t fsize = statbuf.st_size;
//
//    int fd = open(nameJSON, O_RDONLY);
//    if (fd == -1) {
//    	throw runtime_error(string("json.open"));
//    }
//
//    const char *bufJSON = (const char*) mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
//    if (bufJSON == MAP_FAILED) {
//    	throw runtime_error(string("json.mmap"));
//    }
//
//    //Parse input
////    MyHandler handler;
////    Reader reader;
////    StringStream ss(bufJSON);
////    reader.Parse(ss, handler);
//
//    Document document; // Default template parameter uses UTF8 and MemoryPoolAllocator.
//
////    char buffer[fsize];
////    memcpy(buffer, bufJSON, fsize);
////    if (document.ParseInsitu(buffer).HasParseError())
////    {
////    	printf("\nParsing error\n");
////    	return 1;
////    }
//
//    if (document.Parse(bufJSON).HasParseError())
//    {
//    	return 1;
//    }
//
//    // 2. Access values in document.
//    printf("\nAccess values in document:\n");
//    assert(document.IsObject());    // Document is a JSON value represents the root of DOM. Root can be either an object or array.
//
//	assert(document.HasMember("operator"));
//	assert(document["operator"].IsString());
//    printf("operator = %s\n", document["operator"].GetString());
//
//    return 0;
//}

//int main() {
//	const char* catalogJSON = "inputs/plans/catalog.json";
//	const char* plan1JSON = "inputs/plans/reduce-scan.json";
//	const char* plan2JSON = "inputs/plans/reduce-unnest-scan.json";
//	const char* plan3JSON = "inputs/plans/reduce-join.json";
//	const char* plan4JSON = "inputs/plans/reduce-nest.json";
//
//
//	CatalogParser catalog = CatalogParser(catalogJSON);
//
////	PlanExecutor exec1 = PlanExecutor(plan1JSON,catalog,"scanReduce");
////	PlanExecutor exec2 = PlanExecutor(plan2JSON,catalog,"unnestJSON");
////	PlanExecutor exec3 = PlanExecutor(plan3JSON,catalog,"join"); //10
//	PlanExecutor exec4 = PlanExecutor(plan4JSON,catalog,"nest");
//}

/* Satya's bug report:
 * select count(*)
 * from part,partsupp
 * where p_partkey = ps_partkey;
 **/
int main() {
	const char* catalogJSON = "inputs/plans/tpchCatalog.json";

	/* Satya's test case */
	const char* planJSON = "inputs/plans/reduce-join-parts.json";
	/* Satya's test case: Isolating scan of Part table */
//	const char* planJSON = "inputs/plans/reduce-scan-part.json";
	/* Satya's test case: Isolating scan of PartSupp table */
//	const char* planJSON = "inputs/plans/reduce-scan-partsupp.json";

	CatalogParser catalog = CatalogParser(catalogJSON);

	PlanExecutor exec = PlanExecutor(planJSON, catalog, "join");
}
