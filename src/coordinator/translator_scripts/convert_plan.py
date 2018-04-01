import json
import logging
from math import *
import copy


def convert_tuple_type(obj):
    return [{"relName": fix_rel_name(t["rel"]), "attrName": t["attr"]} for t in obj]


def convert_operator(obj):
    op = obj["operator"]
    converters = {
        "agg": convert_agg,
        "join": convert_join,
        "projection": convert_projection,
        "select": convert_selection,
        "scan": convert_scan,
        "sort": convert_sort,
        "unnest": convert_unnest,
        "default": convert_default
    }
    if op in converters:
        conv = converters[op](obj)
    else:
        conv = converters["default"](obj)
    conv["output"] = convert_tuple_type(obj["output"])
    conv.pop("tupleType", None)
    return conv


def convert_default(obj):
    logging.warning("No converter for operator: " + obj["operator"])
    possible_inputs = ["input", "left", "right"]
    lineest = 1
    for inp in possible_inputs:
        if inp in obj:
            obj[inp] = convert_operator(obj[inp])
            lineest = lineest * obj[inp]
    obj["max_line_estimate"] = lineest
    return obj


def convert_expr_default(obj, parent):
    logging.warning("No converter for expression: " + obj["expression"])
    if "e" in obj:
        obj["e"] = convert_expression("e", obj)
    if "type" in obj:
        obj["type"] = convert_type(obj["type"])
    return obj


convert_binexpr_type = {
    "mult": "multiply"
}


def convert_expr_binary(obj, parent):
    if obj["expression"] in convert_binexpr_type:
        obj["expression"] = convert_binexpr_type[obj["expression"]]
    if "type" in obj:
        obj["type"] = convert_type(obj["type"])
    obj["left"] = convert_expression(obj["left"], obj)
    obj["right"] = convert_expression(obj["right"], obj)
    return obj


conv_type = {
    "INTEGER": "int",
    "BIGINT": "int64",
    "BOOLEAN": "bool",
    "VARCHAR": "dstring"
}


def convert_type(type):
    if type.startswith("CHAR("):
        return "dstring"
    if type.startswith("RecordType("):
        return "record"
    if type.endswith(" ARRAY"):
        return "list"
    return conv_type[type]


def convert_expr_int(obj, parent):
    return {"expression": "int", "v": int(obj["v"])}


def convert_expr_varchar(obj, parent):
    assert(parent is not None)
    assert("depends_on" in parent)
    assert(len(parent["depends_on"]) == 1)
    string_attr = parent["depends_on"][0]
    path = string_attr["relName"] + "." + string_attr["attrName"] + ".dict"
    return {
        "expression": "dstring",
        "v": obj["v"][1:-1],
        "dict": {
            "path": path
        }
    }


def convert_expression(obj, parent=None, allow_type_in_project=False):
    if "rel" in obj and "attr" in obj:
        # print(obj)
        x = {
            "expression": "recordProjection",
            "e": {
                "expression": "argument",
                "argNo": -1,
                "type": {
                    "type": "record",
                    "relName": fix_rel_name(obj["rel"])
                },
                "attributes": [{
                    "relName": fix_rel_name(obj["rel"]),
                    "attrName": obj["attr"]
                }]
            },
            "attribute": {
                "relName": fix_rel_name(obj["rel"]),
                "attrName": obj["attr"]
            }
        }
        if allow_type_in_project and "type" in obj:
            tp = convert_type(obj["type"])
            x["attribute"]["type"] = {"type": tp}
            x["e"]["attributes"][0]["type"] = {"type": tp}
    else:
        if "depends_on" in obj:
            dep_new = []
            for d in obj["depends_on"]:
                dep_new.append({
                    "relName": fix_rel_name(d["rel"]),
                    "attrName": d["attr"]
                })
            obj["depends_on"] = dep_new
        # obj["e"] = convert_expression(obj["e"])
        converters = {
            "mult": convert_expr_binary,
            "div": convert_expr_binary,
            "eq": convert_expr_binary,
            "le": convert_expr_binary,
            "lt": convert_expr_binary,
            "gt": convert_expr_binary,
            "ge": convert_expr_binary,
            "or": convert_expr_binary,
            "and": convert_expr_binary,
            "sub": convert_expr_binary,
            "add": convert_expr_binary,
            "INTEGER": convert_expr_int,
            "VARCHAR": convert_expr_varchar,
            "default": convert_expr_default
        }
        if obj["expression"].startswith("CHAR("):
            x = convert_expr_varchar(obj, parent)
        elif obj["expression"] in converters:
            x = converters[obj["expression"]](obj, parent)
        else:
            x = converters["default"](obj, parent)
    if "register_as" in obj:
        x["register_as"] = obj["register_as"]
    return x


# linehints = {
#     "dates": 2556,
#     "lineorder": 600038145,
#     "supplier": 200000,
#     "part": 1400000,
#     "customer": 3000000,
#     "dates_csv": 2556,
#     "lineorder_csv": 600038145,
#     "supplier_csv": 200000,
#     "part_csv": 1400000,
#     "customer_csv": 3000000,
#     "sailors": 10,
#     "reserves": 10,
#     "inputs/ssbm100/date.csv": 2556,
#     "inputs/ssbm100/lineorder.csv": 600038145,
#     "inputs/ssbm100/supplier.csv": 200000,
#     "inputs/ssbm100/part.csv": 1400000,
#     "inputs/ssbm100/customer.csv": 3000000
# }

rel_names = {}
#     "dates": "inputs/ssbm100/date.csv",
#     "lineorder": "inputs/ssbm100/lineorder.csv",
#     "supplier": "inputs/ssbm100/supplier.csv",
#     "part": "inputs/ssbm100/part.csv",
#     "customer": "inputs/ssbm100/customer.csv",
#     "dates_csv": "inputs/ssbm100/date2.tbl",
#     "lineorder_csv": "inputs/ssbm100/lineorder2.tbl",
#     "supplier_csv": "inputs/ssbm100/supplier2.tbl",
#     "part_csv": "inputs/ssbm100/part2.tbl",
#     "customer_csv": "inputs/ssbm100/customer2.tbl",
#     "sailors": "inputs/sailors.csv",
#     "reserves": "inputs/reserves.csv",
#     "inputs/ssbm100/date.csv": "inputs/ssbm100/date.csv",
#     "inputs/ssbm100/lineorder.csv": "inputs/ssbm100/lineorder.csv",
#     "inputs/ssbm100/supplier.csv": "inputs/ssbm100/supplier.csv",
#     "inputs/ssbm100/part.csv": "inputs/ssbm100/part.csv",
#     "inputs/ssbm100/customer.csv": "inputs/ssbm100/customer.csv"
# }

# csvs = {
#     "dates_csv": {
#         "delimiter": "|",
#         "brackets": False
#     },
#     "lineorder_csv": {
#         "delimiter": "|",
#         "brackets": False
#     },
#     "supplier_csv": {
#         "delimiter": "|",
#         "brackets": False
#     },
#     "part_csv": {
#         "delimiter": "|",
#         "brackets": False
#     },
#     "customer_csv": {
#         "delimiter": "|",
#         "brackets": False
#     },
#     "sailors": {
#         "delimiter": ";",
#         "brackets": False
#     },
#     "reserves": {
#         "delimiter": ";",
#         "brackets": False
#     }
# }


def convert_scan(obj):
    conv = {"operator": "scan"}
    conv["max_line_estimate"] = obj["linehint"]  # linehints[obj["name"]]
    conv["plugin"] = obj["plugin"]
    conv["plugin"]["name"] = fix_rel_name(obj["name"])
    conv["plugin"]["projections"] = [{"relName": fix_rel_name(t["rel"]),
                                      "attrName": t["attr"]}
                                     for t in obj["output"]]
    # TODO: some plugin options can be set by query optimizer,
    #       as for example csv's policy
    # if obj["name"] in csvs:
    #     conv["plugin"] = copy.deepcopy(csvs[obj["name"]])
    #     conv["plugin"]["type"] = "csv"
    #     conv["plugin"]["projections"] = [{"relName": fix_rel_name(t["rel"]),
    #                                       "attrName": t["attr"]}
    #                                      for t in obj["output"]]
    #     conv["plugin"]["policy"] = 2
    #     conv["plugin"]["lines"] = linehints[obj["name"]]
    #     # conv["plugin"]["delimiter"] = "|"
    #     # conv["plugin"]["brackets"] = False
    # else:
    #     conv["plugin"] = {"type": "block", "name": fix_rel_name(obj["name"])}
    #     conv["plugin"]["projections"] = [{"relName": fix_rel_name(t["rel"]),
    #                                       "attrName": t["attr"]}
    #                                      for t in obj["output"]]
    return conv


def convert_sort(obj):
    conv = {}
    conv["operator"] = "sort"
    exp = []
    d = []
    assert(len(obj["args"]) == len(obj["tupleType"]))
    # for (arg, t) in zip(obj["args"], obj["tupleType"]):
    for arg in obj["args"]:
        exp.append({
            "direction": arg["direction"],
            "expression": convert_expression(arg["expression"])
        })
        exp[-1]["expression"]["register_as"] = {
            # "type": {
            #     "type": convert_type(t["type"])
            # },
            "attrNo": -1,
            "relName": fix_rel_name(arg["register_as"]["rel"]),
            "attrName": arg["register_as"]["attr"]
        }
    conv["e"] = exp
    # conv["direction"] = d
    if "input" in obj:
        conv["input"] = convert_operator(obj["input"])
    conv["max_line_estimate"] = conv["input"]["max_line_estimate"]
    return conv


def fix_rel_name(obj):
    if obj in rel_names:
        return rel_names[obj]
    return obj


def convert_unnest(obj):
    conv = {}
    conv["operator"] = "unnest"
    conv["p"] = {"expression": "bool", "v": True}
    conv["argNo"] = 0
    conv["path"] = {
        "name": obj["path"][0]["name"],
        "e": convert_expression(obj["path"][0]["e"])
    }
    conv["input"] = convert_operator(obj["input"])
    conv["max_line_estimate"] = conv["input"]["max_line_estimate"] * 10  # FIXME: arbitrary scale factor
    return conv


def convert_groupby(obj):
    conv = {}
    conv["operator"] = "groupby"
    exp = []
    packet = 1
    conv["k"] = []
    # key_size = 0
    for (kexpr, t) in zip(obj["groups"], obj["tupleType"]):
        e = convert_expression(kexpr)
        e["register_as"] = {
            # "type": {
            #     "type": convert_type(t["type"])
            # },
            "attrNo": -1,
            "relName": fix_rel_name(t["rel"]),
            "attrName": t["attr"]
        }
        conv["k"].append(e)
        # key_size = key_size + type_size[convert_type(e["type"]["type"])]
    # conv["w"] = [32 + key_size]
    for (agg, t) in zip(obj["aggs"], obj["tupleType"][len(obj["groups"]):]):
        acc = ""
        if agg["op"] == "cnt":
            e = {
                "expression": convert_type(t["type"]),
                "v": 1
            }
            acc = "sum"
        else:
            e = convert_expression(agg["e"])
            acc = agg["op"]
            # e["type"] = {
            #     "type": convert_type(t["type"]]
            # }
        e["register_as"] = {
            # "type": {
            #     "type": convert_type(t["type"])
            # },
            "attrNo": -1,
            "relName": fix_rel_name(t["rel"]),
            "attrName": t["attr"]
        }
        # conv["w"].append(type_size[convert_type(t["type"])])
        e2 = {
            "e": e,
            "m": acc,
            "packet": packet,
            "offset": 0
        }
        packet = packet + 1
        exp.append(e2)
    conv["e"] = exp
    conv["hash_bits"] = 10              # FIXME: make more adaptive
    conv["maxInputSize"] = 1024*128     # FIXME: requires upper limit!
    if "input" in obj:
        conv["input"] = convert_operator(obj["input"])
    conv["max_line_estimate"] = conv["input"]["max_line_estimate"]
    return conv


def convert_agg(obj):
    if "groups" in obj and len(obj["groups"]) > 0:
        return convert_groupby(obj)
    conv = {}
    conv["operator"] = "reduce"
    conv["p"] = {"expression": "bool", "v": True}
    exp = []
    acc = []
    for (agg, t) in zip(obj["aggs"], obj["tupleType"]):
        if agg["op"] == "cnt":
            acc.append("sum")
            e = {
                "expression": convert_type(t["type"]),
                "v": 1
            }
        else:
            acc.append(agg["op"])
            e = convert_expression(agg["e"])
            # e["type"] = {
            #     "type": convert_type(t["type"]]
            # }
        e["register_as"] = {
            # "type": {
            #     "type": convert_type(t["type"])
            # },
            "attrNo": -1,
            "relName": fix_rel_name(t["rel"]),
            "attrName": t["attr"]
        }
        exp.append(e)
    conv["e"] = exp
    conv["accumulator"] = acc
    if "input" in obj:
        conv["input"] = convert_operator(obj["input"])
    conv["max_line_estimate"] = 1
    return conv


def convert_projection(obj):
    # if (len(obj["output"]) == 0):  # this may happen after an unnest (count(unnest))
    #     return convert_operator(obj["input"])
    conv = {}
    conv["operator"] = "project"
    if (len(obj["output"]) == 0):
        conv["relName"] = "__empty"
    else:
        conv["relName"] = fix_rel_name(obj["output"][0]["rel"])
    conv["e"] = []
    assert(len(obj["output"]) == len(obj["e"]))
    for (t, e) in zip(obj["output"], obj["e"]):
        exp = convert_expression(e, None, obj["input"]["operator"] == "unnest")
        exp["register_as"] = {
            # "type": {
            #     "type": convert_type(t["type"])
            # },
            "attrNo": -1,
            "relName": fix_rel_name(t["rel"]),
            "attrName": t["attr"]
        }
        conv["e"].append(exp)
    conv["input"] = convert_operator(obj["input"])
    conv["max_line_estimate"] = conv["input"]["max_line_estimate"]
    return conv


def convert_selection(obj):
    conv = {}
    conv["operator"] = "select"
    conv["p"] = convert_expression(obj["cond"])
    conv["input"] = convert_operator(obj["input"])
    conv["max_line_estimate"] = conv["input"]["max_line_estimate"]
    return conv


type_size = {
    "bool": 1,  # TODO: check this
    "int": 32,
    "int64": 64,
    "dstring": 32
}


def calcite_build_side(est_left, est_right):
    if est_left < est_right:
        return "left"
    return "right"


def calcite_probe_side(est_left, est_right):
    if est_left < est_right:
        return "right"
    return "left"


def convert_join(obj):  # FIMXE: for now, right-left is in reverse, for the star schema
    conv = {}
    conv["operator"] = "hashjoin-chained"
    if (obj["cond"]["expression"] != "eq"):
        print(obj["cond"]["expression"])
        assert(False)
    join_input = {}
    join_input["left"] = convert_operator(obj["left"])
    join_input["right"] = convert_operator(obj["right"])

    est_left = join_input["left"]["max_line_estimate"]
    est_right = join_input["right"]["max_line_estimate"]

    build_side = calcite_build_side(est_left, est_right)
    probe_side = calcite_probe_side(est_left, est_right)

    build_lines_est = join_input[build_side]["max_line_estimate"]
    conv["hash_bits"] = int(ceil(log(2.4 * build_lines_est, 2)))  # TODO: reconsider
    conv["maxBuildInputSize"] = min(
                                    1024*1024*128,  # FIXME: remove restriction
                                    int(1.2 * build_lines_est)
                                    )

    conv["max_line_estimate"] = est_right * est_left
    conv["build_k"] = convert_expression(obj["cond"][build_side])
    if "type" in obj["cond"][build_side]:
        conv["build_k"]["type"] = {
            "type": convert_type(obj["cond"][build_side]["type"])
        }
    conv["probe_k"] = convert_expression(obj["cond"][probe_side])
    if "type" in obj["cond"][probe_side]:
        conv["probe_k"]["type"] = {
            "type": convert_type(obj["cond"][probe_side]["type"])
        }
    build_e = []
    leftTuple = obj[build_side + "Mat"]
    leftAttr = obj["cond"][build_side].get("attr", None)
    build_packet = 1
    conv["build_w"] = [32 + type_size[conv["build_k"]["type"]["type"]]]
    for t in leftTuple:
        if t["attr"] != leftAttr:
            for x in obj["tupleType"]:
                if x["attr"] == t["attr"]:
                    e = convert_expression(t)
                    out_t = x
                    out_type = convert_type(out_t["type"])
                    e["register_as"] = t["register_as"]
                    conv["build_w"].append(type_size[out_type])
                    be = {"e": e}
                    be["packet"] = build_packet
                    build_packet = build_packet + 1
                    be["offset"] = 0
                    build_e.append(be)
                    break
        # else:
        #     for x in obj["tupleType"]:
        #         if x["attr"] == leftAttr:
        #             out_t = x
        #             out_type = convert_type(out_t["type"])
        #             # conv["build_k"]["register_as"] = leftAttr["register_as"]
        #             # {
        #             #     # "type": {
        #             #     #     "type": out_type
        #             #     # },
        #             #     "attrNo": -1,
        #             #     "relName": fix_rel_name(out_t["rel"]),
        #             #     "attrName": out_t["attr"]
        #             # }
        #             break
    conv["build_k"] = convert_expression(obj["cond"][build_side])
    conv["build_e"] = build_e
    conv["build_input"] = join_input[build_side]
    probe_e = []
    rightTuple = obj[probe_side + "Mat"]
    rightAttr = obj["cond"][probe_side].get("attr", None)
    probe_packet = 1
    conv["probe_w"] = [32 + type_size[conv["probe_k"]["type"]["type"]]]
    for t in rightTuple:
        if t["attr"] != rightAttr:
            for x in obj["tupleType"]:
                if x["attr"] == t["attr"]:
                    e = convert_expression(t)
                    out_t = x
                    out_type = convert_type(out_t["type"])
                    e["register_as"] = t["register_as"]
                    conv["probe_w"].append(type_size[out_type])
                    pe = {"e": e}
                    pe["packet"] = probe_packet
                    probe_packet = probe_packet + 1
                    pe["offset"] = 0
                    probe_e.append(pe)
                    break
        # else:
        #     for x in obj["tupleType"]:
        #         if x["attr"] == rightAttr:
        #             out_t = x
        #             out_type = convert_type(out_t["type"])
        #             # conv["probe_k"]["register_as"] = rightAttr["register_as"]
        #             # {
        #             #     # "type": {
        #             #     #     "type": out_type
        #             #     # },
        #             #     "attrNo": -1,
        #             #     "relName": fix_rel_name(out_t["rel"]),
        #             #     "attrName": out_t["attr"]
        #             # }
        #             break
    conv["probe_k"] = convert_expression(obj["cond"][probe_side])
    conv["probe_e"] = probe_e
    conv["probe_input"] = join_input[probe_side]
    return conv


def translate_plan(obj):
    out = convert_operator(obj)
    if out["operator"] == "project":
        conv = out
        conv["operator"] = "print"
    else:
        conv = {"operator": "print", "e": []}
        for e in obj["output"]:
            conv["e"].append(convert_expression(e))
        conv["input"] = out
        conv["output"] = []
    return conv


if __name__ == "__main__":
    plan = json.load(open("projected_plan.json"))
    out = translate_plan(plan)
    # print(json.dumps(plan, sort_keys=False, indent=4));
    print(json.dumps(out, indent=4))
    # print(json.dumps(plan, indent=4))
