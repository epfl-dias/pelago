from subprocess import *
from sys import *
from translator_scripts.json_to_proteus_plan import *
from os.path import abspath

planner = None
executor = None


def quit():
    if executor:
        executor.stdin.write('quit\n')
        print("Waiting for executor to exit...")
        executor.wait()
        print("Done")
    if planner:
        planner.stdin.write('quit\n')
        print("Waiting for planner to exit...")
        planner.wait()
        print("Done")


if __name__ == "__main__":
    executor = Popen(["./rawmain-server"], stdin=PIPE, cwd=r"""../../opt/raw""")
    planner = Popen(["java", "-jar", "target/scala-2.12/SQLPlanner-assembly-0.1.jar"], stdin=PIPE, stdout=PIPE, cwd=r"""../SQLPlanner""")
    try:
        while True:
            line = stdin.readline().strip()
            if (line.lower().startswith("select")):
                planner.stdin.write(line + '\n')
                while not planner.poll():
                    plannerline = planner.stdout.readline()
                    if plannerline.startswith("Query parsing error:"):
                        print(plannerline.strip())
                        break
                    elif plannerline == "JSON Serialization:\n":
                        json = ""
                        cntbrackets = 0
                        while not planner.poll():
                            jsonline = planner.stdout.readline()
                            json = json + jsonline
                            cntbrackets = cntbrackets + jsonline.count('{')
                            cntbrackets = cntbrackets - jsonline.count('}')
                            assert(cntbrackets >= 0)
                            if cntbrackets == 0:
                                out = plan(json, False)                     \
                                          .get_required_input()             \
                                          .translate_plan()                 \
                                          .annotate_block_operator()        \
                                          .annotate_device_jumps_operator() \
                                          .annotate_device_operator()       \
                                          .deviceaware_operator()           \
                                          .flowaware_operator()             \
                                          .fixlocality_operator()
                                with open("plan.json", 'w') as fplan:
                                    fplan.write(out.dump())
                                    print(abspath(fplan.name))
                                    fname = abspath(fplan.name)
                                executor.stdin.write("execute plan from file " + fname + "\n")
                                break
                        break
            elif (line.lower() in ["quit", ".quit", "exit", ".exit"]):
                raise(KeyboardInterrupt)
            elif (line.lower().startswith('.')):
                executor.stdin.write(line[1:] + '\n')
    except KeyboardInterrupt as e:
        quit()
    except Exception as e:
        if planner:
            planner.terminate()
        raise
