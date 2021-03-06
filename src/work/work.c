#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config/config.h"
#include "../wrapping/wrapping.h"
#include "work.h"

Work newWork(Table table, const unsigned long long bufferStart,
             const unsigned long long bufferEnd) {
  Work work = malloc(sizeof(struct WorkStr));
  int rc_al = checkAllocationError(work);
  if (rc_al != FAILURE) {
    work->tablePointer = table;

    work->bufferStart = bufferStart;
    work->bufferEnd = bufferEnd;
    work->executionTry = 0;
  } else
    work = NULL;

  return work;
}

void destroyWork(void *data) {
  Work work = (Work)data;
  free(work);
}

int compareWork(void *w1, void *w2) {
  int rc_t = -1;

  Work work1 = (Work)w1;
  Work work2 = (Work)w2;

  if (work1->bufferEnd == work2->bufferEnd &&
      work1->bufferStart == work2->bufferStart &&
      strcmp(work1->tablePointer->name, work2->tablePointer->name) == 0)
    rc_t = 0;

  return rc_t;
}
