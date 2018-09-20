%% Rémy Tomasetto - fleur géometrique.
prologues := 3;    % set up MetaPost for EPS generation
beginfig(14)
  draw origin
    withpen pencircle scaled 1mm;
  for i=0 upto 100:
    fill unitsquare
      scaled ((100-i)*0.1mm)
      rotated 31i
      withcolor (0.01i)[black,green];
  endfor;
  p = halfcircle scaled 30mm;
  pickup defaultpen;
  drawarrow p;
  q = subpath(1,3) of p;
  draw origin--point 0 of q withcolor green;
  draw origin--point infinity of q withcolor blue;
endfig;
end                % end of MetaPost run
%Units:  PostScript Points (1/72 in = 0.352777. . . mm)
