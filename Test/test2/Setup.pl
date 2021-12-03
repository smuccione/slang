#!C:\Perl\bin\perl.exe --

###############################################################################
# Setup.pl                                                                    #
###############################################################################
# YaBB: Yet another Bulletin Board                                            #
# Open-Source Community Software for Webmasters                               #
# Version:        YaBB 2.1                                                    #
# Released:       November 8, 2005                                            #
# Distributed by: http://www.yabbforum.com                                    #
# =========================================================================== #
# Copyright (c) 2000-2005 YaBB (www.yabbforum.com) - All Rights Reserved.     #
# Software by: The YaBB Development Team                                      #
#              with assistance from the YaBB community.                       #
# Sponsored by: Xnull Internet Media, Inc. - http://www.ximinc.com            #
###############################################################################

$setupplver = 'YaBB 2.1 $Revision: 1.20 $';

$maxmemsteps = 3000;

if ($ENV{'SERVER_SOFTWARE'} =~ /IIS/) {
	$yyIIS = 1;
	$0 =~ m~(.*)(\\|/)~;
	$yypath = $1;
	$yypath =~ s~\\~/~g;
	chdir($yypath);
	push(@INC, $yypath);
}

### Requirements and Errors ###
$script_root = $ENV{'SCRIPT_FILENAME'};
$script_root =~ s/\/Setup\.(pl|cgi)//ig;

if (-e "Paths.pl") { require "Paths.pl"; }
elsif (-e "$script_root/Paths.pl") { require "$script_root/Paths.pl"; }
elsif (-e "$script_root/Variables/Paths.pl") { require "$script_root/Variables/Paths.pl"; }

# Check if it's blank Paths.pl or filled in one
unless ($lastsaved) {
	$boardsdir = "./Boards";
	$sourcedir = "./Sources";
	$memberdir = "./Members";
	$vardir    = "./Variables";
}

$thisscript = "$ENV{'SCRIPT_NAME'}";
if (-e ("YaBB.cgi")) { $yyext = "cgi"; }
else { $yyext = "pl"; }
if ($boardurl) { $set_cgi = "$boardurl/Setup.$yyext"; }
else { $set_cgi = "Setup.$yyext"; }

# Make sure the module path is present
# Some servers need all the subdirs in @INC too.
push(@INC, "./Modules");
push(@INC, "./Modules/Upload");
push(@INC, "./Modules/Digest");

require "$sourcedir/Subs.pl";
require "$sourcedir/System.pl";
require "$sourcedir/Load.pl";
require "$sourcedir/DateTime.pl";

$windowbg     = "#FEFEFE";
$windowbg2    = "#DDE3EB";
$header       = "#6699cc";
$catbg        = "#ADC7E1";
$maintext_23  = "Unable to open";
$admin_txt106 = "An Error Has Occurred!";
$admin_txt193 = "Back";

$yymenu = "";

if (-e "$vardir/Setup.lock") {
	$NavLink1 = qq~<a href="$set_cgi?action=cats" style="color: #FF3333;">Boards & Categories</a>~;
	$NavLink2 = qq~<a href="$set_cgi?action=members" style="color: #FF3333;">Members</a>~;
	$NavLink3 = qq~<a href="$set_cgi?action=messages" style="color: #FF3333;">Messages</a>~;
	$NavLink4 = qq~<a href="$set_cgi?action=dates" style="color: #FF3333;">Date & Time</a>~;
	$NavLink5 = qq~<a href="$set_cgi?action=cleanup" style="color: #FF3333;">Clean Up</a>~;
	$NavLink6 = qq~<a href="$boardurl/YaBB.$yyext?action=login" style="color: #FF3333;">Login</a>~;

	$ConvDone = qq~
<div style="float: left; width: 102px; height: 10px; margin: 1px; background-color: #6699cc; border: 1px black solid; font-size: 5px;">&nbsp;</div>
<div style="float: left; width: 40px; height: 14px; text-align: right; color: #FF3333;">100%</div><br />
~;

	$ConvNotDone = qq~
<div style="float: left; width: 102px; height: 10px; margin: 1px; background-color: #dddddd; border: 1px black solid; font-size: 5px;">&nbsp;</div>
<div style="float: left; width: 40px; height: 14px; text-align: right; color: #bbbbbb;">0%</div><br />
~;
}

if (-e "$vardir/Setup.lock" && !$action) {

	if (-e "$vardir/Converter.lock") { &FoundLock; }
	&tempstarter;

	$yymenu = qq~Boards & Categories | Members | Messages | Date & Time | Clean Up | Login~;

	$yymain = qq~
<div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">

<form action="$set_cgi?action=prepare" method="post">

	<table width="100%" cellspacing="1" cellpadding="4">
	<tr valign="middle">
		<td width="100%" colspan="2" class="titlebg" align="left">
		YaBB 2 Converter
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/thread.gif" alt="" />
		</td>
		<td class="windowbg2" align="left" style="font-size: 11px;">
		Make sure your YaBB 2 installation is running and that it has all the correct folder paths and URLs.<br />
		Proceed through the following steps to convert your YaBB 1 Gold - SP 1.x forum to YaBB 2!<br /><br />
		<b>If</b> your YaBB 1 Gold - SP 1.x forum is located on the same server as your YaBB 2 installation:
		<ol>
		<li>Insert the path to your YaBB 1 Gold - SP 1.x forum in the input field below</li>
		<li>Click on the 'Continue' button</li>
		</ol>
		<b>Else</b> if your YaBB 1 Gold - SP 1.x forum is located on a different server than your YaBB 2 installation or if you dont know the path to your SP 1.x forum:
		<ol>
		<li>Copy all files in the /Boards, /Members, and /Messages folders from your YaBB 1 Gold - SP 1.x installation, to the corresponding Convert/Boards, 
		Convert/Members, and Convert/Messages folders of your YaBB 2 installation, and chmod them 777.</li>
		<li>Copy cat.txt from the /Variables folder of your YaBB 1 Gold - SP 1.x installation to the Convert/Variables folder of your YaBB 2 installation, and chmod it 666.</li>
		<li>If you have 'Add More Membergroups' installed on your YaBB 1 Gold - SP 1.x, copy MemberStats.txt from the /Variables folder of your YaBB 1 Gold - SP 1.x installation to the Convert/Variables folder of your YaBB 2 installation, and chmod it 666.</li>
		<li>Click on the 'Continue' button</li>
		</ol>
		<div style="width: 100%; text-align: center;">
		<b>Path to your YaBB 1 Gold - SP 1.x files: </b> <input type="text" name="convertdir" value="$convertdir" size="50" />
		</div>
		<br />
		</td>
	</tr>
	<tr valign="middle">
		<td width="100%" colspan="2" class="catbg" align="center">
		<input type="submit" value="Continue" />
		</td>
	</tr>
	</table>

</form>

</div>

~;

	$yyim    = "You are running the YaBB 2 Converter.";
	$yytitle = "YaBB 2 Converter";
	&SetupTemplate;
	exit;
}

if ($action eq "prepare") {
	if (-e "$vardir/Converter.lock") { &FoundLock; }

	&tempstarter;
	&UpdateCookie("delete");

	$username           = 'Guest';
	$iamguest           = '1';
	$iamadmin           = '';
	$iamgmod            = '';
	$password           = '';
	@settings           = ();
	@immessages         = ();
	$yyim               = "";
	$realname           = '';
	$realemail          = '';
	$ENV{'HTTP_COOKIE'} = '';
	$yyuname            = "";

	$convertdir = qq~$FORM{'convertdir'}~;

	if (!-d "$convertdir/Boards") { &setup_fatal_error("Directory: $convertdir/Boards", 1); }
	else { $convboardsdir = "$convertdir/Boards"; }
	if (!-e "$convertdir/Members/memberlist.txt") { &setup_fatal_error("Directory: $convertdir/Members", 1); }
	else { $convmemberdir = "$convertdir/Members"; }
	if (!-d "$convertdir/Messages") { &setup_fatal_error("Directory: $convertdir/Messages", 1); }
	else { $convdatadir = "$convertdir/Messages"; }
	if (!-e "$convertdir/Variables/cat.txt") { &setup_fatal_error("Directory: $convertdir/Variables", 1); }
	else { $convvardir = "$convertdir/Variables"; }

	my $filler  = q~                                                                               ~;
	my $setfile = << "EOF";
\$convertdir = qq~$convertdir~;
\$convboardsdir = qq~$convertdir/Boards~;
\$convmemberdir = qq~$convertdir/Members~;
\$convdatadir = qq~$convertdir/Messages~;
\$convvardir = qq~$convertdir/Variables~;

1;
EOF

	$setfile =~ s~(.+\;)\s+(\#.+$)~$1 . substr( $filler, 0, (70-(length $1)) ) . $2 ~gem;
	$setfile =~ s~(.{64,}\;)\s+(\#.+$)~$1 . "\n   " . $2~gem;
	$setfile =~ s~^\s\s\s+(\#.+$)~substr( $filler, 0, 70 ) . $1~gem;

	fopen(SETTING, ">$vardir/ConvSettings.txt");
	print SETTING $setfile;
	fclose(SETTING);

	$yymenu = qq~$NavLink1 | Members | Messages | Date & Time | Clean Up | Login~;

	$yymain = qq~
<div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">
	<table width="100%" cellspacing="1" cellpadding="4">
	<tr valign="middle">
		<td width="100%" colspan="2" class="titlebg" align="left">
		YaBB 2 Converter
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/thread.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left" style="font-size: 11px;">
		<br />
		<ul>
		<li>Board and Category info found in: <b>$convboardsdir</b></li>
		<li>Members info found in: <b>$convmemberdir</b></li>
		<li>Messages info found in: <b>$convdatadir</b></li>
		<li>cat.txt found in: <b>$convvardir</b></li>
		</ul>
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/info.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left" style="font-size: 11px;">
		Conversion can take a long time depending on the size of your forum (30 seconds to a couple hours). During this time it may look like your browser 
		has frozen, but please BE PATIENT.<br />
		Click on 'Boards & Categories' in the menu.
		</td>
	</tr>
	</table>
</div>
~;

	$yyim    = "You are running the YaBB 2 Converter.";
	$yytitle = "YaBB 2 Converter";
	&SetupTemplate;
	exit;
}

if ($action eq "cats") {
	if (-e "$vardir/Converter.lock") { &FoundLock; }

	&tempstarter;
	&PrepareConv;
	&GetCats;
	&CreateControl;
	&ConvertBoards;

	$yymenu = qq~Boards & Categories | $NavLink2 | Messages | Date & Time | Clean Up | Login~;

	$yymain = qq~
<div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">
	<table width="100%" cellspacing="1" cellpadding="4">
	<tr valign="middle">
		<td width="100%" colspan="2" class="titlebg" align="left">
		YaBB 2 Converter
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/thread.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left">
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Board and Category Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Member Conversion.</div>
		$ConvNotDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Message Conversion.</div>
		$ConvNotDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Date/time Conversion.</div>
		$ConvNotDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Final Cleanup.</div>
		$ConvNotDone
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/info.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left" style="font-size: 11px;">
		New forum.master file has been created.<br />
		New forum.control file has been created.<br />
		All dates in files have been converted to timestamps.
		</td>
	</tr>

	</table>
</div>

~;

	$yyim    = "You are running the YaBB 2 Converter.";
	$yytitle = "YaBB 2 Converter";
	&SetupTemplate;
	exit;
}

if ($action eq "members") {
	if (-e "$vardir/Converter.lock") { &FoundLock; }

	$lstart = 0;

	&tempstarter;
	&ConvertMembers;

	$yymenu = qq~Boards & Categories | Members | $NavLink3 | Date & Time | Clean Up | Login~;

	$yymain = qq~
<div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">
	<table width="100%" cellspacing="1" cellpadding="4">
	<tr valign="middle">
		<td width="100%" colspan="2" class="titlebg" align="left">
		YaBB 2 Converter
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/thread.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left">
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Board and Category Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Member Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Message Conversion.</div>
		$ConvNotDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Date/time Conversion.</div>
		$ConvNotDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Final Cleanup.</div>
		$ConvNotDone
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/info.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left" style="font-size: 11px;">
		New User data files have been created.<br />
		Password encryption is done for each user the first time he/she logs in.
		</td>
	</tr>
	</table>
</div>
~;

	if (-e "$vardir/fixusers.txt") {

		fopen(FIXUSER, "$vardir/fixusers.txt");
		my @fixed = <FIXUSER>;
		fclose(FIXUSER);

		$yymain .= qq~
<br />
<div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">
<table width="100%" cellspacing="1" cellpadding="4">
	<tr>
	<td align="left" class="windowbg" colspan="5">Member(s) with illegal username(s) were found and converted to legal name(s).</td>
	<tr>
	<td align="center" class="catbg">Invalid name</td>
	<td align="center" class="catbg">Fixed name</td>
	<td align="center" class="catbg">Reg. date</td>
	<td align="center" class="catbg">Displayed name</td>
	<td align="center" class="catbg">E-mail</td>
	</tr>
~;
		foreach $userfixed (@fixed) {
			chomp $userfixed;
			($inname, $fxname, $rgdate, $dspname, $tmail) = split(/\|/, $userfixed);
			$yymain .= qq~
	<tr>
	<td align="left" class="windowbg2">$inname</td>
	<td align="left" class="windowbg2">$fxname</td>
	<td align="left" class="windowbg2">$rgdate</td>
	<td align="left" class="windowbg2">$dspname</td>
	<td align="left" class="windowbg2">$tmail</td>
	</tr>
~;

			# unlink ("$vardir/fixusers.txt");

		}
		$yymain .= qq~
</table>
</div>
~;
	}

	$yyim    = "You are running the YaBB 2 Converter.";
	$yytitle = "YaBB 2 Converter";
	&SetupTemplate;
	exit;
}

if ($action eq "members2") {

	if (-e "$vardir/Converter.lock") { &FoundLock; }

	&tempstarter;

	$yymenu = qq~Boards & Categories | Members | Messages | Date & Time | Clean Up | Login~;

	$lstart += $INFO{'lstart'};
	$lleft = $INFO{'memleft'};
	$ltot  = $lstart + $lleft;
	if ($lstart) {
		$lwidth = int(($lstart / $ltot) * 100);
		$lpct   = qq~$lwidth%~;
		$lwidth .= qq~px~;
		$lbar = qq~<div style="position: relative; top: 0px; left: 0px; width: $lwidth; height: 10px; margin: 0px; background-color: #6699cc; border: 0px; font-size: 5px;">&nbsp;</div>~;

	}

	$yymain = qq~
<div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">
	<table width="100%" cellspacing="1" cellpadding="4">
	<tr valign="middle">
		<td width="100%" colspan="2" class="titlebg" align="left">
		YaBB 2 Converter
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/thread.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left">
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Board and Category Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Member Conversion.</div>
		<div style="float: left; width: 102px; height: 10px; margin: 1px; background-color: #dddddd; border: 1px black solid; font-size: 5px;">$lbar</div>
		<div style="float: left; width: 40px; height: 14px; text-align: right; color: #FF3333;">$lpct</div><br />
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Message Conversion.</div>
		$ConvNotDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Date/time Conversion.</div>
		$ConvNotDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Final Cleanup.</div>
		$ConvNotDone
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/info.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left" style="font-size: 11px;">
		<div id="memcontinued">
		To prevent server time-out due to the amount of members, conversion is split into more steps.<br />
		There are <b>$INFO{'memleft'}</b> left to convert.<br />
		If nothing happens in 5 seconds <a href="$set_cgi?action=members" onclick="clearMeminfo();">click here to continue</a>....
		</div>
		</td>
	</tr>
	</table>
</div>

<script type="text/javascript" language="JavaScript">
<!--
	var convtext = 'Converting - please wait\!';
	function clearMeminfo() {
		document.getElementById("memcontinued").innerHTML = convtext;
	}

	function membtick() {
		clearMeminfo();
		location.href="$set_cgi?action=members;lstart=$lstart";
	}

setTimeout("membtick()",3000)
// -->
</script>
~;

	$yyim    = "You are running the YaBB 2 Converter.";
	$yytitle = "YaBB 2 Converter";
	&SetupTemplate;
	exit;
}

if ($action eq "messages") {

	if (-e "$vardir/Converter.lock") { &FoundLock; }

	&tempstarter;
	&ConvertMessages;

	$yymenu = qq~Boards & Categories | Members | Messages | $NavLink4 | Clean Up | Login~;

	$yymain = qq~
<div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">
	<table width="100%" cellspacing="1" cellpadding="4">
	<tr valign="middle">
		<td width="100%" colspan="2" class="titlebg" align="left">
		YaBB 2 Converter
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/thread.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left">
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Board and Category Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Member Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Message Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Date/time Conversion.</div>
		$ConvNotDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Final Cleanup.</div>
		$ConvNotDone
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/info.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left" style="font-size: 11px;">
		New style message files have been created.
		</td>
	</tr>
	</table>
</div>

~;

	$yyim    = "You are running the YaBB 2 Converter.";
	$yytitle = "YaBB 2 Converter";
	&SetupTemplate;
	exit;
}

if ($action eq "messages2") {

	$next_count = $INFO{'count'};
	$tot_count  = $INFO{'totboard'};

	if (-e "$vardir/Converter.lock") { &FoundLock; }

	&tempstarter;

	if ($next_count) {
		$mwidth = int(($next_count / $tot_count) * 100);
		$mpct   = qq~$mwidth%~;
		$mwidth .= qq~px~;
		$mbar = qq~<div style="position: relative; top: 0px; left: 0px; width: $mwidth; height: 10px; margin: 0px; background-color: #6699cc; border: 0px; font-size: 5px;">&nbsp;</div>~;
	}

	$yymenu = qq~Boards & Categories | Members | Messages | Date & Time | Clean Up | Login~;

	$yymain = qq~
<div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">
	<table width="100%" cellspacing="1" cellpadding="4">
	<tr valign="middle">
		<td width="100%" colspan="2" class="titlebg" align="left">
		YaBB 2 Converter
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/thread.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left">
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Board and Category Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Member Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Message Conversion.</div>
		<div style="float: left; width: 102px; height: 10px; margin: 1px; background-color: #dddddd; border: 1px black solid; font-size: 5px;">$mbar</div>
		<div style="float: left; width: 40px; height: 14px; text-align: right; color: #FF3333;">$mpct</div><br />
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Date/time Conversion.</div>
		$ConvNotDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Final Cleanup.</div>
		$ConvNotDone
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/info.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left" style="font-size: 11px;">
		<div id="memcontinued">
		To prevent server timeout due to the amount of messages, conversion is split into more than one step.<br />
		If nothing happens in 5 seconds <a href="$set_cgi?action=messages;count=$next_count" onclick="clearMeminfo();">click here to continue</a>....
		</div>
		</td>
	</tr>
	</table>
</div>

<script type="text/javascript" language="JavaScript">
<!--
	var convtext = 'Converting - please wait\!';
	function clearMeminfo() {
		document.getElementById("memcontinued").innerHTML = convtext;
	}

	function membtick() {
		clearMeminfo();
		location.href="$set_cgi?action=messages;count=$next_count";
	}

setTimeout("membtick()",3000)
// -->
</script>
~;

	$yyim    = "You are running the YaBB 2 Converter.";
	$yytitle = "YaBB 2 Converter";
	&SetupTemplate;
	exit;
}

if ($action eq "dates") {

	if (-e "$vardir/Converter.lock") { &FoundLock; }

	&tempstarter;
	&ConvertTimeToString;

	$yymenu = qq~Boards & Categories | Members | Messages | Date & Time | $NavLink5 | Login~;

	$yymain = qq~
<div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">
	<table width="100%" cellspacing="1" cellpadding="4">
	<tr valign="middle">
		<td width="100%" colspan="2" class="titlebg" align="left">
		YaBB 2 Converter
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/thread.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left">
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Board and Category Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Member Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Message Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Date/time Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #bbbbbb;">Final Cleanup.</div>
		$ConvNotDone
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/info.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left" style="font-size: 11px;">
		New style timestamps have been created throughout the board. All old style dates have been converted.
		</td>
	</tr>
	</table>
</div>

~;

	$yyim    = "You are running the YaBB 2 Converter.";
	$yytitle = "YaBB 2 Converter";
	&SetupTemplate;
	exit;
}

if ($action eq "cleanup") {

	if (-e "$vardir/Converter.lock") { &FoundLock; }

	require "$sourcedir/System.pl";
	require "$boardsdir/forum.master";

	unless ($INFO{'part'}) {
		&MyThreadRecount;

		$yySetLocation = qq~$set_cgi?action=cleanup;part=BoardRecount~;
		&redirectexit;
	}

	if ($INFO{'part'} eq 'BoardRecount') {
		&BoardTotals("convert");
		foreach (keys %board) {
			&MyReCountTotals($_);
		}
		$yySetLocation = qq~$set_cgi?action=cleanup;part=RebuildMem~;
		&redirectexit;
	}

	&tempstarter;

	if ($INFO{'part'} eq 'RebuildMem') {

		# Rebuild Memberlist
		&MyMemberIndex;

		$forumstarttext = "";

		$setforumstart = &conv_stringtotime($forumstart);
		$firstmember   = &timetostring($firstforum);

		if ($setforumstart > $firstforum) {
			$setforumstart  = &timeformat($setforumstart);
			$firstmemberfmt = &timeformat($firstforum);
			$forumstarttext = qq~The Forum Start date is currently set to $setforumstart but the first member was registered $firstmemberfmt. We recommend you go to your 'Admin Center - Forum Settings' and change the Forum Start Date to $firstmember.<br /><br />~;
		}
	}

	$yymenu = qq~Boards & Categories | Members | Messages | Date & Time | Clean Up | $NavLink6~;

	$convtext = "";

	if (-e "Convert/Members/admin.dat") {
		$convtext = qq~After you have tested your forum and made sure everything was converted correctly you can go to your Admin Center and delete /Convert/Boards, /Convert/Members, /Convert/Messages and /Convert/Variables folders and their contents.<br /><br />~;
	}

	$yymain = qq~
<div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">
	<table width="100%" cellspacing="1" cellpadding="4">
	<tr valign="middle">
		<td width="100%" colspan="2" class="titlebg" align="left">
		YaBB 2 Converter
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/thread.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left">
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Board and Category Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Member Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Message Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Date/time Conversion.</div>
		$ConvDone
		<div style="float: left; width: 250px; height: 14px; color: #FF3333;">Final Cleanup.</div>
		$ConvDone
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/info.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left" style="font-size: 11px;">
		$forumstarttext
		$convtext
		We recommend you delete the file "$thisscript". This is to prevent someone else running the converter and damaging your files.<br />
		You may now login to your forum. Enjoy using YaBB 2!
		</td>
	</tr>
	</table>
</div>

~;
	&CreateLock;

	$yyim    = "You are running the YaBB 2 Converter.";
	$yytitle = "YaBB 2 Converter";
	&SetupTemplate;
	exit;
}

sub FoundLock {
	&tempstarter;

	$yymenu = qq~Boards & Categories | Members | Messages | Date & Time | Clean Up | Login~;

	$yymain = qq~
<div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">
	<table width="100%" cellspacing="1" cellpadding="4">
	<tr valign="middle">
		<td width="100%" colspan="2" class="titlebg" align="left">
		YaBB 2 Converter
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/info.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left" style="font-size: 11px;">
		Setup and Converter has already been run, attempting to run the converter will cause damage to your files.<br /><br />
		To run Setup again, remove the file "$vardir/Setup.lock" then re-visit this page.<br />
		To run Converter again, remove the file "$vardir/Converter.lock," then re-visit this page.
		</td>
	</tr>
	</table>
</div>
~;

	$yyim    = "YaBB 2 Converter has already been run.";
	$yytitle = "YaBB 2 Converter";
	&SetupTemplate;
	exit;
}

sub FoundLock2 {
	&tempstarter;

	$yymenu = qq~Boards & Categories | Members | Messages | Date & Time | Clean Up | Login~;

	$yymain = qq~
<div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">
	<table width="100%" cellspacing="1" cellpadding="4">
	<tr valign="middle">
		<td width="100%" colspan="2" class="titlebg" align="left">
		YaBB 2 Setup
		</td>
	</tr>
	<tr valign="middle">
		<td width="5%" class="windowbg" align="center">
		<img src="$imagesdir/info.gif" alt="" />
		</td>
		<td width="95%" class="windowbg2" align="left" style="font-size: 11px;">
		Setup has already been run, attempting to run Setup will cause damage to your files.<br /><br />
		To run Setup again, remove the file "$vardir/Setup.lock," then re-visit this page.
		</td>
	</tr>
	</table>
</div>

~;

	$yyim    = "YaBB 2 Setup has already been run.";
	$yytitle = "YaBB 2 Setup";
	&SetupTemplate;
	exit;
}

# Prepare Conversion ##

sub PrepareConv {
	opendir("BDIR", $boardsdir);
	@boardlist = readdir("BDIR");
	closedir("BDIR");
	foreach $file (@boardlist) {
		unless ($file eq ".htaccess" || $file eq "." || $file eq "..") { unlink "$boardsdir/$file"; }
	}
	opendir("MBDIR", $memberdir);
	@memblist = readdir("MBDIR");
	closedir("MBDIR");
	foreach $file (@memblist) {
		unless ($file eq ".htaccess" || $file eq "admin.vars" || $file eq "." || $file eq "..") { unlink "$memberdir/$file"; }
	}
	opendir("MSDIR", $datadir);
	@msglist = readdir("MSDIR");
	closedir("MSDIR");
	foreach $file (@msglist) {
		unless ($file eq ".htaccess" || $file eq "." || $file eq "..") { unlink "$datadir/$file"; }
	}
}

# / Prepare Conversion ##


# Board + Category Conversion ##

my (@categoryorder, @catboards, @catdata, @boarddata, @allboards);
my (%catinfo, %cat, %board, %boarddata);
my ($catfile, $boardfile, $key, $value, $cnt);

sub GetCats {
	fopen(VDIR, "$convvardir/cat.txt");
	@categoryorder = <VDIR>;
	fclose(VDIR);
	my @allboards;
	foreach $fcat (@categoryorder) {
		chomp $fcat;
		my @catboards;
		fopen(VCAT, "$convboardsdir/$fcat.cat");
		@catdata = <VCAT>;
		fclose(VCAT);
		chomp $catdata[0];
		chomp $catdata[1];
		$catinfo{$fcat} = qq~$catdata[0]|$catdata[1]|1~;

		for ($cnt = 2; $cnt <= $#catdata; $cnt++) {
			chomp $catdata[$cnt];
			unless (!$catdata[$cnt]) {
				push(@catboards, $catdata[$cnt]);
				push(@allboards, $catdata[$cnt]);
			}
		}
		$cat{$fcat} = join(',', @catboards);
	}
	foreach $fboard (@allboards) {
		chomp $fboard;
		fopen(VBRD, "$convboardsdir/$fboard.dat");
		@bdata = <VBRD>;
		fclose(VBRD);
		chomp $bdata[0];

		# get board access data
		if (-e "$convboardsdir/$fboard.mbo") {
			require "$convboardsdir/$fboard.mbo";
		}
		$viewperms      = "$view_groups{$fboard}";
		$visibletoall   = "$showprivboards{$fboard}";
		$board{$fboard} = qq~$bdata[0]|$viewperms|$visibletoall~;
	}
	fopen(FILE, ">$boardsdir/forum.master", 1);
	print FILE qq~\$mloaded = 1;\n~;
	print FILE qq~\@categoryorder = qw(@categoryorder);\n~;
	while (($key, $value) = each(%cat)) {
		# Strip membergroups with a ~ from them
		$value =~ s/\~//g;
		print FILE qq~\$cat{'$key'} = qq\~$value\~;\n~;
	}
	while (($key, $value) = each(%catinfo)) {
		# Strip membergroups with a ~ from them
		$value =~ s/\~//g;
		$value =~ s/\,/\, /g;
		print FILE qq~\$catinfo{'$key'} = qq\~$value\~;\n~;
	}
	while (($key, $value) = each(%board)) {
		# Strip membergroups with a ~ from them
		$value =~ s/\~//g;
		$value =~ s/\,/\, /g;
		print FILE qq~\$board{'$key'} = qq\~$value\~;\n~;
	}
	print FILE qq~\n1;~;
	fclose(FILE);
}

sub CreateControl {
	opendir("BDIR", $convboardsdir);
	@boardlist = grep { /\.dat$/ } readdir("BDIR");
	closedir("BDIR");
	foreach $file (@boardlist) {
		chomp $file;
		$foundboard = substr($file, 0, length($file) - 4);

		push(@boardfiles, $foundboard);

		# get category
		fopen("CINFO", "$convboardsdir/$foundboard.ctb");
		@category = <CINFO>;
		fclose("CINFO");
		chomp $category[0];
		$cntcat = $category[0];

		# get boardinfo
		fopen("BINFO", "$convboardsdir/$foundboard.dat");
		@boardinfo = <BINFO>;
		fclose("BINFO");
		chomp($boardinfo[0], $boardinfo[1], $boardinfo[2], $boardinfo[3]);

		$boardinfo[2] =~ /^\|(.*?)\|$/;
		$boardinfo[2] =~ s/\|(\S?)/,$1/g;
		$cntpic         = "";
		$cntdescription = $boardinfo[1];
		$cntmods        = $boardinfo[2];

		# get board access data
		if (-e "$convboardsdir/$foundboard.mbo") {
			require "$convboardsdir/$foundboard.mbo";
		}

		$cntstartperms = "$start_groups{$foundboard}";
		$cntreplyperms = "$reply_groups{$foundboard}";
		$cntpollperms  = "";
		$cntstartperms =~ s/\,/\, /g;
		$cntreplyperms =~ s/\,/\, /g;
		$cntpollperms  =~ s/\,/\, /g;
		$cntpic      = "$boardpic{$foundboard}";
		$cntzero     = "";
		$cntpassword = "";
		$cnttotals   = "";
		$cntattperms = "";
		$spare       = "";
		push(@boardcontrol, "$cntcat|$foundboard|$cntpic|$cntdescription|$cntmods|$cntmodgroups|$cntstartperms|$cntreplyperms|$cntpollperms|$cntzero|$cntpassword|$cnttotals|$cntattperms|$spare\n");
		# clean up
	}
	fopen("CONTROL", ">$boardsdir/forum.control");
	@boardcontrol = sort(@boardcontrol);
	print CONTROL @boardcontrol;
	fclose("CONTROL");
}

sub ConvertBoards {
	# converting board files and totals
	@stickies = ();
	if (fopen("DATADIR", "$convboardsdir/sticky.stk")) {
		@stickies = <DATADIR>;
		fclose("DATADIR");
	}
	opendir("DATADIR", $convboardsdir);
	@boards = grep { /\.txt$/ } readdir("DATADIR");
	closedir("DATADIR");

	foreach $file (@boards) {
		chomp $file;
		$foundfile = substr($file, 0, length($file) - 4);

		fopen(BOARDFILE, "$convboardsdir/$foundfile.txt") || &setup_fatal_error("$maintext_23 $convboardsdir/$foundfile.txt", 1);
		@boardfile = <BOARDFILE>;
		fclose(BOARDFILE);

		fopen(BOARDFILE, ">$boardsdir/$foundfile.txt") || &setup_fatal_error("$maintext_23 $boardsdir/$foundfile.txt", 1);
		foreach my $line (@boardfile) {
			chomp $line;
			my ($mnum, $msub, $mname, $memail, $mdate, $mreplies, $musername, $micon, $mstate) = split(/\|/, $line);
			if(!-e "$convdatadir/$mnum.txt") { next; }
			$mdate =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
			foreach $sticky (@stickies) {
				chomp $sticky;
				if ($sticky eq $mnum) { $mstate .= "s"; last; }
			}
			$mstate =~ s/1/l/g;
			print BOARDFILE "$mnum|$msub|$mname|$memail|$mdate|$mreplies|$musername|$micon|$mstate\n";
		}
		fclose(BOARDFILE);

		if (-e "$convboardsdir/$foundfile.ttl") {
			fopen(BOARDTTL, "$convboardsdir/$foundfile.ttl") || &setup_fatal_error("$maintext_23 $convboardsdir/$foundfile.ttl", 1);
			$boardttl = <BOARDTTL>;
			fclose(BOARDTTL);
			chomp $boardttl;
			my ($dummy1, $dummy2, $mdate, $dummy3) = split(/\|/, $boardttl);
			$mdate =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;

			fopen(BOARDTTL, ">$boardsdir/$foundfile.ttl") || &setup_fatal_error("$maintext_23 $boardsdir/$foundfile.ttl", 1);
			print BOARDTTL "$dummy1|$dummy2|$mdate|$dummy3\n";
			fclose(BOARDTTL);
		}
	}
}

# / Board + Category Conversion ##

# Member Conversion ##

sub ConvertMembers {
	if (-e "$vardir/fixusers.txt") { unlink "$vardir/fixusers.txt"; }

	fopen(MEMDIR, "$convmemberdir/memberlist.txt");
	@memlist = <MEMDIR>;
	fclose(MEMDIR);

	$prcontinue = 0;
	$lstart     = $INFO{'lstart'};
	$listcnt    = $#memlist - $lstart;

	if ($listcnt > $maxmemsteps) { $prstep = $maxmemsteps; $prcontinue = 1; }
	else { $prstep = $listcnt; }

	for ($i = 0; $i <= $prstep; $i++) {
		$file = $memlist[$lstart];
		chomp $file;
		$uname = $file;
		$lstart++;
		if(!-e "$convmemberdir/$uname.dat") { next; }
		if ($uname !~ /\A[0-9A-Za-z#+-\.@^_]+\Z/) {
			&IllegalUser($uname);
		} else {
			&MyUpdateUser($uname);
		}
	}
	$memleft = $#memlist - $lstart;
	if ($prcontinue) {
		$yySetLocation = qq~$set_cgi?action=members2;memleft=$memleft;lstart=$lstart~;
		&redirectexit;
	}
	if (-e "$convvardir/MemberStats.txt") { &groupconvert; }
}

sub IllegalUser {
	my $user = $_[0];

	my $fixeduser = $user;
	$fixeduser =~ s~[^/\\0-9A-Za-z#%+\,\-\ \.\:@^_]~~g;
	if ($fixeduser !~ /\A[0-9A-Za-z#+-\.@^_]+\Z/) { $fixeduser .= qq~_fix~; }
	$fixeduser = qq~$fixeduser.dat~;
	$fixeduser = &check_existence($memberdir, $fixeduser);
	$fixeduser =~ s/(\S+?)(\.\S+$)/$1/i;
	$fixeduser = qq~$fixeduser.vars~;
	$fixeduser = &check_existence($memberdir, $fixeduser);
	$fixeduser =~ s/(\S+?)(\.\S+$)/$1/i;

	open(LOADOLDUSER, "$convmemberdir/$user.dat");
	my @settings = <LOADOLDUSER>;
	close(LOADOLDUSER);

	for (my $cnt = 0; $cnt < @settings; $cnt++) {
		$settings[$cnt] =~ s/[\r\n]//g;
		chomp $settings[$cnt];
	}

	if($settings[14] =~ m~(\d{2})\/(\d{2})\/(\d{2,4}).*?(\d{2})\:(\d{2})\:(\d{2})~is) {
		$dr_month = $1;
		$dr_day = $2;
		$dr_year = $3;
		$dr_hour = $4;
		$dr_minute = $5;
		$dr_secund = $6;

		if($dr_month > 12) { $dr_month = 12; }
		if($dr_month < 1) { $dr_month = 1; }
		if($dr_day > 31) { $dr_day = 31; }
		if($dr_day < 1) { $dr_day = 1; }
		if(length($dr_year) > 2) { $dr_year = substr($dr_year , length($dr_year) - 2, 2); }
		if($dr_year < 90 && $dr_year > 20) { $dr_year = 90; }
		if($dr_year > 20 && $dr_year < 90) { $dr_year = 20; }
		if($dr_hour > 23) { $dr_hour = 23; }
		if($dr_minute > 59) { $dr_minute = 59; }
		if($dr_secund > 59) { $dr_secund = 59; }

		if($dr_month == 4 || $dr_month == 6 || $dr_month == 9 || $dr_month == 11) {
			$max_days = 30;
		}
		elsif($dr_month == 2 && $dr_year % 4 == 0) {
			$max_days = 29;
		}
		elsif($dr_month == 2 && $dr_year % 4 != 0) {
			$max_days = 28;
		}
		else {
			$max_days = 31;
		}
		if($dr_day > $max_days) { $dr_day = $max_days; }

		$settings[14] = qq~$dr_month/$dr_day/$dr_year $maintxt{'107'} $dr_hour:$dr_minute:$dr_secund~;
	}
	else { $settings[14] = "$forumstart"; }

	my @xtn = qw(msg ims imstore log outbox);
	for (my $cnt; $cnt < @xtn; $cnt++) {
		if (-e "$convmemberdir/$user.$xtn[$cnt]") {
			open(FILEUSER, "$convmemberdir/$user.$xtn[$cnt]");
			@divfiles = <FILEUSER>;
			close(FILEUSER);
			fopen(FILEUSER, ">$memberdir/$fixeduser.$xtn[$cnt]") || &setup_fatal_error("$maintext_23 $memberdir/$fixeduser.$xtn[$cnt]", 1);
			foreach $divlines (@divfiles) {
				chomp $divlines;
				print FILEUSER "$divlines\n";
			}
			fclose(FILEUSER);
		}
	}
	my $msnaddress = "";
	if (-e "$convmemberdir/$user.om") {
		open(MSNFILE, "$convmemberdir/$user.om");
		my @msnsettings = <MSNFILE>;
		close(MSNFILE);
		chomp $msnsettings[0];
		$msnaddress = $msnsettings[0];
	}

	my ($lastonline, $lastpost, $lastim);

	if (-e "$convmemberdir/$user.ll") {
		open(LLFILE, "$convmemberdir/$user.ll");
		($lastonline, $lastpost, $lastim) = <LLFILE>;
		close(LLFILE);
		chomp($lastonline, $lastpost, $lastim);
		$lastonline =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
		$lastpost =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
		$lastim =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
	}

	$regitime = "$settings[14]";
	$regitime =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;

	if ($default_template) { $new_template = $default_template; }
	else { $new_template = qq~Forum default~; }

	if ($settings[1] eq "") { $settings[1] = $user; }
	if($settings[5]) {
		$settings[5] =~ s/&&/&amp;&amp;/g;
		$settings[5] =~ s~\[size=(1|\-2)\]\n*(.*?)\n*\[/size\]~\[size=10\]$2\[/size\]~isg;
		$settings[5] =~ s~\[size=(2|\-1)\]\n*(.*?)\n*\[/size\]~\[size=13\]$2\[/size\]~isg;
		$settings[5] =~ s~\[size=(3)\]\n*(.*?)\n*\[/size\]~\[size=16\]$2\[/size\]~isg;
		$settings[5] =~ s~\[size=(4|\+1)\]\n*(.*?)\n*\[/size\]~\[size=18\]$2\[/size\]~isg;
		$settings[5] =~ s~\[size=(5|\+2)\]\n*(.*?)\n*\[/size\]~\[size=24\]$2\[/size\]~isg;
		$settings[5] =~ s~\[size=(6|\+3)\]\n*(.*?)\n*\[/size\]~\[size=32\]$2\[/size\]~isg;
		$settings[5] =~ s~\[size=(7|\+4)\]\n*(.*?)\n*\[/size\]~\[size=48\]$2\[/size\]~isg;
	}

	%{$uid.$fixeduser} = (
		'password'      => "$settings[0]",
		'realname'      => "$settings[1]",
		'email'         => "$settings[2]",
		'webtitle'      => "$settings[3]",
		'weburl'        => "$settings[4]",
		'signature'     => "$settings[5]",
		'postcount'     => "$settings[6]",
		'position'      => "$settings[7]",
		'icq'           => "$settings[8]",
		'aim'           => "$settings[9]",
		'yim'           => "$settings[10]",
		'gender'        => "$settings[11]",
		'usertext'      => "$settings[12]",
		'userpic'       => "$settings[13]",
		'regdate'       => "$settings[14]",
		'regtime'       => "$regitime",
		'location'      => "$settings[15]",
		'bday'          => "$settings[16]",
		'timeselect'    => "$settings[17]",
		'timeoffset'    => "$timeoffset",
		'hidemail'      => "$settings[19]",
		'msn'           => "$msnaddress",
		'gtalk'         => "$settings[32]",
		'template'      => "$new_template",
		'language'      => "$language",
		'lastonline'    => "$lastonline",
		'lastpost'      => "$lastpost",
		'lastim'        => "$lastim",
		'im_ignorelist' => "$settings[26]",
		'im_notify'     => "$settings[27]",
		'im_popup'      => "$settings[28]",
		'im_imspop'     => "$settings[29]",
		'cathide'       => "$settings[30]",
		'postlayout'    => "$settings[31]",
		'dsttimeoffset' => "$dstoffset",
		'pageindex'     => "1|1|1");

	my @tags = qw(password realname email regdate webtitle weburl signature postcount position addgroups icq aim yim gender usertext userpic regtime location bday timeselect timeoffset timeformat hidemail msn gtalk template language lastonline lastpost lastim im_ignorelist im_notify im_popup im_imspop cathide postlayout session sesquest sesanswer favorites dsttimeoffset pageindex);
	fopen(UPDATEUSER, ">$memberdir/$fixeduser.vars");
	print UPDATEUSER "### User variables for ID: $fixeduser ###\n\n";
	for (my $cnt = 0; $cnt < @tags; $cnt++) {
		print UPDATEUSER "\'$tags[$cnt]\'\,\"${$uid.$fixeduser}{$tags[$cnt]}\"\n";
	}
	fclose(UPDATEUSER);

	my @xtn = qw(msg ims imstore log outbox wlog);
	for (my $cnt; $cnt < @xtn; $cnt++) {
		if (-e "$memberdir/$user.$xtn[$cnt]") { rename "$memberdir/$user.$xtn[$cnt]", "$memberdir/$fixeduser.$xtn[$cnt]"; }
	}

	if (-e "$vardir/fixusers.txt") {
		fopen(FIXUSER, "$vardir/fixusers.txt");
		@userfixed = <FIXUSER>;
		fclose(FIXUSER);
	}
	fopen(FIXUSER, ">$vardir/fixusers.txt");
	for (my $z = 0; $z < @userfixed; $z++) {
		chomp $userfixed[$z];
		print FIXUSER "$userfixed[$z]\n";
	}
	print FIXUSER "$user|$fixeduser|$settings[14]|$settings[1]|$settings[2]\n";
	fclose(FIXUSER);
}

sub MyUpdateUser {
	my $user = $_[0];

	fopen(LOADOLDUSER, "$convmemberdir/$user.dat");
	my @settings = <LOADOLDUSER>;
	fclose(LOADOLDUSER);
	for (my $cnt = 0; $cnt < @settings; $cnt++) {
		$settings[$cnt] =~ s/[\r\n]//g;
		chomp $settings[$cnt];
	}

	my @xtn = qw(msg ims imstore log outbox);
	for (my $cnt; $cnt < @xtn; $cnt++) {
		if (-e "$convmemberdir/$user.$xtn[$cnt]") {
			fopen(FILEUSER, "$convmemberdir/$user.$xtn[$cnt]");
			@divfiles = <FILEUSER>;
			fclose(FILEUSER);
			fopen(FILEUSER, ">$memberdir/$user.$xtn[$cnt]") || &setup_fatal_error("$maintext_23 $memberdir/$user.$xtn[$cnt]", 1);
			foreach $divlines (@divfiles) {
				chomp $divlines;
				print FILEUSER "$divlines\n";
			}
			fclose(FILEUSER);
		}
	}
	my $msnaddress = "";
	if (-e "$convmemberdir/$user.om") {
		fopen(MSNFILE, "$convmemberdir/$user.om");
		my @msnsettings = <MSNFILE>;
		fclose(MSNFILE);
		chomp $msnsettings[0];
		$msnaddress = $msnsettings[0];
	}

	my ($lastonline, $lastpost, $lastim);

	if (-e "$convmemberdir/$user.ll") {
		fopen(LLFILE, "$convmemberdir/$user.ll");
		($lastonline, $lastpost, $lastim) = <LLFILE>;
		fclose(LLFILE);
		chomp($lastonline, $lastpost, $lastim);
		$lastonline =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
		$lastpost =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
		$lastim =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
	}

	if($settings[14] =~ m~(\d{2})\/(\d{2})\/(\d{2,4}).*?(\d{2})\:(\d{2})\:(\d{2})~is) {
		$dr_month = $1;
		$dr_day = $2;
		$dr_year = $3;
		$dr_hour = $4;
		$dr_minute = $5;
		$dr_secund = $6;

		if($dr_month > 12) { $dr_month = 12; }
		if($dr_month < 1) { $dr_month = 1; }
		if($dr_day > 31) { $dr_day = 31; }
		if($dr_day < 1) { $dr_day = 1; }
		if(length($dr_year) > 2) { $dr_year = substr($dr_year , length($dr_year) - 2, 2); }
		if($dr_year < 90 && $dr_year > 20) { $dr_year = 90; }
		if($dr_year > 20 && $dr_year < 90) { $dr_year = 20; }
		if($dr_hour > 23) { $dr_hour = 23; }
		if($dr_minute > 59) { $dr_minute = 59; }
		if($dr_secund > 59) { $dr_secund = 59; }

		if($dr_month == 4 || $dr_month == 6 || $dr_month == 9 || $dr_month == 11) {
			$max_days = 30;
		}
		elsif($dr_month == 2 && $dr_year % 4 == 0) {
			$max_days = 29;
		}
		elsif($dr_month == 2 && $dr_year % 4 != 0) {
			$max_days = 28;
		}
		else {
			$max_days = 31;
		}
		if($dr_day > $max_days) { $dr_day = $max_days; }

		$settings[14] = qq~$dr_month/$dr_day/$dr_year $maintxt{'107'} $dr_hour:$dr_minute:$dr_secund~;
	}
	else { $settings[14] = "$forumstart"; }

	$regitime = "$settings[14]";
	$regitime =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;

	if ($default_template) { $new_template = $default_template; }
	else { $new_template = qq~Forum default~; }

	if ($settings[1] eq "") { $settings[1] = $user; }

	if($settings[5]) {
		$settings[5] =~ s/&&/&amp;&amp;/g;
		$settings[5] =~ s~\[size=(1|\-2)\]\n*(.*?)\n*\[/size\]~\[size=10\]$2\[/size\]~isg;
		$settings[5] =~ s~\[size=(2|\-1)\]\n*(.*?)\n*\[/size\]~\[size=13\]$2\[/size\]~isg;
		$settings[5] =~ s~\[size=(3)\]\n*(.*?)\n*\[/size\]~\[size=16\]$2\[/size\]~isg;
		$settings[5] =~ s~\[size=(4|\+1)\]\n*(.*?)\n*\[/size\]~\[size=18\]$2\[/size\]~isg;
		$settings[5] =~ s~\[size=(5|\+2)\]\n*(.*?)\n*\[/size\]~\[size=24\]$2\[/size\]~isg;
		$settings[5] =~ s~\[size=(6|\+3)\]\n*(.*?)\n*\[/size\]~\[size=32\]$2\[/size\]~isg;
		$settings[5] =~ s~\[size=(7|\+4)\]\n*(.*?)\n*\[/size\]~\[size=48\]$2\[/size\]~isg;
	}

	%{$uid.$user} = (
		'password'      => "$settings[0]",
		'realname'      => "$settings[1]",
		'email'         => "$settings[2]",
		'webtitle'      => "$settings[3]",
		'weburl'        => "$settings[4]",
		'signature'     => "$settings[5]",
		'postcount'     => "$settings[6]",
		'position'      => "$settings[7]",
		'icq'           => "$settings[8]",
		'aim'           => "$settings[9]",
		'yim'           => "$settings[10]",
		'gender'        => "$settings[11]",
		'usertext'      => "$settings[12]",
		'userpic'       => "$settings[13]",
		'regdate'       => "$settings[14]",
		'regtime'       => "$regitime",
		'location'      => "$settings[15]",
		'bday'          => "$settings[16]",
		'timeselect'    => "$settings[17]",
		'timeoffset'    => "$timeoffset",
		'hidemail'      => "$settings[19]",
		'msn'           => "$msnaddress",
		'gtalk'         => "$settings[32]",
		'template'      => "$new_template",
		'language'      => "$language",
		'lastonline'    => "$lastonline",
		'lastpost'      => "$lastpost",
		'lastim'        => "$lastim",
		'im_ignorelist' => "$settings[26]",
		'im_notify'     => "$settings[27]",
		'im_popup'      => "$settings[28]",
		'im_imspop'     => "$settings[29]",
		'cathide'       => "$settings[30]",
		'postlayout'    => "$settings[31]",
		'dsttimeoffset' => "$dstoffset",
		'pageindex'     => "1|1|1");

	my @tags = qw(password realname email regdate webtitle weburl signature postcount position addgroups icq aim yim gender usertext userpic regtime location bday timeselect timeoffset timeformat hidemail msn gtalk template language lastonline lastpost lastim im_ignorelist im_notify im_popup im_imspop cathide postlayout session sesquest sesanswer favorites dsttimeoffset pageindex);
	fopen(UPDATEUSER, ">$memberdir/$user.vars");
	print UPDATEUSER "### User variables for ID: $user ###\n\n";
	for (my $cnt = 0; $cnt < @tags; $cnt++) {
		print UPDATEUSER "\'$tags[$cnt]\'\,\"${$uid.$user}{$tags[$cnt]}\"\n";
	}
	fclose(UPDATEUSER);
}

sub groupconvert {
	require "$convvardir/MemberStats.txt";
	my $i = 0;
	my $z = 1;
	undef %Post;

	$Post{'-1'} = qq~$MemStatNewbie|$MemStarNumNewbie|$MemStarPicNewbie|$MemTypeColNewbie|0|0|0|0|0|0~;

	while ($MemStat[$i]) {
		if ($MemPostNum[$i] eq "x") {
			$NoPost{$z} = qq~$MemStat[$i]|$MemStarNum[$i]|$MemStarPic[$i]|$MemTypeCol[$i]|0|0|0|0|0|0~;
			$z++;
		} else {
			$Post{ $MemPostNum[$i] } = qq~$MemStat[$i]|$MemStarNum[$i]|$MemStarPic[$i]|$MemTypeCol[$i]|0|0|0|0|0|0~;
		}
		$i++;
	}

	fopen(FILE, ">$vardir/membergroups.txt", 1);
	foreach my $key (keys %Group) {
		my $value = $Group{$key};
		print FILE qq~\$Group{'$key'} = '$value';\n~;
	}
	foreach my $key (keys %NoPost) {
		my $value = $NoPost{$key};
		print FILE qq~\$NoPost{'$key'} = '$value';\n~;
	}
	foreach my $key (keys %Post) {
		my $value = $Post{$key};
		print FILE qq~\$Post{'$key'} = '$value';\n~;
	}
	print FILE qq~\n1;~;
	fclose(FILE);
}

# / Member Conversion ##

# Message Conversion ##

sub ConvertMessages {
	# converting message files

	$board_count      = $INFO{'count'};
	$next_board_count = ($INFO{'count'} + 1);

	@stickies = ();
	if (fopen("DATADIR", "$convboardsdir/sticky.stk")) {
		@stickies = <DATADIR>;
		fclose("DATADIR");
	}

	opendir("BOARDSDIR", $convboardsdir);
	@messages = grep { /\.txt$/ } readdir(BOARDSDIR);
	closedir("BOARDSDIR");

	foreach $file (@messages) {

		if ($board_count) {
			$board_count--;
			next;
		}

		chomp $file;
		($foundfile, $ext) = split(/\./, $file);

		fopen("BRDFILE", "$convboardsdir/$foundfile.txt");
		@messagefile = <BRDFILE>;
		fclose("BRDFILE");

		foreach my $line (@messagefile) {
			chomp $line;
			my ($thread, undef) = split(/\|/, $line, 2);
			my $trstate = "";
			foreach $sticky (@stickies) {
				chomp $sticky;
				if ($sticky eq $thread) { $trstate .= "s"; last; }
			}
			$trstate =~ s/1/l/g;

			if (-e "$convdatadir/$thread.txt") {
				$views      = 1;
				$lastposter = "";

				fopen("MSGFILE", "$convdatadir/$thread.txt");
				@messagelines = <MSGFILE>;
				fclose("MSGFILE");

				fopen("MSGFILE", ">$datadir/$thread.txt");
				foreach my $msgline (@messagelines) {
					chomp $msgline;
					my ($subject, $name, $email, $mdate, $username, $icon, $dummy, $user_ip, $message, $ns, $editdate, $editby, $attach_url, $attachment) = split(/\|/, $msgline);

					$message  =~ s~\[quote(\s+author=(.*?)link=(.*?)\s+date=(.*?)\s*)?\]\n*(.*?)\n*\[/quote\]~&QuoteFix($2,$3,$4,$5)~eisg;
					$message  =~ s~\[glow=(.*?)\]\n*(.*?)\n*\[/glow\]~\[glb\]$2\[/glb\]~isg;
					$message  =~ s~\[shadow=(.*?)\]\n*(.*?)\n*\[/shadow\]~\[glb\]$2\[/glb\]~isg;
					$message  =~ s~\[size=(1|\-2)\]\n*(.*?)\n*\[/size\]~\[size=10\]$2\[/size\]~isg;
					$message  =~ s~\[size=(2|\-1)\]\n*(.*?)\n*\[/size\]~\[size=13\]$2\[/size\]~isg;
					$message  =~ s~\[size=(3)\]\n*(.*?)\n*\[/size\]~\[size=16\]$2\[/size\]~isg;
					$message  =~ s~\[size=(4|\+1)\]\n*(.*?)\n*\[/size\]~\[size=18\]$2\[/size\]~isg;
					$message  =~ s~\[size=(5|\+2)\]\n*(.*?)\n*\[/size\]~\[size=24\]$2\[/size\]~isg;
					$message  =~ s~\[size=(6|\+3)\]\n*(.*?)\n*\[/size\]~\[size=32\]$2\[/size\]~isg;
					$message  =~ s~\[size=(7|\+4)\]\n*(.*?)\n*\[/size\]~\[size=48\]$2\[/size\]~isg;
					$mdate    =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
					$editdate =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
					chomp $editdate;
					print MSGFILE "$subject|$name|$email|$mdate|$username|$icon|$dummy|$user_ip|$message|$ns|$editdate|$editby|$attachment\n";
					if($username ne "Guest") { &writerecentlog($thread, $username); }
					$lastpostdate = $mdate;

				}
				fclose("MSGFILE");

				if (-e "$convdatadir/$thread.data") {
					fopen(DATA, "$convdatadir/$thread.data");
					$data = <DATA>;
					fclose(DATA);
					chomp $data;
					($views, $lastposter) = split(/\|/, $data);
				}

				fopen(CTB, ">$datadir/$thread.ctb");
				print CTB "$foundfile\n\n";
				print CTB "$views\n";
				print CTB "$lastposter\n";
				print CTB "$lastpostdate\n";
				print CTB "$trstate\n";
				fclose(CTB);
			}
		}
		$totalbdr      = $#messages + 1;
		$yySetLocation = qq~$set_cgi?action=messages2;count=$next_board_count;totboard=$totalbdr~;
		&redirectexit;

	}
}

sub writerecentlog {
	my ($thread, $username) = @_;
	if (-e "$memberdir/$username.rlog") {
		fopen(RLOG, "$memberdir/$username.rlog");
		%recent = map /(.*)\t(.*)/, <RLOG>;
		fclose(RLOG);
	}
	unless (exists($recent{$thread})) {
		$recent{$thread} = 0;
	}
	$recent{$thread}++;
	fopen(RLOG, ">$memberdir/$username.rlog");
	print RLOG map "$_\t$recent{$_}\n", keys %recent;
	fclose(RLOG);
	undef %recent;
}

# / Message Conversion ##

# Date Conversion ##

sub ConvertTimeToString {

	# Convert Dates in IM's
	# Start with the inbox...
	opendir(MEMDIR, $convmemberdir);
	@inboxlist = grep { /\.msg$/ } readdir(MEMDIR);
	closedir(MEMDIR);
	foreach $file (@inboxlist) {
		chomp $file;
		open("INBOX", "$convmemberdir/$file") || &setup_fatal_error("$maintext_23 $convmemberdir/$file", 1);
		@messagefile = <INBOX>;
		close("INBOX");

		$file =~ s~[^/\\0-9A-Za-z#%+\,\-\ \.\:@^_]~~g;

		fopen("INBOX", ">$memberdir/$file") || &setup_fatal_error("$maintext_23 $memberdir/$file", 1);

		foreach $line (@messagefile) {
			chomp $line;
			($name, $subject, $date, $message, $id, $ip) = split(/\|/, $line);
			$date =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
			print INBOX "$name|$subject|$date|$message|$id|$ip\n";
		}
		fclose("INBOX");
	}

	# Now do the outbox..
	opendir(MEMDIR, $convmemberdir);
	@outboxlist = grep { /\.outbox$/ } readdir(MEMDIR);
	closedir(MEMDIR);
	foreach $file (@outboxlist) {
		chomp $file;
		open("OUTBOX", "$convmemberdir/$file") || &setup_fatal_error("$maintext_23 $convmemberdir/$file.outbox", 1);
		@messagefile = <OUTBOX>;
		close("OUTBOX");

		$file =~ s~[^/\\0-9A-Za-z#%+\,\-\ \.\:@^_]~~g;

		fopen("OUTBOX", ">$memberdir/$file") || &setup_fatal_error("$maintext_23 $memberdir/$file.outbox", 1);

		foreach $line (@messagefile) {
			chomp $line;
			($name, $subject, $date, $message, $id, $ip, $read_flag) = split(/\|/, $line);
			$date =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
			print OUTBOX "$name|$subject|$date|$message|$id|$ip|$read_flag\n";
		}
		fclose("OUTBOX");
	}

	# ...and the Polls
	opendir("DATADIR", $convdatadir);
	@polls = grep { /\.poll$/ } readdir(DATADIR);
	closedir("DATADIR");
	foreach $file (@polls) {
		chomp $file;
		fopen("POLLFILE", "$convdatadir/$file") || &setup_fatal_error("$maintext_23 $convdatadir/$file.poll", 1);
		@pollsfile = <POLLFILE>;
		fclose("POLLFILE");
		fopen("POLLFILE", ">$datadir/$file") || &setup_fatal_error("$maintext_23 $datadir/$file.poll", 1);
		chomp $pollsfile[0];
		my ($dummy1, $dummy2, $dummy3, $dummy4, $dummy5, $pdate, $dummy6, $dummy7, $dummy8, $epdate, $dummy10, $dummy11) = split(/\|/, $pollsfile[0]);
		$pdate  =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
		$epdate =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
		chomp $epdate;
		print POLLFILE "$dummy1|$dummy2|$dummy3|$dummy4|$dummy5|$pdate|$dummy6|$dummy7|$dummy8|$epdate|$dummy10|$dummy11\n";

		for ($i = 1; $i < @pollsfile; $i++) {
			chomp $pollsfile[$i];
			print POLLFILE "$pollsfile[$i]\n";
		}
		fclose("POLLFILE");
	}
	opendir("DATADIR", $convdatadir);
	@polled = grep { /\.polled$/ } readdir(DATADIR);
	closedir("DATADIR");
	foreach $file (@polled) {
		chomp $file;
		fopen("POLLEDFILE", "$convdatadir/$file") || &setup_fatal_error("$maintext_23 $convdatadir/$file.polled", 1);
		@polledfile = <POLLEDFILE>;
		fclose("POLLEDFILE");
		fopen("POLLEDFILE", ">$datadir/$file") || &setup_fatal_error("$maintext_23 $datadir/$file.polled", 1);
		foreach $line (@polledfile) {
			chomp $line;
			my ($dummy1, $dummy2, $dummy3, $pdate) = split(/\|/, $line);
			$pdate =~ s~(\d{2}\/\d{2}\/\d{2,4}).*?(\d{2}\:\d{2}\:\d{2})~&conv_stringtotime("$1 at $2")~eis;
			print POLLEDFILE "$dummy1|$dummy2|$dummy3|$pdate\n";
		}
		fclose("POLLEDFILE");
	}
}

sub QuoteFix {
	my ($qauthor, $qlink, $qdate, $qmessage) = @_;
	$qmessage =~ s~<br>~<br />~ig;
	if ($qauthor eq "" || $qlink eq "" || $qdate eq "") {
		$_ = qq~[quote]QUOTE[/quote]~;
	} else {
		$qdate = &conv_stringtotime($qdate);
		($dummy, $threadlink, $start) = split(/\;/, $qlink);
		($dummy, $start) = split(/=/, $start);
		($dummy, $num)   = split(/=/, $threadlink);
		$_ = qq~[quote author=AUTHOR link=QUOTELINK date=DATE]QUOTE[/quote]~;
	}
	$_ =~ s~AUTHOR~$qauthor~g;
	$_ =~ s~QUOTELINK~$num/$start~g;
	$_ =~ s~DATE~$qdate~g;
	$_ =~ s~QUOTE~$qmessage~g;
	return $_;
}

# / Date Conversion ##

# Cleanup ##

sub MyMemberIndex {
	$firstforum     = stringtotime($forumstart);
	$tmp_firstforum = $firstforum;
	$siglength = 200;

	opendir(MEMBERS, $memberdir);
	@contents = grep { /\.vars$/ } readdir(MEMBERS);
	closedir(MEMBERS);
	foreach $member (@contents) {
		$member =~ s/\.vars$//g;
		if ($member) {
			chomp $member;
			fopen(CHECKUSER, "$memberdir/$member.vars");
			my @settings = <CHECKUSER>;
			fclose(CHECKUSER);
			my $labelsfound = 0;
			$position = "";
			foreach my $setting (@settings) {
				chomp $setting;
				$setting =~ m/\'(.+?)\'\,\"(.+?)\"/ig;
				my $tag   = $1;
				my $value = $2;
				if    ($tag eq "realname")  { $realname  = $value;               $labelsfound++; }
				elsif ($tag eq "email")     { $email     = $value;               $labelsfound++; }
				elsif ($tag eq "regdate")   { $regtime   = stringtotime($value); $labelsfound++; }
				elsif ($tag eq "postcount") { $postcount = $value;               $labelsfound++; }
				elsif ($tag eq "position")  { $position  = $value;               $labelsfound++; }
				elsif ($tag eq "signature") { $signature = $value;               $labelsfound++; }
				if ($regtime && $firstforum > $regtime) { $firstforum = $regtime; }
				if (length($signature) > $siglength) { $siglength = length($signature); }
				if ($labelsfound == 6) { last; }
			}
			if (!$position) { $position = &MyMemberPostGroup($postcount); }
			$formatregdate = sprintf("%010d", $regtime);
			$memberlist{$member} = qq~$formatregdate~;
			$memberinf{$member}  = qq~$realname\|$email\|$position\|$postcount~;
		}
	}

	&ManageMemberlist("save");
	&ManageMemberinfo("save");

	fopen(MEMBERLISTREAD, "$memberdir/memberlist.txt");
	my @num = <MEMBERLISTREAD>;
	fclose(MEMBERLISTREAD);
	($latestmember, undef) = split(/\t/, $num[$#num], 2);
	my $membertotal = $#num + 1;
	undef @num;
	fopen(MEMTTL, ">$memberdir/members.ttl");
	print MEMTTL qq~$membertotal|$latestmember~;
	fclose(MEMTTL);

	if ($tmp_firstforum > $firstforum || $siglength > 200) { &SetInstall2; }
	return 0;
}

sub MyMemberPostGroup {
	$userpostcnt = $_[0];
	$grtitle     = "";
	foreach $postamount (sort { $b <=> $a } keys %Post) {
		if ($userpostcnt > $postamount) {
			($grtitle, undef) = split(/\|/, $Post{$postamount}, 2);
			last;
		}
	}
	return $grtitle;
}

sub MyThreadRecount {
	opendir(DIRECTORY, "$datadir");
	while ($file = readdir(DIRECTORY)) {
		next unless grep { /\.txt$/ } $file;
		($filename, $fileext) = split(/\./, $file);
		fopen(MSG, "$datadir/$filename.txt");
		@messages = <MSG>;
		fclose(MSG);
		@lastmessage = split(/\|/, $messages[$#messages]);
		&MessageTotals("load", $filename);
		${$filename}{'replies'} = $#messages;
		${$filename}{'lastposter'} = $lastmessage[4] eq "Guest" ? qq~Guest-$lastmessage[1]~ : $lastmessage[4];
		&MessageTotals("update", $filename);
	}
	closedir(DIRECTORY);
}

sub MyReCountTotals {
	my $cntboard = $_[0];
	unless ($cntboard) { return undef; }
	my (@threads, $threadcount, $messagecount, $i, $threadline);
	fopen(BOARD, "$boardsdir/$cntboard.txt");
	@threads = <BOARD>;
	fclose(BOARD);
	$threadcount  = @threads;
	$messagecount = $threadcount;
	if ($threadcount) {

		for ($i = 0; $i < @threads; $i++) {
			@threadline = split(/\|/, $threads[$i]);
			$messagecount += $threadline[5];
		}
	}
	&BoardTotals("load", $cntboard);
	${ $uid . $cntboard }{'threadcount'}  = $threadcount;
	${ $uid . $cntboard }{'messagecount'} = $messagecount;
	&BoardTotals("update", $cntboard);
	&BoardSetLastInfo($cntboard);
}

# / Cleanup ##

sub conv_stringtotime {
	unless ($_[0]) { return 0; }
	$splitvar = $_[0];
	$splitvar =~ m~(\d{2})\/(\d{2})\/(\d{2,4}).*?(\d{2})\:(\d{2})\:(\d{2})~;
	$amonth = int($1) || 1;
	$aday   = int($2) || 1;
	$ayear  = int($3) || 0;
	$ahour  = int($4) || 0;
	$amin   = int($5) || 0;
	$asec   = int($6) || 0;

	if    ($ayear >= 36 && $ayear <= 99) { $ayear += 1900; }
	elsif ($ayear >= 00 && $ayear <= 35) { $ayear += 2000; }
	if    ($ayear < 1904) { $ayear = 1904; }
	elsif ($ayear > 2036) { $ayear = 2036; }

	if    ($amonth < 1)  { $amonth = 0; }
	elsif ($amonth > 12) { $amonth = 11; }
	else { --$amonth; }

	if($amonth == 3 || $amonth == 5 || $amonth == 8 || $amonth == 10) { $max_days = 30; }
	elsif($amonth == 1 && $ayear % 4 == 0) { $max_days = 29; }
	elsif($amonth == 1 && $ayear % 4 != 0) { $max_days = 28; }
	else { $max_days = 31; }
	if($aday > $max_days) { $aday = $max_days; }

	if    ($ahour < 1)  { $ahour = 0; }
	elsif ($ahour > 23) { $ahour = 23; }
	if    ($amin < 1)   { $amin  = 0; }
	elsif ($amin > 59)  { $amin  = 59; }
	if    ($asec < 1)   { $asec  = 0; }
	elsif ($asec > 59)  { $asec  = 59; }

	return (timelocal($asec, $amin, $ahour, $aday, $amonth, $ayear));
}

sub setup_fatal_error {

	my $e = $_[0];
	my $v = $_[1];
	$e .= "\n";
	if ($v) { $e .= $! . "\n"; }

	$yymenu = qq~Boards & Categories | ~;
	$yymenu .= qq~Members | ~;
	$yymenu .= qq~Messages | ~;
	$yymenu .= qq~Date & Time | ~;
	$yymenu .= qq~Clean Up | ~;
	$yymenu .= qq~Login~;

	&tempstarter;

	$yymain .= qq~
<table border="0" width="80%" cellspacing="1" class="bordercolor" align="center" cellpadding="4">
  <tr>
    <td class="titlebg"><span class="text1"><b>$admin_txt106</b></span></td>
  </tr><tr>
    <td class="windowbg"><br /><span class="text1">$e</span><br /><br /></td>
  </tr>
</table>
<center><br /><a href="javascript:history.go(-1)">$admin_txt193</a></center>
~;
	$yyim    = "YaBB 2 Convertor Error.";
	$yytitle = "YaBB 2 Convertor Error.";
	&SetupTemplate;
	exit;
}

if (!-e "$vardir/Setup.lock" && !$action) {
	$rand_integer   = int(rand(99999));
	$rand_cook_user = "Y2User-$rand_integer";
	$rand_cook_pass = "Y2Pass-$rand_integer";
	$rand_cook_sess = "Y2Sess-$rand_integer";

	open(COOKFILE, ">$vardir/cook.txt");
	print COOKFILE "$rand_cook_user\n";
	print COOKFILE "$rand_cook_pass\n";
	print COOKFILE "$rand_cook_sess\n";
	close(COOKFILE);

	&adminlogin;
}

if (!-e "$vardir/Setup.lock") {

	open(COOKFILE, "$vardir/cook.txt");
	@cookinfo = <COOKFILE>;
	close(COOKFILE);

	chomp $cookinfo[0];
	$cookieusername = "$cookinfo[0]";
	chomp $cookinfo[1];
	$cookiepassword = "$cookinfo[1]";
	chomp $cookinfo[2];
	$cookiesession_name = "$cookinfo[2]";
}

if (!-e "$vardir/Setup.lock" && $action eq "adminlogin2") { &adminlogin2; }
if (!-e "$vardir/Setup.lock" && $action eq "setup1")      { &autoconfig; }
if (!-e "$vardir/Setup.lock" && $action eq "setup2")      {
	&BrdInstall;
	&MemInstall;
	&MesInstall;
	&VarInstall;
	&save_paths;
}
if (!-e "$vardir/Setup.lock" && $action eq "setinstall") { &SetInstall2; &SetInstall; }
if (!-e "$vardir/Setup.lock" && $action eq "setinstall2") { &SetInstall2; }
if (!-e "$vardir/Setup.lock" && $action eq "setup3")      { &CheckInstall; }
if (!-e "$vardir/Setup.lock" && $action eq "ready")       { &ready; }

$yymain = "End of script reached without action: $action";
&Output2;
exit;

sub adminlogin {

	if (-e "$vardir/Setup.lock") {
		&FoundLock2;
	}

	$yymain .= qq~
	<br /><br /><br /><form action="$set_cgi?action=adminlogin2" method="post"><center>
	<table width="20%" border="0" bgcolor= "#000000" cellspacing="1" cellpadding="0">
	<tr><td>
	<table width="100%" border="0" bgcolor= "$windowbg" cellspacing="1" cellpadding="3">
	<tr>
		<td width="100%" align="center">
		<span style="font-family: Arial; font-size: 13px; color: #000000;">
		Enter the password for user <b>admin</b><br />to gain access to the Setup Utility
		</span>
		</td>
	</tr>
	<tr>
		<td width="100%" align="center">
		<span style="font-family: Arial; font-size: 13px; color: #000000;">
		<input type="password" size="30" name="password" />
		<input type="hidden" name="username" value="admin" />
		<input type="hidden" name="cookielength" value="1500" />
		</span>
		</td>
	</tr>
	<tr>
		<td width="100%" align="center">
		<span style="font-family: Arial; font-size: 13px; color: #000000;">
		<input type="submit" value="Submit" />
		</span>
		</td>
	</tr>
	</table>
	</td></tr></table></center></form>
	~;
	&Output2;
	exit;
}

sub adminlogin2 {

	if (-e "$vardir/Setup.lock") { &FoundLock2; }

	if ($FORM{'password'} eq "") { $yymain = "Setup Error: You should fill in your password!"; &Output2; }
	$username = $FORM{'username'};
	if (-e ("$memberdir/$username.dat") || -e ("$memberdir/$username.vars")) {
		$Group{'Administrator'} = "YaBB Administrator|5|staradmin.gif|red|0|0|0|0|0|0";
		&LoadUser($username);
		my $spass = ${ $uid . $username }{'password'};
		$cryptpass = &encode_password($FORM{'password'});
		if ($spass ne $cryptpass && $spass ne $FORM{'password'}) { $yymain = "Setup Error: Login Failed!"; &Output2; }

	} else {
		$yymain = "Setup Error: Could not find the admin data file! Please check your access rights.";
		&Output2;
	}

	if ($FORM{'cookielength'} < 1 || $FORM{'cookielength'} > 9999) { $FORM{'cookielength'} = $Cookie_Length; }
	if (!$FORM{'cookieneverexp'}) { $ck{'len'} = "\+$FORM{'cookielength'}m"; }
	else { $ck{'len'} = 'Sunday, 17-Jan-2038 00:00:00 GMT'; }
	$password = &encode_password("$FORM{'password'}");
	${ $uid . $username }{'session'} = &encode_password($user_ip);
	chomp ${ $uid . $username }{'session'};

	&UpdateCookie("write", "$username", "$password", "${$uid.$username}{'session'}", "/", "$ck{'len'}");
	&LoadUserSettings;
	$yymain .= qq~
	<br /><br /><br /><form action="$set_cgi?action=setup1" method="post"><center>
	<table width="50%" border="0" bgcolor= "#000000" cellspacing="1" cellpadding="0">
	<tr><td>
	<table width="100%" border="0" bgcolor= "$windowbg" cellspacing="1" cellpadding="3">
	<tr>
		<td width="100%" align="center">
		<span style="font-family: Arial; font-size: 13px; color: #000000;">
		You are now logged in, $settings[1]!<br />Click Continue to proceed with the Setup.
		</span>
		</td>
	</tr>
	<tr>
		<td width="100%" align="center">
		<span style="font-family: Arial; font-size: 13px; color: #000000;">
		<input type="submit" value="Continue Set Up" />
		</span>
		</td>
	</tr>
	</table>
	</td></tr></table></center></form>
	~;

	&Output2;
	exit;
}

sub Output2 {

	if ($yySetCookies1 || $yySetCookies2 || $yySetCookies3) {

		print header(
			-status  => '200 OK',
			-cookie  => [$yySetCookies1, $yySetCookies2, $yySetCookies3],
			-charset => $yycharset);
	} else {
		print header(
			-status  => '200 OK',
			-charset => $yycharset);
	}

	print qq~
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title>YaBB 2 Setup</title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1" />
</head>
<body>

<!-- Main Content -->
<div style="height: 40px;">&nbsp;</div>
$yymain<br />

</body>
</html>
~;
	exit;
}

sub autoconfig {

	if (-e "$vardir/Setup.lock") { &FoundLock2; }

	&LoadCookie;          # Load the user's cookie (or set to guest)
	&LoadUserSettings;
	if (!$iamadmin) { $yymain = qq~Setup Error: You have no access rights to this function. Only user "admin" has if logged in!~; &Output2; }
	# do some fancy auto sensing
	$template   = "default";
	$forumstyle = "Forum";
	$adminstyle = "Admin";
	$yabbfiles  = "yabbfiles";

	# find the script url
	# Getting the last known url one way or another
	if ($ENV{HTTP_REFERER}) {
		$tempboardurl = $ENV{HTTP_REFERER};
	} elsif ($ENV{HTTP_HOST} && $ENV{REQUEST_URI}) {
		$tempboardurl = qq~http://$ENV{HTTP_HOST}$ENV{REQUEST_URI}~;
	}
	$lastslash = rindex($tempboardurl, "/");
	$foundboardurl = substr($tempboardurl, 0, $lastslash);

	## find the webroot ##
	if ($ENV{'SERVER_SOFTWARE'} =~ /IIS/) {
		$this_script = "$ENV{'SCRIPT_NAME'}";
		$_           = $0;
		s~\\~/~g;
		s~$this_script~~;
		$searchroot = $_ . '/';
	} else {
		$searchroot = $ENV{'DOCUMENT_ROOT'};
		s~\\~/~g;
	}
	$firstslash = index($tempboardurl, "/", 8);
	$html_baseurl = substr($tempboardurl, 0, $firstslash);

	# try to find the yabb html basedir directly
	if (-d "$searchroot/$yabbfiles") {
		$fnd_html_root = "$html_baseurl/$yabbfiles";
		$fnd_htmldir   = "$searchroot/$yabbfiles";
		$fnd_htmldir =~ s~//~/~g;
		opendir(HTMLDIR, "$fnd_htmldir");
		@contents = readdir(HTMLDIR);
		closedir(HTMLDIR);
		foreach $name (@contents) {
			chomp $name;
			if (lc($name) eq "avatars"     && (-d "$fnd_htmldir/$name"))             { $fnd_facesdir       = "$fnd_htmldir/$name";             $fnd_facesurl       = "$fnd_html_root/$name"; }
			if (lc($name) eq "modimages"   && (-d "$fnd_htmldir/$name"))             { $fnd_modimgdir      = "$fnd_htmldir/$name";             $fnd_modimgurl      = "$fnd_html_root/$name"; }
			if (lc($name) eq "templates"   && (-d "$fnd_htmldir/$name/$forumstyle")) { $fnd_forumstylesdir = "$fnd_htmldir/$name/$forumstyle"; $fnd_forumstylesurl = "$fnd_html_root/$name/$forumstyle"; }
			if (lc($name) eq "templates"   && (-d "$fnd_htmldir/$name/$adminstyle")) { $fnd_adminstylesdir = "$fnd_htmldir/$name/$adminstyle"; $fnd_adminstylesurl = "$fnd_html_root/$name/$adminstyle"; }
			if (lc($name) eq "smilies"     && (-d "$fnd_htmldir/$name"))             { $fnd_smiliesdir     = "$fnd_htmldir/$name";             $fnd_smiliesurl     = "$fnd_html_root/$name"; }
			if (lc($name) eq "attachments" && (-d "$fnd_htmldir/$name"))             { $fnd_uploaddir      = "$fnd_htmldir/$name";             $fnd_uploadurl      = "$fnd_html_root/$name"; }
			if (-d "$fnd_forumstylesdir/$template") { $fnd_imagesdir   = "$fnd_forumstylesurl/$template"; }
			if (-e "$fnd_htmldir/ubbc.js")          { $fnd_ubbcjspath  = "$fnd_html_root/ubbc.js"; }
			if (-e "$fnd_htmldir/fader.js")         { $fnd_faderpath   = "$fnd_html_root/fader.js"; }
			if (-e "$fnd_htmldir/yabbc.js")         { $fnd_yabbcjspath = "$fnd_html_root/yabbc.js"; }
			if (-e "$fnd_htmldir/post.js")          { $fnd_postjspath  = "$fnd_html_root/post.js"; }
		}
	} else {
		opendir(HTMLDIR, "$searchroot");
		@contents = readdir(HTMLDIR);
		closedir(HTMLDIR);
		foreach $name (@contents) {
			chomp $name;
			if (-d "$searchroot/$name") {
				opendir(HTMLDIR, "$searchroot/$name");
				@subcontents = readdir(HTMLDIR);
				closedir(HTMLDIR);
				foreach $subname (@subcontents) {
					chomp $subname;
					if (lc($subname) eq lc($yabbfiles) && (-d "$searchroot/$name/$subname")) {
						$fnd_htmldir = "$searchroot/$name/$subname";
						$fnd_htmldir =~ s~//~/~g;
						$fnd_html_root = "$html_baseurl/$name/$subname";
					}
				}
			}
		}
		opendir(HTMLDIR, "$fnd_htmldir");
		@tcontents = readdir(HTMLDIR);
		closedir(HTMLDIR);
		foreach $tname (@tcontents) {
			chomp $tname;
			if (lc($tname) eq "avatars"     && (-d "$fnd_htmldir/$tname"))             { $fnd_facesdir       = "$fnd_htmldir/$tname";             $fnd_facesurl       = "$fnd_html_root/$tname"; }
			if (lc($tname) eq "modimages"   && (-d "$fnd_htmldir/$tname"))             { $fnd_modimgdir      = "$fnd_htmldir/$tname";             $fnd_modimgurl      = "$fnd_html_root/$tname"; }
			if (lc($tname) eq "templates"   && (-d "$fnd_htmldir/$tname/$forumstyle")) { $fnd_forumstylesdir = "$fnd_htmldir/$tname/$forumstyle"; $fnd_forumstylesurl = "$fnd_html_root/$tname/$forumstyle"; }
			if (lc($tname) eq "templates"   && (-d "$fnd_htmldir/$tname/$adminstyle")) { $fnd_adminstylesdir = "$fnd_htmldir/$tname/$adminstyle"; $fnd_adminstylesurl = "$fnd_html_root/$tname/$adminstyle"; }
			if (lc($tname) eq "smilies"     && (-d "$fnd_htmldir/$tname"))             { $fnd_smiliesdir     = "$fnd_htmldir/$tname";             $fnd_smiliesurl     = "$fnd_html_root/$tname"; }
			if (lc($tname) eq "attachments" && (-d "$fnd_htmldir/$tname"))             { $fnd_uploaddir      = "$fnd_htmldir/$tname";             $fnd_uploadurl      = "$fnd_html_root/$tname"; }
			if (-d "$fnd_forumstylesdir/$template") { $fnd_imagesdir   = "$fnd_forumstylesurl/$template"; }
			if (-e "$fnd_htmldir/ubbc.js")          { $fnd_ubbcjspath  = "$fnd_html_root/ubbc.js"; }
			if (-e "$fnd_htmldir/fader.js")         { $fnd_faderpath   = "$fnd_html_root/fader.js"; }
			if (-e "$fnd_htmldir/yabbc.js")         { $fnd_yabbcjspath = "$fnd_html_root/yabbc.js"; }
			if (-e "$fnd_htmldir/post.js")          { $fnd_postjspath  = "$fnd_html_root/post.js"; }
		}
	}
	$fnd_boardurl = $foundboardurl;
	$fnd_boarddir = ".";
	if (-d "$fnd_boarddir/Boards")    { $fnd_boardsdir    = "$fnd_boarddir/Boards"; }
	if (-d "$fnd_boarddir/Messages")  { $fnd_datadir      = "$fnd_boarddir/Messages"; }
	if (-d "$fnd_boarddir/Members")   { $fnd_memberdir    = "$fnd_boarddir/Members"; }
	if (-d "$fnd_boarddir/Sources")   { $fnd_sourcedir    = "$fnd_boarddir/Sources"; }
	if (-d "$fnd_boarddir/Admin")     { $fnd_admindir     = "$fnd_boarddir/Admin"; }
	if (-d "$fnd_boarddir/Variables") { $fnd_vardir       = "$fnd_boarddir/Variables"; }
	if (-d "$fnd_boarddir/Languages") { $fnd_langdir      = "$fnd_boarddir/Languages"; }
	if (-d "$fnd_boarddir/Help")      { $fnd_helpfile     = "$fnd_boarddir/Help"; }
	if (-d "$fnd_boarddir/Templates") { $fnd_templatesdir = "$fnd_boarddir/Templates"; }

	if (-e "./Paths.pl") {
		require "./Paths.pl";
	}

	unless ($lastsaved) {
		$boardurl       = $fnd_boardurl;
		$boarddir       = $fnd_boarddir;
		$datadir        = $fnd_datadir;
		$boardsdir      = $fnd_boardsdir;
		$htmldir        = $fnd_htmldir;
		$html_root      = $fnd_html_root;
		$memberdir      = $fnd_memberdir;
		$sourcedir      = $fnd_sourcedir;
		$admindir       = $fnd_admindir;
		$vardir         = $fnd_vardir;
		$langdir        = $fnd_langdir;
		$helpfile       = $fnd_helpfile;
		$templatesdir   = $fnd_templatesdir;
		$forumstylesdir = $fnd_forumstylesdir;
		$forumstylesurl = $fnd_forumstylesurl;
		$adminstylesdir = $fnd_adminstylesdir;
		$adminstylesurl = $fnd_adminstylesurl;
		$facesdir       = $fnd_facesdir;
		$facesurl       = $fnd_facesurl;
		$smiliesdir     = $fnd_smiliesdir;
		$smiliesurl     = $fnd_smiliesurl;
		$modimgdir      = $fnd_modimgdir;
		$modimgurl      = $fnd_modimgurl;
		$uploaddir      = $fnd_uploaddir;
		$uploadurl      = $fnd_uploadurl;
		$ubbcjspath     = $fnd_ubbcjspath;
		$faderpath      = $fnd_faderpath;
		$yabbcjspath    = $fnd_yabbcjspath;
		$postjspath     = $fnd_postjspath;
	}

	# Simple output of env variables, for troubleshooting
	if    ($ENV{'SCRIPT_FILENAME'} ne "") { $support_env_path = $ENV{'SCRIPT_FILENAME'}; }
	elsif ($ENV{'PATH_TRANSLATED'} ne "") { $support_env_path = $ENV{'PATH_TRANSLATED'}; }

	# Remove Setupl.pl and cgi - and also nph- for buggy IIS.
	$support_env_path =~ s~(nph-)?Setup.(pl|cgi)~~ig;
	$support_env_path =~ s~\/\Z~~;

	# replace \'s with /'s for Windows Servers
	$support_env_path =~ s~\\~/~g;


	# Generate Screen

	if (-e "$langdir/$language/Main.lng") {
		require "$langdir/$use_lang/Main.lng";
	} elsif (-e "$langdir/$lang/Main.lng") {
		require "$langdir/$lang/Main.lng";
	} elsif (-e "$langdir/English/Main.lng") {
		require "$langdir/English/Main.lng";
	}
	$mylastdate = &timeformat($lastdate);

	$yymain .= qq~
<form action="$set_cgi?action=setup2" method="post" name="auto_settings" style="display: inline;">
<script language="JavaScript1.2" type="text/javascript">
<!--
function abspathfill(brddir) {
	document.auto_settings.preboarddir.value = brddir;
}
function autofill() {
	var boardurl = document.auto_settings.preboardurl.value || "$boardurl";
	var boarddir = document.auto_settings.preboarddir.value || ".";
	var htmldir = document.auto_settings.prehtmldir.value || "";
	var htmlurl = document.auto_settings.prehtml_root.value || "";
	if(!htmldir) {return 0;}
	if(!htmlurl) {return 0;}
	var confirmvalue = confirm("Do autofill?");
	if(!confirmvalue) {return 0;}
	else {
		// Board URL
		document.auto_settings.boardurl.value = boardurl;

		// cgi Directories
		document.auto_settings.boarddir.value = boarddir;
		document.auto_settings.boardsdir.value = boarddir + "/Boards";
		document.auto_settings.datadir.value = boarddir + "/Messages";
		document.auto_settings.vardir.value = boarddir + "/Variables";
		document.auto_settings.memberdir.value = boarddir + "/Members";
		document.auto_settings.sourcedir.value = boarddir + "/Sources";
		document.auto_settings.admindir.value = boarddir + "/Admin";
		document.auto_settings.langdir.value = boarddir + "/Languages";
		document.auto_settings.templatesdir.value = boarddir + "/Templates";
		document.auto_settings.helpfile.value = boarddir + "/Help";

		// HTML URLs
		document.auto_settings.html_root.value = htmlurl;
		document.auto_settings.forumstylesurl.value = htmlurl + "/Templates/Forum";
		document.auto_settings.adminstylesurl.value = htmlurl + "/Templates/Admin";
		document.auto_settings.uploadurl.value = htmlurl + "/Attachments";
		document.auto_settings.ubbcjspath.value = htmlurl + "/ubbc.js";
		document.auto_settings.faderpath.value = htmlurl + "/fader.js";
		document.auto_settings.yabbcjspath.value = htmlurl + "/yabbc.js";
		document.auto_settings.postjspath.value = htmlurl + "/post.js";
		document.auto_settings.facesurl.value = htmlurl + "/avatars";
		document.auto_settings.smiliesurl.value = htmlurl + "/Smilies";
		document.auto_settings.modimgurl.value = htmlurl + "/ModImages";

		// HTML Directories
		document.auto_settings.uploaddir.value = htmldir + "/Attachments";
		document.auto_settings.htmldir.value = htmldir;
		document.auto_settings.forumstylesdir.value = htmldir + "/Templates/Forum";
		document.auto_settings.adminstylesdir.value = htmldir + "/Templates/Admin";
		document.auto_settings.facesdir.value = htmldir + "/avatars";
		document.auto_settings.smiliesdir.value = htmldir + "/Smilies";
		document.auto_settings.modimgdir.value = htmldir + "/ModImages";
	}
}
//-->
</script>

	<table width="80%" bgcolor="#000000" border="0" cellspacing="1" cellpadding="3" align="center">
	<tr>
		<td colspan="2" bgcolor="$header" align="left">
		<span style="font-family: arial; font-size: 13px; color: #fefefe;">&nbsp;<b>Absolute Path to the main script directory</b></span>
		</td>
	</tr>
	<tr>
		<td width="43%" bgcolor= "$windowbg2" align="left">
			<div style="float: left; width: 80%; text-align: left; font-family: Arial; font-size: 11px; color: #000000;">Only click on the insert button if your server needs the absolute path to the YaBB main script</div>
			<div style="float: left; width: 20%; text-align: right;"><input type="button" onclick="abspathfill('$support_env_path')" value="Insert" style="font-size: 11px;" /></div>
		</td>
		<td width="57%" bgcolor="$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$support_env_path</span></td>
	</tr>
	<tr>
		<td colspan="2" bgcolor= "$header" align="left">
		<span style="font-family: Arial; font-size: 13px; color: #fefefe;">&nbsp;<b>Autofill the form below if changes are necessary.</b></span>
		</td>
	</tr>
	<tr>
		<td width="43%" bgcolor= "$windowbg2" align="left">
			<span style="font-family: Arial; font-size: 13px; color: #000000;">
			Main Script Directory:
			</span><br />
			<span style="font-family: Arial; font-size: 11px; color: #000000;">
			The server path to the board's folder (usually can be left as '.')
			</span>
		</td>
		<td width="57%" bgcolor= "$windowbg" align="left">
			<input type="text" size="60" name ="preboarddir" value="$boarddir" />
		</td>
	</tr>
	<tr>
		<td width="43%" bgcolor= "$windowbg2" align="left">
			<span style="font-family: Arial; font-size: 13px; color: #000000;">
			Board URL:
			</span><br />
			<span style="font-family: Arial; font-size: 11px; color: #000000;">
			URL of your board's folder (without trailing '/')
			</span>
		</td>
		<td width="57%" bgcolor= "$windowbg" align="left">
			<input type="text" size="60" name ="preboardurl" value="$boardurl" />
		</td>
	</tr>
	<tr>
		<td width="43%" bgcolor= "$windowbg2" align="left">
			<span style="font-family: Arial; font-size: 13px; color: #000000;">
			HTML Root Directory:
			</span><br />
			<span style="font-family: Arial; font-size: 11px; color: #000000;">
			Base Path for all html/css files and folders
			</span>
		</td>
		<td width="57%" bgcolor= "$windowbg" align="left">
			<input type="text" size="60" name ="prehtmldir" value="$htmldir" />
		</td>
	</tr>
	<tr>
		<td width="43%" bgcolor= "$windowbg2" align="left">
			<span style="font-family: Arial; font-size: 13px; color: #000000;">
			HTML Root URL:
			</span><br />
			<span style="font-family: Arial; font-size: 11px; color: #000000;">
			Base URL for all html/css files and folders
			</span>
		</td>
		<td width="57%" bgcolor= "$windowbg" align="left">
			<input type="text" size="60" name ="prehtml_root" value="$html_root" />
		</td>
	</tr>
	<tr>
		<td colspan="2" bgcolor= "$catbg" align="center">
			<input type="button" onclick="autofill()" value="Autofill" style="width: 200px;" />
		</td>
	</tr>
</table>
<br /><br />

<table width="80%" bgcolor="#000000" border="0" cellspacing="1" cellpadding="3" align="center">
	<tr>
		<td colspan="4" bgcolor= "$header" width="100%" align="left">
		<input type="hidden" name="lastsaved" value="$realname">
		<input type="hidden" name="lastdate" value="$date">
		<span style="font-family: Arial; font-size: 13px; color: #fefefe;">&nbsp;<b>These are the settings detected on your server and the last saved settings.</b></span>
		</td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$catbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">&nbsp;</span></td>
		<td width="35%" bgcolor= "$catbg" align="center"><span style="font-family: arial; font-size: 13px; color: #000000;"><b>Detected Values</b></span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><span style="font-family: arial; font-size: 13px; color: #000000;"><b>Transfer</b></span></td>
		<td width="35%" bgcolor= "$catbg" align="center"><span style="font-family: arial; font-size: 13px; color: #000000;"><b>Saved: $mylastdate</b></span></td>
	</tr>
	<tr>
		<td colspan="4" bgcolor= "$header" width="100%" align="left">
		<span style="font-family: arial; font-size: 13px; color: #fefefe;">&nbsp; <b>CGI-BIN Settings</b></span>
		</td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Board URL:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_boardurl</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.boardurl.value = '$fnd_boardurl';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="boardurl" value="$boardurl" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Main Script Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_boarddir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.boarddir.value = '$fnd_boarddir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="boarddir" value="$boarddir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Admin Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_admindir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.admindir.value = '$fnd_admindir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="admindir" value="$admindir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Boards Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_boardsdir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.boardsdir.value = '$fnd_boardsdir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="boardsdir" value="$boardsdir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Help Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_helpfile</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.helpfile.value = '$fnd_helpfile';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="helpfile" value="$helpfile" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Languages Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_langdir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.langdir.value = '$fnd_langdir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="langdir" value="$langdir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Member Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_memberdir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.memberdir.value = '$fnd_memberdir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="memberdir" value="$memberdir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Message Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_datadir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.datadir.value = '$fnd_datadir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="datadir" value="$datadir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Sources Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_sourcedir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.sourcedir.value = '$fnd_sourcedir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="sourcedir" value="$sourcedir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Template Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_templatesdir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.templatesdir.value = '$fnd_templatesdir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="templatesdir" value="$templatesdir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Variables Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_vardir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.vardir.value = '$fnd_vardir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="vardir" value="$vardir" /></span></td>
	</tr>
	<tr>
		<td colspan="4" bgcolor= "$header" width="100%" align="left">
		<span style="font-family: arial; font-size: 13px; color: #fefefe;">&nbsp; <b>HTML Settings</b></span>
		</td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">HTML Root Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_htmldir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.htmldir.value = '$fnd_htmldir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="htmldir" value="$htmldir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">HTML Root URL:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_html_root</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.html_root.value = '$fnd_html_root';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="html_root" value="$html_root" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Newsfader URL:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_faderpath</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.faderpath.value = '$fnd_faderpath';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="faderpath" value="$faderpath" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Post.js URL:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_postjspath</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.postjspath.value = '$fnd_postjspath';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="postjspath" value="$postjspath" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">UBBC URL:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_ubbcjspath</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.ubbcjspath.value = '$fnd_ubbcjspath';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="ubbcjspath" value="$ubbcjspath" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">YaBBC.js URL:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_yabbcjspath</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.yabbcjspath.value = '$fnd_yabbcjspath';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="yabbcjspath" value="$yabbcjspath" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Attachment Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_uploaddir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.uploaddir.value = '$fnd_uploaddir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="uploaddir" value="$uploaddir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Attachment URL:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_uploadurl</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.uploadurl.value = '$fnd_uploadurl';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="uploadurl" value="$uploadurl" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Avatar Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_facesdir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.facesdir.value = '$fnd_facesdir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="facesdir" value="$facesdir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Avatar URL:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_facesurl</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.facesurl.value = '$fnd_facesurl';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="facesurl" value="$facesurl" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Mod Images Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_modimgdir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.modimgdir.value = '$fnd_modimgdir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="modimgdir" value="$modimgdir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Mod Images URL:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_modimgurl</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.modimgurl.value = '$fnd_modimgurl';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="modimgurl" value="$modimgurl" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Smilies Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_smiliesdir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.smiliesdir.value = '$fnd_smiliesdir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="smiliesdir" value="$smiliesdir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Smilies URL:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_smiliesurl</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.smiliesurl.value = '$fnd_smiliesurl';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="smiliesurl" value="$smiliesurl" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Admin Style Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_adminstylesdir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.adminstylesdir.value = '$fnd_adminstylesdir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="adminstylesdir" value="$adminstylesdir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Admin Style URL:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_adminstylesurl</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.adminstylesurl.value = '$fnd_adminstylesurl';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="adminstylesurl" value="$adminstylesurl" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Forum Style Dir.:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_forumstylesdir</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.forumstylesdir.value = '$fnd_forumstylesdir';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="forumstylesdir" value="$forumstylesdir" /></span></td>
	</tr>
	<tr>
		<td width="20%" bgcolor= "$windowbg2" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">Forum Style URL:</span></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;">$fnd_forumstylesurl</span></td>
		<td width="10%" bgcolor= "$catbg" align="center"><input type="button" OnClick="javascript: document.auto_settings.forumstylesurl.value = '$fnd_forumstylesurl';return false;" value="->" /></td>
		<td width="35%" bgcolor= "$windowbg" align="left"><span style="font-family: arial; font-size: 13px; color: #000000;"><input type="text" size="40" name ="forumstylesurl" value="$forumstylesurl" /></span></td>
	</tr>
	<tr>
		<td colspan="4" bgcolor= "$catbg" width="100%" align="center">
		<span style="font-family: arial; font-size: 13px; color: #000000;">
		<input type="submit" value="Save Settings" />
		</span>
		</td>
	</tr>
</table>
</form>
	~;
	$yytitle = "Results of Auto-Sensing";
	&Output2;
	exit;
}

sub save_paths {

	if (-e "$vardir/Setup.lock") { &FoundLock2; }

	&LoadCookie;          # Load the user's cookie (or set to guest)
	&LoadUserSettings;
	if (!$iamadmin) { $yymain = qq~Setup Error: You have no access rights to this function. Only user "admin" has if logged in!~; &Output2; }

	$lastsaved      = $FORM{'lastsaved'};
	$lastdate       = $FORM{'lastdate'};
	$boardurl       = $FORM{'boardurl'};
	$boarddir       = $FORM{'boarddir'};
	$htmldir        = $FORM{'htmldir'};
	$uploaddir      = $FORM{'uploaddir'};
	$uploadurl      = $FORM{'uploadurl'};
	$html_root      = $FORM{'html_root'};
	$datadir        = $FORM{'datadir'};
	$boardsdir      = $FORM{'boardsdir'};
	$memberdir      = $FORM{'memberdir'};
	$sourcedir      = $FORM{'sourcedir'};
	$admindir       = $FORM{'admindir'};
	$vardir         = $FORM{'vardir'};
	$langdir        = $FORM{'langdir'};
	$helpfile       = $FORM{'helpfile'};
	$templatesdir   = $FORM{'templatesdir'};
	$forumstylesdir = $FORM{'forumstylesdir'};
	$forumstylesurl = $FORM{'forumstylesurl'};
	$adminstylesdir = $FORM{'adminstylesdir'};
	$adminstylesurl = $FORM{'adminstylesurl'};
	$facesdir       = $FORM{'facesdir'};
	$facesurl       = $FORM{'facesurl'};
	$smiliesdir     = $FORM{'smiliesdir'};
	$smiliesurl     = $FORM{'smiliesurl'};
	$modimgdir      = $FORM{'modimgdir'};
	$modimgurl      = $FORM{'modimgurl'};
	$ubbcjspath     = $FORM{'ubbcjspath'};
	$faderpath      = $FORM{'faderpath'};
	$yabbcjspath    = $FORM{'yabbcjspath'};
	$postjspath     = $FORM{'postjspath'};

	my $filler  = q~                                                                               ~;
	my $setfile = << "EOF";
###############################################################################
# Paths.pl                                                                    #
###############################################################################
# YaBB: Yet another Bulletin Board                                            #
# Open-Source Community Software for Webmasters                               #
# Version:        YaBB 2.1                                                    #
# Released:       November 8, 2005                                            #
# Distributed by: http://www.yabbforum.com                                    #
# =========================================================================== #
# Copyright (c) 2000-2005 YaBB (www.yabbforum.com) - All Rights Reserved.     #
# Software by: The YaBB Development Team                                      #
#              with assistance from the YaBB community.                       #
# Sponsored by: Xnull Internet Media, Inc. - http://www.ximinc.com            #
###############################################################################

\$lastsaved = "$lastsaved";
\$lastdate = "$lastdate";

########## Directories ##########

\$boardurl = "$boardurl";                              		# URL of your board's folder (without trailing '/')
\$boarddir = "$boarddir";                                       # The server path to the board's folder (usually can be left as '.')
\$boardsdir = "$boardsdir";                                     # Directory with board data files
\$datadir = "$datadir";                                         # Directory with messages
\$memberdir = "$memberdir";                                     # Directory with member files
\$sourcedir = "$sourcedir";                                     # Directory with YaBB source files
\$admindir = "$admindir";                                       # Directory with YaBB admin source files
\$vardir = "$vardir";                                           # Directory with variable files
\$langdir = "$langdir";                                         # Directory with Language files and folders
\$helpfile = "$helpfile";									# Directory with Help files and folders
\$templatesdir = "$templatesdir";                               # Directory with template files and folders
\$forumstylesdir = "$forumstylesdir";                               # Directory with forum style files and folders
\$adminstylesdir = "$adminstylesdir";                               # Directory with admin style files and folders
\$htmldir = "$htmldir";                              		# Base Path for all html/css files and folders
\$facesdir = "$facesdir";                              		# Base Path for all avatar files
\$smiliesdir = "$smiliesdir";                              	# Base Path for all smilie files
\$modimgdir = "$modimgdir";                              	# Base Path for all mod images
\$uploaddir = "$uploaddir";                              	# Base Path for all attachment files

########## URL's ##########

\$forumstylesurl = "$forumstylesurl";			  	# Default Forum Style Directory
\$adminstylesurl = "$adminstylesurl";			  	# Default Admin Style Directory
\$ubbcjspath = "$ubbcjspath";			  		# Default Location for the ubbc.js file
\$faderpath = "$faderpath";			  		# Default Location for the fader.js file
\$yabbcjspath = "$yabbcjspath";			 	# Default Location for the yabbc.js file
\$postjspath = "$postjspath";			  		# Default Location for the post.js file
\$html_root = "$html_root";                            		# Base URL for all html/css files and folders
\$facesurl = "$facesurl";                              		# Base URL for all avatar files
\$smiliesurl = "$smiliesurl";                            	# Base URL for all smilie files
\$modimgurl = "$modimgurl";                            	# Base URL for all mod images
\$uploadurl = "$uploadurl";        	                    	# Base URL for all attachment files

1;
EOF

	$setfile =~ s~(.+\;)\s+(\#.+$)~$1 . substr( $filler, 0, (70-(length $1)) ) . $2 ~gem;
	$setfile =~ s~(.{64,}\;)\s+(\#.+$)~$1 . "\n   " . $2~gem;
	$setfile =~ s~^\s\s\s+(\#.+$)~substr( $filler, 0, 70 ) . $1~gem;

	fopen(FILE, ">Paths.pl");
	print FILE $setfile;
	fclose(FILE);

	if (-e "$vardir/Paths.pl") { unlink "$vardir/Paths.pl"; }

	$yySetLocation = qq~$set_cgi?action=setinstall~;
	&redirectexit;
}

sub BrdInstall {
	$no_brddir = 0;
	if (!-d "$boardsdir") { $no_brddir = "1"; return 1; }
}

sub MesInstall {
	$no_mesdir = 0;
	if (!-d "$datadir") { $no_mesdir = "1"; return 1; }
}

sub MemInstall {
	$no_memdir = 0;
	if (!-d "$memberdir") { $no_memdir = "1"; return 1; }
}

sub VarInstall {

	my $varsdir = "$vardir";
	$no_vardir = 0;

	if (!-d "$varsdir") { $no_vardir = "1"; return 1; }

	if (!-e "$varsdir/adminlog.txt") {
		open(ADMLOGFILE, ">$varsdir/adminlog.txt");
		print ADMLOGFILE "";
		close(ADMLOGFILE);
	}

	if (!-e "$varsdir/advsettings.txt") {
		my $filler  = q~                                                                               ~;
		my $setfile = << "EOF";
###############################################################################
# advsettings.txt                                                             #
###############################################################################
# YaBB: Yet another Bulletin Board                                            #
# Open-Source Community Software for Webmasters                               #
# Version:        YaBB 2.1                                                    #
# Released:       November 8, 2005                                            #
# Distributed by: http://www.yabbforum.com                                    #
# =========================================================================== #
# Copyright (c) 2000-2005 YaBB (www.yabbforum.com) - All Rights Reserved.     #
# Software by: The YaBB Development Team                                      #
#              with assistance from the YaBB community.                       #
# Sponsored by: Xnull Internet Media, Inc. - http://www.ximinc.com            #
#               Your source for web hosting, web design, and domains.         #
###############################################################################


########## In-Thread Multi Delete ##########

\$mdadmin = 1;
\$mdglobal = 1;
\$mdmod = 1;
\$adminbin = 0;                                                        # Skip recycle bin step for admins and delete directly

########## Moderation Update ##########

\$adminview = 2;                                                       # Multi-admin settings for Administrators: 0=none, 1=icons 2=single checkbox 3=multiple checkboxes
\$gmodview = 2;                                                        # Multi-admin settings for Global Moderators: 0=none, 1=icons 2=single checkbox 3=multiple checkboxes
\$modview = 2;                                                         # Multi-admin settings for Moderators: 0=none, 1=icons 2=single checkbox 3=multiple checkboxes

########## Advanced Memberview Plus ###########

\$showallgroups = 1;
\$OnlineLogTime = 15;

######### Polls ###########

\$numpolloptions = 8;                                                  # Number of poll options
\$maxpq = 60;                                                          # Maximum Allowed Characters in a Poll Qestion?
\$maxpo = 50;                                                          # Maximum Allowed Characters in a Poll Option?
\$maxpc = 0;                                                           # Maximum Allowed Characters in a Poll Comment?
\$useraddpoll = 1;                                                     # Allow users to add polls to existing threads? (1 = yes)
\$ubbcpolls = 1;                                                       # Allow UBBC tags and smilies in polls? (1 = yes)

########## Advanced Instant Message Box ############

\$numposts = 1;                                                        # Number of posts required to send Instant Messages
\$imspam = off;                                                        # Percent of Users a user is a allowed to send a message at once
\$numibox = 20;                                                        # Number of maximum Messages in the IM-Inbox
\$numobox = 20;                                                        # Number of maximum Messages in the IM-Outbox
\$numstore = 20;                                                       # Number of maximum Messages in the Storage box
\$enable_imlimit = 0;                                                  # Set to 1 to enable limitation of incoming and outgoing im messages
\$imtext = qq~Welcome to my boards~;
\$sendname = admin;
\$imsubject = "Hey Hey :)";
\$send_welcomeim = 1;                                                  ######### Topic Summary Cutter #############

\$cutamount  = "15";                                                   # Number of posts to list in topic summary
\$tsreverse = 1;                                                       # Reverse Topic Summaries (So most recent is first

############## Time Lock ###################

\$tlnomodflag = 1;                                                     # Set to 1 limit time users may modify posts
\$tlnomodtime = 1;                                                     # Time limit on modifying posts (days)
\$tlnodelflag = 1;                                                     # Set to 1 limit time users may delete posts
\$tlnodeltime = 5;                                                     # Time limit on deleting posts (days)
\$tllastmodflag = 1;                                                   # Set to 1 allow users to modify posts up to the specified time limit w/o showing "last Edit" message
\$tllastmodtime = 60;                                                  # Time limit to modify posts w/o triggering "last Edit" message (in minutes)

########## File Attachment Settings ##########

\$limit = 250;                                                         # Set to the maximum number of kilobytes an attachment can be. Set to 0 to disable the file size check.
\$dirlimit = 10000;                                                    # Set to the maximum number of kilobytes the attachment directory can hold. Set to 0 to disable the directory size check.
\$overwrite = 0;                                                       # Set to 0 to auto rename attachments if they exist, 1 to overwrite them or 2 to generate an error if the file exists already.
\@ext = qw(jpg jpeg gif png swf zip);                                  # The allowed file extensions for file attachements. Variable should be set in the form of "jpg bmp gif" and so on.
\$checkext = 1;                                                        # Set to 1 to enable file extension checking, set to 0 to allow all file types to be uploaded
\$amdisplaypics = 1;                                                   # Set to 1 to display attached pictures in posts, set to 0 to only show a link to them.
\$allowattach = 1;                                                     # Set to 1 to allow file attaching, set to 0 to disable file attaching.
\$allowguestattach = 0;                                                # Set to 1 to allow guests to upload attachments, 0 to disable guest attachment uploading.

############# Error Logger #################

\$elmax  = "50";                                                       # Max number of log entries before rotation
\$elenable = 1;                                                        # allow for error logging
\$elrotate = 1;                                                        # Allow for log rotation

1;
EOF

		$setfile =~ s~(.+\;)\s+(\#.+$)~$1 . substr( $filler, 0, (70-(length $1)) ) . $2 ~gem;
		$setfile =~ s~(.{64,}\;)\s+(\#.+$)~$1 . "\n   " . $2~gem;
		$setfile =~ s~^\s\s\s+(\#.+$)~substr( $filler, 0, 70 ) . $1~gem;

		open(SETTING, ">$varsdir/advsettings.txt");
		print SETTING $setfile;
		close(SETTING);
	}

	if (!-e "$varsdir/ConvSettings.txt") {

		my $filler  = q~                                                                               ~;
		my $setfile = << "EOF";
\$convertdir = qq~./Convert~;
\$convboardsdir = qq~./Convert/Boards~;
\$convmemberdir = qq~./Convert/Members~;
\$convdatadir = qq~./Convert/Messages~;
\$convvardir = qq~./Convert/Variables~;

1;
EOF

		$setfile =~ s~(.+\;)\s+(\#.+$)~$1 . substr( $filler, 0, (70-(length $1)) ) . $2 ~gem;
		$setfile =~ s~(.{64,}\;)\s+(\#.+$)~$1 . "\n   " . $2~gem;
		$setfile =~ s~^\s\s\s+(\#.+$)~substr( $filler, 0, 70 ) . $1~gem;

		fopen(SETTING, ">$vardir/ConvSettings.txt");
		print SETTING $setfile;
		fclose(SETTING);
	}

	if (!-e "$varsdir/allowed.txt") {
		open(ALLOWFILE, ">$varsdir/allowed.txt");
		print ALLOWFILE "login\n";
		print ALLOWFILE "logout\n";
		print ALLOWFILE "display\n";
		print ALLOWFILE "messageindex\n";
		print ALLOWFILE "pages\n";
		print ALLOWFILE "profile\n";
		print ALLOWFILE "register\n";
		print ALLOWFILE "resetpass\n";
		print ALLOWFILE "viewprofile";
		close(ALLOWFILE);
	}

	if (!-e "$varsdir/attachments.txt") {
		open(ATTFILE, ">$varsdir/attachments.txt");
		print ATTFILE "";
		close(ATTFILE);
	}

	if (!-e "$varsdir/ban.txt") {
		open(BANFILE, ">$varsdir/ban.txt");
		print BANFILE "";
		close(BANFILE);
	}

	if (!-e "$varsdir/ban_email.txt") {
		open(BANFILE, ">$varsdir/ban_email.txt");
		print BANFILE "";
		close(BANFILE);
	}

	if (!-e "$varsdir/ban_log.txt") {
		open(BANFILE, ">$varsdir/ban_log.txt");
		print BANFILE "";
		close(BANFILE);
	}

	if (!-e "$varsdir/ban_memname.txt") {
		open(BANFILE, ">$varsdir/ban_memname.txt");
		print BANFILE "";
		close(BANFILE);
	}

	if (!-e "$varsdir/clicklog.txt") {
		open(CLICKFILE, ">$varsdir/clicklog.txt");
		print CLICKFILE "";
		close(CLICKFILE);
	}

	if (!-e "$varsdir/errorlog.txt") {
		open(ERRORFILE, ">$varsdir/errorlog.txt");
		print ERRORFILE "";
		close(ERRORFILE);
	}

	if (!-e "$varsdir/flood.txt") {
		open(FLOODFILE, ">$varsdir/flood.txt");
		print FLOODFILE "255.255.255.255|1119313741";
		close(FLOODFILE);
	}

	if (!-e "$varsdir/gmodsettings.txt") {
		my $filler  = q~                                                                               ~;
		my $setfile = << "EOF";
### Gmod Releated Setttings ###

\$allow_gmod_admin = "on"; #

### Areas Gmods can Access ### 

\%gmod_access = (
'modsettings',"",
'flood_control',"",
'advsettings',"on",
'smilies',"on",
'helpadmin',"on",
'setcensor',"on",
'detailedversion',"on",
'managecats',"",
'manageboards',"",
'editnews',"on",
'modagreement',"on",
'modtemp',"",
'modcss',"",
'referer_control',"",
'viewmembers',"on",
'modmemgr',"",
'mailing',"on",
'ipban',"on",
'setreserve',"on",
'clean_log',"on",
'boardrecount',"",
'membershiprecount',"",
'rebuildmemlist',"",
'deleteoldthreads',"",
'manageattachments',"on",
'stats',"on",
'showclicks',"on",
'errorlog',"on",
'view_reglog',"on",
);

\%gmod_access2 = (
admin => "on",
deleteattachment => "on",
manageattachments2 => "on",
removeoldattachments => "on",
removebigattachments => "on",

delgroup => "",
editgroup => "",
editAddGroup2 => "",
modmemgr2 => "",
assigned => "",
assigned2 => "",

reordercats => "",
modifycatorder => "",
modifycat => "",
createcat => "",
catscreen => "",
reordercats2 => "",
addcat => "",
addcat2 => "",

modifyboard => "",
addboard => "",
addboard2 => "",
reorderboards2 => "",

smilieput => "on",
smilieindex => "on",
smiliemove => "on",
addsmilies => "on",

addmember => "on",
addmember2 => "on",
deletemultimembers => "on",

mailmultimembers => "on",
mailing2 => "on",

del_regentry => "on",
clean_reglog => "on",

cleanerrorlog => "on",
deleteerror => "on",

do_clean_log => "on",
modagreement2 => "on",
modsettings2 => "",
advsettings2 => "on",
ml => "on",
referer_control2 => "",
removeoldthreads => "",
ipban2 => "on",
setcensor2 => "on",
setreserve2 => "on",
editnews2 => "on",

flood_control2,"",
);

1;
EOF

		$setfile =~ s~(.+\;)\s+(\#.+$)~$1 . substr( $filler, 0, (70-(length $1)) ) . $2 ~gem;
		$setfile =~ s~(.{64,}\;)\s+(\#.+$)~$1 . "\n   " . $2~gem;
		$setfile =~ s~^\s\s\s+(\#.+$)~substr( $filler, 0, 70 ) . $1~gem;

		open(SETTING, ">$varsdir/gmodsettings.txt");
		print SETTING $setfile;
		close(SETTING);

	}

	if (!-e "$varsdir/Guardian.banned") {
		my $filler  = q~                                                                               ~;
		my $setfile = << "EOF";
\$banned_harvesters = qq~alexibot|asterias|backdoorbot|black.hole|blackwidow|blowfish|botalot|builtbottough|bullseye|bunnyslippers|cegbfeieh|cheesebot|cherrypicker|chinaclaw|copyrightcheck|cosmos |crescent|custo|disco|dittospyder|download demon|ecatch|eirgrabber|emailcollector|emailsiphon|emailwolf|erocrawler|eseek-larbin|express webpictures|extractorpro|eyenetie|fast|flashget|foobot|frontpage|fscrawler|getright|getweb|go!zilla|go-ahead-got-it|grabnet|grafula|gsa-crawler|harvest|hloader|hmview|httplib|httrack|humanlinks|ia_archiver|image stripper|image sucker|indy library|infonavirobot|interget|internet ninja|jennybot|jetcar|joc web spider|kenjin.spider|keyword.density|larbin|leechftp|lexibot|libweb/clshttp|linkextractorpro|linkscan/8.1a.unix|linkwalker|lwp-trivial|mass downloader|mata.hari|microsoft.url|midown tool|miixpc|mister pix|moget|mozilla.*newt|mozilla/3.mozilla/2.01|navroad|nearsite|net vampire|netants|netmechanic|netspider|netzip|nicerspro|npbot|octopus|offline explorer|offline navigator|openfind|pagegrabber|papa foto|pavuk|pcbrowser|propowerbot/2.14|prowebwalker|queryn.metasearch|realdownload|reget|repomonkey|sitesnagger|slysearch|smartdownload|spankbot|spanner |spiderzilla|steeler|superbot|superhttp|surfbot|suzuran|szukacz|takeout|teleport pro|telesoft|the.intraformant|thenomad|tighttwatbot|titan|tocrawl/urldispatcher|true_robot|turingos|turnitinbot|urly.warning|vci|voideye|web image collector|web sucker|web.image.collector|webauto|webbandit|webbandit|webcopier|webemailextrac.*|webenhancer|webfetch|webgo is|webleacher|webmasterworldforumbot|webreaper|websauger|website extractor|website quester|webster.pro|webstripper|webwhacker|webzip|wget|widow|www-collector-e|wwwoffle|xaldon webspider|xenu link sleuth|zeus~;
\$banned_referers = qq~hotsex.com|porn.com~;
\$banned_requests = qq~~;
\$banned_strings = qq~pussy|cunt~;

1;
EOF

		$setfile =~ s~(.+\;)\s+(\#.+$)~$1 . substr( $filler, 0, (70-(length $1)) ) . $2 ~gem;
		$setfile =~ s~(.{64,}\;)\s+(\#.+$)~$1 . "\n   " . $2~gem;
		$setfile =~ s~^\s\s\s+(\#.+$)~substr( $filler, 0, 70 ) . $1~gem;

		open(SETTING, ">$varsdir/Guardian.banned");
		print SETTING $setfile;
		close(SETTING);
	}

	if (!-e "$varsdir/Guardian.settings") {
		my $filler  = q~                                                                               ~;
		my $setfile = << "EOF";
\$use_guardian = 1;
\$use_htaccess = 0;

\$disallow_proxy_on = 0;
\$referer_on = 1;
\$harvester_on = 0;
\$request_on = 0;
\$string_on = 1;
\$union_on = 1;
\$clike_on = 1;
\$script_on = 1;

\$disallow_proxy_notify = 1;
\$referer_notify = 0;
\$harvester_notify = 1;
\$request_notify = 0; 
\$string_notify = 1;
\$union_notify = 1;
\$clike_notify = 1;
\$script_notify = 1;

1;
EOF

		$setfile =~ s~(.+\;)\s+(\#.+$)~$1 . substr( $filler, 0, (70-(length $1)) ) . $2 ~gem;
		$setfile =~ s~(.{64,}\;)\s+(\#.+$)~$1 . "\n   " . $2~gem;
		$setfile =~ s~^\s\s\s+(\#.+$)~substr( $filler, 0, 70 ) . $1~gem;

		open(SETTING, ">$varsdir/Guardian.settings");
		print SETTING $setfile;
		close(SETTING);
	}

	if (!-e "$varsdir/log.txt") {
		open(LOGFILE, ">$varsdir/log.txt");
		print LOGFILE "admin|1105634411|127.0.0.1";
		close(LOGFILE);
	}

	if (!-e "$varsdir/membergroups.txt") {
		my $filler  = q~                                                                               ~;
		my $setfile = << "EOF";
\$Group{'Administrator'} = "YaBB Administrator|5|staradmin.gif|red|0|0|0|0|0|0";
\$Group{'Global Moderator'} = "Global Moderator|5|stargmod.gif|blue|0|0|0|0|0|0";
\$Group{'Moderator'} = "YaBB Moderator|5|starmod.gif|green|0|0|0|0|0|0";
\$Post{'500'} = "God Member|5|starsilver.gif||0|0|0|0|0|0";
\$Post{'250'} = "Senior Member|4|stargold.gif||0|0|0|0|0|0";
\$Post{'100'} = "Full Member|3|starblue.gif||0|0|0|0|0|0";
\$Post{'50'} = "Junior Member|2|stargold.gif||0|0|0|0|0|0";
\$Post{'-1'} = "YaBB Newbies|1|stargold.gif||0|0|0|0|0|0";

1;
EOF

		$setfile =~ s~(.+\;)\s+(\#.+$)~$1 . substr( $filler, 0, (70-(length $1)) ) . $2 ~gem;
		$setfile =~ s~(.{64,}\;)\s+(\#.+$)~$1 . "\n   " . $2~gem;
		$setfile =~ s~^\s\s\s+(\#.+$)~substr( $filler, 0, 70 ) . $1~gem;

		open(SETTING, ">$varsdir/membergroups.txt");
		print SETTING $setfile;
		close(SETTING);
	}

	if (!-e "$varsdir/modlist.txt") {
		open(MODSFILE, ">$varsdir/modlist.txt");
		print MODSFILE "admin\n";
		close(MODSFILE);
	}

	if (!-e "$varsdir/news.txt") {
		open(NEWSFILE, ">$varsdir/news.txt");
		print NEWSFILE "Welcome to our forum.\n";
		print NEWSFILE "We've upgraded to YaBB 2!\n";
		print NEWSFILE "Visit [url=http://www.yabbforum.com]YaBB[/url] today \;\)\n";
		print NEWSFILE "YaBB is sponsored by [url=http://www.ximinc.com]XIMinc[/url]!\n";
		print NEWSFILE "Signup for free on our forum and benefit from new features!\n";
		print NEWSFILE "Latest info can be found on the [url=http://www.yabbforum.com/community/]YaBB Chat and Support Community[/url].\n";
		close(NEWSFILE);
	}

	if (!-e "$varsdir/oldestmes.txt") {
		open(OLDFILE, ">$varsdir/oldestmes.txt");
		print OLDFILE "1\n";
		close(OLDFILE);
	}

	if (!-e "$varsdir/registration.log") {
		open(REGLOG, ">$varsdir/registration.log");
		print REGLOG "";
		close(REGLOG);
	}

	if (!-e "$varsdir/reserve.txt") {
		open(RESERVEFILE, ">$varsdir/reserve.txt");
		print RESERVEFILE "yabb\n";
		print RESERVEFILE "YaBBadmin\n";
		print RESERVEFILE "administrator\n";
		print RESERVEFILE "admin\n";
		print RESERVEFILE "y2\n";
		print RESERVEFILE "xnull\n";
		print RESERVEFILE "yabb2\n";
		print RESERVEFILE "XIMinc\n";
		print RESERVEFILE "yabbforum\n";
		close(RESERVEFILE);
	}

	if (!-e "$varsdir/reservecfg.txt") {
		open(RESERVEFILE, ">$varsdir/reservecfg.txt");
		print RESERVEFILE "checked\n";
		print RESERVEFILE "\n";
		print RESERVEFILE "checked\n";
		print RESERVEFILE "checked\n";
		close(RESERVEFILE);
	}

	if (!-e "$varsdir/secsettings.txt") {
		my $filler  = q~                                                                               ~;
		my $setfile = << "EOF";
###############################################################################
# SecSettings.txt                                                             #
###############################################################################

\$regcheck = 0;                                                        # Set to 1 if you want to enable automatic flood protection enabled
\$codemaxchars = 6;                                                    # Set max length of validation code (15 is max)
\$translayer = 0;                                                      # Set to 1 background for validation image should be transparent
\$stealthurl = 0;                                                      # Set to 1 to mask referer url to hosts if a hyperlink is clicked.
\$referersecurity = 0;                                                 # Set to 1 to activate referer security checking.
\$sessions = 1;                                                        # Set to 1 to activate session id protection.
\$show_online_ip_admin = 1;                                            # Set to 1 to show online IP's to admins.
\$show_online_ip_gmod = 1;                                             # Set to 1 to show online IP's to global moderators.


1;
EOF

		$setfile =~ s~(.+\;)\s+(\#.+$)~$1 . substr( $filler, 0, (70-(length $1)) ) . $2 ~gem;
		$setfile =~ s~(.{64,}\;)\s+(\#.+$)~$1 . "\n   " . $2~gem;
		$setfile =~ s~^\s\s\s+(\#.+$)~substr( $filler, 0, 70 ) . $1~gem;

		open(SETTING, ">$varsdir/secsettings.txt");
		print SETTING $setfile;
		close(SETTING);
	}

	if (!-e "$varsdir/Smilies.txt") {
		my $filler  = q~                                                                               ~;
		my $setfile = << "EOF";
\$SmilieURL[0] = "exclamation.gif";
\$SmilieCode[0] = ":exclamation";
\$SmilieDescription[0] = "Exclaim";
\$SmilieLinebreak[0] = "";

\$SmilieURL[1] = "question.gif";
\$SmilieCode[1] = ":question";
\$SmilieDescription[1] = "Questioning";
\$SmilieLinebreak[1] = "";

\$smiliestyle = "1";
\$showadded = "2";
\$showsmdir = "2";
\$detachblock = "1";
\$winwidth = "400";
\$winheight = "400";
\$popback = "#FFFFFF";
\$poptext = "#000000";

1;
EOF

		$setfile =~ s~(.+\;)\s+(\#.+$)~$1 . substr( $filler, 0, (70-(length $1)) ) . $2 ~gem;
		$setfile =~ s~(.{64,}\;)\s+(\#.+$)~$1 . "\n   " . $2~gem;
		$setfile =~ s~^\s\s\s+(\#.+$)~substr( $filler, 0, 70 ) . $1~gem;

		open(SETTING, ">$varsdir/Smilies.txt");
		print SETTING $setfile;
		close(SETTING);
	}

}

sub SetInstall {
	LoadLanguage("Admin");

	&tempstarter;

	$yymain .= qq~
<form action="$set_cgi?action=setinstall2" method="post">
 <div class="bordercolor" style="padding: 0px; width: 100%; margin-left: 0px; margin-right: 0px;">
   <table width="100%" cellspacing="1" cellpadding="4">
	<tr valign="middle">
		<td width="100%" class="titlebg" align="left">
		System Setup
		</td>
	</tr><tr valign="middle">
		<td width="100%" class="windowbg" align="left">
		Here you can set some of the default settings for your new YaBB 2 forum.<br />
		After finishing the setup procedure, you should login to your forum and go to your Admin Center - Forum Settings to set the options correctly.
		</td>
	</tr><tr valign="middle">
		<td width="100%" class="windowbg2" align="left">
		<div style="float: left; font-family: verdana; width: 45%; text-align: left; font-size: 12px; padding-top: 2px; padding-bottom: 2px;">
		Message Board Name
		</div>
		<div style="float: left; font-family: verdana; width: 55%; text-align: left; font-size: 12px; padding-top: 2px; padding-bottom: 2px;">
		<input type="text" name="mbname" size="35" value="My Perl YaBB Forum" />
		</div>
	<br />
		<div style="float: left; font-family: verdana; width: 45%; text-align: left; font-size: 12px; padding-top: 2px; padding-bottom: 2px;">
		Webmaster E-mail Address
		</div>
		<div style="float: left; font-family: verdana; width: 55%; text-align: left; font-size: 12px; padding-top: 2px; padding-bottom: 2px;">
		<input type="text" name="webmaster_email" size="35" value="webmaster\@mysite.com" />
		</div>
	<br />
		<div style="float: left; font-family: verdana; width: 45%; text-align: left; font-size: 12px; padding-top: 2px; padding-bottom: 2px;">
		Default Time Format
		</div>
		<div style="float: left; font-family: verdana; width: 55%; text-align: left; font-size: 12px; padding-top: 2px; padding-bottom: 2px;">
		<select name="timeselect" size="1">
			<option value="1">01/31/01  13:15:17</option>
			<option value="5">01/31/01  1:15pm</option>
			<option value="4">Jan 12th, 2001, 1:15pm</option>
			<option value="2">31.01.01  13:15:17</option>
			<option value="3">31.01.2001  13:15:17</option>
			<option value="6">31. Jan  13:15</option>
		</select>
		</div>
	<br />
		<div style="float: left; font-family: verdana; width: 45%; text-align: left; font-size: 12px; padding-top: 2px; padding-bottom: 2px;">
		Forum Time Zone
		</div>
		<div style="float: left; font-family: verdana; width: 55%; text-align: left; font-size: 12px; padding-top: 2px; padding-bottom: 2px;">
		<select name="timeoffset">
		<option value="">$time_zone_txt{'1'}</option>
		<option value="12">$time_zone_txt{'2'}</option>
		<option value="11">$time_zone_txt{'3'}</option>
		<option value="10">$time_zone_txt{'4'}</option>
		<option value="9.5">$time_zone_txt{'5'}</option>
		<option value="9">$time_zone_txt{'6'}</option>
		<option value="8">$time_zone_txt{'7'}</option>
		<option value="6.5">$time_zone_txt{'9'}</option>
		<option value="6">$time_zone_txt{'10'}</option>
		<option value="5.5">$time_zone_txt{'11'}</option>
		<option value="5">$time_zone_txt{'12'}</option>
		<option value="4">$time_zone_txt{'13'}</option>
		<option value="3.5">$time_zone_txt{'14'}</option>
		<option value="3">$time_zone_txt{'15'}</option>
		<option value="2">$time_zone_txt{'16'}</option>
		<option value="1">$time_zone_txt{'17'}</option>
		<option value="0" selected="selected">$time_zone_txt{'18'}</option>
		<option value="-1">$time_zone_txt{'19'}</option>
		<option value="-2">$time_zone_txt{'20'}</option>
		<option value="-3">$time_zone_txt{'21'}</option>
		<option value="-3.5">$time_zone_txt{'22'}</option>
		<option value="-4">$time_zone_txt{'23'}</option>
		<option value="-5">$time_zone_txt{'24'}</option>
		<option value="-6">$time_zone_txt{'25'}</option>
		<option value="-7">$time_zone_txt{'26'}</option>
		<option value="-8">$time_zone_txt{'27'}</option>
		<option value="-9">$time_zone_txt{'28'}</option>
		<option value="-10">$time_zone_txt{'29'}</option>
		<option value="-11">$time_zone_txt{'30'}</option>
		</select>
		</div>
		<input type="hidden" name="dstoffset" value="1" />
	<br />
		</td>
	</tr>
	<tr valign="middle">
		<td width="100%" class="catbg" align="center">
		<input type="submit" value="Continue" />
		</td>
	</tr>
	</table>
</div>
</form>
~;

	$yyim    = "You are running YaBB 2 Setup.";
	$yytitle = "YaBB 2 Setup";
	&SetupTemplate;
	exit;

}

sub SetInstall2 {
	if ($action eq "setinstall" || $action eq "setinstall2") {
		my $boarddir = ".";
		$maintenance = 0;
		$guestaccess = 1;
		$mbname      = $FORM{'mbname'} || 'My Perl YaBB Forum';
		$mbname =~ s/\"/\'/g;
		$forumstart          = &timetostring(int(time)) || '01/01/05 at 01:01:01';
		$Cookie_Length       = 1;
		$regdisable          = 0;
		$RegAgree            = 1;
		$preregister         = 0;
		$preregspan          = 24;
		$emailpassword       = 0;
		$emailnewpass        = 0;
		$emailwelcome        = 0;
		$lang                = "English";
		$default_template    = "Forum default";
		$mailprog            = "/usr/sbin/sendmail";
		$smtp_server         = "127.0.0.1";
		$smtp_auth_required  = 1;
		$authuser            = q^admin^;
		$authpass            = q^admin^;
		$webmaster_email     = $FORM{'webmaster_email'} || 'webmaster@mysite.com';
		$mailtype            = 0;
		$maintenancetext     = "We are currently upgrading our forum again. Please check back shortly!";
		$MenuType            = 2;
		$profilebutton       = 0;
		$allow_hide_email    = 1;
		$showlatestmember    = 1;
		$shownewsfader       = 0;
		$Show_RecentBar      = 1;
		$showmodify          = 1;
		$ShowBDescrip        = 1;
		$showuserpic         = 1;
		$showusertext        = 1;
		$showtopicviewers    = 1;
		$showtopicrepliers   = 1;
		$showgenderimage     = 1;
		$showyabbcbutt       = 1;
		$nestedquotes        = 1;
		$parseflash          = 0;
		$enableclicklog      = 0;
		$enable_ubbc         = 1;
		$enable_news         = 1;
		$allowpics           = 1;
		$enable_guestposting = 0;
		$enable_notification = 1;
		$autolinkurls        = 1;
		$timeselected        = $FORM{'timeselect'} || 0;
		$timecorrection      = 0;
		$timeoffset          = $FORM{'timeoffset'} || 0;
		$dstoffset           = $FORM{'dstoffset'} || 0;
		$TopAmmount          = 15;
		$maxdisplay          = 20;
		$maxfavs             = 20;
		$maxrecentdisplay    = 25;
		$maxsearchdisplay    = 15;
		$maxmessagedisplay   = 15;
		$MaxMessLen          = 5500;
		$fontsizemin         = 6;
		$fontsizemax         = 32;
		$MaxSigLen           = 200;
		$ClickLogTime        = 100;
		$max_log_days_old    = 30;
		$fadertime           = 1000;
		$color{'fadertext'}  = "#000000";
		$color{'faderbg'}    = "#ffffff";
		$defaultusertxt      = qq~I Love YaBB 2!~;
		$timeout             = 5;
		$HotTopic            = 10;
		$VeryHotTopic        = 25;
		$barmaxdepend        = 0;
		$barmaxnumb          = 500;
		$defaultml           = "regdate";
		$userpic_width       = 65;
		$userpic_height      = 65;
		$gzcomp              = 0;
		$gzforce             = 0;
		$use_flock           = 1;
		$faketruncation      = 0;
		$debug               = 0;
	} else {
		$forumstart = &timetostring($firstforum);
		$MaxSigLen  = $siglength || 200;
		$fadertime  = 1000;
	}

	my $filler  = q~                                                                               ~;
	my $setfile = << "EOF";
###############################################################################
# Settings.pl                                                                 #
###############################################################################
# YaBB: Yet another Bulletin Board                                            #
# Open-Source Community Software for Webmasters                               #
# Version:        YaBB 2.1                                                    #
# Released:       November 8, 2005                                            #
# Distributed by: http://www.yabbforum.com                                    #
# =========================================================================== #
# Copyright (c) 2000-2005 YaBB (www.yabbforum.com) - All Rights Reserved.     #
# Software by: The YaBB Development Team                                      #
#              with assistance from the YaBB community.                       #
# Sponsored by: Xnull Internet Media, Inc. - http://www.ximinc.com            #
#               Your source for web hosting, web design, and domains.         #
###############################################################################

########## Board Info ##########
# Note: these settings must be properly changed for YaBB to work

\$maintenance = $maintenance;				# Set to 1 to enable Maintenance mode
\$guestaccess = $guestaccess;				# Set to 0 to disallow guests from doing anything but login or register

\$mbname = q^$mbname^;					# The name of your YaBB forum
\$forumstart = "$forumstart";				# The start date of your YaBB Forum
\$Cookie_Length = $Cookie_Length;			# Default minutes to set login cookies to stay for
\$cookieusername = "$cookieusername";			# Name of the username cookie
\$cookiepassword = "$cookiepassword";			# Name of the password cookie
\$cookiesession_name = "$cookiesession_name";			# Name of the Session cookie

\$regdisable = $regdisable;				# Set to 1 to disable user registration (only admin can register)
\$RegAgree = $RegAgree;					# Set to 1 to display the registration agreement when registering
\$preregister = $preregister;				# Set to 1 to use pre-registration and account activation
\$preregspan = $preregspan;				# Time span in hours for users to account activation before cleanup.
\$emailpassword = $emailpassword;			# 0 - instant registration. 1 - password emailed to new members
\$emailnewpass = $emailnewpass;				# Set to 1 to email a new password to members if they change their email address
\$emailwelcome = $emailwelcome;				# Set to 1 to email a welcome message to users even when you have mail password turned off

\$lang = "English";					# Default Forum Language
\$default_template = "Forum default";			# Default Forum Template

\$mailprog = "$mailprog";				# Location of your sendmail program
\$smtp_server = "$smtp_server";				# Address of your SMTP-Server
\$smtp_auth_required = $smtp_auth_required;		# Set to 1 if the SMTP server requires Authorisation
\$authuser = q^$authuser^;				# Username for SMTP authorisation
\$authpass = q^$authpass^;				# Password for SMTP authorisation
\$webmaster_email = q^$webmaster_email^;		# Your email address. (eg: \$webmaster_email = q^admin\@host.com^;)
\$mailtype = $mailtype;					# Mail program to use: 0 = sendmail, 1 = SMTP, 2 = Net::SMTP

########## Layout ##########

\$maintenancetext = "$maintenancetext";			# User-defined text for Maintenance mode (leave blank for default text)
\$MenuType = $MenuType;					# 1 for text menu or anything else for images menu
\$profilebutton = $profilebutton;			# 1 to show view profile button under post, or 0 for blank
\$allow_hide_email = $allow_hide_email;			# Allow users to hide their email from public. Set 0 to disable
\$showlatestmember = $showlatestmember;			# Set to 1 to display "Welcome Newest Member" on the Board Index
\$shownewsfader = $shownewsfader;			# 1 to allow or 0 to disallow NewsFader javascript on the Board Index
							# If 0, you'll have no news at all unless you put <yabb news> tag
							# back into template.html!!!
\$Show_RecentBar = $Show_RecentBar;			# Set to 1 to display the Recent Post on Board Index
\$showmodify = $showmodify;				# Set to 1 to display "Last modified: Realname - Date" under each message
\$ShowBDescrip = $ShowBDescrip;				# Set to 1 to display board descriptions on the topic (message) index for each board
\$showuserpic = $showuserpic;				# Set to 1 to display each member's picture in the message view (by the ICQ.. etc.)
\$showusertext = $showusertext;				# Set to 1 to display each member's personal text in the message view (by the ICQ.. etc.)
\$showtopicviewers = $showtopicviewers;			# Set to 1 to display members viewing a topic
\$showtopicrepliers = $showtopicrepliers;		# Set to 1 to display members replying to a topic
\$showgenderimage = $showgenderimage;			# Set to 1 to display each member's gender in the message view (by the ICQ.. etc.)
\$showyabbcbutt = $showyabbcbutt;                       # Set to 1 to display the yabbc buttons on Posting and IM Send Pages
\$nestedquotes = $nestedquotes;                         # Set to 1 to allow quotes within quotes (0 will filter out quotes within a quoted message)
\$parseflash = $parseflash;				# Set to 1 to parse the flash tag
\$enableclicklog = $enableclicklog;                     # Set to 1 to track stats in Clicklog (this may slow your board down)


########## Feature Settings ##########

\$enable_ubbc = $enable_ubbc;				# Set to 1 if you want to enable UBBC (Uniform Bulletin Board Code)
\$enable_news = $enable_news;				# Set to 1 to turn news on, or 0 to set news off
\$allowpics = $allowpics;				# set to 1 to allow members to choose avatars in their profile
\$enable_guestposting = $enable_guestposting;		# Set to 0 if do not allow 1 is allow.
\$enable_notification = $enable_notification;		# Allow e-mail notification
\$autolinkurls = $autolinkurls;				# Set to 1 to turn URLs into links, or 0 for no auto-linking.

\$timeselected = $timeselected;				# Select your preferred output Format of Time and Date
\$timecorrection = $timecorrection;			# Set time correction for server time in seconds
\$timeoffset = $timeoffset;			# Time Offset to GMT/UTC (0 for GMT/UTC)
\$dstoffset = $dstoffset;			# Time Offset (for daylight savings time, 0 to disable DST)
\$TopAmmount = $TopAmmount;				# No. of top posters to display on the top members list
\$maxdisplay = $maxdisplay;				# Maximum of topics to display
\$maxfavs = $maxfavs;					# Maximum of favorite topics to save in a profile
\$maxrecentdisplay = $maxrecentdisplay;			# Maximum of topics to display on recent posts by a user (-1 to disable)
\$maxsearchdisplay = $maxsearchdisplay;			# Maximum of messages to display in a search query  (-1 to disable search)
\$maxmessagedisplay = $maxmessagedisplay;		# Maximum of messages to display
\$MaxMessLen = $MaxMessLen;  				# Maximum Allowed Characters in a Posts
\$fontsizemin = $fontsizemin;  				# Minimum Allowed Font height in pixels
\$fontsizemax = $fontsizemax;  				# Maximum Allowed Font height in pixels
\$MaxSigLen = $MaxSigLen;				# Maximum Allowed Characters in Signatures
\$ClickLogTime = $ClickLogTime;				# Time in minutes to log every click to your forum (longer time means larger log file size)
\$max_log_days_old = $max_log_days_old;			# If an entry in the user's log is older than ... days remove it
							# Set to 0 if you want it disabled
\$fadertime = $fadertime;				# Length in milliseconds to delay between each item in the news fader
\$color{'fadertext'}  = "$color{'fadertext'}";		# Color of text in the NewsFader (news color)
\$color{'faderbg'}  = "$color{'faderbg'}";		# Color of background in the NewsFader (news color)
\$defaultusertxt = qq~$defaultusertxt~;			# The dafault usertext visible in users posts
\$timeout = $timeout;					# Minimum time between 2 postings from the same IP
\$HotTopic = $HotTopic;					# Number of posts needed in a topic for it to be classed as "Hot"
\$VeryHotTopic = $VeryHotTopic;				# Number of posts needed in a topic for it to be classed as "Very Hot"

\$barmaxdepend = $barmaxdepend;				# Set to 1 to let bar-max-length depend on top poster or 0 to depend on a number of your choise
\$barmaxnumb = $barmaxnumb;				# Select number of post for max. bar-length in memberlist
\$defaultml = "$defaultml";


########## MemberPic Settings ##########

\$userpic_width = $userpic_width;			# Set pixel size to which the selfselected userpics are resized, 0 disables this limit
\$userpic_height = $userpic_height;			# Set pixel size to which the selfselected userpics are resized, 0 disables this limit


########## File Locking ##########

\$gzcomp = $gzcomp;					# GZip compression: 0 = No Compression, 1 = External gzip, 2 = Zlib::Compress
\$gzforce = $gzforce;					# Don't try to check whether browser supports GZip
\$use_flock = $use_flock;				# Set to 0 if your server doesn't support file locking,
							# 1 for Unix/Linux and WinNT, and 2 for Windows 95/98/ME

\$faketruncation = $faketruncation;			# Enable this option only if YaBB fails with the error:
							# "truncate() function not supported on this platform."
							# 0 to disable, 1 to enable.

\$debug = $debug;					# If set to 1 debug info is added to the template
							# tags are <yabb fileactions> and <yabb filenames>
1;
EOF

	$setfile =~ s~(.+\;)\s+(\#.+$)~$1 . substr( $filler, 0, (70-(length $1)) ) . $2 ~gem;
	$setfile =~ s~(.{64,}\;)\s+(\#.+$)~$1 . "\n   " . $2~gem;
	$setfile =~ s~^\s\s\s+(\#.+$)~substr( $filler, 0, 70 ) . $1~gem;

	open(SETTING, ">$vardir/Settings.pl");
	print SETTING $setfile;
	close(SETTING);
	if ($action eq "setinstall2") {
		$yySetLocation = qq~$set_cgi?action=setup3~;
		&redirectexit;
	}

}

sub tempstarter {
	require "Paths.pl";

	$YaBBversion = 'YaBB 2.1';

	# Make sure the module path is present
	# Some servers need all the subdirs in @INC too.
	push(@INC, "./Modules");
	push(@INC, "./Modules/Upload");
	push(@INC, "./Modules/Digest");

	if ($ENV{'SERVER_SOFTWARE'} =~ /IIS/) {
		$yyIIS = 1;
		$0 =~ m~(.*)(\\|/)~;
		$yypath = $1;
		$yypath =~ s~\\~/~g;
		chdir($yypath);
		push(@INC, $yypath);
	}

	# Requirements and Errors
	require "$vardir/Settings.pl";
	require "$vardir/advsettings.txt";
	require "$vardir/secsettings.txt";
	require "$vardir/membergroups.txt";
	if (-e "$vardir/ConvSettings.txt") { require "$vardir/ConvSettings.txt"; }
	else { $convertdir = "./Convert"; }
	require "$sourcedir/Subs.pl";
	require "$sourcedir/DateTime.pl";
	require "$sourcedir/Load.pl";

	&LoadCookie;          # Load the user's cookie (or set to guest)
	&LoadUserSettings;
	&WhatTemplate;
	&WhatLanguage;

	require "$sourcedir/Security.pl";

	&WriteLog;

}

sub CheckInstall {

	&tempstarter;

	my $install_error;
	$windowbg = "#FAFAFA";
	$header   = "#5488BA";
	$catbg    = "#DDDDDD";

	$set_missing = "";
	$set_created = "";
	if (!-e "$vardir/Settings.pl") { $set_missing .= qq~Settings.pl~; }
	else { $set_created .= qq~Settings.pl~; }

	$brd_missing = "";
	$brd_created = "";
	if (!-e "$boardsdir/forum.control") { $brd_missing .= qq~forum.control, ~; }
	else { $brd_created .= qq~forum.control, ~; }
	if (!-e "$boardsdir/forum.master") { $brd_missing .= qq~forum.master, ~; }
	else { $brd_created .= qq~forum.master, ~; }
	if (!-e "$boardsdir/forum.totals") { $brd_missing .= qq~forum.totals, ~; }
	else {
		$brd_created .= qq~forum.totals, ~;
		fopen(FORUMTOT, "$boardsdir/forum.totals");
		@totboards = <FORUMTOT>;
		fclose(FORUMTOT);
	}
	foreach $boardstot (@totboards) {
		chomp $boardstot;
		($brdname, undef, undef, undef, undef, $msgname, undef, undef, undef) = split(/\|/, $boardstot);
		if (!-e "$boardsdir/$brdname.txt") { $brd_missing .= qq~$brdname.txt~; }
		else { $brd_created .= qq~$brdname.txt~; }
	}
	$brd_missing =~ s/\, \Z//;
	$brd_created =~ s/\, \Z//;

	$mem_missing = "";
	$mem_created = "";
	if (!-e "$memberdir/admin.outbox") { $mem_missing .= qq~admin.outbox, ~; }
	else { $mem_created .= qq~admin.outbox, ~; }
	if (!-e "$memberdir/admin.vars") { $mem_missing .= qq~admin.vars, ~; }
	else { $mem_created .= qq~admin.vars, ~; }
	if (!-e "$memberdir/memberlist.txt") { $mem_missing .= qq~memberlist.txt, ~; }
	else { $mem_created .= qq~memberlist.txt, ~; }
	if (!-e "$memberdir/memberinfo.txt") { $mem_missing .= qq~memberinfo.txt, ~; }
	else { $mem_created .= qq~memberinfo.txt, ~; }
	if (!-e "$memberdir/members.ttl") { $mem_missing .= qq~members.ttl~; }
	else { $mem_created .= qq~members.ttl~; }
	$mem_missing =~ s/\, \Z//;
	$mem_created =~ s/\, \Z//;

	$msg_missing = "";
	$msg_created = "";

	if (-e "$boardsdir/forum.totals") {
		fopen(FORUMTOT, "$boardsdir/forum.totals");
		@totboards = <FORUMTOT>;
		fclose(FORUMTOT);
	}
	foreach $boardstot (@totboards) {
		chomp $boardstot;
		($brdname, undef, undef, undef, undef, $msgname, undef, undef, undef) = split(/\|/, $boardstot);
		if (!-e "$datadir/$msgname.ctb") { $msg_missing .= qq~$msgname.ctb, ~; }
		else { $msg_created .= qq~$msgname.ctb, ~; }
		if (!-e "$datadir/$msgname.txt") { $msg_missing .= qq~$msgname.txt, ~; }
		else { $msg_created .= qq~$msgname.txt~; }
	}
	$msg_missing =~ s/\, \Z//;
	$msg_created =~ s/\, \Z//;

	$var_missing = "";
	$var_created = "";
	if (!-e "$vardir/adminlog.txt") { $var_missing .= qq~adminlog.txt, ~; }
	else { $var_created .= qq~adminlog.txt, ~; }
	if (!-e "$vardir/advsettings.txt") { $var_missing .= qq~advsettings.txt, ~; }
	else { $var_created .= qq~advsettings.txt, ~; }
	if (!-e "$vardir/allowed.txt") { $var_missing .= qq~allowed.txt, ~; }
	else { $var_created .= qq~allowed.txt, ~; }
	if (!-e "$vardir/attachments.txt") { $var_missing .= qq~attachments.txt, ~; }
	else { $var_created .= qq~attachments.txt, ~; }
	if (!-e "$vardir/ban.txt") { $var_missing .= qq~ban.txt, ~; }
	else { $var_created .= qq~ban.txt, ~; }
	if (!-e "$vardir/ban_email.txt") { $var_missing .= qq~ban_email.txt, ~; }
	else { $var_created .= qq~ban_email.txt, ~; }
	if (!-e "$vardir/ban_log.txt") { $var_missing .= qq~ban_log.txt, ~; }
	else { $var_created .= qq~ban_log.txt, ~; }
	if (!-e "$vardir/ban_memname.txt") { $var_missing .= qq~ban_memname.txt, ~; }
	else { $var_created .= qq~ban_memname.txt, ~; }
	if (!-e "$vardir/clicklog.txt") { $var_missing .= qq~clicklog.txt, ~; }
	else { $var_created .= qq~clicklog.txt, ~; }
	if (!-e "$vardir/errorlog.txt") { $var_missing .= qq~errorlog.txt, ~; }
	else { $var_created .= qq~errorlog.txt, ~; }
	if (!-e "$vardir/flood.txt") { $var_missing .= qq~flood.txt, ~; }
	else { $var_created .= qq~flood.txt, ~; }
	if (!-e "$vardir/gmodsettings.txt") { $var_missing .= qq~gmodsettings.txt, ~; }
	else { $var_created .= qq~gmodsettings.txt, ~; }
	if (!-e "$vardir/Guardian.banned") { $var_missing .= qq~Guardian.banned, ~; }
	else { $var_created .= qq~Guardian.banned, ~; }
	if (!-e "$vardir/Guardian.settings") { $var_missing .= qq~Guardian.settings, ~; }
	else { $var_created .= qq~Guardian.settings, ~; }
	if (!-e "$vardir/log.txt") { $var_missing .= qq~log.txt, ~; }
	else { $var_created .= qq~log.txt, ~; }
	if (!-e "$vardir/membergroups.txt") { $var_missing .= qq~membergroups.txt, ~; }
	else { $var_created .= qq~membergroups.txt, ~; }
	if (!-e "$vardir/modlist.txt") { $var_missing .= qq~modlist.txt, ~; }
	else { $var_created .= qq~modlist.txt, ~; }
	if (!-e "$vardir/news.txt") { $var_missing .= qq~news.txt, ~; }
	else { $var_created .= qq~news.txt, ~; }
	if (!-e "$vardir/oldestmes.txt") { $var_missing .= qq~oldestmes.txt, ~; }
	else { $var_created .= qq~oldestmes.txt, ~; }
	if (!-e "$vardir/registration.log") { $var_missing .= qq~registration.log, ~; }
	else { $var_created .= qq~registration.log, ~; }
	if (!-e "$vardir/reserve.txt") { $var_missing .= qq~reserve.txt, ~; }
	else { $var_created .= qq~reserve.txt, ~; }
	if (!-e "$vardir/reservecfg.txt") { $var_missing .= qq~reservecfg.txt, ~; }
	else { $var_created .= qq~reservecfg.txt, ~; }
	if (!-e "$vardir/secsettings.txt") { $var_missing .= qq~secsettings.txt, ~; }
	else { $var_created .= qq~secsettings.txt, ~; }
	if (!-e "$vardir/Smilies.txt") { $var_missing .= qq~Smilies.txt~; }
	else { $var_created .= qq~Smilies.txt~; }
	$var_missing =~ s/\, \Z//;
	$var_created =~ s/\, \Z//;

	$yymain .= qq~
<div class="boardcontainer">
	<table width="100%" border="0" cellspacing="1" cellpadding="4">
	<tr><td width="100%" colspan="2" class="titlebg" align="left">
	Checking System Files
	</td></tr>
	<tr><td width="100%" class="catbg" colspan="2" align="left">
	~;
	if ($no_brddir) {
		$install_error = 1;
		$yymain .= qq~
	A problem has occurred in the /Boards folder!
	</td></tr>
	<tr><td width="6%" class="windowbg" align="center">
	<img src="$imagesdir/on.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	No /Boards folder available!
	</td></tr>
~;
	} else {
		if ($brd_missing) {
			$install_error = 1;
			$yymain .= qq~
	A problem has occurred in the /Boards folder!
	</td></tr>
	<tr><td width="6%" class="windowbg" align="left">
	<img src="$imagesdir/on.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	<b>Missing: </b><br />
	$brd_missing
	</td></tr>
~;
		}
		if ($brd_created) {
			if (!$brd_missing) {
				$yymain .= qq~
	Successfully checked the /Boards folder!
	</td></tr>
~;
			}
			$yymain .= qq~
	<tr><td width="6%" class="windowbg" align="center">
	<img src="$imagesdir/off.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	<b>Installed: </b><br />
	$brd_created
	</td></tr>
~;
		}
	}
	$yymain .= qq~
	<tr><td width="100%" class="catbg" colspan="2" align="left">
~;

	if ($no_memdir) {
		$install_error = 1;
		$yymain .= qq~
	A Problem has occurred in the /Members folder!
	</td></tr>
	<tr><td width="6%" class="windowbg" align="center">
	<img src="$imagesdir/on.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	No /Members folder available!
	</td></tr>
~;
	} else {
		if ($mem_missing) {
			$install_error = 1;
			$yymain .= qq~
	A problem has occurred in the /Members folder!
	</td></tr>
	<tr><td width="6%" class="windowbg" align="center">
	<img src="$imagesdir/on.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	<b>Missing: </b><br />
	$mem_missing
	</td></tr>
~;
		}
		if ($mem_created) {
			if (!$mem_missing) {
				$yymain .= qq~
	Successfully checked the /Members folder!
	</td></tr>
~;
			}
			$yymain .= qq~
	<tr><td width="6%" class="windowbg" align="center">
	<img src="$imagesdir/off.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	<b>Installed: </b><br />
	$mem_created
	</td></tr>
~;
		}
	}
	$yymain .= qq~
	<tr><td width="100%" class="catbg" colspan="2" align="left">
~;

	if ($no_mesdir) {
		$install_error = 1;
		$yymain .= qq~
	A problem has occurred in the /Messages folder!
	</td></tr>
	<tr><td width="6%" class="windowbg" align="center">
	<img src="$imagesdir/on.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	No /Messages folder available!
	</td></tr>
~;
	} else {
		if ($msg_missing) {
			$install_error = 1;
			$yymain .= qq~
	A problem has occurred in the /Messages folder!
	</td></tr>
	<tr><td width="6%" class="windowbg" align="center">
	<img src="$imagesdir/on.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	<b>Missing: </b><br />
	$msg_missing
	</td></tr>
~;
		}
		if ($msg_created) {
			if (!$msg_missing) {
				$yymain .= qq~
	Successfully checked the /Messages folder!
	</td></tr>
~;
			}
			$yymain .= qq~
	<tr><td width="6%" class="windowbg" align="center">
	<img src="$imagesdir/off.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	<b>Installed: </b><br />
	$msg_created
	</td></tr>
~;
		}
	}
	$yymain .= qq~
	<tr><td width="100%" class="catbg" colspan="2" align="left">
~;
	if ($no_vardir) {
		$install_error = 1;
		$yymain .= qq~
	A problem has occurred in the /Variables folder!
	</td></tr>
	<tr><td width="6%" class="windowbg" align="center">
	<img src="$imagesdir/on.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	No /Variables folder available!
	</td></tr>
~;
	} else {
		if ($var_missing) {
			$install_error = 1;
			$yymain .= qq~
	A problem has occurred in the /Variables folder!
	</td></tr>
	<tr><td width="6%" class="windowbg" align="center">
	<img src="$imagesdir/on.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	<b>Missing: </b><br />
	$var_missing
	</td></tr>
~;
		}
		if ($var_created) {
			if (!$var_missing) {
				$yymain .= qq~
	Successfully checked the /Variables folder!
	</td></tr>
~;
			}
			$yymain .= qq~
	<tr><td width="6%" class="windowbg" align="center">
	<img src="$imagesdir/off.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	<b>Installed: </b><br />
	$var_created
	</td></tr>
~;
		}
	}

	$yymain .= qq~
	<tr><td width="100%" class="catbg" colspan="2" align="left">
~;

	if ($set_missing) {
		$install_error = 1;
		$yymain .= qq~
	A problem has occurred while creating Settings.pl!
	</td></tr>
~;
	}
	if ($set_created) {
		$yymain .= qq~
	Successfully checked Settings.pl!
	</td></tr>
	<tr><td width="6%" class="windowbg" align="center">
	<img src="$imagesdir/off.gif" alt="" />
	</td><td width="94%" class="windowbg2" align="left">
	Click on 'Continue' to go to your Admin Center - Forum Settings to set the options for your YaBB 2.
	</td></tr>
~;
	}

	if (!$install_error) {

		$yymain .= qq~
	<tr><td width="100%" class="catbg" colspan="2" align="center">
	<form action="$set_cgi?action=ready" method="post" style="display: inline;">
		<input type="submit" value="Continue" />
	</form>
	</td></tr>
~;
	} else {
		$yymain .= qq~
	<tr><td width="100%" class="catbg" colspan="2" align="left">
	<div style="float: left; width: 98%; font-family: verdana; color: #900000; font-size: 11px; padding: 2px;"><b>One or more errors occurred while checking the system files. The problems must be solved before you may continue.</b></div>
	</td></tr>
~;
	}
	$yymain .= qq~
	</table>
</div>
~;
	$yyim    = "You are running YaBB 2 Setup.";
	$yytitle = "YaBB 2 Setup";
	&SetupTemplate;
	exit;
}

sub ready {

	if (-e "$vardir/Setup.lock") {
		&FoundLock2;
	}

	if (-e "AdminIndex.pl") { $start = "AdminIndex.pl"; }
	else { $start = "AdminIndex.cgi"; }
	&CreateLock2;

	$yySetLocation = qq~$start?action=modsettings~;
	&redirectexit;

}

sub CreateLock {
	fopen("LOCKFILE", ">$vardir/Converter.lock");
	print LOCKFILE q~This is a lockfile for the Converter.\n~;
	print LOCKFILE q~It prevents it being run again after it has been run once.\n~;
	print LOCKFILE q~Delete this file if you want to run the Converter again.~;

	fclose("LOCKFILE");
}

sub CreateLock2 {
	fopen("LOCKFILE", ">$vardir/Setup.lock");
	print LOCKFILE q~This is a lockfile for the Setup Utility.\n~;
	print LOCKFILE q~It prevents it being run again after it has been run once.\n~;
	print LOCKFILE q~Delete this file if you want to run the Setup Utility again.~;

	fclose("LOCKFILE");
}

sub SetupImgLoc {
	if (!-e "$forumstylesdir/$useimages/$_[0]") { $thisimgloc = qq~img src="$forumstylesurl/default/$_[0]"~; }
	else { $thisimgloc = qq~img src="$imagesdir/$_[0]"~; }
	return $thisimgloc;
}

sub SetupTemplate {
	if ($yySetCookies1 || $yySetCookies2 || $yySetCookies3) {
		$cookiewritten = "Cookie Set";
		print header(
			-status  => '200 OK',
			-cookie  => [$yySetCookies1, $yySetCookies2, $yySetCookies3],
			-charset => $yycharset);
	} else {
		print header(
			-status  => '200 OK',
			-charset => $yycharset);
	}

	$yyposition = $yytitle;
	$yytitle    = "$mbname - $yytitle";

	$yyimages        = $imagesdir;
	$yydefaultimages = $defaultimagesdir;
	$yystyle         = qq~<link rel="stylesheet" href="$forumstylesurl/$usestyle.css" type="text/css" />~;
	$yystylesheet    = qq~<link rel="stylesheet" href="$forumstylesurl/$usestyle.css" type="text/css" />~;
	$yystyle      =~ s~$usestyle\/~~g;
	$yystylesheet =~ s~$usestyle\/~~g;

	$yytemplate = "$templatesdir/$usehead/$usehead.html";
	fopen(TEMPLATE, "$yytemplate") || die("$maintxt{'23'}: $testfile");
	@yytemplate = <TEMPLATE>;
	fclose(TEMPLATE);

	my $output = '';
	$yyboardname = "$mbname";
	$yytime      = &timeformat($date, 1);
	$yyuname     = $iamguest ? qq~~ : qq~$maintxt{'247'} $realname, ~;
	if ($enable_news) {
		fopen(NEWS, "$vardir/news.txt");
		@newsmessages = <NEWS>;
		fclose(NEWS);
	}
	for (my $i = 0; $i <= $#yytemplate; $i++) {
		$curline = $yytemplate[$i];
		if (!$yycopyin && $curline =~ m~<yabb copyright>~) { $yycopyin = 1; }
		if ($curline =~ m~<yabb newstitle>~ && $enable_news) {
			$yynewstitle = qq~<b>$maintxt{'102'}:</b> ~;
		}
		if ($curline =~ m~<yabb news>~ && $enable_news) {
			srand;
			if ($shownewsfader == 1) {

				$fadedelay = ($maxsteps * $stepdelay);
				$yynews .= qq~
				<script language="JavaScript1.2" type="text/javascript">
					<!--
						var maxsteps = "$maxsteps";
						var stepdelay = "$stepdelay";
						var fadelinks = $fadelinks;
						var delay = "$fadedelay";
						var bcolor = "$color{'faderbg'}";
						var tcolor = "$color{'fadertext'}";
						var fcontent = new Array();
						var begintag = "";~;
				fopen(NEWS, "$vardir/news.txt");
				@newsmessages = <NEWS>;
				fclose(NEWS);
				for (my $j = 0; $j < @newsmessages; $j++) {
					$newsmessages[$j] =~ s/\n|\r//g;
					if ($newsmessages[$j] eq '') { next; }
					if ($i != 0) { $yymain .= qq~\n~; }
					$message = $newsmessages[$j];
					if ($enable_ubbc) {
						if (!$yyYaBBCloaded) { require "$sourcedir/YaBBC.pl"; }
						&DoUBBC;
					}
					$message =~ s/\"/\\\"/g;    # "
					$yynews .= qq~
						fcontent[$j] = "$message";\n~;
				}
				$yynews .= qq~
						var closetag = '';
						//window.onload = fade;
					// -->
				</script>
				<script language="JavaScript1.2" type="text/javascript" src="$faderpath"></script>
				~;
			} else {
				if ($enable_ubbc) {
					if (!$yyYaBBCloaded) { require "$sourcedir/YaBBC.pl"; }
					&DoUBBC;
				}
				$message = $newsmessages[int rand(@newsmessages)];
				&DoUBBC;
				$ubbcnews = $message;
				$yynews   = qq~$ubbcnews~;
			}
		}
		$yyurl = $scripturl;
		$curline =~ s~<yabb\s+(\w+)>~${"yy$1"}~g;
		$curline =~ s~img src\=\"$imagesdir\/(.+?)\"~&SetupImgLoc($1)~eisg;
		$curline =~ s~alt\=\"(.*?)\"~alt\=\"$1\" title\=\"$1\"~ig;
		$output .= $curline;
	}
	if ($yycopyin == 0) {
		$output = q~<center><h1><b>Sorry, the copyright tag <yabb copyright> must be in the template.<br />Please notify this forum's administrator that this site is using an ILLEGAL copy of YaBB!</b></h1></center>~;
	}
	print $output;
}

1;
