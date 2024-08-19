/*
SLANG support functions

*/

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"

#include <regex>
#include <string_view>

/*
 * regex - Regular expression pattern matching  and replacement
 *
 * By:  Ozan S. Yigit (oz)
 *      Dept. of Computer Science
 *      York University
 *
 * Original code available from http://www.cs.yorku.ca/~oz/
 * Translation to C++ by Neil Hodgson neilh@scintilla.org
 * Removed all use of register.
 * Converted to modern function prototypes.
 * Put all global/static variables into an object so this code can be
 * used from multiple threads etc.
 *
 * These routines are the PUBLIC DOMAIN equivalents of regex
 * routines as found in 4.nBSD UN*X, with minor extensions.
 *
 * These routines are derived from various implementations found
 * in software tools books, and Conroy's grep. They are NOT derived
 * from licensed/restricted software.
 * For more interesting/academic/complicated implementations,
 * see Henry Spencer's regexp routines, or GNU Emacs pattern
 * matching module.
 *
 * Modification history removed.
 *
 * Interfaces:
 *      RESearch::Compile:        compile a regular expression into a NFA.
 *
 *			char const *RESearch::Compile(s)
 *			char const *s;
 *
 *      RESearch::Execute:        execute the NFA to match a pattern.
 *
 *			int RESearch::Execute(s)
 *			char const *s;
 *
 *	RESearch::ModifyWord		change RESearch::Execute's understanding of what a "word"
 *			looks like (for \< and \>) by adding into the
 *			hidden word-syntax table.
 *
 *			void RESearch::ModifyWord(s)
 *			char const *s;
 *
 *      RESearch::Substitute:	substitute the matched portions in a new string.
 *
 *			int RESearch::Substitute(src, dst)
 *			char const *src;
 *			char const *dst;
 *
 *	re_fail:	failure routine for RESearch::Execute.
 *
 *			void re_fail(msg, op)
 *			char const *msg;
 *			char op;
 *
 * Regular Expressions:
 *
 *      [1]     char    matches itself, unless it is a special
 *                      character (metachar): . \ [ ] * + ^ $
 *
 *      [2]     .       matches any character.
 *
 *      [3]     \       matches the character following it, throw
 *			when followed by a left or right round bracket,
 *			a digit 1 to 9 or a left or right angle bracket.
 *			(see [7], [8] and [9])
 *			It is used as an escape character for all
 *			other meta-characters, and itself. When used
 *			in a set ([4]), it is treated as an ordinary
 *			character.
 *
 *      [4]     [set]   matches one of the characters in the set.
 *                      If the first character in the set is "^",
 *                      it matches a character NOT in the set, i.e.
 *			complements the set. A shorthand S-E is
 *			used to specify a set of characters S upto
 *			E, inclusive. The special characters "]" and
 *			"-" have no special meaning if they appear
 *			as the first chars in the set.
 *                      examples:        match:
 *
 *                              [a-z]    any lowercase alpha
 *
 *                              [^]-]    any char throw ] and -
 *
 *                              [^A-Z]   any char throw uppercase
 *                                       alpha
 *
 *                              [a-zA-Z] any alpha
 *
 *      [5]     *       any regular expression form [1] to [4], followed by
 *                      closure char (*) matches zero or more matches of
 *                      that form.
 *
 *      [6]     +       same as [5], throw it matches one or more.
 *
 *      [7]             a regular expression in the form [1] to [10], enclosed
 *                      as \(form\) matches what form matches. The enclosure
 *                      creates a set of tags, used for [8] and for
 *                      pattern substution. The tagged forms are numbered
 *			starting from 1.
 *
 *      [8]             a \ followed by a digit 1 to 9 matches whatever a
 *                      previously tagged regular expression ([7]) matched.
 *
 *	[9]	\<	a regular expression starting with a \< construct
 *		\>	and/or ending with a \> construct, restricts the
 *			pattern matching to the beginning of a word, and/or
 *			the end of a word. A word is defined to be a character
 *			string beginning and/or ending with the characters
 *			A-Z a-z 0-9 and _. It must also be preceded and/or
 *			followed by any character outside those mentioned.
 *
 *      [10]            a composite regular expression xy where x and y
 *                      are in the form [1] to [10] matches the longest
 *                      match of x followed by a match for y.
 *
 *      [11]	^	a regular expression starting with a ^ character
 *		$	and/or ending with a $ character, restricts the
 *                      pattern matching to the beginning of the line,
 *                      or the end of line. [anchors] Elsewhere in the
 *			pattern, ^ and $ are treated as ordinary characters.
 *
 *
 * Acknowledgements:
 *
 *	HCR's Hugh Redelmeier has been most helpful in various
 *	stages of development. He convinced me to include BOW
 *	and EOW constructs, originally invented by Rob Pike at
 *	the University of Toronto.
 *
 * References:
 *              Software tools			Kernighan & Plauger
 *              Software tools in Pascal        Kernighan & Plauger
 *              Grep [rsx-11 C dist]            David Conroy
 *		ed - text editor		Un*x Programmer's Manual
 *		Advanced editing on Un*x	B. W. Kernighan
 *		RegExp routines			Henry Spencer
 *
 * Notes:
 *
 *	This implementation uses a bit-set representation for character
 *	classes for speed and compactness. Each character is represented
 *	by one bit in a 128-bit block. Thus, CCL always takes a
 *	constant 16 bytes in the internal nfa, and RESearch::Execute does a single
 *	bit comparison to locate the character in the set.
 *
 * Examples:
 *
 *	pattern:	foo*.*
 *	compile:	CHR f CHR o CLO CHR o END CLO ANY END END
 *	matches:	fo foo fooo foobar fobar foxx ...
 *
 *	pattern:	fo[ob]a[rz]
 *	compile:	CHR f CHR o CCL bitset CHR a CCL bitset END
 *	matches:	fobar fooar fobaz fooaz
 *
 *	pattern:	foo\\+
 *	compile:	CHR f CHR o CHR o CHR \ CLO CHR \ END END
 *	matches:	foo\ foo\\ foo\\\  ...
 *
 *	pattern:	\(foo\)[1-3]\1	(same as foo[1-3]foo)
 *	compile:	BOT 1 CHR f CHR o CHR o EOT 1 CCL bitset REF 1 END
 *	matches:	foo1foo foo2foo foo3foo
 *
 *	pattern:	\(fo.*\)-\1
 *	compile:	BOT 1 CHR f CHR o CLO ANY END EOT 1 CHR - REF 1 END
 *	matches:	foo-foo fo-fo fob-fob foobar-foobar ...
 */

#define OKP     1
#define NOP     0

#define CHR     1
#define ANY     2
#define CCL     3
#define BOL     4
#define EOL     5
#define BOT     6
#define EOT     7
#define BOW		8
#define EOW		9
#define REF     10
#define CLO     11
#define CLL     12

#define ENN     0

#define REGEX_COMP_ID	0xA600D60D

/*
 * The following defines are not meant to be changeable.
 * They are for readability only.
 */
#define BLKIND	0370
#define BITIND	07

#define ASCIIB	0177

const char bitarr[] = { 1,2,4,8,16,32,64,'\200' };

#define badpat(x)	(*nfa = ENN, x)

#define MAXCHR	256
#define CHRBIT	8
#define BITBLK	(MAXCHR/CHRBIT)

class CharacterIndexer
{
public:
	CharacterIndexer ( char const *str )
	{
		string = str;
	}

	char CharAt ( size_t index ) const
	{
		return string[index];
	};

	char CharAtU ( size_t index ) const
	{
		if ( (string[index] >= 'a') && (string[index] <= 'z') )	// convert to uppercase
		{
			return (char)((int)string[index] - 'a' + 'A');
		}
		return string[index];
	};

	virtual ~CharacterIndexer ( )
	{}
private:
	char const *string;
};

class RESearch
{

public:
	RESearch ( )
	{
		Init ( );
	}

	~RESearch ( )
	{
		Clear ( );
	}

	void Init ( )
	{
		sta = NOP;               	/* status of lastpat */
		bol = 0;
		for ( size_t i = 0; i < MAXTAG; i++ )
			pat[i] = 0;
		for ( size_t j = 0; j < BITBLK; j++ )
			bittab[j] = 0;
	}

	void Clear ( )
	{
		for ( size_t i = 0; i < MAXTAG; i++ )
		{
			delete[]pat[i];
			pat[i] = 0;
			bopat[i] = NOTFOUND;
			eopat[i] = NOTFOUND;
		}
	}

	bool GrabMatches ( CharacterIndexer &ci )
	{
		bool success = true;
		for ( size_t i = 0; i < MAXTAG; i++ )
		{
			if ( (bopat[i] != NOTFOUND) && (eopat[i] != NOTFOUND) )
			{
				size_t len = eopat[i] - bopat[i];
				pat[i] = new char[len + 1];
				if ( pat[i] )
				{
					for ( size_t j = 0; j < len; j++ )
						pat[i][j] = ci.CharAt ( bopat[i] + j );
					pat[i][len] = '\0';
				} else
				{
					success = false;
				}
			}
		}
		return success;
	}

	void ChSet ( char c )
	{
		bittab[((c) &BLKIND) >> 3] |= bitarr[(c) &BITIND]; // NOLINT (bugprone-narrowing-conversions)
	}

	void ChSetWithCase ( char c, bool caseSensitive )
	{
		if ( caseSensitive )
		{
			ChSet ( c );
		} else
		{
			if ( (c >= 'a') && (c <= 'z') )
			{
				ChSet ( c );
				ChSet ( static_cast<char>(c - 'a' + 'A') );
			} else if ( (c >= 'A') && (c <= 'Z') )
			{
				ChSet ( c );
				ChSet ( static_cast<char>(c - 'A' + 'a') );
			} else
			{
				ChSet ( c );
			}
		}
	}

	const char escapeValue ( char ch )
	{
		switch ( ch )
		{
			case 'a':	return '\a';
			case 'b':	return '\b';
			case 'f':	return '\f';
			case 'n':	return '\n';
			case 'r':	return '\r';
			case 't':	return '\t';
			case 'v':	return '\v';
		}
		return 0;
	}

	char const *Compile ( char const *pat, size_t length, bool caseSensitive, bool posix )
	{
		char *mp = nfa;          /* nfa pointer       */
		char *lp;              /* saved pointer..   */
		char *sp = nfa;          /* another one..     */
		char const *mpMax = mp + MAXNFA - BITBLK - 10;
		char  op;

		size_t tagi = 0;          /* tag stack index   */
		size_t tagc = 1;          /* actual tag count  */

		size_t n;
		char mask;		/* xor mask -CCL/NCL */
		size_t c1, c2;

		if ( !pat || !length )
		{
			if ( sta )
			{
				return 0;
			} else
			{
				return badpat ( "No previous regular expression" );
			}
		}
		sta = NOP;

		char const *p = pat;               /* pattern pointer   */
		for ( size_t i = 0; i < length; i++, p++ )
		{
			if ( mp > mpMax )
			{
				return badpat ( "Pattern too long" );
			}
			lp = mp;
			switch ( *p )
			{

				case '.':               /* match any char..  */
					*mp++ = ANY;
					break;

				case '^':               /* match beginning.. */
					if ( p == pat )
						*mp++ = BOL;
					else
					{
						*mp++ = CHR;
						*mp++ = *p;
					}
					break;

				case '$':               /* match endofline.. */
					if ( !*(p + 1) )
					{
						*mp++ = EOL;
					}  else
					{
						*mp++ = CHR;
						*mp++ = *p;
					}
					break;

				case '[':               /* match char class..*/
					*mp++ = CCL;

					i++;
					if ( *++p == '^' )
					{
						mask = '\377';
						i++;
						p++;
					} else
						mask = 0;

					if ( *p == '-' )
					{		/* real dash */
						i++;
						ChSet ( *p++ );
					}
					if ( *p == ']' )
					{	/* real brace */
						i++;
						ChSet ( *p++ );
					}
					while ( *p && *p != ']' )
					{
						if ( *p == '\\' )
						{
							i++;
							p++;
							switch ( *p )
							{
								case 'd':
									c1 = '0';
									c2 = '9';
									while ( c1 <= c2 )
									{
										ChSetWithCase ( static_cast<char>(c1++), caseSensitive );
									}
									break;
								case 'w':
									c1 = 'a';
									c2 = 'z';
									while ( c1 <= c2 )
									{
										ChSetWithCase ( static_cast<char>(c1++), caseSensitive );
									}
									c1 = 'A';
									c2 = 'Z';
									while ( c1 <= c2 )
									{
										ChSetWithCase ( static_cast<char>(c1++), caseSensitive );
									}
									ChSetWithCase ( static_cast<char>(' '), caseSensitive );
									ChSetWithCase ( static_cast<char>('\t'), caseSensitive );
									ChSetWithCase ( static_cast<char>('\n'), caseSensitive );
									ChSetWithCase ( static_cast<char>('\r'), caseSensitive );
									break;
								case 's':
									ChSetWithCase ( static_cast<char>(' '), caseSensitive );
									ChSetWithCase ( static_cast<char>('\t'), caseSensitive );
									ChSetWithCase ( static_cast<char>('\n'), caseSensitive );
									ChSetWithCase ( static_cast<char>('\r'), caseSensitive );
									break;
							}
						}
						if ( *p == '-' && *(p + 1) && *(p + 1) != ']' )
						{
							i++;
							p++;
							c1 = *(p - 2) + 1;
							i++;
							c2 = (size_t)(unsigned char)*p++;
							while ( c1 <= c2 )
							{
								ChSetWithCase ( static_cast<char>(c1++), caseSensitive );
							}
						} else if ( *p == '\\' && *(p + 1) )
						{
							i++;
							p++;
							char escape = escapeValue ( *p );
							if ( escape )
								ChSetWithCase ( escape, caseSensitive );
							else
								ChSetWithCase ( *p, caseSensitive );
							i++;
							p++;
						} else
						{
							i++;
							ChSetWithCase ( *p++, caseSensitive );
						}
					}
					if ( !*p )
						return badpat ( "Missing ]" );

					for ( n = 0; n < BITBLK; bittab[n++] = (char) 0 )
						*mp++ = static_cast<char>(mask ^ bittab[n]);

					break;

				case '*':               /* match 0 or more.. */
				case '+':               /* match 1 or more.. */
					if ( p == pat )
						return badpat ( "Empty closure" );
					lp = sp;		/* previous opcode */
					if ( *lp == CLO )		/* equivalence..   */
						break;

					if ( *(p + 1) == '?' )
					{
						i++;
						p++;
						op = CLL;
					} else
					{
						op = CLO;
					}

					switch ( *lp )
					{

						case BOL:
						case BOT:
						case EOT:
						case BOW:
						case EOW:
						case REF:
							return badpat ( "Illegal closure" );
						default:
							break;
					}

					if ( (op == CLL && *(p - 1) == '+') || (op == CLO && *p == '+') )
						for ( sp = mp; lp < sp; lp++ )
							*mp++ = *lp;

					*mp++ = ENN;
					*mp++ = ENN;
					sp = mp;
					while ( --mp > lp )
						*mp = mp[-1];
					*mp = op;
					mp = sp;
					break;

				case '\\':              /* tags, backrefs .. */
					i++;
					mask = 0;
					switch ( *++p )
					{

						case '<':
							*mp++ = BOW;
							break;
						case '>':
							if ( *sp == BOW )
								return badpat ( "Null pattern inside \\<\\>" );
							*mp++ = EOW;
							break;
						case '1':
						case '2':
						case '3':
						case '4':
						case '5':
						case '6':
						case '7':
						case '8':
						case '9':
							n = *p - '0';
							if ( tagi > 0 && tagstk[tagi] == n )
								return badpat ( "Cyclical reference" );
							if ( tagc > n )
							{
								*mp++ = static_cast<char>(REF);
								*mp++ = static_cast<char>(n);
							} else
								return badpat ( "Undetermined reference" );
							break;
						case 'a':
						case 'b':
						case 'n':
						case 'f':
						case 'r':
						case 't':
						case 'v':
							*mp++ = CHR;
							*mp++ = escapeValue ( *p );
							break;
						case 'd':
							c1 = '0';
							c2 = '9';
							while ( c1 <= c2 )
							{
								ChSetWithCase ( static_cast<char>(c1++), caseSensitive );
							}
							*mp++ = CCL;
							for ( n = 0; n < BITBLK; bittab[n++] = (char) 0 )
							{
								*mp++ = static_cast<char>(mask ^ bittab[n]);
							}
							break;
						case 'w':
							c1 = 'a';
							c2 = 'z';
							while ( c1 <= c2 )
							{
								ChSetWithCase ( static_cast<char>(c1++), caseSensitive );
							}
							c1 = 'A';
							c2 = 'Z';
							while ( c1 <= c2 )
							{
								ChSetWithCase ( static_cast<char>(c1++), caseSensitive );
							}
							ChSetWithCase ( static_cast<char>(' '), caseSensitive );
							ChSetWithCase ( static_cast<char>('\t'), caseSensitive );
							ChSetWithCase ( static_cast<char>('\n'), caseSensitive );
							ChSetWithCase ( static_cast<char>('\r'), caseSensitive );
							*mp++ = CCL;
							for ( n = 0; n < BITBLK; bittab[n++] = (char) 0 )
							{
								*mp++ = static_cast<char>(mask ^ bittab[n]);
							}
							break;
						case 's':
							ChSetWithCase ( static_cast<char>(' '), caseSensitive );
							ChSetWithCase ( static_cast<char>('\t'), caseSensitive );
							ChSetWithCase ( static_cast<char>('\n'), caseSensitive );
							ChSetWithCase ( static_cast<char>('\r'), caseSensitive );
							*mp++ = CCL;
							for ( n = 0; n < BITBLK; bittab[n++] = (char) 0 )
							{
								*mp++ = static_cast<char>(mask ^ bittab[n]);
							}
							break;

						default:
							if ( !posix && *p == '(' )
							{
								if ( tagc < MAXTAG )
								{
									tagstk[++tagi] = tagc;
									*mp++ = BOT;
									*mp++ = static_cast<char>(tagc++);
								} else
									return badpat ( "Too many \\(\\) pairs" );
							} else if ( !posix && *p == ')' )
							{
								if ( *sp == BOT )
									return badpat ( "Null pattern inside \\(\\)" );
								if ( tagi > 0 )
								{
									*mp++ = static_cast<char>(EOT);
									*mp++ = static_cast<char>(tagstk[tagi--]);
								} else
									return badpat ( "Unmatched \\)" );
							} else
							{
								*mp++ = CHR;
								*mp++ = *p;
							}
					}
					break;

				default:               /* an ordinary char  */
					if ( posix && *p == '(' )
					{
						if ( tagc < MAXTAG )
						{
							tagstk[++tagi] = tagc;
							*mp++ = BOT;
							*mp++ = static_cast<char>(tagc++);
						} else
							return badpat ( "Too many () pairs" );
					} else if ( posix && *p == ')' )
					{
						if ( *sp == BOT )
							return badpat ( "Null pattern inside ()" );
						if ( tagi > 0 )
						{
							*mp++ = static_cast<char>(EOT);
							*mp++ = static_cast<char>(tagstk[tagi--]);
						} else
							return badpat ( "Unmatched )" );
					} else if ( caseSensitive )
					{
						*mp++ = CHR;
						*mp++ = *p;
					} else
					{
						*mp++ = CCL;
						mask = 0;
						ChSetWithCase ( *p, false );
						for ( n = 0; n < BITBLK; bittab[n++] = (char) 0 )
							*mp++ = static_cast<char>(mask ^ bittab[n]);
					}
					break;
			}
			sp = lp;
		}
		if ( tagi > 0 )
			return badpat ( (posix ? "Unmatched (" : "Unmatched \\(") );
		*mp = ENN;
		sta = OKP;
		return 0;
	}

	/*
	 * Execute:
	 * 	execute nfa to find a match.
	 *
	 *	special cases: (nfa[0])
	 *		BOL
	 *			Match only once, starting from the
	 *			beginning.
	 *		CHR
	 *			First locate the character without
	 *			calling PMatch, and if found, call
	 *			PMatch for the remaining string.
	 *		ENN
	 *			Compile failed, poor luser did not
	 *			check for it. Fail fast.
	 *
	 *	If a match is found, bopat[0] and eopat[0] are set
	 *	to the beginning and the end of the matched fragment,
	 *	respectively.
	 *
	 */

	size_t Execute ( CharacterIndexer const &ci, size_t lp, size_t endp )
	{
		char c;
		size_t ep = NOTFOUND;
		char const *ap = nfa;

		bol = lp;
		failure = 0;

		Clear ( );

		switch ( *ap )
		{

			case BOL:			/* anchored: match from BOL only */
				ep = PMatch ( ci, lp, endp, ap, 0 );
				break;
			case EOL:			/* just searching for end of line normal path doesn't work */
				if ( *(ap + 1) == ENN )
				{
					lp = endp;
					ep = lp;
					break;
				} else
				{
					return 0;
				}
			case CHR:			/* ordinary char: locate it fast */
				c = *(ap + 1);
				while ( (lp < endp) && (ci.CharAt ( lp ) != c) )
					lp++;
				if ( lp >= endp )		/* if EOS, fail, else fall thru. */
					return 0;
			default:			/* regular matching all the way. */
				while ( lp < endp )
				{
					ep = PMatch ( ci, lp, endp, ap, 0 );
					if ( ep != NOTFOUND )
						break;
					lp++;
				}
				break;
			case ENN:			/* munged automaton. fail always */
				return 0;
		}
		if ( ep == NOTFOUND )
			return 0;

		bopat[0] = lp;
		eopat[0] = ep;
		return 1;
	}

	size_t Executei ( CharacterIndexer const &ci, size_t lp, size_t endp )
	{
		char c;
		size_t ep = NOTFOUND;
		char const *ap = nfa;

		bol = lp;
		failure = 0;

		Clear ( );

		switch ( *ap )
		{

			case BOL:			/* anchored: match from BOL only */
				ep = PMatch ( ci, lp, endp, ap, 1 );
				break;
			case EOL:			/* just searching for end of line normal path doesn't work */
				if ( *(ap + 1) == ENN )
				{
					lp = endp;
					ep = lp;
					break;
				} else
				{
					return 0;
				}
			case CHR:			/* ordinary char: locate it fast */
				c = *(ap + 1);
				if ( (c >= 'a') && (c <= 'z') )	// convert to uppercase
				{
					c = (char)((int)c + 'A' - 'a');
				}
				while ( (lp < endp) && (ci.CharAtU ( lp ) != c) )
					lp++;
				if ( lp >= endp )		/* if EOS, fail, else fall thru. */
					return 0;
			default:			/* regular matching all the way. */
				while ( lp < endp )
				{
					ep = PMatch ( ci, lp, endp, ap, 1 );
					if ( ep != NOTFOUND )
						break;
					lp++;
				}
				break;
			case ENN:			/* munged automaton. fail always */
				return 0;
		}
		if ( ep == NOTFOUND )
			return 0;

		bopat[0] = lp;
		eopat[0] = ep;
		return 1;
	}

	/*
	 * PMatch: internal routine for the hard part
	 *
	 * 	This code is partly snarfed from an early grep written by
	 *	David Conroy. The backref and tag stuff, and various other
	 *	innovations are by oz.
	 *
	 *	special case optimizations: (nfa[n], nfa[n+1])
	 *		CLO ANY
	 *			We KNOW .* will match everything upto the
	 *			end of line. Thus, directly go to the end of
	 *			line, without recursive PMatch calls. As in
	 *			the other closure cases, the remaining pattern
	 *			must be matched by moving backwards on the
	 *			string recursively, to find a match for xy
	 *			(x is ".*" and y is the remaining pattern)
	 *			where the match satisfies the LONGEST match for
	 *			x followed by a match for y.
	 *		CLO CHR
	 *			We can again scan the string forward for the
	 *			single char and at the point of failure, we
	 *			execute the remaining nfa recursively, same as
	 *			above.
	 *
	 *	At the end of a successful match, bopat[n] and eopat[n]
	 *	are set to the beginning and end of subpatterns matched
	 *	by tagged expressions (n = 1 to 9).
	 *
	 */

	/*
	 * character classification table for word boundary operators BOW
	 * and EOW. the reason for not using ctype macros is that we can
	 * let the user add into our own table. see ModifyWord. This table
	 * is not in the bitset form, since we may wish to extend it in the
	 * future for other character classifications.
	 *
	 *	TRUE for 0-9 A-Z a-z _
	 */
	inline static char chrtyp[MAXCHR] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
		0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 0, 0, 0, 0, 1, 0, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 0, 0, 0, 0, 0
	};

#define inascii(x)	(0177&(x))
#define iswordc(x) 	chrtyp[inascii(x)]
#define isinset(x,y) 	((x)[((y)&BLKIND)>>3] & bitarr[(y)&BITIND])

/*
 * skip values for CLO|CLL XXX to skip past the closure
 */

#define ANYSKIP	2 	/* [CLO|CLL] ANY ENN ...	     */
#define CHRSKIP	3	/* [CLO|CLL] CHR chr ENN ...     */
#define CCLSKIP 34	/* [CLO|CLL] CCL 32bytes ENN ... */

/*
 * ModifyWord:
 *	add new characters into the word table to change Execute's
 *	understanding of what a word should look like. Note that we
 *	only accept additions into the word definition.
 *
 *	If the string parameter is 0 or null string, the table is
 *	reset back to the default containing A-Z a-z 0-9 _. [We use
 *	the compact bitset representation for the default table]
 */

	inline static char deftab[16] = {
		0, 0, 0, 0, 0, 0, '\377', 003, '\376', '\377', '\377', '\207',
		'\376', '\377', '\377', 007
	};

	void ModifyWord ( char const *s )
	{
		size_t i;

		if ( !s || !*s )
		{
			for ( i = 0; i < MAXCHR; i++ )
				if ( !isinset ( deftab, i ) )
					iswordc ( i ) = 0;
		} else
			while ( *s )
				iswordc ( *s++ ) = 1;
	}

	/*
	 * Replace:
	 *	substitute the matched portions of the src in dst.
	 *
	 *	&	substitute the entire matched pattern.
	 *
	 *	\digit	substitute a subpattern, with the given	tag number.
	 *		Tags are numbered from 1 to 9. If the particular
	 *		tagged subpattern does not exist, null is substituted.
	 */
	size_t Replace ( CharacterIndexer const &ci, char const *src, BUFFER *dst )
	{
		char c;
		size_t pin;
		size_t bp;
		size_t ep;

		if ( !*src || !bopat[0] )
			return 0;

		while ( (c = *src++) != 0 )
		{
			switch ( c )
			{
				case '&':
					pin = 0;
					break;

				case '\\':
					c = *src++;
					if ( c >= '0' && c <= '9' )
					{
						pin = c - '0';
						break;
					}

				default:
					bufferPut ( dst, c );
					continue;
			}

			if ( (bp = bopat[pin]) != 0 && (ep = eopat[pin]) != 0 )
			{
				while ( ci.CharAt ( bp ) && bp < ep )
				{
					bufferPut ( dst, ci.CharAt ( bp++ ) );
				}
				if ( bp < ep )
				{
					return 0;
				}
			}
		}
		bufferPut ( dst, 0 );
		return 1;
	}

	/*
	 * Substitute:
	 *	substitute the matched portions of the src in dst... portions of ci not matched are included
	 *
	 *	&	substitute the entire matched pattern.
	 *
	 *	\digit	substitute a subpattern, with the given	tag number.
	 *		Tags are numbered from 1 to 9. If the particular
	 *		tagged subpattern does not exist, null is substituted.
	 */
	size_t Substitute ( CharacterIndexer const &ci, char const *src, BUFFER *dst )
	{
		char c;
		size_t  pin;
		size_t bp;
		size_t ep;

		if ( !*src || !bopat[0] )
		{
			return 0;
		}

		bp = 0;
		while ( bp < bopat[0] )
		{
			bufferPut ( dst, ci.CharAt ( bp++ ) );
		}

		while ( (c = *src++) != 0 )
		{
			switch ( c )
			{
				case '&':
					pin = 0;
					break;

				case '\\':
					c = *src++;
					if ( c >= '0' && c <= '9' )
					{
						pin = c - '0';
						break;
					}

				default:
					bufferPut ( dst, c );
					continue;
			}

			if ( (bp = bopat[pin]) != 0 && (ep = eopat[pin]) != 0 )
			{
				while ( ci.CharAt ( bp ) && bp < ep )
				{
					bufferPut ( dst, ci.CharAt ( bp++ ) );
				}
				if ( bp < ep )
				{
					return 0;
				}
			}
		}

		bp = eopat[0];
		while ( ci.CharAt ( bp ) )
		{
			bufferPut ( dst, ci.CharAt ( bp++ ) );
		}

		bufferPut ( dst, 0 );
		return 1;
	}

	/*
	 * Substitute:All
	 *	substitute the matched portions of the src in dst... only start portions of ci not matched are included
	 *
	 *	&	substitute the entire matched pattern.
	 *
	 *	\digit	substitute a subpattern, with the given	tag number.
	 *		Tags are numbered from 1 to 9. If the particular
	 *		tagged subpattern does not exist, null is substituted.
	 */
	size_t SubstituteAll ( CharacterIndexer const &ci, char const *src, BUFFER *dst )
	{
		char c;
		size_t  pin;
		size_t bp;
		size_t ep;

		if ( !*src || (bopat[0] == -1) )
		{
			return 0;
		}

		bp = 0;
		while ( bp < bopat[0] )
		{
			bufferPut ( dst, ci.CharAt ( bp++ ) );
		}

		while ( (c = *src++) != 0 )
		{
			switch ( c )
			{
				case '&':
					pin = 0;
					break;

				case '\\':
					c = *src++;
					if ( c >= '0' && c <= '9' )
					{
						pin = c - '0';
						break;
					}

				default:
					bufferPut ( dst, c );
					continue;
			}

			if ( (bp = bopat[pin]) != 0 && (ep = eopat[pin]) != 0 )
			{
				while ( ci.CharAt ( bp ) && bp < ep )
				{
					bufferPut ( dst, ci.CharAt ( bp++ ) );
				}
				if ( bp < ep )
				{
					return 0;
				}
			}
		}
		return 1;
	}

	char const *GetNFA ( )

	{
		return nfa;
	}

	size_t SetNFA ( char const *newNfa, size_t len )

	{
		if ( (*((unsigned long *) newNfa) != REGEX_COMP_ID) || (len != MAXNFA) )
		{
			if ( Compile ( newNfa, len, 1, 0 ) )
			{
				return 0;
			}
		} else
		{
			memcpy ( nfa, newNfa + sizeof ( unsigned long ), MAXNFA );
		}
		return 1;
	}

	enum
	{
		MAXTAG = 10
	};
	enum
	{
		MAXNFA = 2048
	};
	enum
	{
		NOTFOUND = -1
	};

	size_t bopat[MAXTAG];
	size_t eopat[MAXTAG];
	char *pat[MAXTAG];

private:
	size_t PMatch ( CharacterIndexer const &ci, size_t lp, size_t endp, char const *ap, bool noCase )
	{
		size_t op, c, n;
		size_t e;		/* extra pointer for CLO */
		size_t bp;		/* beginning of subpat.. */
		size_t ep;		/* ending of subpat..	 */
		size_t are;			/* to save the line ptr. */
		char const *patStart;

		while ( (op = (size_t)(unsigned char)*ap++) != ENN )
		{
			switch ( op )
			{
				case CHR:
					if ( noCase )
					{
						c = (size_t)(unsigned char)*ap++;
						if ( (c >= 'a') && (c <= 'z') )	// convert to uppercase
						{
							c = c + 'A' - 'a';
						}
						if ( ci.CharAtU ( lp++ ) != c )
						{
							return NOTFOUND;
						}
					} else
					{
						if ( ci.CharAt ( lp++ ) != *ap++ )
							return NOTFOUND;
					}
					break;
				case ANY:
					if ( lp++ >= endp )
						return NOTFOUND;
					break;
				case CCL:
					if ( noCase )
					{
						if ( !(isinset ( ap, ci.CharAt ( lp ) ) || isinset ( ap, ci.CharAtU ( lp ) )) )
						{
							return NOTFOUND;
						}
						lp++;
					} else
					{
						c = (size_t)(unsigned char)ci.CharAt ( lp++ );
						if ( !isinset ( ap, c ) )
						{
							return NOTFOUND;
						}
					}
					ap += BITBLK;
					break;
				case BOL:
					if ( lp != bol )
						return NOTFOUND;
					break;
				case EOL:
					if ( lp < endp )
						return NOTFOUND;
					break;
				case BOT:
					bopat[(size_t)(unsigned char)*ap++] = lp;
					break;
				case EOT:
					eopat[(size_t)(unsigned char)*ap++] = lp;
					break;
				case BOW:
					if ( (lp != bol && iswordc ( ci.CharAt ( lp - 1 ) )) || !iswordc ( ci.CharAt ( lp ) ) )
					{
						return NOTFOUND;
					}
					break;
				case EOW:
					if ( lp == bol || !iswordc ( ci.CharAt ( lp - 1 ) ) || iswordc ( ci.CharAt ( lp ) ) )
					{
						return NOTFOUND;
					}
					break;
				case REF:
					n = (size_t)(unsigned char)*ap++;
					bp = bopat[n];
					ep = eopat[n];
					while ( bp < ep )
					{
						if ( noCase )
						{
							if ( ci.CharAtU ( bp++ ) != ci.CharAtU ( lp++ ) )
								return NOTFOUND;
						} else
						{
							if ( ci.CharAt ( bp++ ) != ci.CharAt ( lp++ ) )
								return NOTFOUND;
						}
					}
					break;
				case CLO:
					are = lp;
					switch ( *ap )
					{
						case ANY:
							lp = endp;
							n = ANYSKIP;
							break;
						case CHR:
							c = (size_t)(unsigned char)*(ap + 1);
							if ( noCase )
							{
								while ( (lp < endp) && (c == ci.CharAtU ( lp )) )
								{
									lp++;
								}
							} else
							{
								while ( (lp < endp) && (c == ci.CharAt ( lp )) )
								{
									lp++;
								}
							}
							n = CHRSKIP;
							break;
						case CCL:
							if ( noCase )
							{
								while ( (lp < endp) && (isinset ( ap + 1, ci.CharAt ( lp ) ) || isinset ( ap + 1, ci.CharAtU ( lp ) )) )
								{
									lp++;
								}
							} else
							{
								while ( (lp < endp) && isinset ( ap + 1, ci.CharAt ( lp ) ) )
								{
									lp++;
								}
							}
							while ( (lp < endp) && isinset ( ap + 1, ci.CharAt ( lp ) ) )
								lp++;
							n = CCLSKIP;
							break;
						default:
							failure = true;
							//re_fail("closure: bad nfa.", *ap);
							return NOTFOUND;
					}

					ap += n;

					while ( lp >= are )
					{
						if ( (e = PMatch ( ci, lp, endp, ap, noCase )) != NOTFOUND )
						{
							return e;
						}
						--lp;
					}
					return NOTFOUND;
				case CLL:
					switch ( *ap )
					{
						case ANY:
							ap += ANYSKIP;

							while ( lp < endp )
							{
								if ( (e = PMatch ( ci, lp, endp, ap, noCase )) != NOTFOUND )
								{
									return e;
								}
								lp++;
							}
							break;
						case CHR:
							c = (size_t)(unsigned char)*(ap + 1);
							ap += ANYSKIP;
							if ( noCase )
							{
								while ( (lp < endp) && (c == ci.CharAtU ( lp )) )
								{
									if ( (e = PMatch ( ci, lp, endp, ap, noCase )) != NOTFOUND )
									{
										return e;
									}
									lp++;
								}
							} else
							{
								while ( (lp < endp) && (c == ci.CharAt ( lp )) )
								{
									if ( (e = PMatch ( ci, lp, endp, ap, noCase )) != NOTFOUND )
									{
										return e;
									}
									lp++;
								}
							}
							break;
						case CCL:
							patStart = ap + 1;
							ap += CCLSKIP;
							if ( noCase )
							{
								while ( (lp < endp) && (isinset ( patStart, ci.CharAt ( lp ) ) || isinset ( ap + 1, ci.CharAtU ( lp ) )) )
								{
									if ( (e = PMatch ( ci, lp, endp, ap, noCase )) != NOTFOUND )
									{
										return e;
									}
									lp++;
								}
							} else
							{
								while ( (lp < endp) && isinset ( patStart, ci.CharAt ( lp ) ) )
								{
									if ( (e = PMatch ( ci, lp, endp, ap, noCase )) != NOTFOUND )
									{
										return e;
									}
									lp++;
								}
							}
							break;
						default:
							failure = true;
							//re_fail("closure: bad nfa.", *ap);
							return NOTFOUND;
					}
					return NOTFOUND;
				default:
					//re_fail("RESearch::Execute: bad nfa.", static_cast<char>(op));
					return NOTFOUND;
			}
		}
		return lp;
	}

	size_t bol;
	size_t  tagstk[MAXTAG];             /* subpat tag stack..*/
	char nfa[MAXNFA];		/* automaton..       */
	size_t sta;
	char bittab[BITBLK];		/* bit table for CCL */
						/* pre-set bits...   */
	bool failure;
};

static size_t vmRegexAt ( VAR *nfa, VAR *str, bool noCase )

{
	RESearch	 regEx;

	while ( TYPE ( nfa ) == slangType::eREFERENCE )
	{
		nfa = nfa->dat.ref.v;
	}

	if ( TYPE ( nfa ) != slangType::eSTRING )
	{
		throw ( errorNum::scINVALID_PARAMETER );
	}

	while ( TYPE ( str ) == slangType::eREFERENCE )
	{
		str = str->dat.ref.v;
	}

	if ( TYPE ( str ) != slangType::eSTRING )
	{
		throw ( errorNum::scINVALID_PARAMETER );
	}

	if ( !regEx.SetNFA ( nfa->dat.str.c, nfa->dat.str.len ) )
	{
		throw ( errorNum::scINVALID_PARAMETER );
	}

	if ( noCase )
	{
		if ( regEx.Executei ( CharacterIndexer ( str->dat.str.c ), 0, str->dat.str.len ) == RESearch::NOTFOUND )
		{
			return 0;
		} else
		{
			return regEx.bopat[0] + 1;
		}
	} else
	{
		if ( regEx.Execute ( CharacterIndexer ( str->dat.str.c ), 0, str->dat.str.len ) == RESearch::NOTFOUND )
		{
			return 0;
		} else
		{
			return regEx.bopat[0] + 1;
		}
	}
}

static size_t vmRegexAtLen ( VAR *nfa, VAR *str, bool noCase )

{
	RESearch	 regEx;

	while ( TYPE ( nfa ) == slangType::eREFERENCE )
	{
		nfa = nfa->dat.ref.v;
	}

	if ( TYPE ( nfa ) != slangType::eSTRING )
	{
		throw ( errorNum::scINVALID_PARAMETER );
	}

	while ( TYPE ( str ) == slangType::eREFERENCE )
	{
		str = str->dat.ref.v;
	}

	if ( TYPE ( str ) != slangType::eSTRING )
	{
		throw ( errorNum::scINVALID_PARAMETER );
	}

	if ( !regEx.SetNFA ( nfa->dat.str.c, nfa->dat.str.len ) )
	{
		throw ( errorNum::scINVALID_PARAMETER );
	}

	if ( noCase )
	{
		if ( regEx.Executei ( CharacterIndexer ( str->dat.str.c ), 0, str->dat.str.len ) == RESearch::NOTFOUND )
		{
			return 0;
		} else
		{
			return regEx.eopat[0] - regEx.bopat[0];
		}
	} else
	{
		if ( regEx.Execute ( CharacterIndexer ( str->dat.str.c ), 0, str->dat.str.len ) == RESearch::NOTFOUND )
		{
			return 0;
		} else
		{
			return regEx.eopat[0] - regEx.bopat[0];
		}
	}
}

static VAR vmRegexMatch ( vmInstance *instance, VAR_STR *nfa, VAR_STR *str, bool noCase )
{
	RESearch	 regEx;

	if ( !regEx.SetNFA ( nfa->dat.str.c, nfa->dat.str.len ) )
	{
		throw ( errorNum::scINVALID_PARAMETER );
	}

	VAR_ARRAY arr ( instance );

	if ( noCase )
	{
		if ( regEx.Executei ( CharacterIndexer ( str->dat.str.c ), 0, str->dat.str.len ) == RESearch::NOTFOUND )
		{
			return arr;
		}
	} else
	{
		if ( regEx.Execute ( CharacterIndexer ( str->dat.str.c ), 0, str->dat.str.len ) == RESearch::NOTFOUND )
		{
			return arr;
		}
	}

	for ( int64_t loop = 0; loop < RESearch::MAXTAG; loop++ )
	{
		if ( (regEx.bopat[loop] != -1) && (regEx.eopat[loop] != -1) && (regEx.eopat[loop] > regEx.bopat[loop]) )
		{
			// matched string
			*(*arr[loop])[1] = VAR_STR ( instance, str->dat.str.c + regEx.bopat[loop], regEx.eopat[loop] - regEx.bopat[loop] );
			// start of match
			*(*arr[loop])[2] = VAR ( regEx.bopat[loop] + 1 );
			// end of match
			*(*arr[loop])[3] = VAR ( regEx.eopat[loop] );
		}
	}
	return arr;
}

static VAR vmRegexMatchAll ( vmInstance *instance, VAR_STR *nfa, VAR_STR *str, bool noCase )

{
	RESearch		 regEx;

	if ( !regEx.SetNFA ( nfa->dat.str.c, nfa->dat.str.len ) )
	{
		throw ( errorNum::scINVALID_PARAMETER );
	}

	VAR_ARRAY arr ( instance );

	size_t startOffset = 0;
	int64_t ctr = 1;
	while ( startOffset < str->dat.str.len )
	{
		if ( noCase )
		{
			if ( regEx.Executei ( CharacterIndexer ( str->dat.str.c + startOffset ), 0, str->dat.str.len - startOffset ) == RESearch::NOTFOUND )
			{
				return arr;
			}
		} else
		{
			if ( regEx.Execute ( CharacterIndexer ( str->dat.str.c + startOffset ), 0, str->dat.str.len - startOffset ) == RESearch::NOTFOUND )
			{
				return arr;
			}
		}

		// add it to the array
		if ( (regEx.bopat[0] != -1) && (regEx.eopat[0] != -1) && (regEx.eopat[0] > regEx.bopat[0]) )
		{
			// matched string
			*(*arr[ctr])[1] = VAR_STR ( instance, str->dat.str.c + regEx.bopat[0], regEx.eopat[0] - regEx.bopat[0] );
			// start of match
			*(*arr[ctr])[2] = VAR ( regEx.bopat[0] + 1 + startOffset);
			// end of match
			*(*arr[ctr])[3] = VAR ( regEx.eopat[0] + startOffset );
			ctr++;
			startOffset += regEx.eopat[0];
		} else
		{
			break;
		}
	}
	return arr;
}

static VAR_STR vmRegexExtract ( vmInstance *instance, VAR_STR *nfa, VAR_STR *str, VAR_STR *repPat, bool noCase )
{
	RESearch	regEx;
	BUFFER		buff;

	if ( !regEx.SetNFA ( nfa->dat.str.c, nfa->dat.str.len ) )
	{
		throw ( errorNum::scINVALID_PARAMETER );
	}

	if ( noCase )
	{
		if ( regEx.Executei ( CharacterIndexer ( str->dat.str.c ), 0, str->dat.str.len ) == RESearch::NOTFOUND )
		{
			return VAR_STR ( "" );
		}
	} else
	{
		if ( regEx.Execute ( CharacterIndexer ( str->dat.str.c ), 0, str->dat.str.len ) == RESearch::NOTFOUND )
		{
			return VAR_STR ( "" );
		}
	}

	if ( (regEx.bopat[0] != -1) && (regEx.eopat[0] != -1) && (regEx.eopat[0] > regEx.bopat[0]) )
	{
		if ( !regEx.Replace ( CharacterIndexer ( str->dat.str.c ), repPat->dat.str.c, &buff ) )
		{
			return VAR_STR ( "" );
		}
	} else
	{
		bufferPut ( &buff, 0 );
	}

	return VAR_STR ( instance, buff );
}

static VAR_STR vmRegexReplace ( vmInstance *instance, VAR_STR *nfa, VAR_STR *str, VAR_STR *repPat, bool noCase )
{
	RESearch	regEx;
	BUFFER		buff;

	if ( !regEx.SetNFA ( nfa->dat.str.c, nfa->dat.str.len ) )
	{
		throw ( errorNum::scINVALID_PARAMETER );
	}

	if ( noCase )
	{
		if ( regEx.Executei ( CharacterIndexer ( str->dat.str.c ), 0, str->dat.str.len ) == RESearch::NOTFOUND )
		{
			return VAR_STR ( "" );
		}
	} else
	{
		if ( regEx.Execute ( CharacterIndexer ( str->dat.str.c ), 0, str->dat.str.len ) == RESearch::NOTFOUND )
		{
			return VAR_STR ( "" );
		}
	}

	if ( (regEx.bopat[0] != -1) && (regEx.eopat[0] != -1) && (regEx.eopat[0] > regEx.bopat[0]) )
	{
		if ( !regEx.Substitute ( CharacterIndexer ( str->dat.str.c ), repPat->dat.str.c, &buff ) )
		{
			return VAR_STR ( "" );
		}
	} else
	{
		bufferWrite ( &buff, str->dat.str.c, str->dat.str.len + 1 );
	}

	return VAR_STR ( instance, buff );
}

static VAR_STR vmRegexReplaceAll ( vmInstance *instance, VAR_STR *nfa, VAR_STR *str, VAR_STR *repPat, bool noCase )
{
	RESearch	regEx;
	BUFFER		buff;

	if ( !regEx.SetNFA ( nfa->dat.str.c, nfa->dat.str.len ) )
	{
		throw ( errorNum::scINVALID_PARAMETER );
	}

	size_t startOffset = 0;
	while ( startOffset < str->dat.str.len )
	{
		if ( noCase )
		{
			if ( regEx.Executei ( CharacterIndexer ( str->dat.str.c + startOffset ), 0, str->dat.str.len - startOffset ) == RESearch::NOTFOUND )
			{
				return VAR_STR ( "" );
			}
		} else
		{
			if ( regEx.Execute ( CharacterIndexer ( str->dat.str.c + startOffset ), 0, str->dat.str.len - startOffset ) == RESearch::NOTFOUND )
			{
				return VAR_STR ( "" );
			}
		}

		if ( (regEx.bopat[0] != -1) && (regEx.eopat[0] != -1) && (regEx.eopat[0] > regEx.bopat[0]) )
		{
			if ( !regEx.SubstituteAll ( CharacterIndexer ( str->dat.str.c + startOffset ), repPat->dat.str.c, &buff ) )
			{
				return VAR_STR ( "" );
			}
		} else
		{
			regEx.eopat[0] = 0;
			break;
		}

		startOffset += regEx.eopat[0];
	}

	auto bp = regEx.eopat[0] + startOffset;
	while ( (unsigned int) bp < str->dat.str.len )
	{
		bufferPut ( &buff, str->dat.str.c[bp++] );
	}

	bufferPut ( &buff, 0 );

	return VAR_STR ( instance, buff );
}

static VAR_STR RegexCompile ( vmInstance *instance, VAR_STR *var )
{
	RESearch	 regEx;

	if ( regEx.Compile ( var->dat.str.c, var->dat.str.len, 1, 0 ) )
	{
		throw ( errorNum::scINVALID_PARAMETER );
	}

	return VAR_STR ( instance, regEx.GetNFA ( ), RESearch::MAXNFA );
}

static auto RegexAt ( VAR_STR *var, VAR_STR *str )
{
	return vmRegexAt ( var, str, 0 );
}

static auto  RegexAti ( VAR *var, VAR *str )
{
	return vmRegexAt ( var, str, 1 );
}

static auto RegexAtLen ( VAR *var, VAR *str )
{
	return vmRegexAtLen ( var, str, 0 );
}

static auto RegexAtLeni ( VAR *var, VAR *str )
{
	return vmRegexAtLen ( var, str, 1 );
}

static auto RegexMatch ( vmInstance *instance, VAR_STR *var, VAR_STR *str )
{
	return vmRegexMatch ( instance, var, str, 0 );
}

static auto RegexMatchi ( vmInstance *instance, VAR_STR *var, VAR_STR *str )
{
	return vmRegexMatch ( instance, var, str, 1 );
}

static auto RegexExtract ( vmInstance *instance, VAR_STR *var, VAR_STR *str, VAR_STR *repPat )
{
	return vmRegexExtract ( instance, var, str, repPat, 0 );
}

static auto RegexExtracti ( vmInstance *instance, VAR_STR *var, VAR_STR *str, VAR_STR *repPat )
{
	return vmRegexExtract ( instance, var, str, repPat, 1 );
}

static auto RegexReplace ( vmInstance *instance, VAR_STR *var, VAR_STR *str, VAR_STR *repPat )
{
	return vmRegexReplace ( instance, var, str, repPat, 0 );
}

static auto RegexReplacei ( vmInstance *instance, VAR_STR *var, VAR_STR *str, VAR_STR *repPat )
{
	return vmRegexReplace ( instance, var, str, repPat, 1 );
}

static auto RegexMatchAll ( vmInstance *instance, VAR_STR *var, VAR_STR *str )
{
	return vmRegexMatchAll ( instance, var, str, 0 );
}

static auto RegexMatchAlli ( vmInstance *instance, VAR_STR *var, VAR_STR *str )
{
	return vmRegexMatchAll ( instance, var, str, 1 );
}

static auto RegexReplaceAll ( vmInstance *instance, VAR_STR *var, VAR_STR *str, VAR_STR *repPat )
{
	return vmRegexReplaceAll ( instance, var, str, repPat, 0 );
}

static auto RegexReplaceAlli ( vmInstance *instance, VAR_STR *var, VAR_STR *str, VAR_STR *repPat )
{
	return vmRegexReplaceAll ( instance, var, str, repPat, 1 );
}

void builtinRegexOldInit ( class vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		NAMESPACE ( "fgl" )
			FUNC ( "regexCompile", RegexCompile);
			FUNC ( "regexAt", RegexAt ) CONST PURE;
			FUNC ( "regexAti", RegexAti ) CONST PURE;
			FUNC ( "regexAtLen", RegexAtLen ) CONST PURE;
			FUNC ( "regexAtLenI", RegexAtLeni ) CONST PURE;
			FUNC ( "regexMatch", RegexMatch ) CONST PURE;
			FUNC ( "regexMatchI", RegexMatchi ) CONST PURE;
			FUNC ( "regexMatchAll", RegexMatchAll ) CONST PURE;
			FUNC ( "regexMatchAllI", RegexMatchAlli ) CONST PURE;
			FUNC ( "regexExtract", RegexExtract ) CONST PURE;
			FUNC ( "regexExtractI", RegexExtracti ) CONST PURE;
			FUNC ( "regexReplace", RegexReplace );
			FUNC ( "regexReplaceI", RegexReplacei );
			FUNC ( "regexReplaceAll", RegexReplaceAll );
			FUNC ( "regexreplaceAllI", RegexReplaceAlli );
		END;
	END;
}
