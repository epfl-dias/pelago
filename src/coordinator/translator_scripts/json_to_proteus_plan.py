import json
import logging
from filterColumns import get_required_input
from convert_plan import translate_plan
from annotate_block_operators import annotate_block_operator
from annotate_device_jumps_operators import annotate_device_jumps_operator
from annotate_device_operators import annotate_device_operator
from annotate_partitioning import annotate_partitioning
from fix_device_flow import deviceaware_operator
from fix_locality import fixlocality_operator
from fix_flow import flowaware_operator
from fix_partitioning import fix_partitioning
from to_hybr import mark_split_union_exchanges, create_split_union

class plan:
    def __init__(self, str, is_file=True):
        if is_file:
            self.p = json.load(open(str))
        else:
            self.p = json.loads(str)

    def get_required_input(self):
        self.p = get_required_input(self.p)
        return self

    def translate_plan(self):
        self.p = translate_plan(self.p)
        return self

    def annotate_partitioning(self):
        self.p = annotate_partitioning(self.p)
        return self

    def fix_partitioning(self):
        self.p = fix_partitioning(self.p)
        return self

    def annotate_block_operator(self):
        self.p = annotate_block_operator(self.p)
        return self

    def annotate_device_jumps_operator(self):
        self.p = annotate_device_jumps_operator(self.p)
        return self

    def annotate_device_operator(self):
        self.p = annotate_device_operator(self.p)
        return self

    def deviceaware_operator(self, explicit_memcpy):
        self.p = deviceaware_operator(self.p, explicit_memcpy)
        return self

    def fixlocality_operator(self, explicit_memcpy):
        self.p = fixlocality_operator(self.p, explicit_memcpy)
        return self

    def flowaware_operator(self):
        self.p = flowaware_operator(self.p)
        return self

    def to_hybrid(self):
        mark_split_union_exchanges(self.p)
        self.p = create_split_union(self.p)
        return self

    def dump(self):
        return json.dumps(self.p, indent=4)

    def get_obj_plan(self):
        return self.p

    def prepare(self, explicit_memcpy=True, parallel=True, cpu_only=False, hybrid=False):
        if hybrid:
            cpu_only = False
        self.get_required_input()                                         \
            .translate_plan()                                             \
            .annotate_block_operator()
        if not cpu_only:
            self.annotate_device_jumps_operator()
        self.annotate_device_operator()
        if parallel:
            self.annotate_partitioning()
        self.deviceaware_operator(explicit_memcpy)                        \
            .flowaware_operator()
        # if explicit_memcpy:
        self.fixlocality_operator(explicit_memcpy)
        if parallel:
            self.fix_partitioning()
        if hybrid:
            self.to_hybrid();
        return self


if __name__ == "__main__":
    out = plan("../../../plan-calcite.json").prepare()
    with open('/home/periklis/Documents/Coding/PelagoVisPlans/flare.json', 'w') as outf:
        json.dump(out.p, outf, indent=4)
