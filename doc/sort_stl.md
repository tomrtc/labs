
* stable sorting : do not change the sub ordering. 
    merge sort
std::stable_sort(first, last)
    
partial sorting : 
    heap sort : nice sort but less acceptable solution for today memory hierachy timings.
    Computer performance are very difficult to think about, they do a lot of operations in the same cycle ; an the bottleneck of memory create discontinuous behavior.
std::partial_sort(first, last, last) : sort everything.

unstable sorting: 
    quick sort : not stable
    
    
shape patterns in inuput : distributions
uniform ramdom input 
already sorted input : some sort algorithm are bad at this pattern.
reverse sorted input.
zip distribution ; 
incorrect but description : the most problabe inpute is 1 then the second is 1/2 the thrird 1/3 ... n 1/n . a serie of harmonic.
wrong beacause the sum of probabilities is 1!!
more correct ; item i, 1/i divided by stirling number(n)
