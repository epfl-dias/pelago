import json
import logging
from math import * 

parallel = True

input_keys = ["input", "build_input", "probe_input"]
block_ops = ["scan"]
tuple_ops = ["reduce", "hashjoin-chained", "select", "project", "print", "groupby"]

num_of_gpus = 2
num_of_cpus = 24


def sel_dop(obj):
    if obj["gpu"]:
        return num_of_gpus
    return num_of_cpus


def mark_partitioning(obj, out_dop=1):
    # if req_partitioning and len(req_partitioning) > 0:
    #     obj["requested_output_partitioning"] = req_partitioning
    # for inp in input_keys:
    #     if inp in obj:
    #         mark_partitioning(obj[inp])
    if parallel:
        if obj["operator"] == "scan":
            obj["partitioning"] = []
            obj["dop"] = 1
            return 1
        elif obj["operator"] == "hashjoin-chained":
            mark_partitioning(obj["build_input"])  # , [obj["build_k"]])
            mark_partitioning(obj["probe_input"])  # , [obj["probe_k"]])
            shuffled = False
            if obj["build_input"]["max_line_estimate"] < 5e7 and obj["build_k"] not in obj["build_input"]["partitioning"]:
                projs = []
                for t in obj["build_input"]["output"]:
                    projs.append({
                        "relName": t["relName"],
                        "attrName": t["attrName"],
                        "isBlock": True
                    })
                obj["build_input"] = {
                    "operator": "broadcast",
                    "partitioning": [],
                    "projections": projs,
                    "input": obj["build_input"],
                    "blockwise": True,
                    "output": obj["build_input"]["output"],
                    "max_line_estimate": obj["build_input"]["max_line_estimate"] * sel_dop(obj),
                    "numOfParents": sel_dop(obj),
                    "dop": sel_dop(obj),
                    "producers": obj["build_input"]["dop"],
                    "gpu": False,
                    "slack": 8
                }
                if obj["probe_input"]["dop"] == 1 and obj["probe_input"]["blockwise"]:
                    obj["probe_input"] = {
                        "operator": "split",
                        "partitioning": [],
                        "input": obj["probe_input"],
                        "blockwise": obj["probe_input"]["max_line_estimate"] >= 1024,
                        "output": obj["probe_input"]["output"],
                        "max_line_estimate": obj["probe_input"]["max_line_estimate"],
                        "numOfParents": sel_dop(obj),
                        "producers": 1,
                        "dop": sel_dop(obj),
                        "gpu": False,
                        "slack": 2
                    }
                shuffled = True
                obj["broadcast-based"] = True
                obj["partitioning"] = []
                obj["dop"] = sel_dop(obj)
                obj["maxBuildInputSize"] = max(int(obj["maxBuildInputSize"] / (sel_dop(obj)/2)), 128*1024*1024)
                obj["hash_bits"]         = min(int(ceil(log(2 * obj["maxBuildInputSize"], 2))), obj["hash_bits"])  # TODO: reconsider
                return obj["dop"]
            else:
                if obj["build_input"]["dop"] == 1 and obj["build_input"]["blockwise"]:
                    obj["build_input"] = {
                        "operator": "split",
                        "partitioning": [],
                        "input": obj["build_input"],
                        "blockwise": obj["build_input"]["max_line_estimate"] >= 1024,
                        "output": obj["build_input"]["output"],
                        "max_line_estimate": obj["build_input"]["max_line_estimate"],
                        "numOfParents": sel_dop(obj),
                        "producers": 1,
                        "dop": sel_dop(obj),
                        "gpu": False,
                        "slack": 2
                    }
                if obj["build_k"] not in obj["build_input"]["partitioning"]:
                    projs = []
                    for t in obj["build_input"]["output"]:
                        projs.append({
                            "relName": t["relName"],
                            "attrName": t["attrName"],
                            "isBlock": True
                        })
                    obj["build_input"] = {
                        "operator": "shuffle",
                        "partitioning": [obj["build_k"]],
                        "projections": projs,
                        "input": obj["build_input"],
                        "blockwise": True,
                        "output": obj["build_input"]["output"],
                        "max_line_estimate": obj["build_input"]["max_line_estimate"],
                        "numOfParents": sel_dop(obj),
                        "dop": sel_dop(obj),
                        "producers": obj["build_input"]["dop"],
                        "gpu": False,
                        "slack": 16
                    }
                if obj["probe_input"]["dop"] == 1 and obj["probe_input"]["blockwise"]:
                    obj["probe_input"] = {
                        "operator": "split",
                        "partitioning": [],
                        "input": obj["probe_input"],
                        "blockwise": obj["probe_input"]["max_line_estimate"] >= 1024,
                        "output": obj["probe_input"]["output"],
                        "max_line_estimate": obj["probe_input"]["max_line_estimate"],
                        "numOfParents": sel_dop(obj),
                        "producers": 1,
                        "dop": sel_dop(obj),
                        "gpu": False,
                        "slack": 2
                    }
                if obj["probe_k"] not in obj["probe_input"]["partitioning"]:
                    projs = []
                    for t in obj["probe_input"]["output"]:
                        projs.append({
                            "relName": t["relName"],
                            "attrName": t["attrName"],
                            "isBlock": True
                        })
                    obj["probe_input"] = {
                        "operator": "shuffle",
                        "partitioning": [obj["probe_k"]],
                        "projections": projs,
                        "input": obj["probe_input"],
                        "blockwise": True,
                        "output": obj["probe_input"]["output"],
                        "max_line_estimate": obj["probe_input"]["max_line_estimate"],
                        "numOfParents": sel_dop(obj),
                        "dop": sel_dop(obj),
                        "producers": obj["probe_input"]["dop"],
                        "gpu": False,
                        "slack": 16
                    }
                obj["partitioning"] = sorted([obj["build_k"], obj["probe_k"]])
                obj["dop"] = sel_dop(obj)
                obj["maxBuildInputSize"] = max(int(obj["maxBuildInputSize"] / (sel_dop(obj)/2)), 128*1024*1024)
                obj["hash_bits"]         = min(int(ceil(log(2 * obj["maxBuildInputSize"], 2))), obj["hash_bits"])  # TODO: reconsider
                return obj["dop"]
        elif obj["operator"] == "groupby":
            # TODO first partition and then group by, or groupby localy and then groupby again globally ?
            mark_partitioning(obj["input"])  # , [obj["k"]])
            if obj["input"]["dop"] == 1 and obj["input"]["blockwise"]:
                obj["input"] = {
                    "operator": "split",
                    "partitioning": [],
                    "input": obj["input"],
                    "blockwise": obj["input"]["max_line_estimate"] >= 1024,
                    "output": obj["input"]["output"],
                    "numOfParents": sel_dop(obj),
                    "producers": 1,
                    "dop": sel_dop(obj),
                    "gpu": False,
                    "slack": 2
                }
            if obj["k"] not in obj["input"]["partitioning"]:
                projs = []
                for t in obj["input"]["output"]:
                    projs.append({
                        "relName": t["relName"],
                        "attrName": t["attrName"],
                        "isBlock": True
                    })
                ps = []
                ks = []
                for t in obj["k"]:
                    ps.append({
                        "relName": t["register_as"]["relName"],
                        "attrName": t["register_as"]["attrName"],
                        "isBlock": True
                    })
                    ks.append({
                        "expression": "recordProjection",
                        "e": {
                            "expression": "argument",
                            "argNo": -1,
                            "type": {
                                "type": "record",
                                "relName": t["register_as"]["relName"]
                            },
                            "attributes": [{
                                "relName": t["register_as"]["relName"],
                                "attrName": t["register_as"]["attrName"]
                            }]
                        },
                        "attribute": {
                            "relName": t["register_as"]["relName"],
                            "attrName": t["register_as"]["attrName"]
                        }
                    })
                ks = []
                for t in obj["e"]:
                    ps.append({
                        "relName": t["e"]["register_as"]["relName"],
                        "attrName": t["e"]["register_as"]["attrName"],
                        "isBlock": True
                    })
                    ks.append({
                        "expression": "recordProjection",
                        "e": {
                            "expression": "argument",
                            "argNo": -1,
                            "type": {
                                "type": "record",
                                "relName": t["e"]["register_as"]["relName"]
                            },
                            "attributes": [{
                                "relName": t["e"]["register_as"]["relName"],
                                "attrName": t["e"]["register_as"]["attrName"]
                            }]
                        },
                        "attribute": {
                            "relName": t["e"]["register_as"]["relName"],
                            "attrName": t["e"]["register_as"]["attrName"]
                        }
                    })
                projs2 = []
                for t in obj["input"]["output"]:
                    projs2.append({
                        "relName": t["relName"],
                        "attrName": t["attrName"],
                        "isBlock": True
                    })
                obj["input"] = {
                                "operator": "shuffle",
                                "partitioning": ps,
                                "projections": projs2,
                                "input": obj["input"],
                                "blockwise": True,
                                "output": obj["input"]["output"],
                                "numOfParents": sel_dop(obj),
                                "dop": sel_dop(obj),
                                "producers": obj["input"]["dop"],
                                "gpu": False,
                                "slack": 16
                               }
            obj["partitioning"] = ps
            # obj["k"] = ks
            obj["dop"] = sel_dop(obj)
            # obj["maxInputSize"] = max(int(obj["maxInputSize"] / (sel_dop(obj)/2)), 128*1024*1024)
            # obj["hash_bits"]    = int(ceil(log(2 * obj["maxInputSize"], 2)))  # TODO: reconsider
            return obj["dop"]
        elif obj["operator"] == "reduce":
            mark_partitioning(obj["input"])  # , [obj["k"]])
            if obj["input"]["dop"] == 1 and obj["input"]["blockwise"]:
                obj["input"] = {
                    "operator": "split",
                    "partitioning": [],
                    "input": obj["input"],
                    "blockwise": obj["input"]["max_line_estimate"] >= 1024,
                    "output": obj["input"]["output"],
                    "numOfParents": sel_dop(obj),
                    "producers": 1,
                    "dop": sel_dop(obj),
                    "gpu": False,
                    "slack": 2
                }
            e = []
            projs = []
            for t in obj["e"]:
                e.append({
                    "expression": "recordProjection",
                    "e": {
                        "expression": "argument",
                        "argNo": -1,
                        "type": {
                            "type": "record",
                            "relName": t["register_as"]["relName"]
                        },
                        "attributes": [{
                            "relName": t["register_as"]["relName"],
                            "attrName": t["register_as"]["attrName"]
                        }]
                    },
                    "attribute": {
                        "relName": t["register_as"]["relName"],
                        "attrName": t["register_as"]["attrName"]
                    }
                })
                projs.append({
                    "relName": t["register_as"]["relName"],
                    "attrName": t["register_as"]["attrName"],
                    "isBlock": False
                })
            obj["input"] = {
                            "operator": "union",
                            "partitioning": [],
                            "projections": projs,
                            "input": {
                                "operator": "reduce",
                                "partitioning": [],
                                "p": obj["p"],
                                "e": obj["e"],
                                "blockwise": False,
                                "accumulator": obj["accumulator"],
                                "max_line_estimate": 1,
                                "input": obj["input"],
                                "output": obj["output"],
                                "gpu": obj["gpu"],
                                "dop": obj["input"]["dop"]
                            },
                            "blockwise": False,
                            "output": obj["output"],
                            "numOfParents": 1,
                            "producers": obj["input"]["dop"],
                            "gpu": False,
                            "numa_local": False,
                            "slack": 8
                           }
            obj["e"] = e
            obj["p"] = {"expression": "bool", "v": True}
            obj["gpu"] = False
            obj["partitioning"] = []
            obj["dop"] = 1
            return 1
        elif obj["operator"] == "sort":
            mark_partitioning(obj["input"])  # , [obj["k"]])
            if obj["input"]["dop"] != 1:
                # e = []
                # projs = []
                # for t in obj["e"]:
                #     e.append({
                #         "expression": "recordProjection",
                #         "e": {
                #             "expression": "argument",
                #             "argNo": -1,
                #             "type": {
                #                 "type": "record",
                #                 "relName": t["register_as"]["relName"]
                #             },
                #             "attributes": [{
                #                 "relName": t["register_as"]["relName"],
                #                 "attrName": t["register_as"]["attrName"]
                #             }]
                #         },
                #         "attribute": {
                #             "relName": t["register_as"]["relName"],
                #             "attrName": t["register_as"]["attrName"]
                #         }
                #     })
                #     projs.append({
                #         "relName": t["register_as"]["relName"],
                #         "attrName": t["register_as"]["attrName"],
                #         "isBlock": False
                #     })
                obj["input"] = {
                                "operator": "union",
                                # "partitioning": [],
                                "projections": obj["input"]["output"],
                                "input": obj["input"],
                                "blockwise": True,
                                "output": obj["input"]["output"],
                                "numOfParents": 1,
                                "producers": obj["input"]["dop"],
                                "gpu": False,
                                "numa_local": False,
                                "slack": 8
                               }
            # obj["e"] = e
            # obj["p"] = {"expression": "bool", "v": True}
            obj["partitioning"] = []
            obj["dop"] = 1
            return 1
        else:
            part = None
            d = None
            for inp in input_keys:
                if inp in obj:
                    mark_partitioning(obj[inp])
                    if obj[inp]["dop"] == 1 and obj[inp]["blockwise"]:
                        obj[inp] = {
                            "operator": "split",
                            "partitioning": [],
                            "input": obj[inp],
                            "blockwise": obj[inp]["max_line_estimate"] >= 1024,
                            "output": obj[inp]["output"],
                            "numOfParents": sel_dop(obj),
                            "producers": 1,
                            "dop": sel_dop(obj),
                            "gpu": False,
                            "slack": 2
                        }
                    if part is None:
                        part = obj[inp]["partitioning"]
                        d = obj[inp]["dop"]
                    else:
                        assert(part == obj[inp]["partitioning"])
                        assert(d == obj[inp]["dop"])
            obj["partitioning"] = part[:]
            obj["dop"] = d
            return d
    else:
        obj["partitioning"] = [[]]
    return obj


# def decise_partitioning(obj, req_partitioning):
#     if "required_output_partitioning" in obj:
#         if "partitioning" in obj:
#             pass
#         else:
#             for inp in input_keys:
#                 if inp in obj:
#                     decise_partitioning(obj[inp], None)


def annotate_partitioning(obj):
    assert(obj["operator"] == "print")
    mark_partitioning(obj["input"])  # , sel_dop(obj["input"]))
    if obj["input"]["dop"] != 1:
        projs = []
        bw = obj["input"]["max_line_estimate"] >= 1024
        for t in obj["input"]["output"]:
            projs.append({
                "relName": t["relName"],
                "attrName": t["attrName"],
                "isBlock": bw
            })
        obj["input"] = {
                        "operator": "union",
                        "partitioning": [],
                        "projections": obj["input"]["output"][:],
                        "input": obj["input"],
                        "blockwise": bw,
                        "output": obj["input"]["output"][:],
                        "numOfParents": 1,
                        "producers": obj["input"]["dop"],
                        "dop": 1,
                        "gpu": False,
                        "numa_local": False,
                        "slack": 8
                       }
    obj["dop"] = 1
    # obj = decise_partitioning(obj)
    return obj


if __name__ == "__main__":
    plan = json.load(open("flow_annotated_plan.json"))
    out = annotate_partitioning(plan)
    # print(json.dumps(plan, sort_keys=False, indent=4));
    print(json.dumps(out, indent=4))
    # print(json.dumps(plan, indent=4))
