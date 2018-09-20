%Create file fig.mp with editor (
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
  pickup pencircle scaled 10;
  draw  fullcircle  scaled 150 withcolor.3white;
  linecap := butt;
  ahlength := 25;
  drawarrow halfcircle scaled 150 withcolor green;

endfig;
end                % end of MetaPost run
%Units:  PostScript Points (1/72 in = 0.352777. . . mm)
