/*
    SLANG support functions

*/


#define _CRT_RAND_S // NOLINT (bugprone-reserved-identifier)

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"
#include "vmAllocator.h"

#include <math.h>

static int vmSgn ( double x )

{
   return ( x < 0 ? -1 : ( x > 0 ? 1 : 0 ) );
}

static double vmAbs ( double x )

{
   return ( x < 0 ? -x : x );
}

static double vmRound ( double x )

{
   double w;
   double r;

   r = modf ( x, &w );

   if ( r >= 0.5 )
   {
      return ( w + 1 );
   } else
   if ( r <= -0.5 )
   {
      return ( w - 1 );
   } else
   {
      return ( w      );
   }
}

static double vmFract ( double x )
{
	double	w;

	return ( modf ( x, &w ) );
}

static int64_t vmInt ( double x )

{
   return (int64_t)x;
}

static double DegreesToRadians ( char const *angsgn, double angd, double angm, double angs) 
{
	double s, angle;

	if ( !fglstrccmp ( angsgn, "N" ) || !fglstrccmp ( angsgn, "E" ) )
	{
		s = 1;
	} else
	{
		s = -1;
	}

	angle =s * ( angd + angm / 60 + angs / 3600) / 57.29578;

	return angle;
}


// calculate the distance between two coordinates
static double LongLatDistance ( double lat1, double long1, double lat2, double long2) 
{
	const double a = 6378.14;
	const double f = 0.0033528131778969;
	double F, G, lambda, S, C, omega, R, D, H1, H2, s;

	F = (lat1+lat2)/2;
	G = (lat1-lat2)/2;
	lambda = (long1-long2)/2;

	S = pow(sin(G)*cos(lambda),2)+pow(cos(F)*sin(lambda),2);
	C = pow(cos(G)*cos(lambda),2)+pow(sin(F)*sin(lambda),2);
	omega = atan(sqrt(S/C));
	R = sqrt(S*C)/omega;
	D = 2*omega*a;
	H1 =(3*R-1)/2/C;
	H2 =(3*R+1)/2/S;
	s = D*(1+f*H1*pow(sin(F)*cos(G),2)-f*H2*pow(cos(F)*sin(G),2));

	return s;
}

/* we only need these because sin, atan, etc. are all overloaded... so our metaprogram has no way of determining which one we want :( */
static double vmSin( double value )
{
	return sin( value );
}

static double vmCos( double value )
{
	return cos( value );
}

static double vmTan( double value )
{
	return tan( value );
}

static double vmATan( double value )
{
	return atan( value );
}

static double vmACos( double value )
{
	return acos( value );
}

static double vmASin( double value )
{
	return asin( value );
}

static double vmLog( double value )
{
	return log( value );
}

static double vmLog10( double value )
{
	return log10( value );
}

static double vmExp( double value )
{
	return exp( value );
}

static double vmSqrt( double value )
{
	return sqrt( value );
}

static uint32_t vmRand_s ( void )
{
	unsigned int value;
	rand_s ( &value );
	return value;
}
void builtinMathInit ( class vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		FUNC ( "ABS", vmAbs ) CONST PURE;
		FUNC ( "SIN", vmSin ) CONST PURE;
		FUNC ( "COS", vmCos ) CONST PURE;
		FUNC ( "TAN", vmTan ) CONST PURE;
		FUNC ( "ATAN", vmATan ) CONST PURE;
		FUNC ( "ACOS", vmACos ) CONST PURE;
		FUNC ( "ASIN", vmASin ) CONST PURE;
		FUNC ( "LOG", vmLog ) CONST PURE;
		FUNC ( "LOG10", vmLog10 ) CONST PURE;
		FUNC ( "EXP", vmExp ) CONST PURE;
		FUNC ( "SQRT", vmSqrt ) CONST PURE;
		FUNC ( "ROUND", vmRound ) CONST PURE;
		FUNC ( "INT", vmInt ) CONST PURE;
		FUNC ( "FRACT", vmFract ) CONST PURE;
		FUNC ( "SGN", vmSgn ) CONST PURE;
		FUNC ( "DegreesToRadians", DegreesToRadians ) CONST PURE;
		FUNC ( "LongLatDistance", LongLatDistance ) CONST PURE;

		FUNC ( "rand", rand );
		FUNC ( "rand_s", vmRand_s );
		FUNC ( "srand", srand );
	END;
}
