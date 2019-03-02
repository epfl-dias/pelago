import jpype
import jaydebeapi
import time
import os
import types
import threading

import warnings

import pandas as pd

PROJECT_DIR = os.getenv("PROJECT_DIR", os.path.realpath(os.path.dirname(os.path.dirname(os.path.dirname(__file__)))))
INSTALL_DIR = os.getenv("INSTALL_DIR", os.path.realpath(os.path.join(PROJECT_DIR, "opt")))

path_to_avatica = os.path.join(PROJECT_DIR, "opt", "lib", "avatica-1.13.0.jar")

if "classpath" in locals() or "classpath" in globals():
    classpath = classpath + ":" + str(path_to_avatica)
else:
    classpath = str(path_to_avatica)

if not jpype.isJVMStarted():
    assert(path_to_avatica in classpath)
    jpype.startJVM(jpype.getDefaultJVMPath(), "-Djava.class.path=" + classpath)
else:
    warnings.warn("JVM already started. Keeping old JVM configuration.")


def createConnection(host='http://localhost:8081'):
    proteus = jaydebeapi.connect(jclassname="org.apache.calcite.avatica.remote.Driver",
                              url="jdbc:avatica:remote:url=" + host + ";serialization=PROTOBUF",
                              driver_args={})
    
    def getExecTime(self, default_exec_time):
        # FIXME: currently we do not include EnumerableOrderBy, so we have to do the selection in python
        self.execute("select * from SessionTimings")
        return self.fetchall()[-1]
        
    def getTimingHeader(self):
        try:
            self.execute("select * from SessionTimings")
            cols = [x[0] for x in self.description]
        except:
            cols = ["Total Time"]
        return cols + ["Query", "# rows", "label"]
    
    # header = getTimingHeader()
    
    def executeQuery(self, sql, silent=True, no_fetch=False, label=""):
        # execute sql query
        start_t = time.time()
        self.execute(sql)
        end_t = time.time()
        # fetch all the results and put them in a dataframe
        results = []
        if not no_fetch:
            results = self.fetchall()
        n_rows = self.rowcount
        p = pd.DataFrame(results, columns = [x[0] for x in self.description])
        # fetch timings
        exec_time = self.getExecTime((end_t - start_t) * 1000)
        timing = pd.DataFrame([exec_time + (sql, n_rows, label)], columns = self.getHeader())
        if not silent:
            display(timing)
            print((time.time() - start_t)*1000)
        return (p, timing)
    
    # def createTimingDF():
        # return pd.DataFrame([], columns = header);
    def getHeader(self):
        if self.header is None:
            self.header = getTimingHeader(self)
        return self.header
    
    def alterSession(self, conf):
        if "hwmode" in conf:
            try:
                self.execute("ALTER SESSION SET hwmode = " + str(conf["hwmode"]));
            except jaydebeapi.DatabaseError as e:
                pass
        if "cpudop" in conf:
            try:
                self.execute("ALTER SESSION SET cpudop = " + str(conf["cpudop"]));
            except jaydebeapi.DatabaseError as e:
                pass
        if "gpudop" in conf:
            try:
                self.execute("ALTER SESSION SET gpudop = " + str(conf["gpudop"]));
            except jaydebeapi.DatabaseError as e:
                pass
    
    def getCursor(self):
        curs = self.__cursor()
        curs.header             = None
        curs.getHeader          = types.MethodType(getHeader   , curs)
        curs.executeTimedQuery  = types.MethodType(executeQuery, curs)
        curs.getExecTime        = types.MethodType(getExecTime , curs)
        curs.alterSession       = types.MethodType(alterSession, curs)
        return curs
    
    # proteus.createTimingDF = types.MethodType(createTimingDF, proteus)
    proteus.__cursor    = proteus.cursor
    proteus.cursor      = types.MethodType(getCursor, proteus)
    proteus.name        = "Proteus"

    return proteus