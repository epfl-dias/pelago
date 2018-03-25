import json
import logging


input_keys = ["input", "build_input", "probe_input"]
block_ops = ["scan"]
tuple_ops = ["reduce", "hashjoin-chained", "select", "project", "groupby", "sort"]


def deviceaware_operator(obj, explicit_memcpy=True):
    for inp in input_keys:
        if inp in obj:
            inpobj = obj[inp]
            if "gpu" not in obj:
                continue
            if "gpu" not in inpobj:
                continue
            if obj["gpu"] != inpobj["gpu"]:
                obj[inp] = {}
                if obj["gpu"]:
                    obj[inp]["operator"] = "cpu-to-gpu"
                else:
                    obj[inp]["operator"] = "gpu-to-cpu"
                    obj[inp]["granularity"] = "thread"  # FIMXE: not always!
                # isBlock = (inpobj["operator"] != "reduce")  # inpobj["blockwise"]
                isBlock = (inpobj["operator"] != "reduce")  # and inpobj["operator"] != "groupby")  # inpobj["blockwise"]
                isBlock = isBlock or inpobj["blockwise"] or obj["blockwise"]
                # if (not explicit_memcpy) and (not obj["gpu"]):
                #     isBlock = False
                projs = []
                for t in inpobj["output"]:
                    projs.append({
                        "relName": t["relName"],
                        "attrName": t["attrName"],
                        "isBlock": isBlock
                        })
                obj[inp]["projections"] = projs
                obj[inp]["input"] = deviceaware_operator(inpobj, explicit_memcpy)
                obj[inp]["output"] = inpobj["output"]
                obj[inp]["blockwise"] = isBlock
                if "partitioning" in inpobj:
                    obj[inp]["partitioning"] = inpobj["partitioning"][:]
                    obj[inp]["dop"] = inpobj["dop"]
                obj[inp]["queueSize"] = 128*1024
            else:
                obj[inp] = deviceaware_operator(inpobj, explicit_memcpy)
    return obj


if __name__ == "__main__":
    plan = json.load(open("device_annotated_plan.json"))
    out = deviceaware_operator(plan)
    # print(json.dumps(plan, sort_keys=False, indent=4));
    print(json.dumps(out, indent=4))
    # print(json.dumps(plan, indent=4))
