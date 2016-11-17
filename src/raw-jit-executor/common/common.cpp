/*
	RAW -- High-performance querying over raw, never-seen-before data.

							Copyright (c) 2014
		Data Intensive Applications and Systems Labaratory (DIAS)
				École Polytechnique Fédérale de Lausanne

							All Rights Reserved.

	Permission to use, copy, modify and distribute this software and
	its documentation is hereby granted, provided that both the
	copyright notice and this permission notice appear in all copies of
	the software, derivative works or modified versions, and any
	portions thereof, and that both notices appear in supporting
	documentation.

	This code is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
	DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
	RESULTING FROM THE USE OF THIS SOFTWARE.
*/

#include "common/common.hpp"

double
diff(struct timespec st, struct timespec end)
{
	struct timespec tmp;

	if ((end.tv_nsec-st.tv_nsec)<0) {
		tmp.tv_sec = end.tv_sec - st.tv_sec - 1;
		tmp.tv_nsec = 1e9 + end.tv_nsec - st.tv_nsec;
	} else {
		tmp.tv_sec = end.tv_sec - st.tv_sec;
		tmp.tv_nsec = end.tv_nsec - st.tv_nsec;
	}

	return tmp.tv_sec + tmp.tv_nsec * 1e-9;
}

void
fatal(const char *err)
{
    perror(err);
    exit(1);
}

void
exception(const char *err)
{
    printf("Exception: %s\n", err);
    exit(1);
}

bool verifyTestResult(const char *testsPath, const char *testLabel)	{
	/* Compare with template answer */
	/* correct */
	struct stat statbuf;
	string correctResult = string(testsPath) + testLabel;
	stat(correctResult.c_str(), &statbuf);
	size_t fsize1 = statbuf.st_size;
	int fd1 = open(correctResult.c_str(), O_RDONLY);
	if (fd1 == -1) {
		throw runtime_error(string("csv.open: ")+correctResult);
	}
	char *correctBuf = (char*) mmap(NULL, fsize1, PROT_READ | PROT_WRITE,
			MAP_PRIVATE, fd1, 0);

	/* current */
	stat(testLabel, &statbuf);
	size_t fsize2 = statbuf.st_size;
	int fd2 = open(testLabel, O_RDONLY);
	if (fd2 == -1) {
		throw runtime_error(string("csv.open: ")+testLabel);
	}
	char *currResultBuf = (char*) mmap(NULL, fsize2, PROT_READ | PROT_WRITE,
			MAP_PRIVATE, fd2, 0);
	cout << correctBuf << endl;
	cout << currResultBuf << endl;
	bool areEqual = (strcmp(correctBuf, currResultBuf) == 0) ? true : false;

	close(fd1);
	munmap(correctBuf, fsize1);
	close(fd2);
	munmap(currResultBuf, fsize2);
	if (remove(testLabel) != 0) {
		throw runtime_error(string("Error deleting file"));
	}

	return areEqual;
}


