# MetaPost information


## emacs integration ##


  * [metapost-mode+.el](metapost-mode+.el "emacs-lisp support file.")


## imagemagick ##

  * convert a metapost pdf result file to another image format :
	`-trim +repage` option removes unnecessary whitespace surrounding the image and sets the canvas size to the trimmed image.

        convert -trim +repage result.pdf result.png
  * Adjust the dot-per-inch of the image with `-density`:

        convert -density 300 -trim +repage result.pdf result.png


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
