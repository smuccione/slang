/*

	print support

*/

#ifndef __SUPPORT_H__
#define __SUPPORT_H__

typedef struct PRINT_URL {
	struct	PRINT_URL	*next;
	char				*url;
	unsigned int		 index;
} PRINT_URL;

typedef struct PRINT_PARAM {
	PRINT_URL			*top;
	HANDLE				 doneSemaphore;
	unsigned long		 waitTime;
} PRINT_PARAM;

#endif /* __SUPPORT_H__ */