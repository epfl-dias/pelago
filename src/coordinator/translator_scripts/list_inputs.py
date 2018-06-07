import json
import os
import sys


def get_inputs(obj):
    possible_inputs = ["input", "build_input", "probe_input"]
    ins = []
    if obj["operator"] == "scan":
        for x in obj["plugin"]["projections"]:
            ins.append(x["relName"] + "." + x["attrName"])
    for inkey in possible_inputs:
        if inkey in obj:
            if isinstance(obj[inkey],(list,)):
                ins += sum([get_inputs(x) for x in obj[inkey]], [])
            else:
                ins += get_inputs(obj[inkey])
    return ins


def createTest(name):
    test_name = os.path.basename(name)
    if test_name.endswith("_plan.json"):
        test_name = test_name[0:-10]
    if test_name.endswith(".json"):
        test_name = test_name[0:-5]
    test_name = os.path.basename(test_name).replace('.', '_')
    test = (r"""
TEST_F(MultiGPUTest, """ + test_name + r""") {
    auto load = [](string filename){
        StorageManager::load(filename, PINNED);
    };
""")
    print(get_inputs(json.load(open(name))))
    for f in set(get_inputs(json.load(open(name)))):
        test = test + ('    load("' + f + '");\n')
    test = test + (r'''
    const char *testLabel = "''' + test_name + r'''";
    const char *planPath  = "inputs/plans/''' + name + r'''";

    runAndVerify(testLabel, planPath);
}''')
    return test


if __name__ == "__main__":
    print(createTest(sys.argv[1]))
