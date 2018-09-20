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
