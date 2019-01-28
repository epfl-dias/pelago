from __future__ import print_function

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
import datetime
from translator_scripts.list_inputs import createTest, get_inputs

from apiclient.discovery import build
from httplib2 import Http
from oauth2client import file, client, tools
from google.oauth2 import service_account
import googleapiclient.discovery
from dateutil.tz import *

datetime_format = r"""%Y-%m-%dT%H:%M:%S%z"""

parser = argparse.ArgumentParser(parents=[tools.argparser])
parser.add_argument('--socket',
                    action="store_true",
                    help="Set to use socket for communication")
parser.add_argument('--port',
                    action="store",
                    help="Set localhost port to use",
                    default=50001,
                    type=int)
parser.add_argument('--q0',
                    action="store",
                    help="First query index",
                    default=0,
                    type=int)
parser.add_argument('--q0FromSheet',
                    action="store_true",
                    help="Get query index from sheet")
parser.add_argument('--gspreadsheetId',
                    action="store",
                    help="google spreadsheets id to write the results, use with care OVERWRITES OLD ENTRIES!",
                    default="",
                    type=str)
parser.add_argument('--sheetName',
                    action="store",
                    help="google spreadsheets sheet name",
                    default="Timings "+datetime.datetime.now(tzlocal()).strftime(datetime_format),
                    type=str)
parser.add_argument('--commitLabel',
                    action="store",
                    help="commit label",
                    default=datetime.datetime.now(tzlocal()).strftime("intermediate"),
                    type=str)
parser.add_argument('--commitTime',
                    action="store",
                    help="commit time",
                    default=datetime.datetime.now(tzlocal()).strftime(datetime_format),
                    type=str)
parser.add_argument('--failOnParseError',
                    action="store_true",
                    help="Throw an exception and exit with an error code upon SQL parsing error")

args            = parser.parse_args()
spreadsheetId   = args.gspreadsheetId
sheetName       = args.sheetName
commitLabel     = args.commitLabel
commitTime      = args.commitTime
q_id            = args.q0;

bench_time      = datetime.datetime.now(tzlocal()).strftime(datetime_format)

service = None

def init_gspreadsheet():
    global service, q_id
    if spreadsheetId != "":
        # Setup the Sheets API
        SERVICE_ACCOUNT_FILE = 'logger_cred.json'
        SCOPES = ['https://www.googleapis.com/auth/spreadsheets']
        credentials = service_account.Credentials.from_service_account_file(SERVICE_ACCOUNT_FILE, scopes=SCOPES)
        service = googleapiclient.discovery.build('sheets', 'v4',credentials=credentials)
        if args.q0FromSheet:
            values = service.spreadsheets().values().get(
                spreadsheetId=spreadsheetId,
                range=sheetName + '!A1:A1').execute().get('values', [q_id])
            q_id = int(values[0][0])

wsize = 0
values = [] 

def colnum_string(n):
    string = ""
    while n > 0:
        n, remainder = divmod(n - 1, 26)
        string = chr(65 + remainder) + string
    return string

def sheets_flush(q_id):
    global wsize, values;
    if spreadsheetId != "" and len(values) > 0:
        # values = [x] 
        # values = assign.to_json(orient='values')
        # [
        #     delta.to_json(orient='values')
        # ]
        body = {
            'values': values
        }
        result = service.spreadsheets().values().update(
            spreadsheetId=spreadsheetId, 
            valueInputOption='USER_ENTERED',
            range=sheetName + '!A' + str(q_id + 2 - len(values) + 1) + ':' + colnum_string(len(values[0]) + 1) + str(q_id + 2), # q_id is 0-based
            body=body).execute()
        print('{0} cells updated.'.format(result.get('updatedCells')))
        result = service.spreadsheets().values().update(
            spreadsheetId=spreadsheetId, 
            valueInputOption='USER_ENTERED',
            range=sheetName + '!A1:A1',
            body={'values': [[q_id + 1]]}).execute()
        print('{0} cells updated.'.format(result.get('updatedCells')))
    wsize  = 0
    values = []
    

def sheets_append(x, label, sql_query, conf):
    global wsize, values, q_id;
    q_id = q_id + 1
    wsize = wsize + 1
    bench_date = r"""=DATEVALUE(LEFT(B_,10))+TIMEVALUE(MID(B_,12,8))-(IF(MID(B_,20,1) = "+",1,-1)*TIMEVALUE(MID(B_, 21, 2) & ":" & RIGHT(B_,2)))""".replace(r"""_""", str(q_id + 1))
    values.append(['Timing', bench_time, commitTime, commitLabel, label, sql_query] + conf + x + ["=MATCH(S" + str(q_id + 1) + ",SORT(UNIQUE(S:S)),0)-1", r"""=TEXTJOIN(" ", false, T""" + str(q_id + 1) + ", D" + str(q_id + 1) + ")", bench_date])
    
    if wsize < 50: return
    
    sheets_flush(q_id - 1)

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

kws = ["result in file ", "error", "ready", "T", "done", "Optimization", "raw_server"]

explicit_memcpy = True
parallel = True
cpu_only = False
hybrid = False

gentests = False
gentests_folder = "."
gentests_exec = False

num_of_gpus = 2
num_of_cpus = 24

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
        self.loaded = []
        if args.socket:
            print("ready")
            conn.send("ready\n".encode())

    def execute_command(self, cmd):
        self.p.stdin.write(cmd + '\n')

    def execute_plan(self, jsonplan):
        to_load = sorted([x for x in set(get_inputs(jsonplan.get_obj_plan())) if x.startswith('inputs/') and "lineorder" in x]);
        print(to_load)
        if (to_load != self.loaded):
            self.p.stdin.write("unloadall\n")
            self.wait_for(lambda x: x.startswith("done"))
            # for file in to_load:
            #     self.p.stdin.write("load cpus " + file + "\n")
            #     self.wait_for(lambda x: x.startswith("done"))
            self.loaded = to_load
        t0 = time.time()
        with open("plan.json", 'w') as fplan:
            fplan.write(jsonplan.dump() + '\n')
            fname = os.path.abspath(fplan.name)
        t1 = time.time()
        cmd = "execute plan from file " + fname
        self.p.stdin.write(cmd + '\n')
        Tcodegen = None
        Texec = None
        Tdataload = 0
        Tcodeopt = 0
        Tcodeoptnload = 0
        while True:
            token = self.wait_for(lambda x:
                                  x.startswith("result in file ") or
                                  x.startswith("error") or
                                  x.startswith("Tcodegen: ") or
                                  x.startswith("Optimization time: ") or
                                  " C: " in x or
                                  " G: " in x or
                                  (x.startswith("Topen (") and ',' not in x) or
                                  x.startswith("Texecute w sync:") or
                                  x.startswith("Texecute:"))
            if token.startswith("Tcodegen: "):
                Tcodegen = re.match(r'Tcodegen: *(\d+(.\d+)?)ms', token)
                Tcodegen = float(Tcodegen.group(1))
            elif token.startswith("Optimization time: "):
                m = re.match(r'Optimization time: *(\d+(.\d+)?)ms', token)
                Tcodeopt = Tcodeopt + float(m.group(1))
            elif (" C: " in token or " G: " in token):
                m = re.match(r'.* [CG]: *(\d+(.\d+)?)ms', token)
                Tcodeoptnload = Tcodeoptnload + float(m.group(1))
            elif (token.startswith("Topen (") and ',' not in x):
                m = re.match(r'.*\): *(\d+(.\d+)?)ms', token)
                Tdataload = Tdataload + float(m.group(1))
            elif token.startswith("Texecute"):
                Texec = re.match(r'Texecute[^:]*: *(\d+(.\d+)?)ms', token)
                Texec = float(Texec.group(1))
            else:
                if args.socket:
                    conn.send((token+"\n").encode())
                break
        t2 = time.time()
        return ((t1 - t0) * 1000, (t2 - t1) * 1000, Tcodegen, Tdataload, Tcodeopt, Tcodeoptnload, Texec)

    def stop(self):
        ProcessObj.stop(self)
        # self.t.join()

    def kill(self):
        ProcessObj.kill(self)
        # self.t.join()

    def wait_for(self, f):
        # for kw in kws:
        #     if not f(kw):
        #         assert(False)
        # else:
        #     break
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
        schema = os.path.abspath(os.path.join(os.path.dirname(__file__), r"../../src/SQLPlanner/src/main/resources/schema.json"))
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
                                           .prepare(num_of_cpus,
                                                    num_of_gpus,
                                                    explicit_memcpy,
                                                    parallel,
                                                    cpu_only,
                                                    hybrid)
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
    init_gspreadsheet();
    
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
                    is_warmup = re.sub('\s+', ' ', line.lower())                \
                                .startswith(".warmup ")
                    is_test = is_test or re.sub('\s+', ' ', line.lower())     \
                                           .startswith("create test ")
                    if (line.lower().startswith("select ") or is_test):
                        create_test = None
                        label_test  = None
                        if not line.lower().startswith("select"):
                            line = re.sub('\s+', ' ', line.lower())
                            m = re.match(".?create test ([a-zA-Z_0-9%]+) from(.*)", line)
                            if m is None:
                                print("error (malformed .create test)")
                                continue
                            create_test = m.group(1)
                            line = m.group(2).strip()
                        label_test = create_test
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
                        for rep in range(5+1):
                            t0 = time.time()
                            try:
                                plan = planner.get_plan_from_sql(sql_query)
                            except SQLParseError as e:
                                print("error (" + e.details + ")")
                                if args.failOnParseError:
                                    raise
                                else:
                                    continue
                            t1 = time.time()
                            if create_test is None or gentests_exec:
                                (wplan_ms, wexec_ms,
                                    Tcodegen, 
                                    Tdataload,
                                    Tcodeopt,
                                    Tcodeoptnload,
                                    Texec) = executor.execute_plan(plan)
                                if (timings):
                                    t2 = time.time()
                                    total_ms = (t2 - t0) * 1000
                                    plan_ms = (t1 - t0) * 1000
                                    label = ""
                                    conf  = [0, 0]
                                    if label_test is not None:
                                        ident = ""
                                        if '%' in label_test:
                                            if parallel:
                                                ident = ident + "par"
                                            else:
                                                ident = ident + "seq"
                                            if explicit_memcpy:
                                                ident = ident + "_cpy"
                                            else:
                                                ident = ident + "_uva"
                                            if hybrid:
                                                ident = ident + "_hyb" + "_c" + str(num_of_cpus) + "g" + str(num_of_gpus)
                                                conf  = [num_of_cpus, num_of_gpus]
                                            elif cpu_only:
                                                ident = ident + "_cpu" + str(num_of_cpus)
                                                conf  = [num_of_cpus, 0]
                                            else:
                                                ident = ident + "_gpu" + str(num_of_gpus)
                                                conf  = [0, num_of_gpus]
                                        label = label_test.replace('%', ident)
                                    if (not is_warmup) and rep > 0: # rep == 0 is used for warming up the storage manager
                                        sheets_append([total_ms, plan_ms, wplan_ms, wexec_ms, Tcodegen, Tdataload, Tcodeopt, Tcodeoptnload, Texec], label, sql_query, conf)
                                    if rep > 0:
                                        if timings_csv:
                                            print('Timing,' +
                                                  '%.2f,' % (total_ms) +
                                                  '%.2f,' % (plan_ms) +
                                                  '%.2f,' % (wplan_ms) +
                                                  '%.2f,' % (wexec_ms) +
                                                  '%.2f,' % (Tcodegen) +
                                                  '%.2f,' % (Tdataload) +
                                                  '%.2f,' % (Tcodeopt) +
                                                  '%.2f,' % (Tcodeoptnload) +
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
                                                  "ms, Data Load time: " + 
                                                  '%.2f' % (Tdataload) +
                                                  "ms, Code opt time: " + 
                                                  '%.2f' % (Tcodeopt) +
                                                  "ms, Code opt'n'load time: " + 
                                                  '%.2f' % (Tcodeoptnload) +
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
                                    if hybrid:
                                        ident = ident + "_hyb" + "_c" + str(num_of_cpus) + "g" + str(num_of_gpus)
                                    elif cpu_only:
                                        ident = ident + "_cpu" + str(num_of_cpus)
                                    else:
                                        ident = ident + "_gpu" + str(num_of_gpus)
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
                            # timings = True
                            timings_csv = True
                        elif (line.lower() == ".timings csv off"):
                            # timings = True
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
                    elif (line.lower().startswith(".hybrid ")):
                        if (line.lower() == ".hybrid query"):
                            pass
                        elif (line.lower() == ".hybrid on"):
                            hybrid = True
                        elif (line.lower() == ".hybrid off"):
                            hybrid = False
                        else:
                            print("error (unknown option for hybrid execution)")
                        print("Hybrid execution: " + str(hybrid))
                    elif (line.lower().startswith(".cpu dop ")):
                        if (line.lower() == ".cpu dop query"):
                            pass
                        else:
                            num_of_cpus = int(line.lower()[len(".cpu dop "):])
                        print("CPU DOP " + str(num_of_cpus))
                    elif (line.lower().startswith(".gpu dop ")):
                        if (line.lower() == ".gpu dop query"):
                            pass
                        else:
                            num_of_gpus = int(line.lower()[len(".gpu dop "):])
                        print("GPU DOP " + str(num_of_gpus))
                    elif (line.lower().startswith('.')):
                        executor.execute_command(line[1:])
    except KeyboardInterrupt:
        pass
    finally:
        sheets_flush(q_id - 1)
        if conn:
            conn.close()
