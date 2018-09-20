prologues := 3;


def tt_penstroke text t =
 forsuffixes e = l,r: path_.e:=t; endfor
 filldraw path_.l -- reverse path_.r -- cycle enddef;
path path_.l,path_.r;

beginfig(1);
u = cm;
w = 3u;
h = 7u;

y1 = h; y2 = y4 = 0;
x1 = x4 = .9 w; x2 = 0;
x3 = .7w; y3 = .52h;

pair s, t; s = dir 124; t = dir 220;

penpos1(5mm,30);
penpos2(4mm,45);
penpos3(3mm,30);
penpos4(2mm,45);

pickup pensquare rotated 65 scaled 3;
tt_penstroke z1e{t} ..{s}z2e;
tt_penstroke z2e{-s} .. z3e{right} .. {s}z4e;
draw (-90/360*1.5cm,-0.9cm) for i = -90 upto 1060: .. (i/360*1.5cm,0.9cm*(sind(i))) endfor;
endfig;
end.
