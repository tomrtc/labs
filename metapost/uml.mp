prologues := 2;
input metauml;

beginfig(1);
Class.A("Point")
       ("+x: int",
        "+y: int") ();

Class.B("Circle")
       ("radius: int")
       ("+getRadius(): int",
        "+setRadius(r: int):void");

topToBottom(45)(A, B);

drawObjects(A, B);

clink(aggregationUni)(A, B)
endfig;

end;
end
