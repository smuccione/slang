/*
aArray() class

NOTE: until we can find an effective way to know a-prioi the total size of the memory allocated by a map and each node we resort to using the avl tree code

aArrayNew is the problem... this is highly optimized for speed as we use this a LOT for all the dynamic arrays in the engine.

*/

#include "Utility/settings.h"

#include "Stdafx.h"
#include "gdiplus.h"
#include "comdef.h"

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"

using namespace Gdiplus;

enum class CFgl_image_type {
	CFGL_IMAGE_BMP = 1,
	CFGL_IMAGE_JPG = 2,
	CFGL_IMAGE_GIF = 3,
	CFGL_IMAGE_TIF = 4,
};

class CFgl_image
{
	public:
	Bitmap				*m_bmp;

	IStream				*m_stream;

	CFgl_image_type		 m_imageType;

	CFgl_image ( );
	~CFgl_image ( );

	CFgl_image ( CFgl_image const &right )
	{
		throw errorNum::scUNSUPPORTED;
	}

	CFgl_image ( CFgl_image &&right ) noexcept
	{
		std::swap ( m_bmp, right.m_bmp );
		std::swap ( m_stream, right.m_stream );
		std::swap ( m_imageType, right.m_imageType );
	}
};

static void checkObject ( VAR &var, char const *className )
{
	if ( var.type != slangType::eOBJECT_ROOT ) throw errorNum::scINVALID_PARAMETER;
	if ( !var.dat.ref.v ) throw errorNum::scINVALID_PARAMETER;
	if ( strccmp ( var.dat.ref.v->dat.obj.classDef->name, className ) ) throw errorNum::scINVALID_PARAMETER;
}

CFgl_image::CFgl_image()
{
	m_bmp		= NULL;
}

CFgl_image::~CFgl_image()
{
	if ( m_bmp )
	{
		delete m_bmp;
	}
}

static void cImageRelease ( vmInstance *instance, VAR *var )

{
	CFgl_image	*cImage = (CFgl_image *) classGetCargo ( var );

	cImage->~CFgl_image ( );
}

static int GetEncoderClsid (const WCHAR* format, CLSID* pClsid)
{
   UINT  num = 0;  // number of image encoders
   UINT  size = 0; // size of the image encoder array in bytes

   ImageCodecInfo* pImageCodecInfo = NULL;

   GetImageEncodersSize(&num, &size);
   if(size == 0)
      return 0;  // Failure

   pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
   if(pImageCodecInfo == NULL)
      return 0;  // Failure

   GetImageEncoders(num, size, pImageCodecInfo);

   for(UINT j = 0; j < num; ++j)
   {
      if( _wcsicmp(pImageCodecInfo[j].MimeType, format) == 0 )
      {
         *pClsid = pImageCodecInfo[j].Clsid;
         free(pImageCodecInfo);
         return 1;  // Success
      }
   }

   free(pImageCodecInfo);
   return 0;  // Failure
}

static int cImageNew ( vmInstance *instance, VAR_OBJ *obj, nParamType nParams )
{
	IStream			 *iStream;
	VAR				 *var;
	CLSID			  clsid;
	char const		 *imageType = "image/jpeg";

	auto cImage = obj->makeObj<CFgl_image> ( instance );

	switch ( (uint32_t)nParams )
	{
		case 2:
			var = nParams[1];
			switch ( var->type )
			{
				case slangType::eSTRING:
					imageType = var->dat.str.c;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			// intentional fall through
		case 1:
			var = nParams[0];
			switch ( var->type )
			{
				case slangType::eSTRING:
					// read it the image file
					cImage->m_bmp = new Bitmap ( (PWCHAR)_bstr_t(var->dat.str.c) );

					// ok... have to convert over internally to jpeg's so we can be sure to use the Graphics class
					CreateStreamOnHGlobal ( 0, 1, &iStream );

					GetEncoderClsid ( _bstr_t ( imageType ), &clsid );

					// write the bitmap out as a jpeg to the stream
					cImage->m_bmp->Save ( iStream, &clsid, 0 ); 

					Bitmap *oldBitmap;

					oldBitmap = cImage->m_bmp;

					// create a new bitmap from the jpg stream
					cImage->m_bmp = new Bitmap ( iStream );

					iStream->Release();
					// delete the old bitmap
					delete oldBitmap;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER ;
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	return ( 1 );
}

//**********************************************************************************************************************
//**********************************************************************************************************************
//*****************************************                                 ********************************************
//*****************************************          Access Methods         ********************************************
//*****************************************                                 ********************************************
//**********************************************************************************************************************
//**********************************************************************************************************************

static DWORD cImageGetSize ( VAR *obj, char const *type )
{
	CFgl_image	*cImage = (CFgl_image *) classGetCargo ( obj );
	CLSID			 pClsid;
	IStream			*iStream;
	STATSTG			 stg;


	if ( !GetEncoderClsid ( _bstr_t(type), &pClsid ) )
	{
		throw errorNum::scINVALID_PARAMETER ;
	}

	CreateStreamOnHGlobal ( 0, 1, &iStream );

	cImage->m_bmp->Save ( iStream, &pClsid, 0 ); 

	iStream->Stat ( &stg, STATFLAG_NONAME  );

	iStream->Release();

	return ( stg.cbSize.LowPart );
}

static int cImageGetHeight ( VAR *obj )
{
	CFgl_image	*cImage = (CFgl_image *) classGetCargo ( obj );
	return ( static_cast<int>(cImage->m_bmp->GetHeight() ));
}

static int cImageGetWidth ( VAR *obj )
{
	CFgl_image	*cImage = (CFgl_image *) classGetCargo ( obj );
	return (static_cast<int>(cImage->m_bmp->GetWidth()) );
}

static void cImageResize ( VAR *obj, int newX, int newY, int mode )
{
	CFgl_image	*cImage = (CFgl_image *) classGetCargo ( obj );

	Bitmap		*newBmp = new Bitmap ( newX, newY );
	Graphics	 g ( newBmp );

	g.DrawImage ( cImage->m_bmp, 0, 0, newX, newY );

	delete cImage->m_bmp;

	cImage->m_bmp = newBmp;
}

static void cImageRotate( VAR *obj, double angle, int newX, int newY )
{
	CFgl_image	*cImage = (CFgl_image *) classGetCargo ( obj );

	Bitmap		*newBmp = new Bitmap ( newX, newY );
	Graphics	 g ( newBmp );

	g.RotateTransform ( (float)angle );
	g.DrawImage ( cImage->m_bmp, 0, 0, newX, newY );

	delete cImage->m_bmp;

	cImage->m_bmp = newBmp;
}

static void cImageFlip ( VAR *obj, int64_t flipTypeP )
{
	RotateFlipType flipType = (RotateFlipType) flipTypeP;
	CFgl_image	*cImage = (CFgl_image *) classGetCargo ( obj );

	IStream			*iStream;
	CLSID			 pClsid;

	GetEncoderClsid ( _bstr_t("IMAGE/BMP"), &pClsid );
	CreateStreamOnHGlobal ( 0, 1, &iStream );

	cImage->m_bmp->Save ( iStream, &pClsid, 0 ); 

	Image image( iStream );

	iStream->Release();

	image.RotateFlip ( flipType );

	CreateStreamOnHGlobal ( 0, 1, &iStream );

	GetEncoderClsid ( _bstr_t("IMAGE/BMP"), &pClsid );

	image.Save ( iStream, &pClsid, 0 ); 

	delete cImage->m_bmp;

	cImage->m_bmp = new Bitmap ( iStream, 0 );

	iStream->Release();
}

static void cImageResizeAspect ( VAR *obj, int newX, int newY, int mode )
{
	CFgl_image	*cImage = (CFgl_image *) classGetCargo ( obj );
	double			 r;
	double			 newAX, newAY;

	if ( (newX < 0) || (newY < 0) )
	{
		throw errorNum::scINVALID_PARAMETER ;
	}

	r = (double)cImage->m_bmp->GetHeight() / (double)cImage->m_bmp->GetWidth();			// origional rise run

	if ( (double)newY > (double)newX * r  )
	{
		// newY is bigger then true aspect ratio would make it
		// we must compute new Y by using the existing slope

		newAX	= newX;
		newAY	= newX * r;
	} else
	{
		// newX is bigger then the newY would allow

		newAX	= newY / r;
		newAY	= newY;
	}

	cImageResize ( obj, static_cast<int>(newAX), static_cast<int>(newAY), 0 );
}

static VAR cImageClone ( vmInstance *instance, VAR *obj, int32_t x1, int32_t y1, int32_t x2, int32_t y2 )
{
	CFgl_image	*cImage = (CFgl_image *) classGetCargo ( obj );

	Bitmap			*newBmp;

	newBmp	= cImage->m_bmp->Clone ( Rect ( x1, y1, x2, y2 ), cImage->m_bmp->GetPixelFormat() );

	IStream			*iStream;
	HGLOBAL			 mem;
	CLSID			 pClsid;
	SIZE_T			 size;

	GetEncoderClsid ( _bstr_t("IMAGE/BMP"), &pClsid );
	CreateStreamOnHGlobal ( 0, 1, &iStream );

	newBmp->Save ( iStream, &pClsid, 0 ); 

	GetHGlobalFromStream ( iStream, &mem );

	size = GlobalSize ( mem );

	// push image type onto the stack
	PUSH ( VAR_STR ( "IMAGE/BMP", 10 ) );
	PUSH ( VAR_STR ( instance, (char *) GlobalLock ( mem ), size ) );
	GlobalUnlock ( mem );

	iStream->Release();

	return *classNew ( instance, "image", 2 );
}

static VAR cImageEncode ( vmInstance *instance, VAR *obj, char* type, nParamType nParams )
{
	VAR				 ret = VAR_NULL ();
	VAR				*fName;
	SIZE_T			 size;
	FILE			*file;
	IStream			*iStream;
	HGLOBAL			 mem;
	CLSID			 pClsid;

	CFgl_image	*cImage = (CFgl_image *) classGetCargo ( obj );

	if ( !GetEncoderClsid ( _bstr_t(type), &pClsid ) )
	{
		throw errorNum::scINVALID_PARAMETER ;
	}

	switch ( (uint32_t)nParams )
	{
		case 1:
			fName = nParams[1];
			if ( fName->type != slangType::eSTRING )
			{
				throw errorNum::scINVALID_PARAMETER;
			}

			if ( !GetEncoderClsid ( _bstr_t(type), &pClsid ) )
			{
				throw errorNum::scINVALID_PARAMETER ;
			}

			// ok... have to convert over internally to jpeg's so we can be sure to use the Graphics class

			CreateStreamOnHGlobal ( 0, 1, &iStream );

			cImage->m_bmp->Save ( iStream, &pClsid, 0 ); 

			GetHGlobalFromStream ( iStream, &mem );

			size = GlobalSize ( mem );

			if ( fopen_s ( &file, fName->dat.str.c, "w+b" ) )
			{
				throw GetLastError ( );
			}
			fwrite ( GlobalLock ( mem ), size, 1, file );
			fclose ( file );

			ret.type = slangType::eNULL;
			GlobalUnlock ( mem );
			iStream->Release();
			break;
		case 0:
			// ok... have to convert over internally to jpeg's so we can be sure to use the Graphics class

			CreateStreamOnHGlobal ( 0, 1, &iStream );

			cImage->m_bmp->Save ( iStream, &pClsid, 0 ); 

			GetHGlobalFromStream ( iStream, &mem );

			size = GlobalSize ( mem );
			
			ret = VAR_STR ( instance, (char *)GlobalLock ( mem ), size );
			GlobalUnlock ( mem );

			iStream->Release();
			break;
	}
	return ret;
}

ULONG_PTR gdiplusToken;

using namespace Gdiplus;

static void cFontRelease ( vmInstance *instance, VAR *var )
{
	Gdiplus::Font *cFont = (Gdiplus::Font *)classGetCargo ( var );

	cFont->~Font ( );
}

static void cFontNew ( vmInstance *instance, VAR_OBJ *obj, char const *familyName, int64_t emSize, int64_t style, int64_t fontUnit )
{
	Gdiplus::FontFamily family ( (_bstr_t ( familyName )) );
	REAL emSizeParam = (REAL) emSize;
	INT styleParam = (INT) style;
	Unit fontUnitParam = (Unit) fontUnit;

	auto f = obj->makeObj<Gdiplus::Font> ( instance, &family, emSizeParam, styleParam, fontUnitParam );

	if ( f->GetLastStatus ( ) != Ok )
	{
		f->~Font ( );
		throw GetLastError ( );
	}
}

static void cSolidBrushRelease ( vmInstance *instance, VAR *var )
{
	Gdiplus::SolidBrush	*cSolidBrush = (Gdiplus::SolidBrush	*)classGetCargo ( var );

	cSolidBrush->~SolidBrush ( );
}

static void cSolidBrushNew ( vmInstance *instance, VAR_OBJ *obj, VAR_OBJ *color )
{
	checkObject ( *color, "color" );

	auto cColor = color->getObj<Gdiplus::Color> ( );
	auto cSolidBrush = obj->makeObj<Gdiplus::SolidBrush> ( instance, *cColor );

	if ( cSolidBrush->GetLastStatus ( ) != Ok )
	{
		cSolidBrush->~SolidBrush ( );
		throw GetLastError ( );
	}
}

static void cColorNew ( vmInstance *instance, VAR_OBJ *obj, nParamType nParams )
{
	VAR				 *a, *r, *g, *b;
	VAR				 *argb;

	switch ( (uint32_t)nParams )
	{
		case 4:
			a = nParams[0];
			r = nParams[1];
			g = nParams[2];
			b = nParams[3];

			if ( a->type != slangType::eLONG || r->type != slangType::eLONG || g->type != slangType::eLONG || b->type != slangType::eLONG ) throw errorNum::scINVALID_PARAMETER;

			obj->makeObj<Gdiplus::Color>( instance, (BYTE) a->dat.l, (BYTE) r->dat.l, (BYTE) g->dat.l, (BYTE) b->dat.l );
			break;
		case 3:
			r = nParams[1];
			g = nParams[2];
			b = nParams[3];

			if ( r->type != slangType::eLONG || g->type != slangType::eLONG || b->type != slangType::eLONG ) throw errorNum::scINVALID_PARAMETER;
			obj->makeObj<Gdiplus::Color> ( instance, (BYTE) r->dat.l, (BYTE) g->dat.l, (BYTE) b->dat.l );
			break;
		case 1:
			argb = nParams[0];

			if ( argb->type != slangType::eLONG ) throw errorNum::scINVALID_PARAMETER;
			obj->makeObj<Gdiplus::Color> ( instance, (Gdiplus::ARGB) argb->dat.l );
			break;
		case 0:
			obj->makeObj<Gdiplus::Color> ( instance );
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
}

static void cPointFNew ( vmInstance *instance, VAR_OBJ *obj, double x, double y )
{
	obj->makeObj<Gdiplus::PointF> ( instance, (REAL) x, (REAL) y );
}

static void cGraphicsRelease ( vmInstance *instance, VAR *var )
{
	Gdiplus::Graphics	*cGraphics = (Gdiplus::Graphics *)classGetCargo ( var );

	cGraphics->~Graphics ( );
}

static void cGraphicsNew ( vmInstance *instance, VAR_OBJ *obj, VAR_OBJ *image )
{
	checkObject ( *image, "image" );
	auto cImage = image->getObj<CFgl_image>();

	auto cGraphics = obj->makeObj<Gdiplus::Graphics> ( instance, cImage->m_bmp );
	if ( cGraphics->GetLastStatus ( ) != Ok )
	{
		delete cGraphics;
		throw GetLastError ( );
	}
}

static void cGraphicsDrawString ( VAR *graphics, VAR_STR *string, VAR_OBJ *font, VAR_OBJ *pointF, VAR_OBJ *brush )
{
	checkObject ( *font, "Font" );
	checkObject ( *pointF, "PointF" );
	checkObject ( *brush, "SolidBrush" );

	Gdiplus::Font		 *cFont = (Gdiplus::Font *)classGetCargo ( font );
	Gdiplus::PointF		 *cPointF = (Gdiplus::PointF *)classGetCargo ( pointF );
	Gdiplus::SolidBrush	 *cSolidBrush = (Gdiplus::SolidBrush *)classGetCargo ( brush );
	Gdiplus::Graphics	 *cGraphics = (Gdiplus::Graphics *)classGetCargo ( graphics );

	cGraphics->DrawString ( _bstr_t ( string->dat.str.c ), static_cast<INT>(_bstr_t ( string->dat.str.c ).length ( )), cFont, *cPointF, cSolidBrush );
}

void builtinImageInit( class vmInstance *instance, opFile *file )
{
	GdiplusStartupInput	 gdiplusStartupInput;

	GdiplusStartup ( &gdiplusToken, &gdiplusStartupInput, NULL );

	REGISTER( instance, file );
		CLASS ( "imageType" );
			CONSTANT ( "BMP", "'IMAGE/BMP'" );
			CONSTANT ( "GIF", "'IMAGE/GIF'" );
			CONSTANT ( "TIF", "'IMAGE/TIFF'" );
			CONSTANT ( "JPG", "'IMAGE/JPEG'" );
			CONSTANT ( "PNG", "'IMAGE/PNG'" );
		END;

		CLASS ( "rotateFlipType" );
			CONSTANT ( "RotateNoneFlipNone", "0" );
			CONSTANT ( "Rotate90FlipNone", "1" );
			CONSTANT ( "Rotate180FlipNone", "2" );
			CONSTANT ( "Rotate270FlipNone", "3" );
			CONSTANT ( "RotateNoneFlipX", "4" );
			CONSTANT ( "Rotate90FlipX", "5" );
			CONSTANT ( "Rotate180FlipX", "6" );
			CONSTANT ( "Rotate270FlipX", "7" );
			CONSTANT ( "RotateNoneFlipY", "6" );
			CONSTANT ( "Rotate90FlipY", "7" );
			CONSTANT ( "Rotate180FlipY", "4" );
			CONSTANT ( "Rotate270FlipY", "5" );
			CONSTANT ( "RotateNoneFlipXY", "1" );
			CONSTANT ( "Rotate90FlipXY", "2" );
			CONSTANT ( "Rotate180FlipXY", "0" );
			CONSTANT ( "Rotate270FlipXY", "1" );
		END;

		CLASS ( "image" );
			INHERIT ( "imageType" );
			INHERIT ( "rotateFlipType" );

			METHOD ( "new", cImageNew );
			METHOD ( "release", cImageRelease );

			ACCESS ( "height", cImageGetHeight ) CONST;
			ACCESS ( "width", cImageGetWidth ) CONST;

			METHOD ( "size", cImageGetSize, DEF ( 2, "'IMAGE/JPEG'" ) );
			METHOD ( "resize", cImageResize, DEF ( 4, "1" ) );
			METHOD ( "resizeAspect", cImageResizeAspect, DEF ( 4, "1" ) );

			METHOD ( "clone", cImageClone );
			METHOD ( "encode", cImageEncode );
			METHOD ( "rotate", cImageRotate );
			METHOD ( "rotateFlip", cImageFlip );
			GCCB ( cGenericGcCb<CFgl_image>, cGenericCopyCb<CFgl_image> );
		END;

		char tmpBuff[64];

		CLASS ( "FontUnit" );
			CONSTANT ( "UnitWorld", _itoa ( Gdiplus::UnitWorld, tmpBuff, 10 ) );
			CONSTANT ( "UnitDisplay", _itoa ( Gdiplus::UnitDisplay, tmpBuff, 10 ) );
			CONSTANT ( "UnitPoint", _itoa( Gdiplus::UnitPoint, tmpBuff, 10 ) );
			CONSTANT ( "UnitInch", _itoa( Gdiplus::UnitInch, tmpBuff, 10 ) );
			CONSTANT ( "UnitDocument", _itoa( Gdiplus::UnitDocument, tmpBuff, 10 ) );
			CONSTANT ( "UnitMillimeter", _itoa( Gdiplus::UnitMillimeter, tmpBuff, 10 ) );
		END;

		CLASS ( "FontStyle" );
			CONSTANT ( "FontStyleRegular", _itoa( Gdiplus::FontStyleRegular, tmpBuff, 10 ) );
			CONSTANT ( "FontStyleBold", _itoa( Gdiplus::FontStyleBold, tmpBuff, 10 ) );
			CONSTANT ( "FontStyleItalic", _itoa( Gdiplus::FontStyleItalic, tmpBuff, 10 ) );
			CONSTANT ( "FontStyleBoldItalic", _itoa( Gdiplus::FontStyleBoldItalic, tmpBuff, 10 ) );
			CONSTANT ( "FontStyleUnderline", _itoa( Gdiplus::FontStyleUnderline, tmpBuff, 10 ) );
			CONSTANT ( "FontStyleStrikeout", _itoa( Gdiplus::FontStyleStrikeout, tmpBuff, 10 ) );
		END;

		CLASS ( "font" );
			INHERIT ( "FontUnit" );
			METHOD ( "new", cFontNew );
			METHOD ( "release", cFontRelease );
			GCCB ( cGenericGcCb<Gdiplus::Font>, cGenericCopyCb<Gdiplus::Font> );
			END;

		CLASS ( "solidBrush" )
			METHOD ( "new", cSolidBrushNew );
			METHOD ( "release", cSolidBrushRelease );
			GCCB ( cGenericGcCb<Gdiplus::SolidBrush>, cGenericCopyCb<Gdiplus::SolidBrush> );
		END;

		CLASS ( "color" )
			METHOD ( "new", cColorNew );
			GCCB ( cGenericGcCb<Gdiplus::Color>, cGenericCopyCb<Gdiplus::Color> );
		END;

		CLASS ( "PointF" )
			METHOD ( "new", cPointFNew );
			GCCB ( cGenericGcCb<Gdiplus::PointF>, cGenericCopyCb<Gdiplus::PointF> );
		END;

		CLASS ( "graphics" )
			METHOD ( "new", cGraphicsNew );
			METHOD ( "release", cGraphicsRelease );
			METHOD ( "drawString", cGraphicsDrawString );
			GCCB ( cGenericGcCb<Gdiplus::Graphics>, cGenericCopyCb<Gdiplus::Graphics> );
		END;
	END;
}
