// mode.h
#ifndef MODE_H
#define MODE_H

typedef enum
{
    MODE_ROBOT,
    MODE_ENVIRONMENT,
	MODE_PAINT
} Mode;

extern Mode currentMode;

#endif
