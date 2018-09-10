import json
import logging


input_keys = ["input", "build_input", "probe_input"]
block_ops = ["scan"]
tuple_ops = ["reduce", "hashjoin-chained", "select", "project", "groupby"]


def fixlocality_operator(obj, explicit_memcpy=True, target=None):
    if obj["operator"] == "broadcast":
        obj["to_cpu"] = not (target is not None and target == "gpu")
        obj["input"] = fixlocality_operator(obj["input"], explicit_memcpy, None)
    elif obj["operator"] == "block-to-tuples":
        # if target is not None:
        #     print(target)
        # assert(target is None)
        if not obj["gpu"]:  # cpu
            print("->>" + obj["input"]["operator"])
            mem_mov = {"operator": "mem-move-device"}
            mem_mov["to_cpu"] = True
            mem_mov["projections"] = []
            for t in obj["output"]:
                mem_mov["projections"].append({
                    "relName": t["relName"],
                    "attrName": t["attrName"],
                    "isBlock": True
                })
            mem_mov["input"] = fixlocality_operator(obj["input"], explicit_memcpy, None)
            mem_mov["output"] = obj["output"]
            mem_mov["blockwise"] = True
            mem_mov["gpu"] = False
            mem_mov["slack"] = 8
            if "partitioning" in obj["input"]:
                mem_mov["partitioning"] = obj["input"]["partitioning"]
                mem_mov["dop"] = obj["dop"]
            obj["input"] = mem_mov
        else:
            obj["input"] = fixlocality_operator(obj["input"], explicit_memcpy, "gpu")
    elif target and (obj["operator"] == "cpu-to-gpu" or not obj["gpu"]):  # and explicit_memcpy
        for inp in input_keys:
            if inp in obj:
                # if not explicit_memcpy and obj[inp]["operator"] == "split" and obj[inp]["input"]["operator"] == "scan":
                #     print("->" + obj[inp]["operator"] + " " + obj[inp]["input"]["operator"])
                #     print(obj["partitioning"])
                #     print(obj[inp]["projections"])
                #     # print(obj["part"])
                #     print(obj.keys())
                # if not explicit_memcpy and obj[inp]["operator"] == "split" and obj[inp]["input"]["operator"] == "scan" and "inputs/ssbm100/lineorder.csv" == obj[inp]["projections"][0]["relName"]:
                #     obj[inp] = fixlocality_operator(obj[inp], explicit_memcpy, None)
                #     continue
                mem_mov = {"operator": "mem-move-device"}
                if (target == "gpu"):
                    mem_mov_local_to = {"operator": "mem-move-local-to"}
                    mem_mov_local_to["to_cpu"] = (target != "gpu")
                    mem_mov_local_to["projections"] = []
                    for t in obj["output"]:
                        mem_mov_local_to["projections"].append({
                            "relName": t["relName"],
                            "attrName": t["attrName"],
                            "isBlock": True
                        })
                    mem_mov_local_to["input"] = fixlocality_operator(obj[inp], explicit_memcpy, None)
                    mem_mov_local_to["output"] = obj["output"]
                    mem_mov_local_to["blockwise"] = True
                    mem_mov_local_to["gpu"] = False
                    if "partitioning" in obj["input"]:
                        mem_mov_local_to["partitioning"] = obj["input"]["partitioning"]
                        mem_mov_local_to["dop"] = obj["dop"]
                    mem_mov_local_to["slack"] = 4
                    # mem_mov_local_to = fixlocality_operator(obj[inp], explicit_memcpy, None)
                    mem_mov["slack"] = 4
                else:
                    mem_mov_local_to = fixlocality_operator(obj[inp], explicit_memcpy, None)
                    mem_mov["slack"] = 8
                mem_mov["to_cpu"] = (target != "gpu")
                mem_mov["projections"] = []
                for t in obj["output"]:
                    mem_mov["projections"].append({
                        "relName": t["relName"],
                        "attrName": t["attrName"],
                        "isBlock": True
                    })
                mem_mov["input"] = mem_mov_local_to
                mem_mov["output"] = obj["output"]
                mem_mov["blockwise"] = True
                mem_mov["gpu"] = False
                if "partitioning" in obj[inp]:
                    mem_mov["partitioning"] = obj[inp]["partitioning"]
                    mem_mov["dop"] = obj[inp]["dop"]
                obj[inp] = mem_mov
    else:
        for inp in input_keys:
            if inp in obj:
                obj[inp] = fixlocality_operator(obj[inp], explicit_memcpy, target)
    return obj


if __name__ == "__main__":
    plan = json.load(open("deviceaware_flowaware_plan.json"))
    out = fixlocality_operator(plan)
    # print(json.dumps(plan, sort_keys=False, indent=4));
    print(json.dumps(out, indent=4))
    # print(json.dumps(plan, indent=4))
