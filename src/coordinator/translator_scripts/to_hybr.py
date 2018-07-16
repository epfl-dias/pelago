import json
import logging
import copy

parallel = True

input_keys = ["input", "build_input", "probe_input"]
block_ops = ["scan"]
tuple_ops = ["reduce", "hashjoin-chained", "select", "project", "print", "groupby"]

split = 0

def mark_split_union_exchanges(obj):
    global split
    x = True
    for inp in input_keys:
        if inp in obj:
            #note the order, otherwise does not get evaluated
            x = mark_split_union_exchanges(obj[inp]) and x
    if obj["operator"] == "exchange": #x and 
        obj["first_exchange"] = True
        obj["split_id"] = split
        split = split + 1
        # return False
    return x

def convert(obj, cpu_dop, gpu_dop, to_cpu, only_convert = False):
    if only_convert and obj["operator"] == "exchange":
        obj["numOfParents"] = 1
    if not only_convert and obj["operator"] == "exchange":
        new_obj = {"operator": "split"}
        new_obj["split_id"] = obj["split_id"]
        obj["producers"] = 1
        new_obj["gpu"] = False
        if to_cpu:
            obj["numOfParents"] = cpu_dop
            new_obj["numOfParents"] = gpu_dop
            new_obj["projections"] = copy.deepcopy(obj["projections"])
            new_obj["input"] = convert(obj["input"], cpu_dop, gpu_dop, True, True)
            if "target" in obj:
                #for ssb: if has-based exchange => broadcast => put the split after the next operator which is a mem-broadcast!!!
                if obj["input"]["operator"] == "mem-broadcast-device":
                    obj["input"]["num_of_targets"] = cpu_dop
                    new_obj["input"] = copy.deepcopy(obj["input"])
                    new_obj["input"]["num_of_targets"] = gpu_dop
                    # new_obj["input"]["always_share"] = True
                    p = obj["input"]["output"][-1]
                    new_obj["target"] = {
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
                    obj["input"]["input"] = new_obj
                else:
                    assert False #unimplemented
            else:
                obj["input"] = new_obj
        elif "target" in obj:
            # obj["input"]["always_share"] = True
            obj["input"]["input"] = new_obj
        else:
            obj["input"] = new_obj
        return obj
    if to_cpu and "gpu" in obj:
        obj["gpu"] = False
    if to_cpu and "to_cpu" in obj:
        obj["to_cpu"] = True
    if to_cpu and "num_of_targets" in obj:
        obj["num_of_targets"] = cpu_dop
    if to_cpu and obj["operator"] in ["cpu-to-gpu", "gpu-to-cpu", "mem-move-local-to"]:
        return convert(obj["input"], cpu_dop, gpu_dop, to_cpu, only_convert)
    for inp in input_keys:
        if inp in obj:
            obj[inp] = convert(obj[inp], cpu_dop, gpu_dop, to_cpu, only_convert)
    return obj

def has_no_exchange_before_join(obj):
    if "join" in obj["operator"]:
        return True
    if "exchange" in obj["operator"]:
        return False
    for inp in input_keys:
        if inp in obj:
            if not has_no_exchange_before_join(obj[inp]):
                return False
    return True

def create_split_union(obj, cpu_dop, gpu_dop, hybr_mode = False):
    if obj["operator"] == "exchange" and not hybr_mode and has_no_exchange_before_join(obj["input"]):
        if obj["numOfParents"] > 1:
            new_obj = {}
            new_obj["operator"] = "union-all"
            new_obj["gpu"] = False
            new_obj["projections"] = copy.deepcopy(obj["input"]["output"])
            exCPU = {"operator": "exchange", "gpu": False, "producers": cpu_dop}
            exGPU = {"operator": "exchange", "gpu": False, "producers": gpu_dop}
            new_obj["input"] = [exCPU, exGPU]
            exCPU["projections"] = copy.deepcopy(new_obj["projections"])
            exGPU["projections"] = copy.deepcopy(new_obj["projections"])
            exCPU["input"] = convert(copy.deepcopy(obj["input"]), cpu_dop, gpu_dop, True )
            exGPU["input"] = convert(copy.deepcopy(obj["input"]), cpu_dop, gpu_dop, False)
            exCPU["numOfParents"] = 1
            exGPU["numOfParents"] = 1
            obj["input"] = new_obj
            obj["producers"] = 1
            return obj
        else:
            new_obj = {}
            new_obj["operator"] = "union-all"
            new_obj["gpu"] = False
            new_obj["projections"] = copy.deepcopy(obj["projections"])
            exp_cpu = copy.deepcopy(obj)
            exp_cpu["input"] = convert(exp_cpu["input"], cpu_dop, gpu_dop, True)
            exp_cpu["gpu"] = False
            exp_cpu["producers"] = cpu_dop
            obj["input"] = convert(obj["input"], cpu_dop, gpu_dop, False)
            new_obj["input"] = [exp_cpu, obj]
            return new_obj
    for inp in input_keys:
        if inp in obj:
            obj[inp] = create_split_union(obj[inp], cpu_dop, gpu_dop)
    return obj

# obj = json.load(open("/home/periklis/Desktop/in_gpu_plan.json"))
# mark_split_union_exchanges(obj)
# obj = create_split_union(obj)
# print(json.dumps(obj, indent=4))