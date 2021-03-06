#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../config/config.h"
#include "../wrapping/wrapping.h"
#include "worker.h"

#ifndef SLEEP_FLAG
#define SLEEP_FLAG 0
#endif

/**
 * Inits worker
 *
 * args:
 *    ungisned long long *start: contains buffer start
 *    unisgned long long *end  : contains buffer end
 *    int *stopFlag            : flags that indicates if the worker must stop
 *
 * returns:
 *    positive (file descriptor) in case of success, otherwise negative
 */
int initWork(unsigned long long *start, unsigned long long *end, int *stopFlag);

/**
 * Read directives from pipe
 *
 * args:
 *    char *path       : path of the file
 *    char *bufferStart: contains buffer start in chars type
 *    char *bufferEnd  : contains buffer end in chars type
 *    int *stopFlag    : flags that indicates if the worker must stop
 *
 * returns:
 *    0 in case of success, otherwise negative
 */
void readDirectives(char *path, char *bufferStart, char *bufferEnd,
                    int *stopFlag);

/**
 * Checks if char passed is a digit
 *
 * args:
 *    char c: the char to check
 *
 * returns:
 *    0 in case of success, otherwise -1
 */
int isDigit(char c);

/**
 * Gets the available memory in the system
 *
 * returns:
 *    0 if no memory is aviable or there is an error, otherwise the correct
 * amount
 */
unsigned long long getAvailableMemory();
/**
 * Runs work
 *
 * args:
 *    const int fd                  : file descriptor
 *    const unsigned long long start: start position
 *    const unsigned long long end  : end position
 *
 * returns:
 *    0 in case of success, otherwise negative
 */
int executeWork(const int fd, const unsigned long long start,
                const unsigned long long end);

/**
 * Sends an acknowledgment to the father
 *
 * returns:
 *    0 in case of success, otherwise negative
 */
int sendAcknowledgment();

/**
 * Communicates to manager that something went wrong.
 * If this function fails worker terminates its execution
 * returning -1
 *
 * arga:
 *    const unsigned long long written: written
 *    const unsigned long long end    : file buffer end
 *
 * returns:
 *    0 in case of success, otherwise -1
 */
int errorHandler(const unsigned long long written,
                 const unsigned long long end);

int main(int argc, char *argv[]) {
  unsigned long long start;
  unsigned long long end;
  int rc_work = SUCCESS;
  int working = WORKING;
  int stopFlag = 0;
  int rc_ack = SUCCESS;

  int fd = initWork(&start, &end, &stopFlag);
  if (stopFlag == 1) {
    rc_ack = sendAcknowledgment();
    stopFlag = 0;
    if (rc_ack < SUCCESS)
      working = NOT_WORKING;
    else
      fd = initWork(&start, &end, &stopFlag);
  }

  if (fd < SUCCESS) {
    if (fd == CAST_FAILURE) {
      printError("I didn't read a number as start and/or end");
      kill(getpid(), SIGKILL);
    }
    int rc_er = errorHandler(0, end);
    if (rc_er < SUCCESS)
      working = NOT_WORKING;
  }

  while (working == WORKING) {
    rc_work = executeWork(fd, start, end);
    if (rc_work != SUCCESS) {
      int rc_er = errorHandler(rc_work, end);
      if (rc_er < SUCCESS)
        working = NOT_WORKING;
    }
    fd = initWork(&start, &end, &stopFlag);
    if (stopFlag == 1) {
      rc_ack = sendAcknowledgment();
      stopFlag = 0;
      if (rc_ack < SUCCESS)
        working = NOT_WORKING;
      else
        fd = initWork(&start, &end, &stopFlag);
    }

    if (fd < SUCCESS) {
      if (fd == CAST_FAILURE) {
        printError("I didn't read a number as start and/or end");
        kill(getpid(), SIGKILL);
      }
      int rc_er = errorHandler(0, end);
      if (rc_er < SUCCESS)
        working = NOT_WORKING;
    }
  }

  return working;
}

int initWork(unsigned long long *start, unsigned long long *end,
             int *stopFlag) {
  int rc_t = SUCCESS;
  char path[PATH_MAX];
  char bufferStart[PATH_MAX];
  char bufferEnd[PATH_MAX];

  readDirectives(path, bufferStart, bufferEnd, stopFlag);

  if (*stopFlag == 0) {
    int rc_sc = sscanf(bufferStart, "%llu", start);
    if ((rc_sc == SUCCESS || bufferStart[0] == '\0' ||
         (*start == 9 && strcmp(bufferStart, "9") != 0))) {
      rc_t = CAST_FAILURE;
    }
    int rc_sc2 = sscanf(bufferEnd, "%llu", end);
    if ((rc_sc2 == SUCCESS || bufferEnd[0] == '\0' ||
         (*end == 9 && strcmp(bufferEnd, "9") != 0))) {
      rc_t = CAST_FAILURE;
    }

    int fd = openFile(path, O_RDONLY);

    if (fd == FAILURE && rc_t == SUCCESS)
      rc_t = READ_DIRECTIVES_FAILURE;
    else if (rc_t == SUCCESS)
      rc_t = fd;
    else
      rc_t = CAST_FAILURE;
  }

  return rc_t;
}

void readDirectives(char *path, char *bufferStart, char *bufferEnd,
                    int *stopFlag) {
  char readBuffer[2] = "a";
  *stopFlag = 0;

  int counter = 0;
  do {
    int rc = readChar(READ_CHANNEL, readBuffer);
    path[counter++] = readBuffer[0];
  } while (readBuffer[0] != '\0' && readBuffer[0] != '\n');
  path[counter] = '\0';
  if (path[strlen(path) - 1] == '\n') {
    path[strlen(path) - 1] = '\0';
  }

  if (strcmp(path, "stop") == 0)
    *stopFlag = 1;

  counter = 0;
  do {
    int rc = readChar(READ_CHANNEL, readBuffer);
    bufferStart[counter++] = readBuffer[0];
  } while (readBuffer[0] != '\0' && readBuffer[0] != '\n');
  bufferStart[counter] = '\0';
  if (bufferStart[strlen(bufferStart) - 1] == '\n') {
    bufferStart[strlen(bufferStart) - 1] = '\0';
  }

  if (strcmp(bufferStart, "stop") == 0 && *stopFlag == 1)
    *stopFlag = 1;
  else
    *stopFlag = 0;

  counter = 0;
  do {
    int rc = readChar(READ_CHANNEL, readBuffer);
    bufferEnd[counter++] = readBuffer[0];
  } while (readBuffer[0] != '\0' && readBuffer[0] != '\n');
  bufferEnd[counter] = '\0';
  if (bufferEnd[strlen(bufferEnd) - 1] == '\n') {
    bufferEnd[strlen(bufferEnd) - 1] = '\0';
  }

  if (strcmp(bufferEnd, "stop") == 0 && *stopFlag == 1)
    *stopFlag = 1;
  else
    *stopFlag = 0;

  if (path[0] == '\0' || bufferStart[0] == '\0' || bufferEnd[0] == '\0') {
    char *msgErr = (char *)malloc(sizeof(char) * 300);
    int rc_ca = checkAllocationError(msgErr);
    if (rc_ca < 0) {
      printError("I can't allocate memory");
    } else {
      sprintf(msgErr, "inside worker with pid: %d", getpid());
      printError(msgErr);
    }
    free(msgErr);
  }
}

int isDigit(char c) {
  int rc_t = FAILURE;
  if (c >= 48 && c <= 57)
    rc_t = SUCCESS;

  return rc_t;
}

unsigned long long getAvailableMemory() {
  int fd = open("/proc/meminfo", O_RDONLY);
  unsigned long long rc_t = 0;
  if (fd > 0) {
    char charRead = 'a';
    int lineToIgnore = 2;
    while (lineToIgnore != 0) {
      charRead = 'a';
      while (charRead != '\n') {
        readChar(fd, &charRead);
      }
      lineToIgnore--;
    }
    char freeMem[PATH_MAX];
    int index = 0;
    charRead = 'a';
    while (charRead != '\n') {
      int howMany = readChar(fd, &charRead);
      if (howMany > 0) {
        freeMem[index++] = charRead;
      }
    }
    freeMem[index - 1] = '\0';

    char available[PATH_MAX];
    index = 0;
    int flag = 0;
    int i = 0;
    for (i = 0; i < strlen(freeMem); i++) {
      if (freeMem[i] == ':') {
        flag = 1;
      } else if (flag == 1) {
        if (isDigit(freeMem[i]) == SUCCESS) {
          available[index++] = freeMem[i];
        }
      }
    }
    available[index] = '\0';

    unsigned long long casted = 0;

    sscanf(available, "%llu", &casted);
    rc_t = casted * 1024;
    closeDescriptor(fd);
  }

  return rc_t;
}

int executeWork(const int fd, const unsigned long long start,
                const unsigned long long end) {
  int rc_t = 0;
  long long rc_se = moveCursorFile(fd, start, SEEK_SET);
  if (rc_se == FAILURE)
    rc_t = CURSOR_FAILURE;

  unsigned long long bytesRead;
  unsigned long long workAmount = end - start + 1;
  unsigned long long availableMem = 0;
  while (availableMem == 0) {
    availableMem = getAvailableMemory();
    if (SLEEP_FLAG == SUCCESS)
      usleep(100);
  }

  unsigned long long step = workAmount + 2;

  if (workAmount + 2 >= availableMem * 50 / 100)
    step = availableMem * 50 / 100;

  if (step > 1000000000)
    step = 1000000000;

  char *charsRead = malloc(step * sizeof(char));
  int rc_al = checkAllocationError(charsRead);
  int lectures = 1;
  unsigned long long written = 0;

  if (workAmount + 2 != step) {
    lectures = (int)(workAmount / step);
    if ((workAmount % step) > 0)
      lectures++;
    workAmount = step;
  }

  if (rc_al == SUCCESS) {
    while (lectures != 0) {
      bytesRead = readDescriptor(fd, charsRead, workAmount);
      int i;
      if (bytesRead > 0)
        for (i = 0; i < workAmount; i++)
          if ((charsRead[i] < 32 || charsRead[i] > 127) && charsRead[i] != '\n')
            charsRead[i] = -15;

      if (lectures == 1)
        charsRead[bytesRead] = '\0';
      if (bytesRead > 0) {
        int rc_wr = write(WRITE_CHANNEL, charsRead, bytesRead);
        if (rc_wr == -1)
          rc_t = WRITE_FAILURE;
        else
          written += rc_wr;
      } else {
        rc_t = READ_FAILURE;
      }
      lectures--;
    }

    if (written != end - start + 1)
      rc_t = FAILURE;
    if (rc_t == SUCCESS) {
      int rc_wr = writeDescriptor(WRITE_CHANNEL, "done");
      if (rc_wr == FAILURE)
        rc_t = WRITE_FAILURE;
    } else {
      rc_t = written;
    }
    free(charsRead);
  } else {
    rc_t = FAILURE;
  }
  closeDescriptor(fd);

  return rc_t;
}

int sendAcknowledgment() {
  int rc_t = SUCCESS;
  // TODO think to remove this istruction
  /* rc_t = writeDescriptor(WRITE_CHANNEL, "ackn"); */
  return rc_t;
}

int errorHandler(const unsigned long long written,
                 const unsigned long long end) {
  int rc_t = 0;

  unsigned long long workAmount = end - written + 1;

  while (workAmount != 0 && rc_t == 0) {
    int rc_wr = writeDescriptor(WRITE_CHANNEL, "a");
    if (rc_wr == -1)
      rc_t = WRITE_FAILURE;
    else
      workAmount--;
  }

  if (workAmount == 0 && rc_t == SUCCESS) {
    int rc_wr = writeDescriptor(WRITE_CHANNEL, "erro");
    if (rc_wr == -1)
      rc_t = WRITE_FAILURE;
  }

  return rc_t;
}
