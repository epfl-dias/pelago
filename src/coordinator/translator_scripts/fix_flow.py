import json
import logging
import copy


input_keys = ["input", "build_input", "probe_input"]
block_ops = ["scan"]
tuple_ops = ["reduce", "hashjoin-chained", "select", "project", "groupby", "sort"]

parallel = True


def unpack_if_needed(obj, root):
    if isinstance(obj, list):
        for x in obj:
            unpack_if_needed(x, root)
    elif isinstance(obj, dict):
        for property, value in obj.iteritems():
            if property not in input_keys and property not in ["output", "build_k_part", "probe_k_part", "k_part"]:
                unpack_if_needed(value, root)
        if "expression" in obj and obj["expression"] == "recordProjection":
            # check if attribute need unpacking and unpack
            relName = obj["attribute"]["relName"]
            attrName = obj["attribute"]["attrName"]
            for inp in input_keys:
                if inp in root:
                    for t in root[inp]["output"]:
                        print("-------> " + str(t) + " " + relName + "_" + attrName)
                        if t["relName"] == relName and "must_unpack" in t and attrName in t["must_unpack"]:
                            print("-------> " + str(t) + " " + relName + "_" + attrName)
                            attrs = []
                            for a in t["must_unpack"]:
                                attrs.append({
                                    "attrName": a,
                                    "relName": relName,
                                    "attrNo": -1
                                })
                            obj["e"] = {
                                "expression": "recordProjection",
                                "e": {
                                    "expression": "argument",
                                    "argNo": -1,
                                    "type": {
                                        "type": "record",
                                        "attributes": attrs
                                    },
                                    "attributes": [{
                                        "relName": relName,
                                        "attrName": t["attrName"]
                                    }]
                                },
                                "attribute": {
                                    "relName": relName,
                                    "attrName": t["attrName"],
                                    "attrNo": -1,
                                    "type": {
                                        "type": "record",
                                        "attributes": attrs
                                    }
                                }
                            }
                            return


def flowaware_operator(obj):
    if "build_k" in obj:
        if "broadcast-based" in obj:
            obj["build_k_part"] = []
        else:
            obj["build_k_part"] = copy.deepcopy(obj["build_k"])
    if "probe_k" in obj:
        if "broadcast-based" in obj:
            obj["probe_k_part"] = []
        else:
            obj["probe_k_part"] = copy.deepcopy(obj["probe_k"])
    if "k" in obj:
        obj["k_part"] = copy.deepcopy(obj["k"])
    for inp in input_keys:
        if inp in obj:
            inpobj = obj[inp]
            if "blockwise" not in obj:
                continue
            if "blockwise" not in inpobj:
                continue
            if obj["blockwise"] != inpobj["blockwise"]:
                obj[inp] = {}
                obj[inp]["input"] = flowaware_operator(inpobj)
                if obj["blockwise"]:
                    obj[inp]["operator"] = "tuples-to-block"
                    if inpobj["operator"] == "cpu-to-gpu":
                        obj[inp]["gpu"] = True
                    elif inpobj["operator"] == "gpu-to-cpu":
                        obj[inp]["gpu"] = False
                    else:
                        obj[inp]["gpu"] = inpobj["gpu"]
                else:
                    obj[inp]["operator"] = "block-to-tuples"
                    obj[inp]["gpu"] = obj["gpu"]
                    if obj["operator"] == "cpu-to-gpu":
                        obj[inp]["gpu"] = True
                    elif obj["operator"] == "gpu-to-cpu":
                        obj[inp]["gpu"] = False
                    else:
                        obj[inp]["gpu"] = obj["gpu"]
                if len(inpobj["output"]) <= 1 or not obj["blockwise"]:
                    projs = []
                    for t in inpobj["output"]:
                        # if "must_unpack" in t:
                        #     for p in t["must_unpack"]:
                        #         projs.append({
                        #             "expression": "recordProjection",
                        #             "e": {
                        #                 "expression": "recordProjection",
                        #                 "e": {
                        #                     "expression": "argument",
                        #                     "argNo": -1,
                        #                     "type": {
                        #                         "type": "record",
                        #                         "relName": t["relName"]
                        #                     },
                        #                     "attributes": [{
                        #                         "relName": t["relName"],
                        #                         "attrName": t["attrName"]
                        #                     }],
                        #                 },
                        #                 "attribute": {
                        #                     "relName": t["relName"],
                        #                     "attrName": t["attrName"]
                        #                 }
                        #             },
                        #             "attribute": {
                        #                 "relName": t["relName"],
                        #                 "attrName": p
                        #             }
                        #         })
                        # else:
                        projs.append({
                            "expression": "recordProjection",
                            "e": {
                                "expression": "argument",
                                "argNo": -1,
                                "type": {
                                    "type": "record",
                                    "relName": t["relName"]
                                },
                                "attributes": [{
                                    "relName": t["relName"],
                                    "attrName": t["attrName"]
                                }]
                            },
                            "attribute": {
                                "relName": t["relName"],
                                "attrName": t["attrName"]
                            }
                        })
                    output = []
                    for t in inpobj["output"][:]:
                        output.append({"attrName": t["attrName"],
                                       "relName": t["relName"],
                                       "isBlock": obj["blockwise"]})
                        if "must_unpack" in t:
                            output[-1]["must_unpack"] = t["must_unpack"][:]
                        # t["isBlock"] = obj["blockwise"]
                    # for t in projs:
                    #     output.append({"attrName": t["attribute"]["attrName"],
                    #                    "relName": t["attribute"]["relName"],
                    #                    "isBlock": obj["blockwise"]})
                else:
                    recs = []
                    name = "record"
                    current_width = 0
                    packed_attrs = []
                    for t in inpobj["output"]:
                        name = name + "_" + t["attrName"]
                        packed_attrs.append(t["attrName"])
                        recs.append({
                                    "name": t["attrName"],
                                    "e": {
                                        "expression": "recordProjection",
                                        "e": {
                                            "expression": "argument",
                                            "argNo": -1,
                                            "type": {
                                                "type": "record",
                                                "relName": t["relName"]
                                            },
                                            "attributes": [{
                                                "relName": t["relName"],
                                                "attrName": t["attrName"]
                                            }]
                                        },
                                        "attribute": {
                                            "relName": t["relName"],
                                            "attrName": t["attrName"]
                                        }
                                    }
                                    })
                    projs = {"expression": "recordConstruction"}
                    projs["type"] = {"type": "record",
                                     "attributes": inpobj["output"][:]}
                    projs["attributes"] = recs
                    projs["register_as"] = {
                          "attrName": name,
                          "relName": inpobj["output"][0]["relName"],
                          "attrNo": -1
                    }
                    projs = [projs]
                    output = [{"attrName": name, "relName": inpobj["output"][0]["relName"], "must_unpack": packed_attrs, "isBlock": True}]
                obj[inp]["projections"] = projs
                obj[inp]["output"] = output
                if "partitioning" in inpobj:
                    obj[inp]["partitioning"] = inpobj["partitioning"][:]
                    obj[inp]["dop"] = inpobj["dop"]
            else:
                obj[inp] = flowaware_operator(inpobj)
            if obj["operator"] in ["cpu-to-gpu", "gpu-to-cpu", "shuffle", "broadcast", "split", "union"]:
                obj["output"] = copy.deepcopy(obj[inp]["output"][:])
                projs = []
                for t in obj["output"]:
                    projs.append({
                        "relName": t["relName"],
                        "attrName": t["attrName"],
                        "isBlock": obj["blockwise"]
                        })
                obj["projections"] = projs
    if obj["operator"] not in ["cpu-to-gpu", "gpu-to-cpu", "shuffle", "broadcast", "split", "union", "tuples-to-block", "block-to-tuples"]:
        print(obj["operator"])
        for inp in input_keys:
            if inp in obj:
                print(obj[inp]["output"])
        unpack_if_needed(obj, obj)
        if "build_e" in obj:
            print(obj['build_e'])
    return obj


if __name__ == "__main__":
    plan = json.load(open("deviceaware_plan.json"))
    out = flowaware_operator(plan)
    # print(json.dumps(plan, sort_keys=False, indent=4));
    print(json.dumps(out, indent=4))
    # print(json.dumps(plan, indent=4))
