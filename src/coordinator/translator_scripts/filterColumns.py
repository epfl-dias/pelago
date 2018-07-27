import json
import copy


def get_dependencies(obj):
    if isinstance(obj, list):
        deps = []
        for x in obj:
            deps += get_dependencies(x)
        return deps
    if not isinstance(obj, dict):
        return []
    deps = []
    if "rel" in obj and "attr" in obj:
        deps += [{"rel": obj["rel"], "attr": obj["attr"]}]
    for key in obj:
        deps += get_dependencies(obj[key])
    obj["depends_on"] = deps
    return deps


def get_required_input(obj, output=None):
    possible_inputs = ["input", "left", "right"]
    deps = []
    if output is not None:
        deps += output
    for key in obj:
        if key not in possible_inputs and key != "tupleType":
            deps += get_dependencies(obj[key])
    if obj["operator"] == "join":
        obj["leftMat"] = copy.deepcopy(obj["left"]["tupleType"])
        for (mat, out) in zip(obj["leftMat"], obj["tupleType"][0:len(obj["leftMat"])]):
            mat["register_as"] = {
                "attrNo": -1,
                "relName": out["rel"],
                "attrName": out["attr"]
            }
        print(obj["leftMat"])
        print(obj["tupleType"])
        assert(len(obj["tupleType"]) >= len(obj["leftMat"]))
        obj["rightMat"] = copy.deepcopy(obj["right"]["tupleType"])
        for (mat, out) in zip(obj["rightMat"], obj["tupleType"][len(obj["leftMat"]):]):
            mat["register_as"] = {
                "attrNo": -1,
                "relName": out["rel"],
                "attrName": out["attr"]
            }
        print(obj["rightMat"])
        li = None
        for i in range(0, len(obj["leftMat"])):
            if obj["cond"]["left"]["attr"] == obj["leftMat"][i]["attr"]:
                li = i
                obj["cond"]["left"]["register_as"] = obj["leftMat"][i]["register_as"]
                break
        ri = None
        for i in range(0, len(obj["rightMat"])):
            if obj["cond"]["right"]["attr"] == obj["rightMat"][i]["attr"]:
                ri = i
                obj["cond"]["right"]["register_as"] = obj["rightMat"][i]["register_as"]
                break
        del obj["leftMat"][li]
        del obj["rightMat"][ri]
        print("leftcond: " + str(obj["cond"]["left"]))
    # deps = [tuple(x.items()) for x in deps]
    # print(attrs)
    # print(deps)
    if output is not None:
        attrs = [x["attr"] for x in output]
        actual_output = []
        for out in obj["tupleType"]:
            if out["attr"] in attrs:
                actual_output.append({"rel": out["rel"],
                                      "attr": out["attr"],
                                      "type": out["type"]})
        # for projection, also delete expressions
        if obj["operator"] == "projection":
            actual_e = []
            for (out, e1) in zip(obj["tupleType"], obj["e"]):
                if out["attr"] in attrs:
                    actual_e.append(e1)
            obj["e"] = actual_e
        elif obj["operator"] == "join":
            rmat = []
            tmpOutType = [{
                "relName": out["rel"],
                "attrName": out["attr"]
            } for out in actual_output]
            print(obj["rightMat"])
            for rm in obj["rightMat"]:
                print(rm)
                if {"attrName": rm["register_as"]["attrName"], "relName": rm["register_as"]["relName"]} in tmpOutType:
                    rmat.append(rm)
            obj["rightMat"] = rmat
            lmat = []
            for rm in obj["leftMat"]:
                if {"attrName": rm["register_as"]["attrName"], "relName": rm["register_as"]["relName"]} in tmpOutType:
                    lmat.append(rm)
            obj["leftMat"] = lmat
    else:
        actual_output = obj["tupleType"][:]
    obj["output"] = actual_output
    # print(actual_output)
    for inkey in possible_inputs:
        if inkey in obj:
            get_required_input(obj[inkey], deps)
    return obj


if __name__ == "__main__":
    plan = json.load(open("plan.json"))
    out = get_required_input(plan)
    # print(json.dumps(plan, sort_keys=False, indent=4));
    print(json.dumps(out, indent=4))
