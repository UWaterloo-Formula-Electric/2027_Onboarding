/**
  *****************************************************************************
  * @file    boardTypes.h
  * @brief   Board architecture selection, standalone replacement for
  *          common/Inc/boardTypes.h
  * @details Inc/bsp.h switches on IS_BOARD_F7 and #errors out if no board
  * type matches. Upstream this comes from common/, where tail.mk derives it
  * from BOARD_ARCHITECTURE in board.mk. The standalone Makefile passes
  * -DBOARD_TYPE_F7 instead.
  *****************************************************************************
  */

#ifndef BOARD_TYPES_H
#define BOARD_TYPES_H

#ifdef BOARD_TYPE_F7
#define IS_BOARD_F7 1
#else
#define IS_BOARD_F7 0
#endif

#endif /* BOARD_TYPES_H */
