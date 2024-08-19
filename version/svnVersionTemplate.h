/*
	SVN Versioning template
*/

#pragma once

#define STRINGIFY(x) #x

#define SVN_REVISION			$WCREV$
#define SVN_LOCAL_MODIFICATIONS	$WCMODS?1:0$
