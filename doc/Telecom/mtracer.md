TEL/Monitel Off-loading work

Formatting strings, required by mtracer log system, is a slow operation.
One possible improvement is moving all string formatting to the mtracer process.  The producer half-com assembles a list of arguments; the log_statement in the pseudo-code is a small container like boost::variant items.
A compile time parsing of mtracer format string can allow to store parameters as-is with copy of generated strings only. Static string should be possibly optimized too.
Profiling revealed that copying the format string was a significant part of the overall time. Since a string is a dynamic container it had to be dynamically copied. It seemed wasteful since the format strings are actually static: they are known at compile time.
To avoid dynamic allocation of individual log items the number of parameters had to be limited. The lower the number the better, but I still needed enough to be useful on around 8 parameters. 
The solution is to pass pointers to the strings instead. I was a bit concerned about the safety of this approach. If somebody accidentally called this with a dynamic string the consumer would end up reading a dangling pointer. I created a literal_string type; the log system only accepted this type and it couldn’t be created implicitly.

I took advantage of a macro trick to make this easy to use. C++ offers automatic string concatenation of string literals.
#define SAFE_LITERAL( str ) "" str

#define WRITE_LOG( str, ... ) write_log( literal_string( SAFE_LITERAL(str) ), params( VA_ARGS ) )

The "" str part only works if str is an actual string literal. The second macro can be used without worrying about the string literal aspect. If something other than a literal is passed compilation fails.

This approach improved the performance of logging.

The consumer thread is responsible for formatting these parameters and writing the string to the log file.
There was a dangerous problem waiting in this logic. While the consumer is formatting strings it isn’t reading from the buffers. This gives the buffer a longer time to accumulate a high number of log items, resulting in more memory overhead.
This large buffer in turn causes the consumer to take even more time formatting, thus the subsequent buffer could be even larger!

The system had lots of idle time; activity came in bursts. It was important for the consumer to cleanup the buffers quickly during those bursts, and then use the idle time to format the strings.
I made the consumer write only a few log statements before returning to buffer cleanup. This slowed down the consumer with added locks and memory use, but prevented the worst of the burst problem.
The producer writes to the head of the ring buffer. After each write it updates the position of the head. The consumer reads from the tail of the buffer. After each read it updates the tail of the buffer. So long as the head never catches up the tail we’re fine.

This works because both the producer and consumer know they can safely access the memory without any kind of lock. Each log_statement in the buffer is distinct and can safely be accessed in parallel. The information about tail and head position is maintained with atomic operations.

    Now, even more than on lock-free, a strong understanding of memory ordering is required. Having two threads access the ring buffer in this manner, in a low-latency system, leaves absolutely no room for error. Recall the consumer is continually polling so it immediately sees changes. I didn’t have access to C++11’s atomic type at the time. I was relying on GCC internals.

I wasn’t content to leave open the “what happens” scenario should the head catch up to the tail. I built in a warning when it reached 2/3 full. If somehow it caught up it then fell back to an actual mutex lock and waited for the consumer to catch up. The consumer placed a priority on cleaning out these buffers. As often as possible it moved data out of the ring buffer into its own private memory. Thus the warning only sometimes appeared, and the actual lock almost never. I used the occurrence of the warning as an indication to increase buffer size.

This approach, without any kind locks, is known as a “wait-free” algorithm. It provides a guarantee that the producer will always make progress. It never waits. For real-time systems this is great since it means the logging code path has a nearly constant execution time (near-zero variance). For us that was around 70ns.
A note on the consumer

The consumer takes the brunt of the work. It’s priority is to clean out the ring buffers. This results in having a lot of pending log statements from several threads mixed up in memory. Before actually writing to the log it needs to sort these statements. This could of course be done afterwards, but we wanted to have sane logs we could look at in real-time.

Formatting was quite slow, so the consumer actually falls behind the producers during any burst of activity. Fortunately, the behaviour of low-latency trading leaves a lot of essentially idle time. When something happens all cores are utilized to 100% for several milliseconds, but then long periods, even up to 1 second, can pass with nothing happening. This idle time was essential for the consumer thread to catch up with logging.

It is also important to use asynchronous IO, which just complicates it more. In order to clear ring buffers in time the consumer can never be waiting on a write operation to complete. I don’t recall now if I actually used async IO, or created yet another thread which simply blocked on IO. Though the actual danger of blocking here is somewhat limited. The file buffers on Linux are relatively large, and even with thousands of log items to write, the amount of data relatively small.
Recommended

I still highly recommend this approach. It is suitable not just for logging, but for any situation where you need to offload processing from a real-time thread. The solution is only practically available to a language like C++. The basic ring buffer can be done in C easily, but to get a clean logging solution would be hard without C++’s conveniences (in particular variants, tuple types, and parametric functions). The requirement to tightly control memory means it outside the reach of any language that doesn’t provide pointers and raw memory access.




Atomic
A faster way to implement the continuous read buffer is to use a lock free three buffer system
as proposed by Reto Carrara [6]. While this data structure uses slightly more memory space
than the normal blocking buffer, it significantly reduces access times on the buffer.
Carrara’s design uses three buffers to store data in. One buffer is used to read data from, the
other two are written to in turn. Whenever data has been written to one buffer it becomes the
new read buffer. The only thing that is critical to be synchronised is the update of changes
on which buffer is used for which purpose, called the read- and write consensus. This can be
done atomically by using the test and set method [7]. Carrara uses system specific code to
implement test and set which usually requires to descend to assembly code level. C++11’s new
std::atomic types offer an abstraction on these atomic operations which makes the test and set
function easy to implement


std::atomic<bool > touched;
bool test_and_set(void) {
return touched.exchange(true);
}

template <typename Type_>
class three_buffer : public buffer<Type_> {
public:
explicit three_buffer(Type_ default_val)
: buffer_{default_val}
, data_{}
, write_{data_}
, read_{data_}
{
}

void put(Type_ element) override {
int last__ = write_.get_read_consensus();
int last_write__ = write_.get_last_written();
int index__ = permutator_[last__][last_write__];
buffer_[index__] = element;
write_.setLastWritten(index__);
}

Type_ get(void) override {
return buffer_[read_.get_read_consensus()];
}

private:
const int permutator_[3][3]
= { { 1, 2, 1 }, { 2, 2, 0 }, { 1, 0, 0 } };
data_buffer<Type_> buffer_;
consensus_data data_;
write_consensus write_;
read_consensus read_;
};
