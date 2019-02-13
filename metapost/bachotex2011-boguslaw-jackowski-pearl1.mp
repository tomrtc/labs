%%% Computing the area surrounded by a cyclic path

% The following code computes the area surrounded by a cyclic path.
% Its idea is stemmed from the discussion between Dan Luecking and Larry
% Siebenmann on MF/MP Discussion List.

vardef area(expr p) = % p is a Bézier segment; result = \int y dx
	save xa, xb, xc, xd, ya, yb, yc, yd;
	(xa,20ya)=point 0 of p;
	(xb,20yb)=postcontrol 0 of p;
	(xc,20yc)=precontrol 1 of p;
	(xd,20yd)=point 1 of p;
	(xb-xa)*(10ya + 6yb + 3yc + yd)
	+(xc-xb)*( 4ya + 6yb + 6yc + 4yd)
	+(xd-xc)*( ya + 3yb + 6yc + 10yd)
enddef;

vardef Area(expr P) = % P is a cyclic path; result = area of the interior
  area(subpath (0,1) of P)
  for t=1 upto length(P)-1: + area(subpath (t,t+1) of P) endfor
enddef;

show Area(fullcircle scaled 200);

% log file:
% >> -31415.46234

end.

