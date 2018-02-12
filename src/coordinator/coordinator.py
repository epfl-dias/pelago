from subprocess import *
from sys import *
from translator_scripts.json_to_proteus_plan import *
from os.path import abspath

import socket
import argparse

# using flags
#SOCKET = False	# Set to False if not using sockets
#EXECUTOR_STDOUT = PIPE	# Set to None to directly redirect stdout for executor process to stdout of terminal

HOST = 'localhost'
RECV_SIZE = 1024

sock = None
conn = None
addr = None

planner = None
executor = None

def echo_exec(break_string="ready", connection=None):
    if(args.executor_pipe==PIPE):
        while not executor.poll():
            exec_line = executor.stdout.readline()
            print(exec_line.strip())
            if(exec_line.startswith(break_string)):
            	if(connection!=None):
            	    connection.send(exec_line.encode())
            	break

def init_socket(host, port):
    global sock, conn, addr
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((host, port))
    sock.listen(1)
    conn, addr = sock.accept()
    
    print("Socket connection initialized with: ", addr)


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
    if conn:
        conn.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--socket', action="store_true", help="Set to use socket for communication")
    parser.add_argument('--executor_pipe', action="store_const", const=PIPE, help="Set to redirect stdout of executor to PIPE")
    parser.add_argument('--port', action="store", help="Set localhost port to use", default=50001, type=int)

    args = parser.parse_args()
    
    executor = Popen(["./rawmain-server"], stdin=PIPE, stdout=args.executor_pipe, cwd=r"""../../opt/raw""")

    # TODO EVENTUALLY SEND A SIGNAL TO SOCKET THAT EVERYTHING IS OK IF OUTPUT IS READY
    echo_exec("ready")

    planner = Popen(["java", "-jar", "target/scala-2.12/SQLPlanner-assembly-0.1.jar"], stdin=PIPE, stdout=PIPE, cwd=r"""../SQLPlanner""")
    
    if(args.socket):
        init_socket(HOST, args.port)

    try:
        while True:
            if(args.socket):
        	    line = conn.recv(RECV_SIZE).decode().strip()
            else:
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
                                

                                if(args.socket):
                                    echo_exec("result in file", conn)
                                else:
                                    echo_exec("result in file")

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
