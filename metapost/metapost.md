# MetaPost information

Metapost can be found in texlive Tex distribution.


  * shell main command is mp or mpost.
  * makempx:
  a control program that extracts the text parts of your .mp files and converts the lower level commands to a file with .mpx extension.  This extraction is only done when the
  .mpx file is older than the .mp file.  Once extraction is accomplished the following two programs take care of the conversion.

* mpto:
this program extracts the btex ... etex parts and the verbatimtex ... etex
parts from your .mp file and places them in a texfile.

* dvitomp:
converts .dvi files to .mpx files.

## emacs integration ##


  * [metapost-mode+.el](metapost-mode+.el "emacs-lisp support file.")


## imagemagick ##

  * convert a metapost pdf result file to another image format :
	`-trim +repage` option removes unnecessary whitespace surrounding the image and sets the canvas size to the trimmed image.

        convert -trim +repage result.pdf result.png
  * Adjust the dot-per-inch of the image with `-density`:

        convert -density 300 -trim +repage result.pdf result.png

  * EPS from command line :  creates code-1.eps which is not only proper EPS, but also
  has any used font glyphs embedded.

        mpost -s 'prologues=3"' -s 'outputtemplate="%j-%c.%o"' code.mp

  * SVG

        mpost -s 'outputformat="svg"'  -s 'outputtemplate="%j-%c.%o"' code.mp

## potrace ##


> [Potrace](http://potrace.sourceforge.net/ "apt install potrace") is a tool for  tracing a bitmap, which means, transforming
> a bitmap  into a smooth, scalable  image. The input is  a bitmap (PBM,
> PGM, PPM, or BMP format), and the output is one of several vector file
> formats. A  typical use  is to  create SVG or  PDF files  from scanned
> data, such as company or university logos, handwritten notes, etc. The
> resulting image is not "jaggy" like a bitmap, but smooth. It can then
> be rendered at any resolution.

  * potrace and mkbitmap commands could be investigated to help with metpost.

## EPS postscript ##

**metapost** needs a special variable `prologues` to be set to 2 or 3 (?) when generating standalone eps file.

	``` tex
	%Create file figure.mp with editor (
	% starts comment):
	prologues := 3;    % set up MetaPost for EPS generation

	beginfig(14)
	  draw origin
		withpen pencircle scaled 1mm;

		  draw unitsquare scaled 10cm;
		 % draw (0,0)--(10,0)--(20,10)--(0,10)--(0,0);
		  draw (60,60)--(80,60)--(80,80)--(60,80)--(60,60);
		  draw (40,40)--(100,40)--(100,100)--(40,100)--(40,40);
		  draw (40,40)--(60,60);
		  draw (100,40)--(80,60);
		  draw (100,100)--(80,80);
		  draw (40,100)--(60,80);
		endfig;
		end                % end of MetaPost run
		%Units:  PostScript Points (1/72 in = 0.352777. . . mm)
		```

pstoedit - a tool converting PostScript and PDF files into various vector graphic formats
with mpost - MetaPost format
mftrace - Scalable Fonts for MetaFont
mftrace is a small Python program that lets you trace a TeX bitmap font into a PFA or PFB font (A PostScript Type1 Scalable Font) or TTF (TrueType) font. It is licensed under the GNU GPL.

Scalable fonts offer many advantages over bitmaps, as they allow documents to render correctly at many printer resolutions. Moreover, Ghostscript can generate much better PDF, if given scalable PostScript fonts.
fonts:
FontForge (formerly known as PfaEdit) is a freeware font editor for Unix and Linux that can create and edit TrueType, OpenType, bitmap (.bdf) and some PostScript fonts, and can also convert between formats.
birdfont/stable 2.18.3-1 amd64
  font editor that lets you create outline vector graphics and export fonts

Font Manager
Simple font management for GTK+ desktop environments

https://fontmanager.github.io/

GNU FreeFont : GNU FreeFont is a free family of scalable outline fonts, suitable for general use on computers and for desktop publishing. It is Unicode-encoded for compatibility with all modern operating systems.

The fonts are ready to use, avalable in TrueType and OpenType formats.


https://www.gnu.org/software/freefont/

https://www.dafont.com/dustismo.font
Toga Sans™ is the result of an evening spent with the Bitstream Vera Sans™ font and the pfaedit program. Essentially, just as Microsoft's Tahoma™ was designed as a screen-width friendlier version of their Verdana™ (and Bitstream Vera Sans appears to have been strongly influenced by Verdana), Toga is meant to be a narrower version of Vera. In fact, that's what it literally is, because I ended up just using "scale" on all the characters and then adjusting the metrics accordingly. It looks better than the versions where I tried to manually adjust kern pairs and whatnot. Per the Bitstream Vera Sans license agreement, I have renamed the font and removed Bitstream's name despite the minimal amount of work I did, and this modified font is available under the same terms.
Toga Sans™ is the result of an evening spent with the Bitstream Vera Sans™ font and the pfaedit program. Essentially, just as Microsoft's Tahoma™ was designed as a screen-width friendlier version of their Verdana™ (and Bitstream Vera Sans appears to have been strongly influenced by Verdana), Toga is meant to be a narrower version of Vera. In fact, that's what it literally is, because I ended up just using "scale" on all the characters and then adjusting the metrics accordingly. It looks better than the versions where I tried to manually adjust kern pairs and whatnot. Per the Bitstream Vera Sans license agreement, I have renamed the font and removed Bitstream's name despite the minimal amount of work I did, and this modified font is available under the same terms.


http://home.kabelfoon.nl/~slam/fonts/fonts.html
http://home.kabelfoon.nl/~slam/fonts/fonts.html
Legendum is a typeface not unlike MS's Verdana. It has been made for optimal screen readibility. It is anti-aliased on Windows as well. It now contains all normal Latin characters as well as all Greek characters, plus punctuation. All are instructed. I use this permanently on my desktop, and have found it extremely nice to look at for longer stretches of time.

It has the OpenType features "kern" (kerning), "lnum" (lining numerals; it's got old-style numerals by default), "mark"/"mkmk" (mark positioning). It comes in two flavours: Legendum_legacy.otf has hundreds of precomposed characters (such as é) while Legendum.otf does not, but OpenType-aware OSes and/or applications may be able to produce them anyway. The file for the latter is a lot smaller though. Most of the Latin combined glyphs are there, and (I think) all of the Greek ones (including polytonic Greek).

Garogier is a typeface not unlike Garamond (hence the name). It will have optical size through its instructions (i.e. at low sizes, also when it is used on a high-resolution device such as a printer, the characters are changed slightly to increase readablity). This is supported by Windows 98 and up (this includes 2000 and XP). Freetype does not support it (yet).

I have not supplied a hinted version for the moment though. The font has kerning through the OpenType feature "kern", supports ligatures ("liga"), both lining and old-style figures ("lnum") and small caps ("smcp"). If you are used to looking at Greek letters: I'd like to have some feedback on those before I go on to instruct them.
http://junicode.sourceforge.net/
 Junicode (short for Junius-Unicode) is a TrueType/OpenType font for medievalists with extensive coverage of the Latin Unicode ranges, plus Runic and Gothic. The font comes in four faces. Of these, regular and italic are fullest, featuring complete implementation of the Medieval Unicode Font Initiative recommendation, version 4.0. The bold and bold italic faces are less full.

For lovers of old-style type, the Junicode package also includes Foulis Greek, an eighteenth-century Greek typeface with numerous variants and old-style ligatures.
