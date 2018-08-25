nsert–1: // insert record data into the queue.
counter = 1 ;
insertQ(start = start(data), step = step(data), counter++ ) ;


nsert–2: // breath-first tree traversal looking for unfilled table location
fetchQ(location, step, position ) ;
if location designates an unfilled bucket then goto insert–3.
insertQ( (location + step ) mod tableSize, step, counter++ ) ;
for ( i = 0 ; i < b ; i ++ )
insertQ( (location + step( table[location][i ] ) mod tableSize ,
step( table[location][i ] ) , counter++ ) ;
         continue insert–2 ;


         nsert–3: // determine path from tree position to root.
for ( i = 0, path [0] = position ; path[i ] > 1 ; i ++ )
path[i +1] = ( path[i ] + b – 1 )/(b+1) ;



         insert–4: // reorganize records following path from root to position
for ( index = start ; index != location ;
index = ( index + step ) mod tableSize )
if ( ( slot = ( path[ – –i ] – 2 ) ) mod (b+1) > 0 ) // not leftmost link.
step = step( table[index][slot – 1] ) ;
swap ( table[index][slot – 1], data ) ;



         nsert–5: // insert data into location in first available slot.
table[location][first available slot] = data ;


         insert ( Data & data )
shortest = n + 1 ;
for ( j = 0 ; j < d ; j ++ ) {
value = hash[ j ] ( data );
start [ j ] = value % n ;
stride[ j ] = 1 + value % ( n − 1) ;
variety [ j ] = ( value/( n ∗ ( n − 1)) ) % s ;
count = 0, location [ j ] = ( start [ j ] − stride[ j ] ) % n ;
do { location [ j ] = ( location [ j ] + stride [ j ] ) % n ;
if duplicate found in bucket [ location [ j ] ] return false ;
count + + ;
} while bucket [ location ] is full ;
if count < shortest
shortest = count, best [ 0 ] = j, number = 1 ;
elseif count == shortest
best [ number + + ] = j ;
}
// for ( ...
select one shortest path j from best array.
copy data into bucket [ location [ j ] ] ;
predictor [ variety [ j ] ] = true ; return true ;
Figure 1: Insert method

         etch ( Data & data )
for ( j = 0 , number = 0 ; j < d ; j ++ ) {
value = hash[ j ] ( data ) ;
start [ j ] = value % n ;
stride[ j ] = 1 + value % ( n − 1) ;
variety = ( value/( n ∗ ( n − 1) ) ) % s ;
location[j ] = ( start[j ] − stride[j ] ) % n ;
if predictor[variety] is set
which[number++] = j ;
}
// for ( ... select possible paths.
while ( number > 0 ) {
for (j = 0; number > 0; j++) {
location[j ] = (location[j ] + stride[j ]) % n ;
if bucket[location[j ]] contains data
copy data record and return true ;
if bucket[location[j ]] is not full
remove probe sequence which[j ] from
consideration and decrement number.
}
// for ( ...
}
// while ( ...
return false ;
Figure 3: Fetch method



         nsert ( Data & data )
shortest = n + 1 ;
for ( j = 0 ; j < d ; j ++ ) {
value = hash[ j ] ( data );
start [ j ] = value % n ;
stride[ j ] = 1 + value % ( n − 1) ;
variety [ j ] = ( value/( n ∗ ( n − 1)) ) % s ;
count = 0, location [ j ] = ( start [ j ] − stride[ j ] ) % n ;
do { location [ j ] = ( location [ j ] + stride [ j ] ) % n ;
if duplicate found in bucket [ location [ j ] ] return false ;
count + + ;
} while bucket [ location ] is full ;
if count < shortest
shortest = count, best [ 0 ] = j, number = 1 ;
elseif count == shortest
best [ number + + ] = j ;
}
// for ( ...

         select one shortest path j from best array.
copy data into bucket [ location [ j ] ] ;
predictor [ variety [ j ] ] = true ; return true ;




         fetch ( Data & data )
for ( j = 0 , number = 0 ; j < d ; j ++ ) {
value = hash[ j ] ( data ) ;
start [ j ] = value % n ;
stride[ j ] = 1 + value % ( n − 1) ;
variety = ( value/( n ∗ ( n − 1) ) ) % s ;
location[j ] = ( start[j ] − stride[j ] ) % n ;
if predictor[variety] is set
which[number++] = j ;
}
// for ( ... select possible paths.
while ( number > 0 ) {
for (j = 0; number > 0; j++) {
location[j ] = (location[j ] + stride[j ]) % n ;
if bucket[location[j ]] contains data
copy data record and return true ;
if bucket[location[j ]] is not full
remove probe sequence which[j ] from
consideration and decrement number.
}
// for ( ...
}
// while ( ...
return false ;
Figure 3. Fetch method

         We present two table data type methods insert and fetch
for double hashing with choice in figures 1 and 3. The al-
gorithms are very similar to those of ordinary double hash-
ing with the exception that the insert method must try all
d probe sequences to select a shortest access path and the
fetch method must interleave the appropriate subset of the
d probe sequences. In both figures, the hash array, contains
pointers to each of the d hash functions; this array is ini-
tialized during table construction. The for statement, in fig-ure 1, determines which of the d probe sequences will lo-
cate an un-filled bucket with the fewest probes; the indices
of these shortest probe sequences are stored within the array
best. The second phase of the insert involves selecting one
of the shortest probe sequences j to use to store the record.
There are several strategies possible here; we view the best
entries to be equally-likely to be chosen. Finally the data is
copied into bucket [ location [ j ] ] and the variety [ j ] pre-
dictor is set.

         The pseudo-code shows the simplicity of the scheme; the passbit selection pbit is independent of both the index
and step values.
boolean
access ( Table table, Data dat )
{
unsigned val , index , step , pbir ;
if ( table == NULL ) return false ;
val = table->value ( data ) ;
index = val mod table->size ;
step = val mod ( table->size - 1 ) + 1 ;
pbit = ( val/(table->size*(table->size-1)) ) mod table->g ;
.....
}
PROcedure store (k:integer);
  label 1, 2;
  const empty = 0 {means empty];
  a, kw, aw, i, n, b, q, qw:integer;
  procedure updatepredictor;
     w:integer;
  begin if q > max then w := max else w  :=  q;
         if w () qw then L[a, n] := w
  end;
begin {main procedure}
  a := h'(0, k);
  if T[a] = empty then storeitem (a, k) {completed}
  ELSE
   begin kw := T(a]; aw := h'(0, kw);
      if aw ( ) a then {displace the nonsynonym key kw}
     begin L[a, g(kw)] := 0; storeitem (a, k);
            k:=kw;a:=aw
     end;




  [hereafter, k:the key to be stored, a:the home address of k}
          n := g(k); b := a; i := O;
        1:q := L[b, n]; qw := q;
        ifq=O
        then {search empty cell}
           repeat q := q + 1; i := i + 1;
                   if i > M then table-full else b := h'(i, k)
           until T[b] = empty
        else {trace the cluster}
           2:if i + q > M then table-full
              else begin b := h'(i + q, k);
                    if h'(0, T[b]) = a and g(T[b]) = n
                    then begin J := J + q; updatepredictor; goto 1
                       end
                    else begin q := q + 1;
                                if T[b] () empty then goto 2
                       end
                 end;
       storeitem (b, k); updatepredictor {completed}  
     end






procedure search (k:integer);                
   a, b, n, i:integer;
begin
   a := h'(0, k); b := a; n := g(k); i := O;
   while T[b] ( ) k and L[b, n] ( ) 0
   {that is, not equal to k, but synonyms are not exhausted}
   do begin q := L[b, n]; i := i + q; b := h'(i, k);
      if q > = max
      then while h'(O, T[b]) () a or g(T[b]) () n
          [search a synonym one-by-one]
         do begin i := i + 1; b := h'(i, k) end;
      end;
   if T[b] = k then found else not-found
