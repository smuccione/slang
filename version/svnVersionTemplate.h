/*
	SVN Versioning template
*/

#pragma once

#define STRINGIFY(x) #x

#define SVN_REVISION			$WCREV$
#define SVN_BUILD_TIME			STRINGIFY($WCNOW$)
#define SVN_LOCAL_MODIFICATIONS	$WCMODS?1:0$
