/**
  *****************************************************************************
  * @file    errorHandler.h
  * @brief   Fatal error handler
  *****************************************************************************
  */

#ifndef HIL_ERROR_HANDLER_H
#define HIL_ERROR_HANDLER_H

// _handleError is what common/Inc/generalErrorHandler.h's handleError() macro
// expands to, and common/Src/debug.c calls it. common/Src/generalErrorHandler.c
// is not linked here because it needs a generated <board>_dtc.h, so this board
// provides its own.
void _handleError(char *file, int line);

#endif /* HIL_ERROR_HANDLER_H */
