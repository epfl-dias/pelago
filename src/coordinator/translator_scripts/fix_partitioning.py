import json
import logging
import copy

parallel = True

input_keys = ["input", "build_input", "probe_input"]
block_ops = ["scan"]
tuple_ops = ["reduce", "hashjoin-chained", "select", "project", "print", "groupby"]


def do_partitioning(obj, part=[]):
    # if req_partitioning and len(req_partitioning) > 0:
    part = copy.deepcopy(part)
    # obj["partitioning"] = part
    if parallel:
        if obj["operator"] == "scan":
            print(part)
            assert(len(part) == 0)
            p = []
        elif obj["operator"] == "hashjoin-chained":
            p = do_partitioning(obj["build_input"], obj.get("build_k_part", obj["build_k"]))
            p = do_partitioning(obj["probe_input"], obj.get("probe_k_part", obj["probe_k"]))
        elif obj["operator"] == "groupby":
            p = do_partitioning(obj["input"], obj.get("k_part", obj["k"]))
        elif obj["operator"] == "tuples-to-block" and len(part) > 0:
            p = do_partitioning(obj["input"], [])
            obj["operator"] = "hash-rearrange"
            obj["buckets"] = obj["dop"]
            if "register_as" in obj["projections"][0]:
                relName = obj["projections"][0]["register_as"]["relName"]
            elif obj["projections"][0]["expression"] == "recordProjection":
                relName = obj["projections"][0]["attribute"]["relName"]
            if isinstance(part, list):
                if len(part) == 1:
                    obj["e"] = part[0]
                    obj["e"].pop("register_as", None)
                    # obj["e"] = {
                    #     "expression": "recordProjection",
                    #     "e": {
                    #         "expression": "argument",
                    #         "argNo": -1,
                    #         "type": {
                    #             "type": "record",
                    #             "relName": part[0]["register_as"]["relName"]
                    #         },
                    #         "attributes": [{
                    #             "relName": part[0]["register_as"]["relName"],
                    #             "attrName": part[0]["register_as"]["attrName"]
                    #         }]
                    #     },
                    #     "attribute": {
                    #         "relName": part[0]["register_as"]["relName"],
                    #         "attrName": part[0]["register_as"]["attrName"]
                    #     }
                    # }
                else:
                    obj["e"] = {
                                "expression": "recordConstruction",
                                "type": {
                                  "type": "record"
                                },
                                "attributes": []
                            }
                    for x in part:
                        if "register_as" in x:
                            xrelName = x["register_as"]["relName"]
                            xattrName = x["register_as"]["attrName"]
                        elif x["expression"] == "recordProjection":
                            xrelName = x["attribute"]["relName"]
                            xattrName = x["attribute"]["attrName"]
                        else:
                            assert(False)
                        obj["e"]["attributes"].append({
                            "name": xattrName,
                            "e": x
                        })
                        obj["e"]["attributes"][-1]["e"].pop("register_as", None)
            else:
                obj["e"] = copy.deepcopy(part)
                obj["e"].pop("register_as", None)
            p = {
                "relName": relName,
                "attrName": "partition_hash_" + relName,
                "attrNo": -1
            }
            obj["hashProject"] = {
                                  "type": {
                                    "type": "int"
                                  },
                                  "relName": relName,
                                  "attrName": "partition_hash_" + relName,
                                  "attrNo": -1
                                }
            # obj["e"]["register_as"] = p
            obj["output"].append(p)
        elif obj["operator"] == "shuffle":
            p = do_partitioning(obj["input"], part)
            obj["operator"] = "exchange"
            obj["target"] = {
                                "expression": "recordProjection",
                                "e": {
                                    "expression": "argument",
                                    "argNo": -1,
                                    "type": {
                                        "type": "record",
                                        "relName": p["relName"]
                                    },
                                    "attributes": [{
                                        "relName": p["relName"],
                                        "attrName": p["attrName"]
                                    }]
                                },
                                "attribute": {
                                    "relName": p["relName"],
                                    "attrName": p["attrName"]
                                }
                            }
        elif obj["operator"] == "broadcast":
            do_partitioning(obj["input"], [])
            p = obj["projections"][0]
            obj["operator"] = "exchange"
            obj["target"] = {
                                "expression": "recordProjection",
                                "e": {
                                    "expression": "argument",
                                    "argNo": -1,
                                    "type": {
                                        "type": "record",
                                        "relName": p["relName"]
                                    },
                                    "attributes": [{
                                        "relName": p["relName"],
                                        "attrName": "__broadcastTarget"
                                    }]
                                },
                                "attribute": {
                                    "relName": p["relName"],
                                    "attrName": "__broadcastTarget"
                                }
                            }
            mem_mov = {"operator": "mem-broadcast-device"}
            mem_mov["to_cpu"] = obj["to_cpu"]
            mem_mov["num_of_targets"] = obj["numOfParents"]
            mem_mov["projections"] = []
            for t in obj["output"]:
                mem_mov["projections"].append({
                    "relName": t["relName"],
                    "attrName": t["attrName"],
                    "isBlock": True
                })
            mem_mov["input"] = obj["input"]
            mem_mov["output"] = copy.deepcopy(obj["output"])
            mem_mov["output"].append({
                "relName": t["relName"],
                "attrName": "__broadcastTarget",
                "isBlock": False
            })
            mem_mov["blockwise"] = True
            mem_mov["gpu"] = False
            if "partitioning" in obj["input"]:
                mem_mov["partitioning"] = obj["input"]["partitioning"]
                mem_mov["dop"] = obj["dop"]
            obj["input"] = mem_mov
            # obj["projections"] = obj["input"]["output"][:]
        elif obj["operator"] == "split":
            p = do_partitioning(obj["input"], part)
            obj["operator"] = "exchange"
            obj["projections"] = obj["input"]["output"][:]
            obj["rand_local_cpu"] = True
            for p in obj["projections"]:
                p["isBlock"] = obj["blockwise"]
        elif obj["operator"] == "union":
            p = do_partitioning(obj["input"], part)
            obj["operator"] = "exchange"
            obj["projections"] = obj["input"]["output"][:]
            for p in obj["projections"]:
                p["isBlock"] = obj["blockwise"]
        elif obj["operator"] == "gpu-to-cpu":
            out = obj["input"]["output"][:]
            p = do_partitioning(obj["input"], part)
            if obj["input"]["output"] != out:
                out.append(obj["input"]["output"][-1])
                obj["output"] = out
                obj["projections"].append(obj["input"]["output"][-1])
        elif obj["operator"] == "cpu-to-gpu":
            out = obj["input"]["output"][:]
            p = do_partitioning(obj["input"], part)
            if obj["input"]["output"] != out:
                out.append(obj["input"]["output"][-1])
                obj["output"] = out
                obj["projections"].append(obj["input"]["output"][-1])
        else:
            for inp in input_keys:
                if inp in obj:
                    p = do_partitioning(obj[inp], part)
        obj["partitioning"] = p
    return p


def fix_partitioning(obj):
    do_partitioning(obj)
    return obj
