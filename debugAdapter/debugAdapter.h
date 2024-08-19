// engine debug adapter

#pragma once
#include "bcVM/vmDebug.h"

extern	void				 debugServerInit ( class vmInstance *instance );
extern  class taskControl	*debugAdapterInit ( uint16_t port );

