from subprocess import *
from sys import *
import translator_scripts.json_to_proteus_plan as jsonplanner
import os.path
import readline
import atexit
import time
import Queue
import threading
import re
import json
import socket
import argparse
from translator_scripts.list_inputs import createTest


HOST = 'localhost'
RECV_SIZE = 1024

sock = None
conn = None
addr = None

planner = None
executor = None

history_file = os.path.expanduser(r"~/.rawdb-history")

timings = False
timings_csv = False
q = Queue.Queue()

kws = ["result in file ", "error", "ready", "T"]

explicit_memcpy = True
parallel = True
cpu_only = False

gentests = False
gentests_folder = "."
gentests_exec = False


def quit(execu, plan):
    execu.stop()
    plan.stop()


class ProcessObj:
    def __init__(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        # With KeyboardInterrupt, child usually has already been killed
        if exception_type is None or exception_type is KeyboardInterrupt:
            self.stop()
        else:
            self.kill()

    def stop(self):
        if self.p and self.p.poll() is None:
            self.p.stdin.write('quit\n')
            print("Waiting for " + self.name + " to exit...")
            self.p.wait()
            print("Done")

    def kill(self):
        if self.p and self.p.poll() is None:
            self.p.terminate()


def echo_output(pipe):
    for line in iter(pipe.readline, ''):
        print("raw: " + line[:-1])
        for kw in kws:
            if line.startswith(kw):
                q.put(line[:-1])
                break


class ExecutorError(Exception):
    def __init__(self, message):
        # Call the base class constructor with the parameters it needs
        Exception.__init__(self, message)


class Executor(ProcessObj):
    def __init__(self, args):
        ProcessObj.__init__(self)
        wd = os.path.join(os.path.dirname(__file__), r"../../opt/raw")
        self.p = Popen(["./rawmain-server"], stdin=PIPE, stdout=PIPE, cwd=wd)
        self.t = threading.Thread(target=echo_output, args=(self.p.stdout, ))
        self.t.start()
        self.name = "executor"
        self.wait_for(lambda x: (x == "ready"))
        if args.socket:
            print("ready")
            conn.send("ready\n".encode())

    def execute_command(self, cmd):
        self.p.stdin.write(cmd + '\n')

    def execute_plan(self, jsonplan):
        t0 = time.time()
        with open("plan.json", 'w') as fplan:
            fplan.write(jsonplan.dump())
            fname = os.path.abspath(fplan.name)
        t1 = time.time()
        cmd = "execute plan from file " + fname
        self.p.stdin.write(cmd + '\n')
        Tcodegen = None
        Texec = None
        while True:
            token = self.wait_for(lambda x:
                                  x.startswith("result in file ") or
                                  x.startswith("error") or
                                  x.startswith("Tcodegen: ") or
                                  x.startswith("Texecute w sync:") or
                                  x.startswith("Texecute:"))
            if token.startswith("Tcodegen: "):
                Tcodegen = re.match(r'Tcodegen: *(\d+(.\d+)?)ms', token)
                Tcodegen = float(Tcodegen.group(1))
            elif token.startswith("Texecute"):
                Texec = re.match(r'Texecute[^:]*: *(\d+(.\d+)?)ms', token)
                Texec = float(Texec.group(1))
            else:
                if args.socket:
                    conn.send((token+"\n").encode())
                break
        t2 = time.time()
        return ((t1 - t0) * 1000, (t2 - t1) * 1000, Tcodegen, Texec)

    def stop(self):
        ProcessObj.stop(self)
        # self.t.join()

    def kill(self):
        ProcessObj.kill(self)
        # self.t.join()

    def wait_for(self, f):
        for kw in kws:
            if f(kw):
                break
        else:
            assert(False)
        while not self.p.poll():
            try:
                res = q.get(True, 0.01)
                if f(res):
                    return res
            except Queue.Empty:
                pass
        raise(ExecutorError("Executor exited"))


class SQLParseError(Exception):
    def __init__(self, message, details):
        # Call the base class constructor with the parameters it needs
        Exception.__init__(self, message)
        self.details = details


class Planner(ProcessObj):
    def __init__(self, args):
        ProcessObj.__init__(self)
        wd = os.path.join(os.path.dirname(__file__), r"../SQLPlanner")
        schema = os.path.abspath(os.path.join(os.path.dirname(__file__), r"../../opt/raw/inputs/plans/schema.json"))
        cmd = ["java", "-jar", "target/scala-2.12/SQLPlanner-assembly-0.1.jar", schema]
        self.p = Popen(cmd, stdin=PIPE, stdout=PIPE, cwd=wd)
        self.name = "planner"
        # self.stdin = self.p.stdin
        # self.stdout = self.p.stdout

    def get_plan_from_sql(self, sql_query):
        self.p.stdin.write(sql_query + '\n')
        while not self.p.poll():
            plannerline = self.p.stdout.readline()
            if plannerline.startswith("Query parsing error:"):
                raise(SQLParseError("Planning failed", plannerline.strip()))
            elif plannerline == "JSON Serialization:\n":
                jsonstr = ""
                cntbrackets = 0
                while not self.p.poll():
                    jsonline = self.p.stdout.readline()
                    jsonstr = jsonstr + jsonline
                    cntbrackets = cntbrackets + jsonline.count('{')
                    cntbrackets = cntbrackets - jsonline.count('}')
                    assert(cntbrackets >= 0)
                    if cntbrackets == 0:
                        with open("plan-calcite.json", 'w') as fplan:
                            fplan.write(json.dumps(json.loads(jsonstr),
                                                   indent=4))
                        jplan = jsonplanner.plan(jsonstr, False)            \
                                           .prepare(explicit_memcpy,
                                                    parallel,
                                                    cpu_only)
                        return jplan
        raise(SQLParseError("Planning failed", "child exited"))


def init_socket(host, port):
    global sock, conn, addr
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((host, port))
    sock.listen(1)
    conn, addr = sock.accept()

    print("Socket connection initialized with: ", addr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--socket',
                        action="store_true",
                        help="Set to use socket for communication")
    parser.add_argument('--port',
                        action="store",
                        help="Set localhost port to use",
                        default=50001,
                        type=int)

    args = parser.parse_args()

    if(args.socket):
        init_socket(HOST, args.port)
    else:
        try:
            readline.read_history_file(history_file)
        except IOError:
            pass
        atexit.register(readline.write_history_file, history_file)

    try:
        with Executor(args) as executor:
            with Planner(args) as planner:
                while True:
                    if(args.socket):
                        line = conn.recv(RECV_SIZE).decode().strip()
                        print(line)
                    else:
                        line = raw_input('> ').strip()
                    is_test = re.sub('\s+', ' ', line.lower())                \
                                .startswith(".create test ")
                    is_test = is_test or re.sub('\s+', ' ', line.lower())     \
                                           .startswith("create test ")
                    if (line.lower().startswith("select ") or is_test):
                        create_test = None
                        if not line.lower().startswith("select"):
                            line = re.sub('\s+', ' ', line.lower())
                            m = re.match(".?create test ([a-zA-Z_0-9%]+) from(.*)", line)
                            if m is None:
                                print("error (malformed .create test)")
                                continue
                            create_test = m.group(1)
                            line = m.group(2).strip()
                        if not gentests:
                            create_test = None
                        sql_query = line
                        bare_sql_input = "// Query:"
                        if line.strip() != "":
                            bare_sql_input = bare_sql_input + "\n//     " + line
                        # hlen = readline.get_current_history_length()
                        # readline.remove_history_item(hlen - 1)
                        if not args.socket:
                            while ';' not in line:
                                line = raw_input('>> ')
                                bare_sql_input = bare_sql_input + "\n//     "
                                bare_sql_input = bare_sql_input + line
                                line = line.strip()
                                # hlen = readline.get_current_history_length()
                                # readline.remove_history_item(hlen - 1)
                                sql_query = sql_query + " " + line
                            sql_query = sql_query.split(';')
                            qlen = len(sql_query)
                            if qlen > 2:
                                print("error (multiple ';' in a single query)")
                                continue
                            if qlen > 1 and sql_query[1].strip() != "":
                                print("error (text after query end)")
                            sql_query = sql_query[0]
                            readline.add_history(sql_query + ';')
                        t0 = time.time()
                        try:
                            plan = planner.get_plan_from_sql(sql_query)
                        except SQLParseError as e:
                            print("error (" + e.details + ")")
                            continue
                        t1 = time.time()
                        if create_test is None or gentests_exec:
                            (wplan_ms, wexec_ms,
                                Tcodegen, Texec) = executor.execute_plan(plan)
                            if (timings):
                                t2 = time.time()
                                total_ms = (t2 - t0) * 1000
                                plan_ms = (t1 - t0) * 1000
                                if timings_csv:
                                    print('Timing,' + 
                                          '%.2f,' % (total_ms) +
                                          '%.2f,' % (plan_ms) +
                                          '%.2f,' % (wplan_ms) +
                                          '%.2f,' % (wexec_ms) +
                                          '%.2f,' % (Tcodegen) +
                                          '%.2f' % (Texec))
                                else:
                                    print("Total time: " +
                                          '%.2f' % (total_ms) +
                                          "ms, Planning time: " +
                                          '%.2f' % (plan_ms) +
                                          "ms, Flush plan time: " +
                                          '%.2f' % (wplan_ms) +
                                          "ms, Total executor time: " +
                                          '%.2f' % (wexec_ms) +
                                          "ms, Codegen time: " +
                                          '%.2f' % (Tcodegen) +
                                          "ms, Execution time: " +
                                          '%.2f' % (Texec) +
                                          "ms")
                        if create_test is not None:
                            ident = ""
                            if '%' in create_test:
                                if parallel:
                                    ident = ident + "par"
                                else:
                                    ident = ident + "seq"
                                if explicit_memcpy:
                                    ident = ident + "_cpy"
                                else:
                                    ident = ident + "_uva"
                                if cpu_only:
                                    ident = ident + "_cpu"
                            create_test = create_test.replace('%', ident)
                            plan_path = gentests_folder + "/plans/" + create_test + "_plan.json"
                            with open(plan_path, 'w') as p:
                                p.write(plan.dump())
                            with open(gentests_folder + "/" + create_test + "_test.cpp", 'w') as p:
                                p.write("// Options during test generation:"
                                        + "\n//     parallel    : "
                                        + str(parallel)
                                        + "\n//     memcpy      : "
                                        + str(explicit_memcpy)
                                        + "\n//     cpu_only    : "
                                        + str(cpu_only) + "\n")
                                p.write(bare_sql_input)
                                p.write(createTest(plan_path) + "\n")
                    elif (line.lower() in ["quit", ".quit", "exit", ".exit",
                                           ".q", "q"]):
                        break
                    elif (line.lower().startswith(".timings ")):
                        if (line.lower() == ".timings on"):
                            timings = True
                        elif (line.lower() == ".timings off"):
                            timings = False
                        elif (line.lower() == ".timings csv on"):
                            timings = True
                            timings_csv = True
                        elif (line.lower() == ".timings csv off"):
                            timings = True
                            timings_csv = False
                        elif (line.lower() == ".timings csv"):
                            timings = True
                            timings_csv = True
                        else:
                            print("error (unknown option for timings)")
                        print("Show timings set to " + str(timings) +
                              " (csv format? " + str(timings_csv) + ")")
                    elif (line.lower().startswith(".memcpy ")):
                        if (line.lower() == ".memcpy query"):
                            pass
                        elif (line.lower() == ".memcpy on"):
                            explicit_memcpy = True
                        elif (line.lower() == ".memcpy off"):
                            explicit_memcpy = False
                        else:
                            print("error (unknown option for memcpy)")
                        print("Explicit memcpys: " +
                              str(explicit_memcpy))
                    elif (line.lower().startswith(".parallel ")):
                        if (line.lower() == ".parallel query"):
                            pass
                        elif (line.lower() == ".parallel on"):
                            parallel = True
                        elif (line.lower() == ".parallel off"):
                            parallel = False
                        else:
                            print("error (unknown option for parallel execution)")
                        print("Parallel execution: " + str(parallel))
                    elif (line.lower().startswith(".gentests ")):
                        if (line.lower() == ".gentests query"):
                            pass
                        elif (line.lower() == ".gentests on"):
                            gentests = True
                        elif (line.lower() == ".gentests off"):
                            gentests = False
                        elif (line.lower().startswith(".gentests path ")):
                            gentests_folder = line[len(".gentests path "):]
                        elif (line.lower() == ".gentests no-execute"):
                            gentests_exec = False
                        elif (line.lower() == ".gentests execute"):
                            gentests_exec = True
                        else:
                            print("error (unknown option for test generation)")
                        print("Generate tests: " + str(gentests))
                    elif (line.lower().startswith(".cpuonly ")):
                        if (line.lower() == ".cpuonly query"):
                            pass
                        elif (line.lower() == ".cpuonly on"):
                            cpu_only = True
                        elif (line.lower() == ".cpuonly off"):
                            cpu_only = False
                        else:
                            print("error (unknown option for cpuonly execution)")
                        print("Cpu only execution: " + str(cpu_only))
                    elif (line.lower().startswith('.')):
                        executor.execute_command(line[1:])
    except KeyboardInterrupt:
        pass
    finally:
        if conn:
            conn.close()
