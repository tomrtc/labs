 State reflection
Overview

The Stingray engine has two controller threads -- the main thread and the render thread. These two threads build up work for our job system, which is distributed on the remaining threads. The main thread and the render thread are pipelined, so that while the main thread runs the simulation/update for frame N, the render thread is processing the rendering work for the previous frame (N-1). This post will dive into the details how state is propagated from the main thread to the render thread.

I will use code snippets to explain how the state reflection works. It's mostly actual code from the engine but it has been cleaned up to a certain extent. Some stuff has been renamed and/or removed to make it easier to understand what's going on.
The main loop

Here is a slimmed down version of the update loop which is part of the main thread:

while (!quit())
{
    // Calls out to the mandatory user supplied `update` Lua function, Lua is used
    // as a scripting language to manipulate objects. From Lua worlds, objects etc
    // can be created, manipulated, destroyed, etc. All these changes are recorded
    // on a `StateStream` that is a part of each world.
    _game->update();

    // Flush state changes recorded on the `StateStream` for each world to
    // the rendering world representation.
    unsigned n_worlds = _worlds.size();
    for (uint32_t i = 0; i < n_worlds; ++i) {
        auto &world = *_worlds[i];
        _render_interface->update_world(world);
    }

    // Begin a new render frame.
    _render_interface->begin_frame();

    // Calls out to the user supplied `render` Lua function. It's up to the script
    // to call render on worlds(). The script controls what camera and viewport
    // are used when rendering the world.
    _game->render();

    // Present the frame.
    _render_interface->present_frame();

    // End frame.
    _render_interface->end_frame(_delta_time);

    // Never let the main thread run more than 1 frame a head of the render thread.
    _render_interface->wait_for_fence(_frame_fence);

    // Create a new fence for the next frame.
    _frame_fence = _render_interface->create_fence();
}

First thing to point out is the _render_interface. This is not a class full of virtual functions that some other class can inherit from and override as the name might suggest. The word "interface" is used in the sense that it's used to communicate from one thread to another. So in this context the _render_interface is used to post messages from the main thread to the render thread.

As said in the first comment in the code snippet above, Lua is used as our scripting language and from Lua things such as worlds, objects, etc can be created, destroyed, manipulated, etc.

The state between the main thread and the render thread is very rarely shared, instead each thread has its own representation and when state is changed on the main thread that state is reflected over to the render thread. E.g., the MeshObject, which is the representation of a mesh with vertex buffers, materials, textures, shaders, skinning, data etc to be rendered, is the main thread representation and RenderMeshObject is the corresponding render thread representation. All objects that have a representation on both the main and render thread are setup to work the same way:

class MeshObject : public RenderStateObject
{
};

class RenderMeshObject : public RenderObject
{
};

The corresponding render thread class is prefixed with Render. We use this naming convention for all objects that have both a main and a render thread representation.

The main thread objects inherit from RenderStateObject and the render thread objects inherit from RenderObject. These structs are defined as:

struct RenderStateObject
{
    uint32_t render_handle;
    StateReflection *state_reflection;
};

struct RenderObject
{
    uint32_t type;
};

The render_handle is an ID that identifies the corresponding object on the render thread. state_reflection is a stream of data that is used to propagate state changes from the main thread to the render thread. type is an enum used to identify the type of render objects.
Object creation

In Stingray a world is a container of renderable objects, physical objects, sounds, etc. On the main thread, it is represented by the World class, and on the render thread by a RenderWorld.

When a MeshObject is created in a world on the main thread, there's an explicit call to WorldRenderInterface::create() to create the corresponding render thread representation:

MeshObject *mesh_object = MAKE_NEW(_allocator, MeshObject);
_world_render_interface.create(mesh_object);

The purpose of the call to WorldRenderInterface::create is to explicitly create the render thread representation, acquire a render_handle and to post that to the render thread:

void WorldRenderInterface::create(MeshObject *mesh_object)
{
    // Get a unique render handle.
    mesh_object->render_handle = new_render_handle();

    // Set the state_reflection pointer, more about this later.
    mesh_object->state_reflection = &_state_reflection;

    // Create the render thread representation.
    RenderMeshObject *render_mesh_object = MAKE_NEW(_allocator, RenderMeshObject);

    // Pass the data to the render thread
    create_object(mesh_object->render_handle, RenderMeshObject::TYPE, render_mesh_object);
}

The new_render_handle function speaks for itself.

uint32_t WorldRenderInterface::new_render_handle()
{
    if (_free_render_handles.any()) {
        uint32_t handle = _free_render_handles.back();
        _free_render_handles.pop_back();
        return handle;
    } else
        return _render_handle++;
}

There is a recycling mechanism for the render handles and a similar pattern reoccurs at several places in the engine. The release_render_handle function together with the new_render_handle function should give the complete picture of how it works.

void WorlRenderInterface::release_render_handle(uint32_t handle)
{
    _free_render_handles.push_back(handle);
}

There is one WorldRenderInterface per world which contains the _state_reflection that is used by the world and all of its objects to communicate with the render thread. The StateReflection in its simplest form is defined as:

struct StateReflection
{
    StateStream *state_stream;
};

The create_object function needs a bit more explanation though:

void WorldRenderInterface::create_object(uint32_t render_handle, RenderObject::Type type, void *user_data)
{
    // Allocate a message on the `state_stream`.
    ObjectManagementPackage *omp;
    alloc_message(_state_reflection.state_stream, WorldRenderInterface::CREATE, &omp);

    omp->object_type = RenderWorld::TYPE;
    omp->render_handle = render_handle;
    omp->type = type;
    omp->user_data = user_data;
}

What happens here is that alloc_message will allocate enough bytes to make room for a MessageHeader together with the size of ObjectManagementPackage in a buffer owned by the StateStream. The StateStream is defined as:

struct StateStream
{
    void *buffer;
    uint32_t capacity;
    uint32_t size;
};

capacity is the size of the memory pointed to by buffer, size is the current amount of bytes allocated from buffer.

The MessageHeader is defined as:

struct MessageHeader
{
    uint32_t type;
    uint32_t size;
    uint32_t data_offset;
};

The alloc_message function will first place the MessageHeader and then comes the data, some ASCII to the rescue:

+-------------------------------------------------------------------+
| MessageHeader | data                                              |
+-------------------------------------------------------------------+
<- data_offset ->
<-                          size                                   ->

The size and data_offset mentioned in the ASCII are two of the members of MessageHeader, these are assigned during the alloc_message call:

template<Class T>
void alloc_message(StateStream *state_stream, uint32_t type, T **data)
{
    uint32_t data_size = sizeof(T);

    uint32_t message_size = sizeof(MessageHeader) + data_size;

    // Allocate message and fill in the header.
    void *buffer = allocate(state_stream, message_size, alignof(MessageHeader));
    auto header = (MessageHeader*)buffer;

    header->type = type;
    header->size = message_size;
    header->data_offset = sizeof(MessageHeader);

    *data = memory_utilities::pointer_add(buffer, header->data_offset);
}

The buffer member of the StateStream will contain several consecutive chunks of message headers and data blocks.

+-----------------------------------------------------------------------+
| Header | data | Header | data | Header | data | Header | data | etc   |
+-----------------------------------------------------------------------+

This is the necessary code on the main thread to create an object and populate the StateStream which will later on be consumed by the render thread. A very similar pattern is used when changing the state of an object on the main thread, e.g:

void MeshObject::set_flags(renderable::Flags flags)
{
    _flags = flags;

    // Allocate a message on the `state_stream`.
    SetVisibilityPackage *svp;
    alloc_message(state_reflection->state_stream, MeshObject::SET_VISIBILITY, &svp);

    // Fill in message information.
    svp->object_type = RenderMeshObject::TYPE;

    // The render handle that got assigned in `WorldRenderInterface::create`
    // to be able to associate the main thread object with its render thread
    // representation.
    svp->handle = render_handle;

    // The new flags value.
    svp->flags = _flags;
}

Getting the recorded state to the render thread

Let's take a step back and explain what happens in the main update loop during the following code excerpt:

// Flush state changes recorded on the `StateStream` for each world to
// the rendering world representation.
unsigned n_worlds = _worlds.size();
for (uint32_t i = 0; i < n_worlds; ++i) {
    auto &world = *_worlds[i];
    _render_interface->update_world(world);
}

When Lua has been creating, destroying, manipulating, etc objects during update() and is done, each world's StateStream which contains all the recorded changes is ready to be sent over to the render thread for consumption. The call to RenderInterface::update_world() will do just that, it roughly looks like:

void RenderInterface::update_world(World &world)
{
    UpdateWorldMsg uw;

    // Get the render thread representation of the `world`.
    uw.render_world = render_world_representation(world);

    // The world's current `state_stream` that contains all changes made
    // on the main thread.
    uw.state_stream = world->_world_reflection_interface.state_stream;

    // Create and assign a new `state_stream` to the world's `_world_reflection_interface`
    // that will be used for the next frame.
    world->_world_reflection_interface->state_stream = new_state_stream();

    // Post a message to the render thread to update the world.
    post_message(UPDATE_WORLD, &uw);
}

This function will create a new message and post it to the render thread. The world being flushed and its StateStream are stored in the message and a new StateStream is created that will be used for the next frame. This new StateStream is set on the WorldRenderInterface of the World, and since all objects being created got a pointer to the same WorldRenderInterface they will use the newly created StateStream when storing state changes for the next frame.
Render thread

The render thread is spinning in a message loop:

void RenderInterface::render_thread_entry()
{
    while (!_quit) {
        // If there's no message -- put the thread to sleep until there's
        // a new message to consume.
        RenderMessage *message = get_message();

        void *data = data(message);
        switch (message->type) {
            case UPDATE_WORLD:
                internal_update_world((UpdateWorldMsg*)(data));
                break;

            // ... And a lot more case statements to handle different messages. There
            // are other threads than the main thread that also communicate with the
            // render thread. E.g., the resource loading happens on its own thread
            // and will post messages to the render thread.
        }
    }
}

The internal_update_world() function is defined as:

void RenderInterface::internal_update_world(UpdateWorldMsg *uw)
{
    // Call update on the `render_world` with the `state_stream` as argument.
    uw->render_world->update(uw->state_stream);

    // Release and recycle the `state_stream`.
    release_state_stream(uw->state_stream);
}

It calls update() on the RenderWorld with the StateStream and when that is done the StateStream is released to a pool.

void RenderWorld::update(StateStream *state_stream)
{
    MessageHeader *message_header;
    StatePackageHeader *package_header;

    // Consume a message and get the `message_header` and `package_header`.
    while (get_message(state_stream, &message_header, (void**)&package_header)) {
        switch (package_header->object_type) {
            case RenderWorld::TYPE:
            {
                auto omp = (WorldRenderInterface::ObjectManagementPackage*)package_header;
                // The call to `WorldRenderInterface::create` created this message.
                if (message_header->type == WorldRenderInterface::CREATE)
                    create_object(omp);
            }
            case (RenderMeshObject::TYPE)
            {
                if (message_header->type == MeshObject::SET_VISIBILITY) {
                    auto svp = (MeshObject::SetVisibilityPackage*>)package_header;

                    // The `render_handle` is used to do a lookup in `_objects_lut` to
                    // to get the `object_index`.
                    uint32_t object_index = _object_lut[package_header->render_handle];

                    // Get the `render_object`.
                    void *render_object = _objects[object_index];

                    // Cast it since the type is already given from the `object_type`
                    // in the `package_header`.
                    auto rmo = (RenderMeshObject*)render_object;

                    // Call update on the `RenderMeshObject`.
                    rmo->update(message_header->type, package_header);
                }
            }
            // ... And a lot more case statements to handle different kind of messages.
        }
    }
}

The above is mostly infrastructure to extract messages from the StateStream. It can be a bit involved since a lot of stuff is written out explicitly but the basic idea is hopefully simple and easy to understand.

On to the create_object call done when (message_header->type == WorldRenderInterface::CREATE) is satisfied:

void RenderWorld::create_object(WorldRenderInterface::ObjectManagementPackage *omp)
{
    // Acquire an `object_index`.
    uint32_t object_index = _objects.size();

    // Same recycling mechanism as seen for render handles.
    if (_free_object_indices.any()) {
        object_index = _free_object_indices.back();
        _free_object_indices.pop_back();
    } else {
        _objects.resize(object_index + 1);
        _object_types.resize(object_index + 1);
    }

    void *render_object = omp->user_data;
    if (omp->type == RenderMeshObject::TYPE) {
        // Cast the `render_object` to a `MeshObject`.
        RenderMeshObject *rmo = (RenderMeshObject*)render_object;

        // If needed, do more stuff with `rmo`.
    }

    // Store the `render_object` and `type`.
    _objects[object_index] = render_object;
    _object_types[object_index] = omp->type;

    if (omp->render_handle >= _object_lut.size())
        _object_lut.resize(omp->handle + 1);
    // The `render_handle` is used
    _object_lut[omp->render_handle] = object_index;
}

So the take away from the code above lies in the general usage of the render_handle and the object_index. The render_handle of objects are used to do a look up in _object_lut to get the object_index and type. Let's look at an example, the same RenderWorld::update code presented earlier but this time the focus is when the message is MeshObject::SET_VISIBILITY:

void RenderWorld::update(StateStream *state_stream)
{
    StateStream::MessageHeader *message_header;
    StatePackageHeader *package_header;

    while (get_message(state_stream, &message_header, (void**)&package_header)) {
        switch (package_header->object_type) {
            case (RenderMeshObject::TYPE)
            {
                if (message_header->type == MeshObject::SET_VISIBILITY) {
                    auto svp = (MeshObject::SetVisibilityPackage*>)package_header;

                    // The `render_handle` is used to do a lookup in `_objects_lut` to
                    // to get the `object_index`.
                    uint32_t object_index = _object_lut[package_header->render_handle];

                    // Get the `render_object` from the `object_index`.
                    void *render_object = _objects[object_index];

                    // Cast it since the type is already given from the `object_type`
                    // in the `package_header`.
                    auto rmo = (RenderMeshObject*)render_object;

                    // Call update on the `RenderMeshObject`.
                    rmo->update(message_header->type, svp);
                }
            }
        }
    }
}

The state reflection pattern shown in this post is a fundamental part of the engine. Similar patterns appear in other places as well and having a good understanding of this pattern makes it much easier to understand the internals of the engine.
Hello, allocator!

The job of a memory allocator is to take a big block of memory (from the OS) and chop it up into smaller pieces for individual allocations:

void *A = malloc(10);
void *B = malloc(100);
void *C = malloc(20);

------------------------------------------------------
|  A  |  free  |   B   |  C  |         free          |
------------------------------------------------------

The allocator needs to be fast at serving an allocation request, i.e. finding a suitable piece of free memory. It also needs to be fast at freeing memory, i.e. making a previously used piece of memory available for new allocations. Finally, it needs to prevent fragmentation -- more about that in a moment.

Suppose we put all free blocks in a linked list and allocate memory by searching that list for a block of a suitable size. That makes allocation an O(n) operation, where n is the total number of free blocks. There could be thousands of free blocks and following the links in the list will cause cache misses, so to make a competitive allocator we need a faster method.

Fragmentation occurs when the free memory cannot be used effectively, because it is chopped up into little pieces:

------------------------------------------------------
|  A  |  free  |   B   |  C  |         free          |
------------------------------------------------------

Here, we might not be able to service a large allocation request, because the free memory is split up in two pieces. In a real world scenario, the memory can be fragmented into thousands of pieces.

The first step in preventing fragmentation is to ensure that we have some way of merging free memory blocks together. Otherwise, allocating blocks and freeing them will leave the memory buffer in a chopped up state where it is unable to handle any large requests:

-------------------------------------------------------
|  free  |  free  |  free  |  free  |  free  |  free  |
-------------------------------------------------------

Merging needs to be a quick operation, so scanning the entire buffer for adjacent free blocks is not an option.

Note that even if we merge all neighboring free blocks, we can still get fragmentation, because we can't merge the free blocks when there is a piece of allocated memory between them:

-----------------------------------------------------------
| free | A |  free  | B | free | C |   free    | D | free |
-----------------------------------------------------------

Some useful techniques for preventing this kind of fragmentation are:

    Use separate allocators for long-lived and short-lived allocations, so that the short-lived allocations don't create "holes" between the long lived ones.
    Put "small" allocations in a separate part of the buffer so they don't interfere with the big ones.
    Make the memory blocks relocatable (i.e. use "handles" rather than pointers).
    Allocate whole pages from the OS and rely on the page mapping to prevent fragmentation.

The last approach can be surprisingly efficient if you have a small page size and follow the advice suggested earlier in this series, to try to have a few large allocations rather than many small ones. On the other hand, a small page size means more TLB misses. But maybe that doesn't matter so much if you have good data locality. Speculation, speculation! I should provide some real numbers instead, but that is too much work!

Three techniques used by many allocators are in-place linked lists, preambles and postambles.

In-place linked lists is a technique for storing linked lists of free memory blocks without using any extra memory. The idea is that since the memory in the blocks is free anyway, we can just store the prev and next pointers directly in the blocks themselves, which means we don't need any extra storage space.

A preamble is a little piece of data that sits just before the allocated memory and contains some information about that memory block. The allocator allocates extra memory for the preamble and fills it with information when the memory is allocated:

void *A = malloc(100);

------------------------
| pre |    A     | post|
------------------------

In C we pretty much need to have a preamble, because when the user calls free(void *p) on a pointer p, we get no information about how big the memory block allocated at p is. That information needs to come from somewhere and a preamble is a reasonable option, because it is easy to access from the free() code:

struct Preamble
{
    unsigned size;
    ...
};

void free(void *p)
{
    Preamble *pre = (Preamble *)p - 1;
    unsigned size = pre->size;
}

Note that there are other options. We could use a hash table to store the size of each pointer. We could reserve particular areas in the memory buffer for allocations of certain sizes and use pointer compare to find the area (and hence the size) for a certain pointer. But hash tables are expensive, and having certain areas for allocations of certain sizes only really work if you have a limited number of different sizes. So preambles are a common option.

They are really annoying though. They increase the size of all memory allocations and they mess with alignment. For example, suppose that the user wants to allocate 4 K of memory and that our OS uses 4 K pages. Without preambles, we could just allocate a page from the OS and return it. But if we need a four byte preamble, then we will have to allocate 8 K from the OS so that we have somewhere to put those extra four bytes. So annoying!

And what makes it even more annoying is that in most cases storing the size is pointless, because the caller already knows it. For example, in C++, when we do:

delete x;

The runtime knows the actual type of x, because otherwise it wouldn't be able to call the destructor properly. But since it knows the type, it knows the size of that type and it could provide that information to the allocator when the memory is freed..

Similarly, if the memory belongs to an std::vector, the vector class has a capacity field that stores how big the buffer is, so again the size is known.

In fact, you could argue that whenever you have a pointer, some part of the runtime has to know how big that memory allocation is, because otherwise, how could the runtime use that memory without causing an access violation?

So we could imagine a parallel world where instead of free(void *) we would have free(void *, size_t) and the caller would be required to explicitly pass the size when freeing a memory block. That world would be a paradise for allocators. But alas, it is not the world we live in.

(You could enforce this parallel world in a subsystem, but I'm not sure if it is a good idea to enforce it across the board in a bigger project. Going against the grain of the programming language can be painful.)

A postamble is a similar piece of data that is put at the end of an allocated memory block.

Postambles are useful for merging. As mentioned above, when you free a memory block, you want to merge it with its free neighbors. But how do you know what the neighbors are and if they are free or not?

For the memory block to the right it is easy. That memory block starts where yours end, so you can easily get to it and check its preamble.

The neighbor to the left is trickier. Since you don't know how big that memory block might be, you don't know where to find its preamble. A postamble solves that problem, since the postamble of the block to the left will always be located just before your block.

Again, the alternative to using preambles and postambles to check for merging is to have some centralized structure with information about the blocks that you can query. And the challenge is to make such queries efficient.

If you require all allocations to be 16-byte aligned, then having both a preamble and a postamble will add 32 bytes of overhead to your allocations. That is not peanuts, especially if you have many small allocations. You can get around that by using slab or block allocators for such allocations, or even better, avoid them completely and try to make fewer and bigger allocations instead, as already mentioned in this series.
The buddy allocator

With that short introduction to some general allocation issues, it is time to take a look at the buddy allocator.

The buddy allocator works by repeatedly splitting memory blocks in half to create two smaller "buddies" until we get a block of the desired size.

If we start with a 512 K block allocated from the OS, we can split it to create two 256 K buddies. We can then take one of those and split it further into two 128 K buddies, and so on.

When allocating, we check to see if we have a free block of the appropriate size. If not, we split a larger block as many times as necessary to get a block of a suitable size. So if we want 32 K, we split the 128 K block into 64 K and then split one of those into 32 K.

At the end of this, the state of the allocator will look something like this:

Buddy allocator after 32 K allocation:

    -----------------------------------------------------------------
512 |                               S                               |
    -----------------------------------------------------------------
256 |               S               |               F               |
    -----------------------------------------------------------------
128 |       S       |       F       |
    ---------------------------------
 64 |   S   |   F   |                        S - split
    -----------------                        F - free
 32 | A | F |                                A - allocated
    ---------

As you can see, this method of splitting means that the block sizes will always be a powers of two. If you try to allocate something smaller, say 13 K, the allocation will be rounded up to the nearest power of two (16 K) and then get assigned a 16 K block.

So there is a significant amount of fragmentation happening here. This kind of fragmentation is called internal fragmentation since it is wasted memory inside a block, not wasted space between the blocks.

Merging in the buddy allocator is dead simple. Whenever a block is freed, we check if it's buddy is also free. If it is, we merge the two buddies back together into the single block they were once split from. We continue to do this recursively, so if this newly created free block also has a free buddy, they get merged together into an even bigger block, etc.

The buddy allocator is pretty good at preventing external fragmentation, since whenever something is freed there is a pretty good chance that we can merge, and if we can't the "hole" should be filled pretty soon by a similarly sized allocation. You can still imagine pathological worst-case scenarios. For example, if we first allocate every leaf node and then free every other of those allocations we would end up with a pretty fragmented memory. But such situations should be rare in practice.

Worst case fragmentation, 16 K block size

    -----------------------------------------------------------------
512 |                               S                               |
    -----------------------------------------------------------------
256 |               S               |               S               |
    -----------------------------------------------------------------
128 |       S       |       S       |       S       |       S       |
    -----------------------------------------------------------------
 64 |   S   |   S   |   S   |   S   |   S   |   S   |   S   |   S   |
    -----------------------------------------------------------------
 32 | S | S | S | S | S | S | S | S | S | S | S | S | S | S | S | S |
    -----------------------------------------------------------------
 16 |A|F|A|F|A|F|A|F|A|F|A|F|A|F|A|F|A|F|A|F|A|F|A|F|A|F|A|F|A|F|A|F|
    -----------------------------------------------------------------

I'm being pretty vague here, I know. That's because it is quite hard in general to say something meaningful about how "good" an allocator is at preventing fragmentation. You can say how good it does with a certain allocation pattern, but every program has a different allocation pattern.
Implementing the buddy allocator

Articles on algorithms and data structures are often light on implementation details. For example, you can find tons of articles describing the high-level idea behind the buddy allocator as I've outlined it above, but not much information about how to implement the bloody thing!

This is a pity, because the implementation details can really matter. For example, it's not uncommon to see someone carefully implement the A*-algorithm, but using a data structure for the open and closed sets that completely obliterates the performance advantages of the algorithm.

So let's get into a bit more detail.

We start with allocation. How can we find a free block of a requested size? We can use the technique described above: we put the free blocks of each size in an implicit linked list. To find a free block we just take the first item from the list of blocks of that size, remove it from the list and return it.

If there is no block of the right size, we take the block of the next higher size and split that. We use one of the two blocks we get and put the other one on the free list for that size. If the list of blocks of the bigger size is also empty, we can go to the even bigger size, etc.

To make things easier for us, let's introduce the concept of levels. We say that the single block that we start with, representing the entire buffer, is at level 0. When we split that we get two blocks at level 1. Splitting them, we get to level 2, etc.

We can now write the pseudocode for allocating a block at level n:

if the list of free blocks at level n is empty
    allocate a block at level n-1 (using this algorithm)
    split the block into two blocks at level n
    insert the two blocks into the list of free blocks for level n
remove the first block from the list at level n and return it

The only data structure we need for this is a list of pointers to the first free block at each level:

static const int MAX_LEVELS = 32;
void *_free_lists[MAX_LEVELS];

The prev and next pointers for the lists are stored directly in the free blocks themselves.

We can also note some mathematical properties of the allocator:

total_size == (1<<num_levels) * leaf_size
size_of_level(n) == total_size / (1<<n)
max_blocks_of_level(n) = (1<<n)

Note that MAX_LEVELS = 32 is probably enough since that gives a total size of leaf_size * 4 GB and we know leaf_size will be at least 16. (The leaf nodes must have room for the prev and next pointers of the linked list and we assume a 64 bit system.)

Note also that we can create a unique index for each block in the buddy allocator as (1<<level) + index_in_level - 1. The node at level 0 will have index 0. The two nodes at level 1 will have index 1 and 2, etc:

Block indices

    -----------------------------------------------------------------
512 |                               0                               |
    -----------------------------------------------------------------
256 |               1               |               2               |
    -----------------------------------------------------------------
128 |       3       |       4       |       5       |       6       |
    -----------------------------------------------------------------
 64 |   7   |   8   |   9   |  10   |  11   |  12   |  13   |  14   |
    -----------------------------------------------------------------
 32 |15 |16 |17 |18 |19 |20 |21 |22 |23 |24 |25 |26 |27 |28 |29 |30 |
    -----------------------------------------------------------------

The total number of entries in the index is (1 << num_levels) - 1. So if we want to store some data per block, this is how much memory we will need. For the sake of simplicity, let's ignore the - 1 part and just round it of as (1 << num_levels).

What about deallocation?

The tricky part is the merging. Doing the merging is simple, we just take the two blocks, remove them from the free list at level n and insert the merged block into the free list at level n-1.

The tricky part is to know when we should merge. I.e. when we are freeing a block p, how do we know if it is buddy is also free, so that we can merge them?

First, note that we can easily compute the address of the buddy. Suppose we have free a block p at level n. We can compute the index of that in the level as:

index_in_level_of(p,n) == (p - _buffer_start) / size_of_level(n)

If the index i is even, then the buddy as at index i+1 and otherwise the buddy is at i-1 and we can use the formula above to solve for the pointer, given the index.

So given the address of the buddy, let's call it buddy_ptr, how can we know if it is free or not? We could look through the free list for level n. If we find it there we know it is free and otherwise it's not. But there could be thousands of blocks and walking the list is hard on the cache.

To do better, we need to store some kind of extra information.

We could use preambles and postambles as discussed earlier, but that would be a pity. The buddy allocator has such nice, even block sizes: 1 K, 2 K, 4 K, we really don't want to mess that up with preambles and postambles.

But what we can do is to store a bit for each block, telling us if that block is free or allocated. We can use the block index as described above to access this bitfield. This will require a total of (1 << num_level) bits. Since the total size of the tree is (1 << num_levels) * leaf_size bytes, we can see that the overhead of storing these extra bits is 1 / 8 / leaf_size. With a decent leaf_size of say 128 (small allocations are better handled by a slab alloactor anyway) the overhead of this table is just 0.1 %. Not too shabby.

But in fact we can do even better. We can get by with just half a bit per block. That sounds impossible, but here is how:

For each pair of buddies A and B we store the single bit is_A_free XOR is_B_free. We can easily maintain the state of that bit by flipping it each time one of the buddies is freed or allocated.

When we consider making a merge we know that one of buddies is free, because it is only when a block has just been freed that we consider a merge. This means we can find out the state of the other block from the XORed bit. If it is 0, then both blocks are free. If it is 1 then it is just our block that is free.

So we can get by with just one bit for every pair of blocks, that's half a bit per block, or an overhead of just 1 / 16 / leaf_size.

At this point, careful readers may note that I have been cheating.

All this time I have assumed that we know the level n of the block that we are freeing. Otherwise we cannot compute the address of the buddy or its index in the node tree.

But to know the level n of ptr we must know the size of its allocated block. So this only really works if the user passes the size of the allocation when freeing the block. I.e, the free(void *, size_t) interface that we discussed earlier.

If we want to support the simpler and more common API free(void *p), the alloator needs to somehow store the size of each alloation.

Again, using a preamble is possible, but we don't really want to.

We could use an array, indexed by (p - _buffer_start) / leaf_size to store the sizes. Note that this is not the same as the block index. We can't use the block index, since we don't know the level. Instead this is an index of size 1 << (num_levels - 1) with one entry for each possible pointer that the buddy allocator can return.

We don't have to store the full size (32 bits) in the index, just the level. That's 5 bits assuming that MAX_LEVELS = 32. Since the number of entries in this index is half that of the block index this ammounts to 2.5 bits per block.

But we can do even better.

Instead of storing the size explicitly, we can use the block index and store a single bit to keep track of whether the block at that level has been split or not.

To find the level n of an allocated block we can use the algorithm:

n = num_levels - 1
while n > 0
    if block_has_been_split(ptr, n-1)
        return n
    n = n - 1
return 0

Since the leaf blocks can't be split, we only need 1 << (num_levels - 1) entries in the split index. This means that the cost of the split index is the same as for the merge index, 0.5 bits per block. It's a bit amazing that we can do all this with a total overhead of just 1 bit per block.

The prize of the memory savings is that we now have to loop a bit to find the allocated size. But num_levels is usually small (in any case <= 32) and since we only have 1 bit per entry the cache usage is pretty good. Furthermore, with this approach it is easy to offer both a free(void *) and a free(void *, size_t) interface. The latter can be used by more sophisticated callers to avoid the loop to calculate the block size.
Memory arrangements

Where do we store this 1 bit of metadata per block? We could use a separate buffer, but it is not that elegant. It would mean that our allocator would have to request two buffers from the system, one for the data and one for the metadata.

Instead, let's just put the metadata in the buffer itself, at the beginning where we can easily find it. We mark the blocks used to store the metadata as allocated so that they won't be used by other allocations:

Initial state of memory after reserving metadata:

    -----------------------------------------------------------------
512 |                               S                               |
    -----------------------------------------------------------------
256 |               S               |               F               |
    -----------------------------------------------------------------
128 |       S       |       F       |
    ---------------------------------
 64 |   S   |   S   |
    -----------------
 32 | S | S | S | F |
    -----------------
 16 |A|A|A|A|A|F|
    -------------
    ********** Metadata

Note that when allocating the metadata we can be a bit sneaky and not round up the allocation to the nearest power of two. Instead we just take as many leaf blocks as we need. That is because when we allocate the metadata we know that the allocator is completely empty, so we are guaranteed to be able to allocate adjacent leaf blocks. In the example above we only have to use 5 * 16 = 80 K for the metadata instead of the 128 K we would have used if we rounded up.

(The size of the metadata has been greatly exaggerated in the illustration above to show this effect. In reality, since the tree in the illustration has only six levels, the metadata is just 1 * (1 << 6) = 64 bits, that's 8 bytes, not 80 K.)

Note that you have to be a bit careful when allocating the metadata in this way, because you are allocating memory for the metadata that your memory allocation functions depend on. That's a chicken-and-egg problem. Either you have to write a special allocation routine for this initial allocation, or be very careful with how you write your allocation code so that this case is handled gracefully.

We can use the same technique to handle another pesky issue.

It's a bit irritating that the size of the buddy allocator has to be a power of two of the leaf size. Say that we happen to have 400 K of memory lying around somewhere. It would be really nice if we could use all of that memory instead of just the first 256 K.

We can do that using the same trick. For our 400 K, we can just create a 512 K buddy allocator and mark the first 144 K of it as "already allocated". We also offset the start of the buffer, so that the start of the usable memory coincides with the start of the buffer in memory. Like this:

    -----------------------------------------------------------------
512 |                               S                               |
    -----------------------------------------------------------------
256 |               S               |               F               |
    -----------------------------------------------------------------
128 |       S       |       S       |
    ---------------------------------
 64 |   S   |   S   |   S   |   F   |
    ---------------------------------
 32 | S | S | S | S | S | F |
    -------------------------
 16 |A|A|A|A|A|A|A|A|A|A|
    ---------------------
    *******************    Unusable, unallocated memory
MET                    *   Metadata
                       ^
                       +-- Usable memory starts here

Again, this requires some care when writing the code that does the initial allocation so that it doesn't write into the unallocated memory and causes an access violation.
The buddy allocator and growing buffers

As mentioned in the previous post, the buddy allocator is perfect for allocating dynamically growing buffers, because what we want there is allocations that progressively double in size, which is exactly what the different levels of the buddy allocator offer.

When a buffer needs to grow, we just allocate the next level from the buddy allocator and set the capacity of the buffer so that it fills up all that space.

Note that this completely avoids the internal fragmentation issue, which is otherwise one of the biggest problems with the buddy allocator. There will be no internal fragmentation because the dynamic buffers will make use of all the available space.

In the next post, I'll show how all of this ties together.
<div class='post-body entry-content' id='post-body-1375867256772306031' itemprop='description articleBody'>
<p>Making technical illustrations in drawing programs is tedious and boring. We are programmers, we should be <em>programming</em> our illustrations. Luckily, with JavaScript and its canvas, we can. And we can make them move!</p>

<p>To try this out, and to brush up my rusty JavaScript so that I can hang with all the cool web kids, I made an illustration of the how the buddy allocator described in the last article works:</p>
 Allocation Adventures 2: Arrays of Arrays

Last week's post ended with a puzzle: How can we allocate an array of dynamically growing and shrinking things in an efficient and data-oriented way? I.e. using contiguous memory buffers and as few allocations as possible.

The example in that post was kind of complicated, and I don't want to get lost in the details, so let's look at a simpler version of the same fundamental problem.

Suppose we want to create a TagComponent that allows us to store a number of unsigned tags for an entity.

These tags will be hashes of strings such as "player", "enemy", "consumable", "container", etc and the TagComponent will have some sort of efficient lookup structure that allows us to quickly find all entities with a particular tag.

But to keep things simple, let's ignore that for now. For now we will just consider how to store these lists of tags for all our entities. I.e. we want to find an alternative to:

std::vector< std::vector < unsigned> > data;

that doesn't store every list in a separate memory allocation.
Fixed size

If we can get away with it, we can get rid of the "array of arrays" by setting a hard limit on the number of items we can store per entity. In that case, the data structure becomes simply:

enum {MAX_TAGS = 8};
struct Tags
{
    unsigned n;
    unsigned tags[MAX_TAGS];
};
Array<Tags> data;

Now all the data is contained in a single buffer, the data buffer for Array<Tags>.

Sometimes the hard limit is inherent in the problem itself. For example, in a 2D grid a cell can have at most four neighbors.

Sometimes the limit is a widely accepted compromise between cost and quality. For example, when skinning meshes it is usually consider ok to limit the number of bone influences per vertex to four.

Sometimes there is no sensible limit inherent to the problem itself, but for the particular project that we are working on we can agree to a limit and then design the game with that limit in mind. For example we may know that there will never be more than two players, never more than three lights affecting an object, never more than four tags needed for an entity, etc.

This of course requires that we are writing, or at least configuring, the engine for a particular project. If we are writing a general engine to be used for a lot of games it is hard to set such limits without artificially constraining what those games will be able to do.

Also, since the fixed size must be set to the maximum array size, every entity that uses fewer entries than the maximum will waste some space. If we need a high maximum this can be a significant problem and it might make sense to go with a dynamic solution even though there is an upper limit.

So while the fixed size approach can be good in some circumstances, it doesn't work in every situation.
Linked list

Instead of using arrays, we can put the tags for a particular entity in a linked list:

struct Tag
{
    unsigned tag;
    Tag *next;
};
Array<Tag *> data;

Using a linked list may seem like a very bad choice at first. A linked list can give us a cache miss for every next pointer we follow. This would give us even worse performance than we would get with vector < vector < unsigned > >.

But the nodes in the linked list do not necessarily have to be allocated individually on the heap. We can do something similar to what we did in the last post: allocate the nodes in a buffer and refer to them using offsets rather than pointers:

struct Node
{
    unsigned tag;
    unsigned next;
};
Array<Node> nodes;

With this approach we only have a single allocation -- the buffer for the array that contains all the tag nodes -- and we can follow the indexes in the next field to walk the list.

Side note: Previously I have always used UINT_MAX to mark an nil value for an unsigned. So in the struct above, I would have used UINT_MAX for the next value to indicate the end of the list. But recently, I've switched to using 0 instead. I think it is nice to be able to memset() a buffer to 0 to reset all values. I think it is nice that I can just use if (next) to check if the value is valid. It is also nice that the invalid value will continue to be 0 even if I later decide to change the type to int or uint_16t. It does mean that I can't use the nodes[0] entry, since that is reserved for the nil value, but I think the increased simplicity is worth it.

Using a single buffer rather than separate allocations gives us much better cache locality, but the next references can still jump around randomly in that buffer. So we can still get cache misses. If the buffer is large, this can be as bad as using freely allocated nodes.

Another thing to note is that we are wasting a significant amount of memory. Only half of the memory is used for storing tags, the rest of it is wasted on the next pointers.

We can try to address both these problems by making the nodes a little bigger:

enum {MAX_TAGS_PER_NODE = 8};
struct Node
{
    unsigned n;
    unsigned tags[MAX_TAGS_PER_NODE];
    unsigned next;
};
Array<Node> nodes;

This is just as before, except we have more than one tag per node. This gives better cache performance because we can now process eight tags at a time before we have to follow a next pointer and jump to a different memory location. Memory use can also be better. If the nodes are full, we are using 80 % of the memory for actual tags, rather than 50 % as we had before.

However, if the nodes are not full we could be wasting even more memory than before. If entities have three tags on average, then we are only using 30 % of the memory to store tags.

We can balance cache performance and memory use by changing MAX_TAGS_PER_NODE. Increasing it gives better cache coherence, because we can process more tags before we need to jump to a different memory location. However, increasing it also means more wasted memory. It is probably good to set the size so that "most entities" fit into a single node, but a few special ones (players and enemies maybe) need more.

One interesting thing to note about the cache misses is that we can get rid of them by sorting the nodes. If we sort them so that the nodes in the same next chain always appear directly after one another in the array, then walking the list will access the data linearly in memory, just as if we were accessing an array:

--------------------------------------------------
|  A1 --|--> A2 --|--> A3 |  B  |  C1 --|--> C2  |
--------------------------------------------------

Note that a complete ordering is not required, it is enough if the linked nodes end up together. Single nodes, such as the B node above could go anywhere.

Since these are dynamic lists where items will be added and removed all the time, we can't really do a full O(n log n) sort every time something changes. That would be too expensive. But we could sort the list "incrementally". Every time the list is accessed, we do a little bit of sorting work. As long as the rate of mutation is low compared to the rate of access, which you would expect in most circumstances, our sorting should be able to keep up with the mutations and keep the list "mostly sorted".

You would need a sorting algorithm that can be run incrementally and that works well with already sorted data. Two-way bubble sort perhaps? I haven't thought too deeply about this, because I haven't implemented this method in practice.
Custom memory allocator

Another option is to write a custom memory allocator to divide the bigger buffer up into smaller parts for memory allocations.

You might think that this is a much too complex solution, but a custom memory allocator doesn't necessarily need to be a complex thing. In fact, both the fixed size and linked list approaches described above could be said to be using a very simple kind of custom memory allocator: one that just allocates fixed blocks from an array. Such an allocator does not need many lines of code.

Another criticism against this approach is that if we are writing our own custom memory allocator, aren't we just duplicating the work that malloc() or new already does? What's the point of first complaining a lot about how problematic the use of malloc() can be and then go on to write our very own (and probably worse) implementation of malloc()?

The answer is that malloc() is a generic allocator that has to do well in a lot of different situations. If we have more detailed knowledge of how the allocator is used, we can write an allocator that is both simpler and performs better. For example, as seen above, when we know the allocations are fixed size we can make a very fast and simple allocator. System software typically uses such allocators (check out the slab allocator for instance) rather than relying on malloc().

In addition, we also get the benefit that I talked about in the previous post. Having all of a system's allocations in a single place (rather than mixed up with all other malloc() allocations) makes it much easier to reason about them and optimize them.

As I said above, the key to making something better than malloc() is to make use of the specific knowledge we have about the allocation patterns of our system. So what is special about our vector < vector < unsigned > > case?

1. There are no external pointers to the data.

All the pointers are managed by the TagComponent itself and never visible outside that component.

This means that we can "move around" memory blocks as we like, as long as the TagComponent keeps track of and updates its data structures with the new locations. So we don't have to worry (that much) about fragmentation, because when we need to, we can always move things around in order to defrag the memory.

I'm sure you can build something interesting based on that, but I actually want to explore another property:

2. Memory use always grows by a factor of two.

If you look at the implementation of std::vector or a similar class (since STL code tends to be pretty unreadable) you will see that the memory allocated always grows by a factor of two. (Some implementations may use 1.5 or something else, but usually it is 2. The exact figure doesn't matter that much.)

The vector class keeps track of two counters:

    size which stores the number of items in the vector and
    capacity which stores how many items the vector has room for, i.e. how much memory has been allocated.

If you try to push an item when size == capacity, more memory is needed. So what typically happens is that the vector allocates twice as much memory as was previously used (capacity *= 2) and then you can continue to push items.

This post is already getting pretty long, but if you haven't thought about it before you may wonder why the vector grows like this. Why doesn't it grow by one item at a time, or perhaps 16 items at a time.

The reason is that we want push_back() to be a cheap operation -- O(1) using computational complexity notation. When we reallocate the vector buffer, we have to move all the existing elements from the old place to the new place. This will take O(n) time. Here, n is the number of elements in the vector.

If we allocate one item at a time, then we need to allocate every time we push and since re-allocate takes O(n) that means push will also take O(n). Not good.

If we allocate 16 items at a time, then we need to allocate every 16th time we push, which means that push on average takes O(n)/16, which by the great laws of O(n) notation is still O(n). Oops!

But if we allocate 2*n items when we allocate, then we only need to reallocate after we have pushed n more items, which means that push on average takes O(n)/n. And O(n)/n is O(1), which is exactly what we wanted.

Note that it is just on average that push is O(1). Every n pushes, you will encounter a push that takes O(n) time. For this reason, push is said to run in amortized constant time. If you have really big vectors, that can cause an unacceptable hitch and in that case you may want to use something other than a vector to store the data.

Anyways, back to our regular programming.

The fact that our data (and indeed, any kind of dynamic data that uses the vector storage model) grows by powers of two is actually really interesting. Because it just so happens that there is an allocator that is very good at allocating blocks at sizes that are powers of two. It is called the buddy allocator and we will take a deeper look at it in the next post.
<script>
var Buddy = {
    data: {
        console: [],
        allocations: [],
        levels: 5,
        size: 128,
        blockState: [],
        freeLists: [],
    },
    layout: {
        console: {
            font: "18px sans-serif",
            x: 40,
            y: 300,
            lineHeight: 20,
            maxLines: 10,
        },
        allocations: {
            x: 300,
            y: 300,
            headerFont: "18px sans-serif",
            headerLineHeight: 30,
            font: "14px sans-serif",
            rowHeight: 20,
            columns: 8,
            columnWidth: 30,
        },
        tree: {
            font: "14px sans-serif",
            x: 100,
            y: 40,
            width: 512,
            height: 200,
            stateFill: {
                "split" : "#dd9",
                "allocated" : "#c99",
                "free" : "#9c9",
            },
        },
        legend: {
            font: "18px sans-serif",
            x: 550,
            y: 300,
            texts: ["Split", "Allocated", "Free"],
            colors: ["#dd9", "#c99", "#9c9"],
        },
        animationStepMs: 500,
    },

    new: function() {
        var o = Object.create(this);
        o.data = JSON.parse(JSON.stringify(this.data));
        o.layout = JSON.parse(JSON.stringify(this.layout));
        return o;
    },
    numBlocks: function() {
        return (1<<this.data.levels)-1;
    },
    sizeOfLevel: function(level) {
        return this.data.size / (1 << level);
    },
    levelOfSize: function (size) {
        return Math.log2(this.data.size / size);
    },
    levelOfBlock: function (block) {
        return Math.floor(Math.log2(block+1));
    },
    draw: function (ctx) {
        var data = this.data;
        var layout = this.layout;

        this.drawConsole(ctx, data.console, layout.console);
        this.drawAllocations(ctx, data.allocations, layout.allocations);
        this.drawTree(ctx, data, layout.tree);
        this.drawLegend(ctx, layout.legend);
    },
    drawConsole: function(ctx, data, layout) {
        ctx.font = layout.font;
        var y = layout.y;
        for (var i=0; i<data.length; ++i) {
            if (data[i].startsWith(">"))
                ctx.fillStyle = "#000";
            else
                ctx.fillStyle = "#999";
            ctx.fillText(data[i], layout.x, y);
            y = y + layout.lineHeight;
        }
        ctx.fillStyle = "#000";
    },
    drawAllocations: function(ctx, data, layout) {
        var x = layout.x;
        var y = layout.y;
        ctx.font = layout.headerFont;
        ctx.fillText("Allocations", x,y);
        y = y + layout.headerLineHeight;
        ctx.font = layout.font;
        var col = 0;
        var baseX = x;
        for (var i=0; i<data.length; ++i) {
            ctx.fillText(data[i], x, y);
            x = x + layout.columnWidth;
            col++;
            if (col == layout.columns) {
                y += layout.rowHeight;
                x = baseX;
                col = 0;
            }
        }
    },
    drawTree: function(ctx, data, layout) {
        ctx.font = layout.font;
        ctx.fillStyle = "#f00";
        ctx.fillText("freelist", layout.x-40, layout.y-10);
        ctx.fillStyle = "#000";
        var h = layout.height / data.levels;
        for (var i=0; i<data.levels; ++i) {
            var blocks = (1<<i);
            var w = layout.width / blocks;
            ctx.fillText("level " + i, layout.x-80, layout.y+i*h+25);
            var s = this.sizeOfLevel(i);
            ctx.textAlign = "right";
            ctx.fillText(s + " K", layout.x+layout.width+50, layout.y+i*h+25);
            ctx.textAlign = "left";
            for (var j=0; j<blocks; ++j) {
                var block_index = (1<<i) + j - 1;
                var state = data.blockState[block_index];
                if (layout.stateFill[state]) {
                    ctx.fillStyle = layout.stateFill[state];
                    ctx.fillRect(layout.x+j*w,layout.y+i*h,w,h);
                    ctx.fillStyle = "#000";
                }
                ctx.strokeRect(layout.x+j*w,layout.y+i*h,w,h);
            }

            ctx.strokeStyle = "#f00";
            ctx.strokeRect(layout.x-30,layout.y+i*h,20,h);
            var freeList = data.freeLists[i] || [];
            ctx.beginPath();
            var x0 = layout.x-20;
            var y0 = layout.y+i*h+21
            ctx.moveTo(x0,y0);
            for (var j=0; j<freeList.length; ++j) {
                var index_in_level = freeList[j] - ((1<<i)-1);
                var x1 = layout.x + w*index_in_level + w/2
                var y1 = layout.y+i*h + h/2
                var yc = y1 - Math.abs(x0-x1)/4
                ctx.bezierCurveTo(x0, yc, x1, yc, x1, y1);
                x0 = x1;
                y0 = y1;
            }
            ctx.stroke();
            ctx.strokeStyle = "#000";
        }
    },
    drawLegend: function(ctx, layout) {
        ctx.font = layout.font;
        for (var i=0; i<layout.texts.length; ++i) {
            var text = layout.texts[i];
            ctx.fillStyle = layout.colors[i];
            ctx.fillRect(layout.x,layout.y+i*30-15,20,20);
            ctx.strokeRect(layout.x,layout.y+i*30-15,20,20);
            ctx.fillStyle = "#000";
            ctx.fillText(text, layout.x+30, layout.y + i*30);
        }
    },
    log: function*(s) {
        var data = this.data.console;
        var layout = this.layout.console;
        data.push(s);
        while (data.length > layout.maxLines)
            data.shift();
        yield;
    },
    init: function*() {
        var data = this.data;
        for (var i=0; i<data.levels; ++i)
            data.freeLists[i] = [];
        yield* this.log("> init");
        data.blockState[0] = "free";
        yield;
        data.freeLists[0] = [0];
        yield;
        yield* this.log("ok");
    },
    split: function*(level) {
        if (level < 0)
            return;
        var data = this.data;
        if (data.freeLists[level].length == 0)
            yield* this.split(level-1);
        if (data.freeLists[level].length == 0)
            return;
        var block = data.freeLists[level].shift();
        yield;
        data.blockState[block] = 'split';
        yield;
        var b1 = block*2+1;
        var b2 = block*2+2;
        data.blockState[b1] = 'free';
        data.blockState[b2] = 'free';
        yield;
        data.freeLists[level+1].push(b1);
        data.freeLists[level+1].push(b2);
        yield;
    },
    allocate: function*(size) {
        yield* this.log("> allocate(" + size + " K)");
        var data = this.data;
        var level = this.levelOfSize(size);
        if (data.freeLists[level].length == 0 && level>0)
            yield* this.split(level-1);
        if (data.freeLists[level].length == 0) {
            yield* this.log("# OUT OF MEMORY");
            return null;
        }

        var p = data.freeLists[level].shift();
        yield;
        data.blockState[p] = 'allocated';
        yield;
        yield* this.log("= " + p);
        data.allocations.push(p);
        yield;
        return p;
    },
    merge: function*(p) {
        var level = this.levelOfBlock(p);
        if (level == 0)
            return;

        var data = this.data;
        var buddy = (p % 2) ? (p + 1) : (p - 1);
        if (data.blockState[buddy] != 'free')
            return;
        data.blockState[p] = null;
        data.blockState[buddy] = null;
        yield;
        data.freeLists[level].splice(data.freeLists[level].indexOf(p), 1);
        data.freeLists[level].splice(data.freeLists[level].indexOf(buddy), 1);
        yield;
        var parent = Math.floor((p-1)/2);
        data.blockState[parent] = 'free';
        yield;
        data.freeLists[level-1].push(parent);
        yield;
        yield* this.merge(parent);
    },
    free: function*(p) {
        var data = this.data;
        yield* this.log("> free(" + p + ")");
        var level = this.levelOfBlock(p);
        data.blockState[p] = 'free';
        yield;
        data.freeLists[level].push(p);
        yield;
        yield *this.merge(p);
        yield* this.log("ok");
        var allocations = data.allocations;
        var idx = allocations.indexOf(p);
        allocations.splice(idx, 1);
    },
};

function* animate(buddy)
{
    while (true) {
        yield* buddy.init();
        var allocations = buddy.data.allocations;

        for (var i=0; i<100; ++i) {
            if (Math.random() < 0.6 || allocations.length == 0) {
                var data = buddy.data;
                var block = Math.floor(Math.random() * buddy.numBlocks());
                var level = buddy.levelOfBlock(Math.floor(block));
                var p = yield* buddy.allocate(buddy.sizeOfLevel(level));
            } else {
                var idx = Math.floor(Math.random() * allocations.length);
                yield* buddy.free(allocations[idx]);
            }
        }

        var empty = Buddy.new();
        buddy.data = empty.data;
    }
}

function update(canvas, buddy, mutator)
{
    mutator.next();

    var context = canvas.getContext("2d");
    context.save();
    context.clearRect(0,0,canvas.width, canvas.height);

    buddy.draw(context)

    context.restore()

    window.setTimeout( function() {update(canvas, buddy, mutator);}, buddy.layout.animationStepMs );
}

function test(canvas)
{
    var buddy = Buddy.new();
    update(canvas, buddy, animate(buddy));
}
</script>
 Code Snippet: Murmur hash inverse / pre-image
Today's caring by sharing. I needed this non-trivial code snippet today and couldn't find it anywhere on the internet, so here it is for future reference. It computes the inverse / pre-image of a murmur hash. I. e., given a 32 bit murmur hash value, it computes a 32 bit value that when hashed produces that hash value:

/// Inverts a (h ^= h >> s) operation with 8 <= s <= 16
unsigned int invert_shift_xor(unsigned int hs, unsigned int s)
{
	XENSURE(s >= 8 && s <= 16);
	unsigned hs0 = hs >> 24;
	unsigned hs1 = (hs >> 16) & 0xff;
	unsigned hs2 = (hs >> 8) & 0xff;
	unsigned hs3 = hs & 0xff;

	unsigned h0 = hs0;
	unsigned h1 = hs1 ^ (h0 >> (s-8));
	unsigned h2 = (hs2 ^ (h0 << (16-s)) ^ (h1 >> (s-8))) & 0xff;
	unsigned h3 = (hs3 ^ (h1 << (16-s)) ^ (h2 >> (s-8))) & 0xff;
	return (h0<<24) + (h1<<16) + (h2<<8) + h3;
}

unsigned int murmur_hash_inverse(unsigned int h, unsigned int seed)
{
	const unsigned int m = 0x5bd1e995;
	const unsigned int minv = 0xe59b19bd;	// Multiplicative inverse of m under % 2^32
	const int r = 24;

	h = invert_shift_xor(h,15);
	h *= minv;
	h = invert_shift_xor(h,13);

	unsigned int hforward = seed ^ 4;
	hforward *= m;
	unsigned int k = hforward ^ h;
	k *= minv;
	k ^= k >> r;
	k *= minv;

	#ifdef PLATFORM_BIG_ENDIAN
		char *data = (char *)&k;
		k = (data[0]) + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
	#endif

	return k;
}

And for reference, here is the full code, with both the regular murmur hash and the inverses for 32- and 64-bit hashes:

unsigned int murmur_hash ( const void * key, int len, unsigned int seed )
{
	// 'm' and 'r' are mixing constants generated offline.
	// They're not really 'magic', they just happen to work well.

	const unsigned int m = 0x5bd1e995;
	const int r = 24;

	// Initialize the hash to a 'random' value

	unsigned int h = seed ^ len;

	// Mix 4 bytes at a time into the hash

	const unsigned char * data = (const unsigned char *)key;

	while(len >= 4)
	{
		#ifdef PLATFORM_BIG_ENDIAN
			unsigned int k = (data[0]) + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
		#else
			unsigned int k = *(unsigned int *)data;
		#endif

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
	}

	// Handle the last few bytes of the input array

	switch(len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
		h *= m;
	};

	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}

/// Inverts a (h ^= h >> s) operation with 8 <= s <= 16
unsigned int invert_shift_xor(unsigned int hs, unsigned int s)
{
	XENSURE(s >= 8 && s <= 16);
	unsigned hs0 = hs >> 24;
	unsigned hs1 = (hs >> 16) & 0xff;
	unsigned hs2 = (hs >> 8) & 0xff;
	unsigned hs3 = hs & 0xff;

	unsigned h0 = hs0;
	unsigned h1 = hs1 ^ (h0 >> (s-8));
	unsigned h2 = (hs2 ^ (h0 << (16-s)) ^ (h1 >> (s-8))) & 0xff;
	unsigned h3 = (hs3 ^ (h1 << (16-s)) ^ (h2 >> (s-8))) & 0xff;
	return (h0<<24) + (h1<<16) + (h2<<8) + h3;
}

unsigned int murmur_hash_inverse(unsigned int h, unsigned int seed)
{
	const unsigned int m = 0x5bd1e995;
	const unsigned int minv = 0xe59b19bd;	// Multiplicative inverse of m under % 2^32
	const int r = 24;

	h = invert_shift_xor(h,15);
	h *= minv;
	h = invert_shift_xor(h,13);

	unsigned int hforward = seed ^ 4;
	hforward *= m;
	unsigned int k = hforward ^ h;
	k *= minv;
	k ^= k >> r;
	k *= minv;

	#ifdef PLATFORM_BIG_ENDIAN
		char *data = (char *)&k;
		k = (data[0]) + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
	#endif

	return k;
}

uint64 murmur_hash_64(const void * key, int len, uint64 seed)
{
	const uint64 m = 0xc6a4a7935bd1e995ULL;
	const int r = 47;

	uint64 h = seed ^ (len * m);

	const uint64 * data = (const uint64 *)key;
	const uint64 * end = data + (len/8);

	while(data != end)
	{
		#ifdef PLATFORM_BIG_ENDIAN
			uint64 k = *data++;
			char *p = (char *)&k;
			char c;
			c = p[0]; p[0] = p[7]; p[7] = c;
			c = p[1]; p[1] = p[6]; p[6] = c;
			c = p[2]; p[2] = p[5]; p[5] = c;
			c = p[3]; p[3] = p[4]; p[4] = c;
		#else
			uint64 k = *data++;
		#endif

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	const unsigned char * data2 = (const unsigned char*)data;

	switch(len & 7)
	{
	case 7: h ^= uint64(data2[6]) << 48;
	case 6: h ^= uint64(data2[5]) << 40;
	case 5: h ^= uint64(data2[4]) << 32;
	case 4: h ^= uint64(data2[3]) << 24;
	case 3: h ^= uint64(data2[2]) << 16;
	case 2: h ^= uint64(data2[1]) << 8;
	case 1: h ^= uint64(data2[0]);
			h *= m;
	};

	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
}

uint64 murmur_hash_64_inverse(uint64 h, uint64 seed)
{
	const uint64 m = 0xc6a4a7935bd1e995ULL;
	const uint64 minv = 0x5f7a0ea7e59b19bdULL; // Multiplicative inverse of m under % 2^64
	const int r = 47;

	h ^= h >> r;
	h *= minv;
	h ^= h >> r;
	h *= minv;

	uint64 hforward = seed ^ (8 * m);
	uint64 k = h ^ hforward;

	k *= minv;
	k ^= k >> r;
	k *= minv;

	#ifdef PLATFORM_BIG_ENDIAN
		char *p = (char *)&k;
		char c;
		c = p[0]; p[0] = p[7]; p[7] = c;
		c = p[1]; p[1] = p[6]; p[6] = c;
		c = p[2]; p[2] = p[5]; p[5] = c;
		c = p[3]; p[3] = p[4]; p[4] = c;
	#endif

	return k;
}
 Managing Coupling
(This post has also been posted to http://altdevblogaday.com/.)


The only way of staying sane while writing a large complex software system is to regard it as a collection of smaller, simpler systems. And this is only possible if the systems are properly decoupled.
Ideally, each system should be completely isolated. The effect system should be the only system manipulating effects and it shouldn’t do anything else. It should have its own update() call just for updating effects. No other system should care how the effects are stored in memory or what parts of the update happen on the CPU, SPU or GPU. A new programmer wanting to understand the system should only have to look at the files in the effect_system directory. It should be possible to optimize, rewrite or drop the entire system without affecting any other code.
Of course, complete isolation is not possible. If anything interesting is going to happen, different systems will at some point have to talk to one another, whether we like it or not.
The main challenge in keeping an engine “healthy” is to keep the systems as decoupled as possible while still allowing the necessary interactions to take place. If a system is properly decoupled, adding features is simple. Want a wind effect in your particle system? Just write it. It’s just code. It shouldn’t take more than a day. But if you are working in a tightly coupled project, such seemingly simple changes can stretch out into nightmarish day-long debugging marathons.
If you ever get the feeling that you would prefer to test an idea out in a simple toy project rather than in “the real engine”, that’s a clear sign that you have too much coupling.
Sometimes, engines start out decoupled, but then as deadlines approach and features are requested that don’t fit the well-designed APIs, programmers get tempted to open back doors between systems and introduce couplings that shouldn’t really be there. Slowly, through this “coupling creep” the quality of the code deteriorates and the engine becomes less and less pleasant to work with.
Still, programmers cannot lock themselves in their ivory towers. “That feature doesn’t fit my API,” is never an acceptable answer to give a budding artist. Instead, we need to find ways of handling the challenges of coupling without destroying our engines. Here are four quick ideas to begin with:
1. Be wary of “frameworks”.
By a “framework” I mean any kind of system that requires all your other code to conform to a specific world view. For example, a scripting system that requires you to add a specific set of macro tags to all your class declarations.
Other common culprits are:

    Root classes that every object must inherit from
    RTTI/reflection systems
    Serialization systems
    Reference counting systems

Such global systems introduce a coupling across the entire engine. They rudely enforce certain design choices on all subsystems, design choices which might not be appropriate for them. Sometimes the consequences are serious. A badly thought out reference system may prevent subsystems from multithreading. A less than stellar serialization system can make linear loading impossible.
Often, the motivation given for such global systems is that they increase maintainability. With a global serialization system, we just have to make changes at a single place. So refactoring is much easier, it is claimed.
But in practice, the reverse is often true. After a while, the global system has infested so much of the code base that making any significant change to it is virtually impossible. There are just too many things that would have to be changed, all at the same time.
You would be much better off if each system just defined its own save() and load() functions.
2. Use high level systems to mediate between low level systems.
Instead of directly coupling low level systems, use a high level system to shuffle data between them. For example, handling footstep sounds might involve the animation system, the sound system and the material system. But none of these systems should know about the others.
So instead of directly coupling them, let the gameplay system handle their interactions. Since the gameplay system knows about all three systems, it can poll the animation system for events defined in the animation data, sample the ground material from the material system and then ask the sound system to play the appropriate sound.
Make sure that you have a clear separation between this messy gameplay layer, that can poke around in all other systems, and your clean engine code that is isolated and decoupled. Otherwise there is always a risk that the mess propagates downwards and infects your clean systems.
In the BitSquid Tech we put the messy stuff either in Lua or in Flow (our visual scripting tool, similar to Unreal’s Kismet). The language barrier acts as a firewall, preventing the spread of the messiness.
3. Duplicating code is sometimes OK!
Avoiding duplicated code is one of the fundamentals of software design. Entities should not be needlessly multiplied. But there are instances when you are better off breaking this rule.
I’m not advocating copy-paste-programming or writing complicated algorithms twice. I’m saying that sometimes people can get a little overzealous with their code reuse. Code sharing has a price that is not always recognized, in that it increases system coupling. Sometimes a little judiciously applied code duplication can be a better solution.
An typical example is the String class (or std::string if you are thusly inclined). In some projects you see the String class used almost everywhere. If something is a string, it should use the Stringclass, the reasoning seems to be. But many systems that handle strings do not need all the features that you find in your typical String class: locales, find_first_of(), etc. They are fine with just aconst char *, strcmp() and maybe one custom written (potentially duplicated) three-line function. So why not use that, the code will be much simpler and easier to move to SPUs.
Another culprit is FixedArray a. Sure, if you write int a[5] instead you will have to duplicate the code for bounds checking if you want that. But your code can be understood and compiled without fixed_array.h and template instantiation.
And if you have any method that takes a const Vector &v as argument you should probably take const T *begin, const T *end instead. Now you don’t need the vector.h header, and the caller is not forced to use a particular Vector class for storage.
A final example: I just wrote a patching tool that manipulates our bundles (aka pak-files). That tool duplicates the code for parsing the bundle headers, which is already in the engine. Why? Well, the tool is written in C# and the engine in C++, but in this case that is kind of beside the point. The point is that sharing that code would have been a significant effort.
First, it would have had to be broken out into a separate library, together with the related parts of the engine. Then, since the tool requires some functionality that the engine doesn’t (to parse bundles with foreign endianness) I would have to add a special function for the tool, and probably a #define TOOL_COMPILE since I don’t want that function in the regular builds. This means I need a special build configuration for the tool. And the engine code would forever be dirtied with the TOOL_COMPILE flag. And I wouldn’t be able to rearrange the engine code as I wanted in the future, since that might break the tool compile.
In contrast, rewriting the code for parsing the headers was only 10 minutes of work. It just reads a vector of string hashes. It's not rocket science. Sure, if I ever decide to change the bundle format, I might have to spend another 10 minutes rewriting that code. I think I can live with that.
Writing code is not the problem. The messy, complicated couplings that prevent you from writing code is the problem.
4. Use IDs to refer to external objects.
At some point one of your systems will have to refer to objects belonging to another system. For example, the gameplay layer may have to move an effect around or change its parameters.
I find that the most decoupled way of doing that is by using an ID. Let’s consider the alternatives.
Effect *, shared_ptr

    A direct pointer is no good, because it will become invalid if the target object is deleted and the effect system should have full control over when and how its objects are deleted. A standardshared_ptr won’t work for the same reason, it puts the life time of Effect objects out of the control of the effect system.

Weak_ptr, handle

    By this I mean some kind of reference-counted, indirect pointer to the object. This is better, but still too strongly coupled for my taste. The indirect pointer will be accessed both by the external system (for dereferencing and changing the reference count) and by the effect system (for deleting the Effect object or moving it in memory). This has the potential for creating threading problems.

    Also, this construct kind of implies that external systems can dereference and use the Effectwhenever they want to. Perhaps the effect system only allows that when its update() loop is not running and want to assert() that. Or perhaps the effect system doesn’t want to allow direct access to its objects at all, but instead double buffer all changes.

So, in order to allow the effect system to freely reorganize its data and processing in any way it likes, I use IDs to identify objects externally. The IDs are just an integers uniquely identifying an object, that the user can throw away when she is done with them. They don’t have to be “released” like aweak_ptr, which removes a point of interaction between the systems. It also means that the IDs are PODs. We can copy and move them freely in memory, juggle them in Lua and DMA them back-and-forth to our heart’s content. All of this would be a lot more complicated if we had to keep reference counts.
In the system we need a fast way of mapping IDs back to objects. Note that std::map is not a fast way! But there are a number of possibilities. The simplest is to just use a fixed size array with object pointers:
Object *lookup[MAX_OBJECTS];
If your system has a maximum of 4096 objects, use 12 bits from the key to store an index into this array and the remaining 20 bits as a unique identifier (i.e., to detect the case when the original object has been deleted and a new object has been created at the same index). If you need lots of objects, you can go to a 64 bit ID.
That's it for today, but this post really just scratches the surface of decoupling. There are a lot of other interesting techniques to look at, such as events, callbacks and “duck typing”. Maybe something for a future entry... Managing Coupling Part 2 — Polling, Callbacks and Events

In my last post, I talked a bit about the importance of decoupling and how one of the fundamental challenges in system design is to keep systems decoupled while still allowing the necessary interactions to take place.

This time I will look at one specific such challenge: when a low level system needs to notify a high level system that something has happened. For example, the animation system may want to notify the gameplay system that the character’s foot has touched the ground, so that a footstep sound can be played.

(Note that the reverse is not a problem. The high level system knows about the low level system and can call it directly. But the low level system shouldn’t know or care about the high level system.)

There are three common techniques for handling such notifications: polling, callbacks and events.

Polling

A polling system calls some function every frame to check if the event it is interested in has occurred. Has the file been downloaded yet? What about now? Are we there yet?

Polling is often considered “ugly” or “inefficient”. And indeed, in the desktop world, polling is very impolite, since it means busy-waiting and tying up 100 % of the CPU in doing nothing.

But in game development the situation is completely different. We are already doing a ton of stuff every 33 ms (or half a ton of stuff every 17 ms). As long as we don’t poll a huge amount of objects, polling won’t have any impact on the framerate.

And code that uses polling is often easier to write and ends up better designed than code that uses callbacks or events. For example, it is much easier to just check if the A key is pressed inside the character controller, than to write a callback that gets notified if A is pressed and somehow forward that information to the character controller.

So, in my opinion, you should actually prefer to use polling whenever possible (i.e., when you don’t have to monitor a huge number of objects).

Some areas where polling work well are: file downloads, server browsing, game saving, controller input, etc.

An area less suited for polling is physics collisions, since there are N*N possible collisions that you would have to poll for. (You could argue that rather than polling for a collision between two specific objects, you could poll for a collision between any two objects. My reply would be that in that case you are no longer strictly polling, you are in fact using a rudimentary effect system.)

Callbacks

In a callback solution, the low level system stores a list of high level functions to call when certain events occur.

An important question when it comes to callbacks is if the callback should be called immediately when the event occurs, or if it should be queued up and scheduled for execution later in the frame.

I much prefer the latter approach. If you do callbacks immediately you not only trash your instruction and data caches. You also prevent multithreading (unless you use locks everywhere to prevent the callbacks from stepping on each other). And you open yourself up to the nasty bug where a callback through a chain of events ends up destroying the very objects you are looping over.

It is much better to queue up all callbacks and only execute them when the high level system asks for it (with an execute_callbacks() call). That way you always know when the callbacks occur. Side effects can be minimized and the code flow is clearer. Also, with this approach there is no problem with generating callbacks on the SPU and merging the queue with other callback queues later.

The only thing you need to worry about with delayed callbacks is that the objects that the callback refers to might have been destroyed between the time when the callback was generated and the time when it was actually called. But this is neatly handled by using the ID reference system that I talked about in the previous post. Using that technique, the callback can always determine if the objects still exist.

Note that the callback system outlined here has some similarities with the polling system — in that the callbacks only happen when we explicitly poll for them.

It is not self-evident how to represent a callback in C++. You might be tempted to use a member function pointer. Don’t. The casting and typing rules make it near impossible to use them for any kind of generic callback mechanism. Also, don’t use an “observer pattern”, where the callback must be some object that inherits from an AnimationEventObserver class and overrides handle_animation_event(). That just leads to tons of typing and unnecessary heap allocation.

There is an interesting article about fast and efficient C++ delegates at http://www.codeproject.com/KB/cpp/FastDelegate.aspx. It looks solid, but personally I’m not comfortable with making something that requires so many platform specific tricks one of the core mechanisms of my engine.

So instead I use regular C function pointers for callbacks. This means that if I want to call a member function, I have to make a little static function that calls the member function. That is a bit annoying, but better than the alternatives.

(Isn’t it interesting that when you try to design a clean and flexible C++ API it often ends up as pure C.)

When you use C callbacks you typically also want to pass some data to them. The typical approach in the C world is to use a void * to “user data” that is passed to the callback function. I actually prefer a slightly different approach. Since I sometimes want to pass more data than a single void * I use something like this:

struct Callback16
{
    void (*f)(void);
    char data[12];
};

There aren’t a huge amount of callbacks, so using 16 bytes instead of 8 to store them doesn’t matter. You could go to Callback32 if you want the option to store even more data.

When calling the callback, I cast the function pointer to the appropriate type and pass a pointer to its data as the first parameter.

typedef void (*AnimationEventCallback)(void *, unsigned);
AnimationEventCallback f = (AnimationEventCallback)callback.f;
f(callback.data, event_id);

I’m not worried about casting the function pointer back and forth between a generic type and a specific one or about casting the data in and out of a raw buffer. Type safety is nice, but there is an awful lot of power in juggling blocks of raw memory. And you don’t have to worry that much about someone casting the data to the wrong type, because doing so will 99% of the time cause a huge spectacular crash, and the error will be fixed immediately.

Events

Event systems are in many ways similar to callback systems. The only difference is that instead of storing a direct pointer to a callback function, they store an event enum. The high level system that polls the events decides what action to take for each enum.

In my opinion, callbacks work better when you want to listen to specific notifications: “Tell me when this sound has finished playing.” Events work better when you process them in bulk: “Check all collision notifications to see if the forces involved are strong enough to break the objects.” But much of it is a matter of taste.

For storing the event queues (or callback queues) I just use a raw buffer (Vector orchar[FIXED_SIZE]) where I concatenate all events and their data:

[event_1_enum] [event_1_data] [event_2_enum] [event_2_data] …

The high level system just steps through this buffer, processing each event in turn. Note that event queues like this are easy to move, copy, merge and transfer between cores. (Again, the power of raw data buffers.)

In this design there is only a single high level system that polls the events of a particular low level system. It understands what all the events mean, what data they use and knows how to act on them. The sole purpose of the event system (it is not even much of a “system”, just a stream of data) is to pass notifications from the low level to the high.

This is in my opinion exactly what an event system should be. It should not be a magic global switchboard that dispatches events from all over the code to whoever wants to listen to them. Because that would be horrid! Managing Decoupling Part 3 - C++ Duck Typing
Some systems need to manipulate objects whose exact nature are not known. For example, a particle system has to manipulate particles that sometimes have mass, sometimes a full 3D rotation, sometimes only 2D rotation, etc. (A good particle system anyway, a bad particle system could use the same struct for all particles in all effects. And the struct could have some fields called custom_1,custom_2 used for different purposes in different effects. And it would be both inefficient, inflexible and messy.)

Another example is a networking system tasked with synchronizing game objects between clients and servers. A very general such system might want to treat the objects as open JSON-like structs, with arbitrary fields and values:

{
    "score" : 100,
    "name": "Player 1"
}


We want to be able to handle such “general” or “open” objects in C++ in a nice way. Since we care about structure we don’t want the system to be strongly coupled to the layout of the objects it manages. And since we are performance junkies, we would like to do it in a way that doesn’t completely kill performance. I.e., we don’t want everything to inherit from a base class Object and define our JSON-like objects as:

typedef std::map OpenStruct;


Generally speaking, there are three possible levels of flexibility with which we can work with objects and types in a programming language:


1. Exact typing - Only ducks are ducks


We require the object to be of a specific type. This is the typing method used in C and for classes without inheritance in C++.

2. Interface typing - If it says it’s a duck


We require the object to inherit from and implement a specific interface type. This is the typing method used by default in Java and C# and in C++ when inheritance and virtual methods are used. It is more flexible that the exact approach, but still introduces a coupling, because it forces the objects we manage to inherit a type defined by us.

Side rant: My general opinion is that while inheriting interfaces (abstract classes) is a valid and useful design tool, inheriting implementations is usually little more than a glorified “hack”, a way of patching parent classes by inserting custom code here and there. You almost always get a cleaner design when you build your objects with composition instead of with implementation inheritance.

3. Duck typing - If it quacks like a duck


We don’t care about the type of the object at all, as long as it has the fields and methods that we need. An example:

      def integrate_position(o, dt):
          o.position = o.position + o.velocity * dt

This method integrates the position of the object o. It doesn’t care what the type of o is, as long as it has a “position” field and a “velocity” field.

Duck typing is the default in many “scripting” languages such as Ruby, Python, Lua and JavaScript. The reflection interface of Java and C# can also be used for duck typing, but unfortunately the code tends to become far less elegant than in the scripting languages:

      o.GetType().GetProperty(“Position”).SetValue(o, o.GetType().
         GetProperty(“Position”).GetValue(o, null) + o.GetType().
         GetProperty(“Velocity”).GetValue(o, null) * dt, null)

What we want is some way of doing “duck typing” in C++.

Let’s look at inheritance and virtual functions first, since that is the standard way of “generalizing” code in C++. It is true that you could do general objects using the inheritance mechanism. You would create a class structure looking something like:

class Object {...};
class Int : public Object {...};
class Float : public Object{...};

and then use dynamic_cast or perhaps your own hand-rolled RTTI system to determine an object’s class.
But there are a number of drawbacks with this approach. It is quite verbose. The virtual inheritance model requires objects to be treated as pointers so they (probably) have to be heap allocated. This makes it tricky to get a good memory layout. And that hurts performance. Also, they are not PODs so we will have to do extra work if we want to move them to a co-processor or save them to disk.

So I prefer something much simpler. A generic object is just a type enum followed by the data for the object:



To pass the object you just pass its pointer. To make a copy, you make a copy of the memory block. You can also write it straight to disk and read it back, send it over network or to an SPU for off-core processing.

To extract the data from the object you would do something like:

unsigned type = *(unsigned *)o;
if (type == FLOAT_TYPE)
    float f = *(float *)(o + 4);


You don’t really need that many different object types: bool, int, float, vector3, quaternion, string,array and dictionary is usually enough. You can build more complicated types as aggregates of those, just as you do in JSON.

For a dictionary object we just store the name/key and type of each object:



I tend to use a four byte value for the name/key and not care if it is an integer, float or a 32-bit string hash. As long as the data is queried with the same key that it was stored with, the right value will be returned. I only use this method for small structs, so the probability for a hash collision is close to zero and can be handled by “manual resolution”.

If we have many objects with the same “dictionary type” (i.e. the same set of fields, just different values) it makes sense to break out the definition of the type from the data itself to save space:



Here the offset field stores the offset of each field in the data block. Now we can efficiently store an array of such data objects with just one copy of the dictionary type information:



Note that the storage space (and thereby the cache and memory performance) is exactly the same as if we were using an array of regular C structs, even though we are using a completely open free form JSON-like struct. And extracting or changing data just requires a little pointer arithmetic and a cast.

This would be a good way of storing particles in a particle system. (Note: This is an array-of-structures approach, you can of course also use duck typing with a sturcture-of-arrays approach. I leave that as an exercise to the reader.)

If you are a graphics programmer all of this should look pretty familiar. The “dictionary type description” is very much like a “vertex data description” and the “dictionary data” is awfully similar to “vertex data”. This should come as no big surprise. Vertex data is generic flexible data that needs to be processed fast in parallel on in-order processing units. It is not strange that with the same design criterions we end up with a similar solution.


Morale and musings

It is OK to manipulate blocks of raw memory! Pointer arithmetic does not destroy your program! Type casts are not “dirty”! Let your freak flag fly!

Data-oriented-design and object-oriented design are not polar opposites. As this example shows a data-oriented design can in a sense be “more object-oriented” than a standard C++ virtual function design, i.e., more similar to how objects work in high level languages such as Ruby and Lua.

On the other hand, data-oriented-design and inheritance are enemies. Because designs based on base class pointers and virtual functions want objects to live individually allocated on the heap. Which means you cannot control the memory layout. Which is what DOD is all about. (Yes, you can probably do clever tricks with custom allocators and patching of vtables for moving or deserializing objects, but why bother, DOD is simpler.)

You could also store function pointers in these open structs. Then you would have something very similar to Ruby/Lua objects. This could probably be used for something great. This is left as an exercise to the reader. Managing Decoupling Part 4 -- The ID Lookup Table
Today I am going to dig deeper into an important and versatile data structure that pops up all the time in the BitSquid engine -- the ID lookup table.

I have already talked about the advantages of using IDs to refer to objects owned by other systems, but let me just quickly recap.

IDs are better than direct pointers because we don’t get dangling references if the other system decides that the object needs to be destroyed.

IDs are better than shared_ptr<> and weak_ptr<> because it allows the other system to reorganize its objects in memory, delete them at will and doesn’t require thread synchronization to maintain a reference count. They are also POD (plain old data) structures, so they can be copied and moved in memory freely, passed back and forth between C++ and Lua, etc.

By an ID I simply mean an opaque data structure of n bits. It has no particular meaning to us, we just use it to refer to an object. The system provides the mechanism for looking up an object based on it. Since we seldom create more than 4 billion objects, 32 bits is usually enough for the ID, so we can just use a standard integer. If a system needs a lot of objects, we can go to 64 bits.

In this post I’m going to look at what data structures a system might use to do the lookup from ID to system object. There are some requirements that such data structures need to fulfill:

    There should be a 1-1 mapping between live objects and IDs.
    If the system is supplied with an ID to an old object, it should be able to detect that the object is no longer alive.
    Lookup from ID to object should be very fast (this is the most common operation).
    Adding and removing objects should be fast.


Let’s look at three different ways of implementing this data structure, with increasing degrees of sophistication.

The STL Method

The by-the-book object oriented approach is to allocate objects on the heap and use a std::map to map from ID to object.

typedef unsigned ID;

struct System
{
	ID _next_id;
	std::map<ID, Object *> _objects;

	System() {_next_id = 0;}

	inline bool has(ID id) {
		return _objects.count(id) > 0;
	}

	inline Object &lookup(ID id) {
		return *_objects[id];
	}

	inline ID add() {
		ID id = _next_id++;
		Object *o = new Object();
		o->id = id;
		_objects[id] = o;
		return id;
	}

	inline void remove(ID id) {
		Object &o = lookup(id);
		_objects.erase(id);
		delete &o;
	}
};


Note that if we create more than four billion objects, the _next_id counter will wrap around and we risk getting two objects with the same ID.

Apart from that, the only problem with this solution is that it is really inefficient. All objects are allocated individually on the heap, which gives bad cache behavior and the map lookup results in tree walking which is also bad for the cache. We can switch the map to a hash_map for slightly better performance, but that still leaves a lot of unnecessary pointer chasing.

Array With Holes

What we really want to do is to store our objects linearly in memory, because that will give us the best possible cache behavior. We can either use a fixed size array Object[MAX_SIZE] if we know the maximum number of objects that will ever be used, or we can be more flexible and use a std::vector.

Note: If you care about performance and use std::vector<T> you should make a variant of it (call it array<T> for example) that doesn’t call constructors or initializes memory. Use that for simple types, when you don’t care about initialization. A dynamic vector<T> buffer that grows and shrinks a lot can spend a huge amount of time doing completely unnecessary constructor calls.

To find an object in the array, we need to know its index. But just using the index as ID is not enough, because the object might have been destroyed and a new object might have been created at the same index. To check for that, we also need an id value, as before. So we make the ID type a combination of both:

struct ID {
	unsigned index;
	unsigned inner_id;
};


Now we can use the index to quickly look up the object and the inner_id to verify its identity.

Since the object index is stored in the ID which is exposed externally, once an object has been created it cannot move. When objects are deleted they will leave holes in the array.

￼

When we create new objects we don’t just want to add them to the end of the array. We want to make sure that we fill the holes in the array first.

The standard way of doing that is with a free list. We store a pointer to the first hole in a variable. In each hole we store a pointer to the next hole. These pointers thus form a linked list that enumerates all the holes.


An interesting thing to note is that we usually don’t need to allocate any memory for these pointers. Since the pointers are only used for holes (i. e. dead objects) we can reuse the objects’ own memory for storing them. The objects don’t need that memory, since they are dead.

Here is an implementation. For clarity, I have used an explicit member next in the object for the free list rather than reusing the object’s memory:

struct System
{
	unsigned _next_inner_id;
	std::vector<Object> _objects;
	unsigned _freelist;

	System() {
		_next_inner_id = 0;
		_freelist = UINT_MAX;
	}

	inline bool has(ID id) {
		return _objects[id.index].id.inner_id == id.inner_id;
	}

	inline Object &lookup(ID id) {
		return _objects[id.index];
	}

	inline ID add() {
		ID id;
		id.inner_id = _next_inner_id++;
		if (_freelist == UINT_MAX) {
			Object o;
			id.index = _objects.size();
			o.id = id;
			_objects.push_back(o);
		} else {
			id.index = _freelist;
			_freelist = _objects[_freelist].next;
		}
		return id;
	}

	inline void remove(ID id) {
		Object &o = lookup(id);
		o.id.inner_id = UINT_MAX;
		o.next = _freelist;
		_freelist = id.index;
	}
};


This is a lot better than the STL solution. Insertion and removal is O(1). Lookup is just array indexing, which means it is very fast. In a quick-and-dirty-don’t-take-it-too-seriously test this was 40 times faster than the STL solution. In real-life it all depends on the actual usage patterns, of course.

The only part of this solution that is not an improvement over the STL version is that our ID structs have increased from 32 to 64 bits.

There are things that can be done about that. For example, if you never have more than 64 K objects live at the same time, you can get by with 16 bits for the index, which leaves 16 bits for the inner_id. Note that the inner_id doesn’t have to be globally unique, it is enough if it is unique for that index slot. So a 16 bit inner_id is fine if we never create more than 64 K objects in the same index slot.

If you want to go down that road you probably want to change the implementation of the free list slightly. The code above uses a standard free list implementation that acts as a LIFO stack. This means that if you create and delete objects in quick succession they will all be assigned to the same index slot which means you quickly run out of inner_ids for that slot. To prevent that, you want to make sure that you always have a certain number of elements in the free list (allocate more if you run low) and rewrite it as a FIFO. If you always have N free objects and use a FIFO free list, then you are guaranteed that you won’t see an inner_id collision until you have created at least N * 64 K objects.

Of course you can slice and dice the 32 bits in other ways if you hare different limits on the maximum number of objects. You have to crunch the numbers for your particular case to see if you can get by with a 32 bit ID.

Packed Array

One drawback with the approach sketched above is that since the index is exposed externally, the system cannot reorganize its objects in memory for maximum performance.

The holes are especially troubling. At some point the system probably wants to loop over all its objects and update them. If the object array is nearly full, no problem, But if the array has 50 % objects and 50 % holes, the loop will touch twice as much memory as necessary. That seems suboptimal.

We can get rid of that by introducing an extra level of indirection, where the IDs point to an array of indices that points to the objects themselves:


This means that we pay the cost of an extra array lookup whenever we resolve the ID. On the other hand, the system objects are packed tight in memory which means that they can be updated more efficiently. Note that the system update doesn’t have to touch or care about the index array. Whether this is a net win depends on how the system is used, but my guess is that in most cases more items are touched internally than are referenced externally.

To remove an object with this solution we use the standard trick of swapping it with the last item in the array. Then we update the index so that it points to the new location of the swapped object.

Here is an implementation. To keep things interesting, this time with a fixed array size, a 32 bit ID and a FIFO free list.

typedef unsigned ID;

#define MAX_OBJECTS 64*1024
#define INDEX_MASK 0xffff
#define NEW_OBJECT_ID_ADD 0x10000

struct Index {
	ID id;
	unsigned short index;
	unsigned short next;
};

struct System
{
	unsigned _num_objects;
	Object _objects[MAX_OBJECTS];
	Index _indices[MAX_OBJECTS];
	unsigned short _freelist_enqueue;
	unsigned short _freelist_dequeue;

	System() {
		_num_objects = 0;
		for (unsigned i=0; i<MAX_OBJECTS; ++i) {
			_indices[i].id = i;
			_indices[i].next = i+1;
		}
		_freelist_dequeue = 0;
		_freelist_enqueue = MAX_OBJECTS-1;
	}

	inline bool has(ID id) {
		Index &in = _indices[id & INDEX_MASK];
		return in.id == id && in.index != USHRT_MAX;
	}

	inline Object &lookup(ID id) {
		return _objects[_indices[id & INDEX_MASK].index];
	}

	inline ID add() {
		Index &in = _indices[_freelist_dequeue];
		_freelist_dequeue = in.next;
		in.id += NEW_OBJECT_ID_ADD;
		in.index = _num_objects++;
		Object &o = _objects[in.index];
		o.id = in.id;
		return o.id;
	}

	inline void remove(ID id) {
		Index &in = _indices[id & INDEX_MASK];

		Object &o = _objects[in.index];
		o = _objects[--_num_objects];
		_indices[o.id & INDEX_MASK].index = in.index;

		in.index = USHRT_MAX;
		_indices[_freelist_enqueue].next = id & INDEX_MASK;
		_freelist_enqueue = id & INDEX_MASK;
	}
};
 Dependency management in classes
Written on July 30th, 2018 by Taras Kushnir

Object-oriented languages like Ruby, C++ or Java tend to have one common problem among others: dependency injection. It is very rare for a class to be fully self-contained and never depend on anything else in your code. Usually there’re multiple connections between them: simple, transitive or even circular. And designing good dependency injection often makes a huge difference in terms of maintainability and testability of your code. This is the thing that should be done properly rather sooner than later.

Let’s review our possible options. I’d like to start with least maintainable and testable solutions first:
God object

“God object” is a class that has all other dependencies accessible from and that all classes in your code depend on. At a first point of view it makes things look simple: all your classes have only one dependency (the god object itself) and this makes their interface simple. God object on the other hand, has all or majority of the classes of your codebase listed as dependency. This approach may work for small projects that do not have many dependencies but on the long run it makes things complicated to test and the god object itself - a huge monster hard to maintain and change.

class GodObject {
  public getServiceA() { return serviceA; }
  public getServiceB() { return serviceB; }
  // ...

  private ServiceA serviceA;
  private ServiceB serviceB;
  // ...
}

class A {
  public A (GodObject obj) {
    this.serviceA = obj.getServiceA();
    this.serviceB = obj.getServiceB();
  }
}

Dependency injection containers

This is a very popular way of managing the dependencies but on the inside this is a dynamic variation of a god object. It is slightly more generic in a way that it doesn’t depend on classes of your codebase directly, but instead lets you build dependencies “on the fly”.

container.inject<ServiceA>(new ServiceA());
container.inejct<ServiceB>(new ServiceB());

class A {
  public A (DIContainer container) {
    this.serviceA = container.resolve<ServiceA>();
    this.serviceB = container.resolve<ServiceB>();
  }
}

Dependencies of the class A now are implicit: it’s hard to tell them without reading it’s whole code. Also it’s very easy to make a mistake of bringing new dependency to the class which your will forget to satisfy and it will not be checked by the compiler.
Messaging systems

In this case you can make use of technique used in distributed systems to reduce complecity and make use of dependencies: messages. A message is a self-contained piece of “parameters pack” (or data transfer object) passed to the unknown at a compile time target. Your class accepts “message hub” as a depedency. Message hub is a class that connects senders with receivers of the messages. Arbitrary entity can sign up for certain messages as a receiver and when anybody will produce messages of this kind, message hub will notify all interested receivers.

source.addListener(target);
source.emit(new Message());

class Target {
  public handleMessage(Message msg) {
    // do something
  }
}

This sounds pretty much as slightly inverted dependency injection container. Instead of requesting known services to call them directly, you send known messages (parameters) to unknown APIs. Learning dependencies of your class would require to read it’s source code completely. If you will not satisfly the dependency the compiler will not let you know about it.
Dependency setters

A step aside from dependency injection containers are public setters that inject the dependencies directly. They make interface of a class more explicit but tend to bloat it significantly.

class A {
  public void setServiceA(ServiceA serviceA) {
    this.serviceA = serviceA;
  }

  public void setServiceB(ServiceB serviceB) {
    this.serviceB = serviceB;
  }

  // ...
}

Also this approach makes you check if you have injected services A and B in places where you want to make use of them.
Construction time injection

Injecting the dependencies at construction time is very clean and maintainable way to manage dependencies. A class constructor that accepts generic interfaces of dependencies is very easy to test.

class A {
  public A (IServiceA serviceA, IServiceB serviceB) {
    this.serviceA = serviceA;
    this.serviceB = serviceB;
  }
}

A contract of this class is explicit, immutable and known on the compile time. You cannot introduce new implicit dependency somewhere inside your class. It is easy to inject fake objects for testing purposes. Also it introduces single place where you check validity of the injected dependencies so you don’t have to do it all over your code of class A. If your construction time dependencies begin to overflow one line of code you know it’s time to refactor your classes into smaller ones.
Bottom line

I would say it’s obvious why construction time injection should be preffered among others: strict API and compile-time checks. Of course this is not a silver bullet but if you don’t have strong arguments for anything else - make yourself a favor and use it. Implementing spellchecking in desktop application in C++
Written on June 5th, 2016 by Taras Kushnir

When user is supposed to enter significant amount of text in your application, it’s better to help him/her to control it with checking spelling. Basically, to check spelling you need a dictionary with words and algorithm to order these words. Also it might be useful to provide user with possible corrections for any spelling error. Here where Hunspell comes handy. It’s an open source library built on top of MySpell library and used in a significant number of projects varying from open source projects like Firefox to proprietary like OS X. It contains bindings to a number of platforms (.NET, Ruby etc.) and should be fairly easy to integrate to your project. In this post I’ll discuss how to integrate it to C++/Qt project.

First of all, you should download Hunspell source code and try to build it for your platform. You can take a look at README for the instructions. Once you’re done, it’s time to link just built library with your project. Also you should add Hunspell include files to your include path in order to use the API.

After you’re done, let’s include C++ Hunspell API to your header and add following variable to your class

Hunspell *m_Hunspell;

Constructor of Hunspell class takes paths to DIC and AFF files (wordlist and affix files). If you’re building cross-platform solution, it will be useful to know, that to handle utf-8 paths in Windows, you need to prefix paths to DIC and AFF files with “\?". Loading code in my Qt project looks like this:

#ifdef Q_OS_WIN
// specific Hunspell handling of UTF-8 encoded pathes
affPath = "\\\\?\\" + QDir::toNativeSeparators(affPath);
dicPath = "\\\\?\\" + QDir::toNativeSeparators(dicPath);
#endif

try {
    m_Hunspell = new Hunspell(affPath.toUtf8().constData(),
                              dicPath.toUtf8().constData());
    LOG_DEBUG << "Hunspell with AFF" << affPath << "and DIC" << dicPath;
    m_Encoding = QString::fromLatin1(m_Hunspell->get_dic_encoding());
    m_Codec = QTextCodec::codecForName(m_Encoding.toLatin1().constData());
}
catch(...) {
    LOG_DEBUG << "Error in Hunspell with AFF" << affPath << "and DIC" << dicPath;
    m_Hunspell = NULL;
}

In this code except of instantiating Hunspell class we also get right Codec to query the dictionary. Now you can use API’s of Hunspell class to access spellchecking API. To get real AFF and DIC files, you can check out a number of open source projects which use spellchecking and hunspell - e.g. OpenOffice.

The most common operation is, of course, to check if particular word is spelled OK or not:

bool isSpellingCorrect(const QString &word) const {
    bool isOk = false;
    try {
        isOk = m_Hunspell->spell(m_Codec->fromUnicode(word).constData()) != 0;
    }
    catch (...) {
        isOk = false;
    }
    return isOk;
}

This demonstrates also usage of Codec retrieved before.

Besides of checking spelling, it’s useful to provide user with corrections for the particular word. Hunspell class has API for this and it can be used like this:

QStringList suggestCorrections(const QString &word) {
    QStringList suggestions;
    char **suggestWordList = NULL;

    try {
        // Encode from Unicode to the encoding used by current dictionary
        int count = m_Hunspell->suggest(&suggestWordList, m_Codec->fromUnicode(word).constData());
        QString lowerWord = word.toLower();

        for (int i = 0; i < count; ++i) {
            QString suggestion = m_Codec->toUnicode(suggestWordList[i]);
            suggestions << suggestion;
            free(suggestWordList[i]);
        }
    }
    catch (...) {
        LOG_WARNING << "Error for keyword:" << word;
    }

    return suggestions;
}

This code demonstrates usage of suggest() API of Hunspell object. Also useful tip would be to check case of the suggestion, since Hunspell can correct you word like “europe” with “Europe” and stuff like that.

If you’re going to check spelling on the fly it might be a good idea to combine this approach with producer-consumer implemented in Qt. So your app’s UI will be responsive while background worker will serve spelling requests. A very fast approach to auto complete (or search suggestions)
You've seen search engines suggest queries when you begin typing the first few letters of your search string. This is being done by Duck Duck Go as well as Google (to name a few). This is typically done by maintaining a list of past queries and/or important strings that the search engine thinks are worthy of being suggested to a user that is trying to find something similar. These suggestions are effective only if the search engine spits them out very fast since these should show up on the screen before the user has finished typing what he/she wanted to type. Hence the speed with which these suggestions are made is very critical to the usefulness of this feature.

Let us consider a situation (and a possible way of approaching this problem) in which when a user enters the first few letters of a search query, he/she is presented with some suggestions that have as their prefix, the string that the user has typed. Furthermore, these suggestions should be ordered by some score that is associated with each such suggestion.

Approach-1:

Our first attempt at solving this would probably involve keeping the initial list of suggestions sorted in lexicographic order so that a simple binary search can give us the 2 ends of the list of strings that serve as candidate suggestions. These are all the strings that have the user's search query as a prefix. We now need to sort all these candidates by their associated score in non-increasing order and return the first 6 (say). We will always return a very small subset (say 6) of the candidates because it is not feasible to show all candidates since the user's screen is of bounded size and we don't want to overload the user with too many options. The user will get better suggestions as he/she types in more letters into the query input box.

We immediately notice that if the candidate list (for small query prefixes say of length 3) is large (a few thousand), then we will be spending a lot of time sorting these candidates by their associated score. The cost of sorting is O(n log n) since the candidate list may be as large as the original list in the worst case. Hence, this is the total cost of the approch. Apache's solr uses this approach. Even if we keep the scores bound within a certain range and use bucket sort, the cost is still going to be O(n). We should definitely try to do better than this.


Approach-2:

One way of speeding things up is to use a Trie and store (pointers or references to) the top 6 suggestions at or below that node in the node itself. This idea is mentioned here. This results in O(m) query time, where m is the length of the prefix (or user's search query).

However, this results in too much wasted space because:

    Tries are wasteful of space and
    You need to store (pointers or references to) 6 suggestions at each node which results in a lot of redundancy of data


We can mitigate (1) by using Radix(or Patricia) Trees instead of Tries.


Approach-3:

There are also other approaches to auto-completion such as prefix expansion that are using by systems such as redis. However, these approaches use up memory proportional to the square of the size of each suggestion (string). The easy way to get around this is to store all the suggestions as a linear string (buffer) and represent each suggestion as an (index,offset) pair into that buffer. For example, suppose you have the strings:

    hello world
    hell breaks lose
    whiteboard
    blackboard


Then your buffer would look like this:
hello worldhell breaks losewhiteboardblackboard
And the 4 strings are actually represented as:
(0,11), (11,16), (27,10), (37,10)

Similarly, each prefix of the suggestion "whiteboard" is:

    w => (27,1)
    wh => (27,2)
    whi => (27,3)
    whit => (27,4)
    white => (27,5)
    whiteb => (27,6)
    and so on...


Approach-4:

We can do better by using Segment (or Interval) Trees. The idea is to keep the suggestions sorted (as in approach-1), but have an additional data structure called a Segment Tree which allows us to perform range searches very quickly. You can query a range of elements in Segment Tree very efficiently. Typically queries such as min, max, sum, etc... on ranges in a segment tree can be answered in O(log n) where n is the number of leaf nodes in the Segment Tree. So, once we have the 2 ends of the candidate list, we perform a range search to get the element with the highest score in the list of candidates. Once we get this node, we insert this range (with the maximum score in that range as the key) into the priority queue. The top element in the queue is popped and split at the location where the element with the highest score occurs and the scores for the 2 resulting ranges are computed and pushed back into the priority queue. This continues till we have popped 6 elements from the priority queue. It is easy to see that we will have never considered more than 2k ranges (here k = 6).

Hence, the complexity of the whole process is the sum of:

    The complexity for the range calculation: O(log n) (omitting prefix match cost) and
    The complexity for a range search on a Segment Tree performed 2k times: O(2k log n) (since the candidate list can be at most 'n' in length)


Giving a total complexity of:
O(log n) + O(2k log n)
which is: O(k log n)

Update (29th October, 2010): I have implemented the approach described above in the form of a stand-alone auto-complete server using Pyhton and Mongrel2. You can download it from here. (11th February, 2012): lib-face is now called cpp-libface and has a new home at github!

Some links I found online:
LingPipe - Text Processing and Computationsal Linguistics Tool Kit
What is the best open source solution for implementing fast auto-complete?
Suggest Trees (SourceForge): Though these look promising, treaps in practice can degenerate into a linear list.
Autocomplete Data Structures
Why is the autocomplete for Quora so fast?
Autocomplete at Wikipedia
How to implement a simple auto-complete functionality? (Stack Overflow)
Auto Compete Server (google code)
Three Autocomplete implementations compared
Trie Data Structure for Autocomplete?
What is the best autocomplete/suggest algorithm,datastructure [C++/C] (Stack Overflow)
Efficient auto-complete with a ternary search tree Implementing autocomplete for English in C++
Written on March 27th, 2016 by Taras Kushnir

When it comes to implementing autocompletion in C++ in some type of input field, the question is which algorithm to choose and where to get the source for completion. In this post I’ll try to answer both questions.

As for the algorithm, SO gives us hints about tries, segment trees and others. You can find good article about them. Author has implemented some of them in a repository called FACE (fastest auto-complete in the east). You can easily find it on GitHub. This solution is used for the autocompletion in search engine Duck-Duck-Go which should tell you how good it is. Unfortunately their solution requires dependencies on libuv and joyent http-parser, which is not good in case you need just to integrate autocompletion functionality into your C++ application, but not build auto-complete server and send queries to it. Another drawback - libuv and cpp-libface itself fails to compile in Windows which is bad in case you’re building cross-platform solution.

You can find out how to built FACE into your cross-platform C++ application below.

Here comes my fork: library version of cpp-libface. I’ve removed dependencies on libuv and http-parser. For Windows, one would need also mman - memory-mapped files IO. The only such thing I was able to find was mman-win32 - cygwin port of mman from *nix’es. After puking thousands of warning, VS compiler did it’s job and in real life mman-win32 worked so it’s ready at least for testing. I’ve created two Makefiles: for Windows and OS X/Linux. After building you will get library FACE (libface.lib or libface.a) and PoC executable for testing of auto-completion through the command line.

As for the interface of libface, I have left only two methods:

    bool import(const char *ac_file);
    vp_t prompt(std::string prefix, uint_t n = 16);

one of which allows you to import a file and another - generate completion suggestions.

But it’s only half of the story. Also you need source for completion. Library FACE is able to digest TSV files (Tab-Separated Values) where first column is frequency of phrase/word and second column - phrase/word itself. After searching through the internet for some time I’ve found frequency tables for different languages for Android. There are in the XML with simple structure and simple Ruby script written in 5 minutes transformed them into TSV:

require 'nokogiri'
doc = File.open(ARGV.first) { |f| Nokogiri::XML(f) }

result_filename = ARGV.first.sub(/[.]xml$/, '.tsv')
tsv_file = File.open(result_filename, 'w')

doc.xpath('//w').each do |node|
  frequency = node['f'];
  word = node.content

  tsv_file.puts("#{frequency}\t#{word}")
end

tsv_file.close

puts "Done"

Now everything is ready and you can easily integrate library FACE into your C++ solution and fed it with tsv dicts.Memory-align buffers and structures to WORD size of your architecture (4 bytes for 32-bit and 8 bytes for 64-bit)
Use arrays instead of linked lists (to use memory block caching)
Avoid “if” stamements in loops
If-clause should contain code, which is more likely to execute (if-condition == true)
Use inlining for short functions
Use objects allocated on stack but not on heap (local objects for functions instead of allocated with malloc/new)
Use pre-calculated hardcoded data (e.g. you can store first N prime numbers or first N Fibonacci numbers in order not to calculate them every time you need one) Developer competencies list with useful links (preparing to the interview)
Written on October 7th, 2012 by Taras Kushnir

Starting short training before competency test at my current work. You can find list of things you have to know. And comparison to those from Amazon. Below you can find advices what or where to read to be ready to interview or smth like this

Needed materials and topics can be found here:

Data Structures - array, linked list, stack, queue, dictionary, hash table, tree, binary search tree, hashtable, heap, AVL/RB tree, B-tree, skip list, segment tree

Useful links: Arrays article, Article about structures, used to implement dictionaries, Skip list implementation

Algorithms - sorting, searching, data structure traversal algorithms, algorithm complexity, O() notation, space/time complexity, greedy and dynamics algorithms, tree, graph algorithms, N-logN sorting

Useful links: Several O(N*logN) sorting implementations, quite good Wiki article about Binary search

Graph algorithms - dfs, bfs, loops (usual and Euler’s), Floyd-Warshall, Kruskal’s algorithm

Concurency - shared state and synchronization problems, sync primitives, basic parallel algorithms, sync primitives efficiency, data and task parallelism patterns, deadlocks, race conditions, thread scheduling details, GUI threading models

The best info about concurency&threads&processes you can find in Richter’s “CLR via C# 3rd edition” book and in APUE book (“Advanced Programming in Unix Environment”). Richter gives samples and explanations, and APUE gives real world situation. Thread scheduling article, WPF threading model

Coding skills - OOP, refactoring, static and dynamic typing, script and compiled languages, closures, declarative programming, lazy evaluation, tail recursion, functional programming, code generation

Basic things can be found in Wiki and StackOverflow. Declarative programming topic on SO, about tail recursion and lazy evaluation see some functional language (say, ocaml). Short intro to code generation in wiki

Low-level programming - PC architecture, memory, processor, multitasking, address space, heap memory, stack, virtual machine concept, kernel mode vs user mode, process context, memory address translation, swap, static and dynamic linking, compilation, interpretation, jit compilation, garbage collection, memory addressing, interrupts + microcode

Multitasking article, Stack&Heap, Quite nice pdf about Virtual Memory and address translation

Architecture - architecture layers, common-used design patterns, describe component with diagrams, SOA, communication with RPC or message-based

Database - SQL queries, transactions, ACID, views, stored procedures, triggers, serializable transactions, normal forms

Testing - unit tests, refactor code to be able to test it, integration tests, moqs and stubs

Network and communication - basic understanding of network concepts, web services, HTTP, DNS, SSL, socket-level programming, SOAP, JSON, whole network stack, OSI model, stateful/stateless models

Source control, CI - merge code, resolve conflicts, CI, automation scripts

For comparison, same table from Amazon

Tech Prep Tips

Algorithm Complexity: you need to know Big-O.

Sorting: know how to sort: the details of at least one n*log(n) sorting algorithm, preferably two (say, quicksort and merge sort). Merge sort can be highly useful in situations where quicksort is impractical, so take a look at it.

Hashtables

Trees: basic tree construction, traversal and manipulation algorithms. Binary trees, n-ary trees, and trie-trees at the very very least. At least one flavor of balanced binary tree,  whether it’s a red/black tree, a splay tree or an AVL tree. Tree traversal algorithms: BFS and  DFS, the difference between inorder, postorder and preorder.

Graphs: There are three basic ways to represent a graph in memory (objects and pointers,

matrix, and adjacency list), each representation and its pros and cons.

The basic graph traversal algorithms: breadth-first search and depth-first search. Their

computational complexity, their tradeoffs, and how to implement them in real code.

Dijkstra and A*, if you get a chance.

Whenever someone gives you a problem, think graphs. They are the most fundamental and flexible way of representing any kind of a relationship, so it’s about a 50-50 shot that any interesting design problem has a graph involved in it. Make absolutely sure you can’t   think of a way to solve it using graphs before moving on to other solution types. This tip is  important!

Other data structures

Math – a plus if you go over it, but not a must

Basic discrete math questions. Counting problems, probability problems, and other Discrete Math 101 situations. Familiarity with n-choose-k problems and their ilk.

Operating Systems: Processes, threads and concurrency issues. Locks and mutexes and

semaphores and monitors and how they work. Deadlock and livelock and how to avoid them. What resources a processes needs, and a thread needs, and how context switching  works, and how it’s initiated by the operating system and underlying hardware. A little  scheduling.

Concurrent Programming in Java.

Coding: Preferably C++ or Java. C# is OK. Please know a fair amount of detail about yourix Sysadmin Aphorisms
With Commentary

The man page has an EXAMPLES section.
    It also has a FILES section. These are all useful and can help clear up the sometimes obtuse descriptions found in the OPTIONS section.

    You can also learn quite a lot by tracking down the man pages listed in the SEE ALSO section. Often, what you're looking for may be found there.

    Originally submitted by William S. Annis on 21dec99.

Do you really need perl for this problem?
    There is a strong tendency for people who first learn perl to use it to attack all problems. This is often overkill. Don't rewrite grep in perl, use grep.

    Originally submitted by William S. Annis on 21dec99.

Does the user think it's fixed?
    When fixing a problem reported by a user, make sure the fix you think works also works for them.

    Originally submitted by William S. Annis on 21dec99.

Swap is three times real memory.
    Will you add more memory later?

    Originally submitted by William S. Annis on 21dec99.

Don't rewrite cat -n
    Make sure the tool you've decided to write doesn't already exist. Someone has probably already seen the problem you are trying to solve.

    I've seen cat -n rewritten several times. I've also been told this functionality existed even earlier in the command nl(1).

    Originally submitted by William S. Annis on 21dec99.

What have you changed? Whom did you tell?
    It is vitally important that the sysadmins you work with know what it is you're doing to the system. This goes beyond merely using your version control system dutifully on any system files. It means telling your coworkers when you do things. This is especially vital when the admin staff is spread throughout a building, city or the world and direct interaction isn't constant.

    Where I work currently we have an email alias to which admins are expected to send reports on system things they change which have any small chance of breaking things. This alias sends mail to all the admins, and further is logged into a file - one per year - in a directory devoted to these. Now, this logging may be objected to as being redundant if you're already using version control, but this alias gives the admin a chance to go into background which might be omitted in the change log. It's also a lot easier to grep for changes this way, not surprisingly.

    Finally, Catherine Fulmer also sent me these comments about keeping your coworkers informed:

        In addition to your email alias for sysadmins to share information, we use another technique that I find very useful. First we train all sysads to use su to become root (forcing it to always be "su - root") and make exit (and/or shutdown) an alias that sends the command history to the email group.

        This not only tells everyone that someone did something as root (in case they forget to), but comes in handy as reference to see what you did the last time when installing software xyz...

        Alias example (we use ksh for root):

        HISTSIZE=1999;export HISTSIZE
        alias exit="{ fc -l 1 | mailx -s su_`hostname` suadmin; kill -1 $$; }"

        One thing that we also tend to do is use an echo command to add comments to the history (occassionally even amusing ones).

        1       cdsa
        2       cd bin
        3       l
        4       ./OFFSITE_ALL /tmp/offsave 2>&1
        5        echo offsite complete
        6       exit

    Originally submitted by William S. Annis on 21dec99.

What was the return code?
    If you write a script that uses a command capable of failing - all of them basically - you should check the return codes. Many commands will in fact return different error codes for different sorts of problems, conveniently listed in the man page near the end.

    If you're using perl, use C style error code checking. If you're using shell (Bourne, Bash or Korn shell - never program in the C shell) checking for errors will add a few lines of code, but the check itself is trivial, although surprisingly few people seem aware of the syntax to do so.

    Everyone is familiar with the standard file checks in shell:

            if [ ! -x /usr/sbin/mount ]
            then
                echo "Where did /usr/sbin/mount go?!  Exiting."
                exit 1
            fi


    Since every command in the unix world returns an error code (remember the strange convention here: zero is true/success and non-zero is false/failure) you can simply do the following to catch an error:

            # Note: you don't need the square brackets here.
            if ! mkdir -m 0750 $udir
            then
                echo "I can't make $udir!  Bailing."
                exit 1
            fi


    As an aside, grep(1) has a lovely property: it returns a failure exit status when a search fails, so you can do things like this:

            # Redirect output if you don't want your script to show matches.
            if ! grep $user /etc/passwd > /dev/null
            then
                echo "User $user doesn't exist."
                exit 1
            fi


    Originally submitted by William S. Annis on 21dec99.

Where does your symbolic link point today?
    There are a slew of denial of service and security attacks that take advantage of the many programs that don't verify that they are in fact talking to a real file and not a symbolic link. Tricks like this:

              # ln -s /etc/passwd /etc/syslog.pid


    will cause any number of trials and tribulations when syslogd restarts or when the machine reboots. There are of course many more insidious and subtle examples. See any good security site for more details on this.

    Originally submitted by William S. Annis on 22dec99.

Don't HUP RPC.
    Most RPC services get very irritable and stop working when either they or the portmapper/rpcbind daemon get killed or receive a HUP signal. So don't do it.

    Originally submitted by William S. Annis on 21dec99.

Users want to work, not to hear about Unix admin.
    When you fix a problem for a user, tell them it's fixed. Don't tell them how you fixed the problem, or even give them the details of what the problem is unless they want to know or if something they are doing is causing the problem.

    Tech talk is not interesting to most people, and worse, may make them feel inadequate or that they're being patronized.

    Originally submitted by William S. Annis on 21dec99.

Who's using this computer right now?
    That is, don't do wild things on production machines. See who's logged on and what daemons are running before any unplanned reboot.

    Originally submitted by William S. Annis on 21dec99.

A web browser is a user.
    This could just as easily have been 'DNS is a user' or 'NFS is a user.' The point is that although no one may be logged into a machine, many people may still be depending on it.

    Originally submitted by William S. Annis on 21dec99.

Linux puts everything somewhere else.
    For people who did sysadmin before Linux came on the scene, this is perhaps the most irritating feature of that environment. Config files, startup scripts, binaries... everything seems to end up someplace a little odd.

    So, it is very important for new admins whose only previous experience is with Linux to keep that in mind and not try to turn everything they get their hands on into Linux. In fact, this can be said of any OS. Each has their own differences which have to be learned and for the most part accepted. I've seen what happens when Solaris is bent into looking like AIX. It ain't pretty.

    Originally submitted by William S. Annis on 21dec99.

Backticks are fundamental to shell, poison to perl.
    When using perl, use open(CMD, "command |") instead of doing everything via backticks. Backticks result in wildly non-portable code, especially if you do complex pipes and shell tricks within them. If you're being tricky in backticks in perl, perhaps you should use shell for the whole project.

    Originally submitted by William S. Annis on 21dec99.

RPC treats every network interface like a computer
    This includes virtual network interfaces.

    If you're using NFS and the netgroup map, you need to remember to include the name and IP of the new NIC in that map, or you'll get permissions errors.

    As it turns out, some OSes handle this correctly and direct all outgoing traffic through the primary interface, but you can't guarantee that in many other cases.

    Originally submitted by William S. Annis on 21dec99.

Does it really have to run as root?
    When setting up cron jobs to do useful things for you, make sure you're not running them as root when that isn't necessary. Creating a system admin account to keep such cron jobs in is a good idea.

    Originally submitted by William S. Annis on 21dec99.

Why are you still root?
    When you're done doing a root task, log out of the root account. It's much too dangerous staying root when you don't need that power.

    One popular restatement of this: "Put the knives away when you're done with them."

    Originally submitted by William S. Annis on 21dec99.

It's not finished until you can close the rack door.
    That is, keep your cables neat. Apart from looking better, it will make it much easier to hunt down what's attached to whom.

    Originally submitted by Tom Limoncelli on 21dec99.

It's not finished until it's documented.
    This may originally have been said by Tom Limoncelli.

    I myself am a big fan of building documentation into whatever you've done to the extend that's possible. Using Perl's POD to build a man page right into a program is a good example. We love Python's doc strings.

    Originally submitted by David Todd on 21dec99 .

Documentation isn't done until someone else understands it.
    Make sure your documentation can actually be used by someone else. In particular, a procedure to follow to accomplish some task should be written in much the same way you'd write a program. This includes a debugging phase where someone unfamiliar with the task follows the procedure to see if it actually works.

    Originally submitted by William S. Annis on 12jan2000.

security = 1/convenience
    Choosing a balance between security and convenience can be maddeningly difficult. Any choice is probably going to irritate some users.

    Originally submitted by Steve Barnet on 21dec99.

Friday afternoon is a good time not to change production systems.
    Unless you're on call and like coming in on saturday evening.

    Originally submitted by Steve Barnet on 21dec99.

Someone had this problem before.
    And almost certainly wrote a utility to deal with it already.

    Originally submitted by David Todd on 21dec99 .

Make sure you can go backward before you go forward.
    "Backup files, backup tapes, backup hardware, backup admins."

    Originally submitted by David Todd on 21dec99 .

Email alone is not communication.
    Just because you sent someone email that does not necessarily mean you have communicated with them. There's no reason not to call someone if you're not sure.

    Sending email is almost an instinctual form of communication among Unix admins, but do try to avoid the common mistake of sending email to someone who's having email problems.

    Originally submitted by David Todd on 21dec99 .

There's no such thing as a temporary fix.
    Make sure you can live with it longer than you think you will have to.

    Originally submitted by David Todd on 21dec99 .

Renormalize, renormalize, renormalize.
    Two copies of the data means twice as much work, and n^2 as much hassle.

    Originally submitted by David Todd on 21dec99.

Is it really your responsibility to fix this machine?
    One of your coworkers may already be working on it. Also, there are a slew of change control issues lurking in this sentence. Finally, if you're part of a team with well-defined specialties, perhaps you should focus on doing your own part, not everyone else's, fun though their job may seem at the moment.

    Originally submitted by Chris Josephes on 21dec99.

Your way of thinking isn't the only way of thinking.
    'Nuff said. There's often a lot to learn from a new viewpoint.

    Originally submitted by Chris Josephes on 21dec99.

cron is not a shell script tool.
    Now, we've all done this. We have a problem that can be solved by running a few quick commands nightly. So we create a cron entry to do so. Then we realize we forgot something, and change the cron entry a little, until you end up with something much like this (from Chris's original email):

            0 0 * * * (cd /var/log; mv *.log oldlogs/; PID=`ps -ef | grep syslog`;
            kill -HUP $PID; mailx -s "Here's the output of my cron job"
            name_withheld@somewhere.com)


    Don't do this. Write a script and call that from cron.

    Originally submitted by Chris Josephes on 21dec99.

Don't rm big files, cat them.
    This one is a touch subtle, and is also one everyone has messed up at least once in their careers. Unix uses a reference counting scheme to decide when to wipe a file's inode off the disk. So, a file with several hard links to it will have a count greater than one (see your man page on ls to find out which column of a long list gives the reference count).

    The entertaining part comes into play when you realize that an open file has its ref count moved up. This prevents you from removing a file out from beneath some poor, hapless program.

    So, you've got a machine with no space in /var and you find to your horror that your cron logs are vast.

    So you remove that log file.

    The disk space stays the same.

    The file is still there on the disk, and will remain there until its reference count goes to zero. Only the name is missing. You'll have to restart cron at this point, to get it to close the log.

    The correct way to deal with huge files when shutting off the service to clean the log file is not an option is to cat /dev/null into them, which obliterates the contents: cat /dev/null > huge_file. This may also render it unparsable to log parsing tools if an incomplete log line ends up at the head of the file, so make sure it's genuinely not possible to shut down the service for the 30 seconds or so it would take to clean the log file normally

    (Some admins object to using this trick since the resulting log file can get corrupted. YMMV - your mileage may vary).

    One side benefit of this behavior comes into play for certain security situations. You can open a temporary file and unlink it before shoving data into it. So long as you never close it you can continue to use that file with no one knowing it exists. Well, except lsof.

    Originally submitted by Ben Woodard on 21dec99.

The best time to version control a config file is before you modify it.
    Don't wait until you need to change a file before you register it with your change control system. It's too easy to forget in the heat of the moment.

    Originally submitted by Tom Limoncelli on 21dec99.

Every config file has a history.
    Do you know it? You should.

    A config file should know where it came from, especially if you're using a central repository for canonical versions of these files.

    It should know how old it is, which most version control systems will handle for you. Don't confuse the Unix mtime date with when the file was last edited. Some configuration management systems may screw that up.

    It should know what changes it has experienced in its life. Again, a good version control system will let you do this, so check in changes regularly, don't just let them accumulate.

    Finally, if the location information in the config file doesn't say how it was copied to all machines, you should include that information as well, so your cohorts will know whether then need to run make or rdist or something else on a configuration repository machine.

    Originally submitted by Tom Limoncelli on 21dec99.

Think twice. Hit enter once.

    Originally submitted by Michael Jennings on 22dec99.

666: permissions of the devil.
    This is an obvious security problem for any system files. In fact, can you think of a single file everyone should be able to write to?

    Originally submitted by Michael Jennings on 22dec99.

`t' is for sticky; `t' is for /tmp
    The various high bits of the stat structure have different meanings for directories than they do on files. The sticky bit (chmod 1555 *file*) was originally used to say that you wanted a binary file to linger for a while in memory (as I recall). On a directory, however, the sticky bit says "even if a person has write permission to this directory, don't let them remove files they don't own." So, the correct permissions for /tmp are attained thus: chmod 1777 /tmp

    Originally submitted by Michael Jennings on 22dec99.

When everything breaks, don't forget to take breaks.
    To quote Derek in full:

        This one is related to, though different from `Think twice, hit return once' and `remember, it's only money you're loosing, not human life'. Aside from needing time, even in panicky, rushed situations, to get your head straight about how to fix something, your body _always_ needs to stop typing every once in a while and stretch, relax, etc. You can push yourself beyond this with `What I'm doing right now is too important to take a break' but in the end, you might not be working at all, and no one wants that, not even your panicked boss, who's breathing down your neck for a solution.

    Originally submitted by Derek Wright on 27dec99.

Keystrokes kill. Automate.
    Now, while there are several aphorisms warning of the dangers of reinventing the wheel, you should still be on the lookout for things you find yourself typing a lot. Especially in these days of increasing awareness of the dangers RSI, saving keystrokes is important.

    Originally submitted by Derek Wright on 27dec99.

Fess up.
    If you break something visible, don't blame it on the system. You're supposed to be the one keeping it running well anyway, right?

    Originally submitted by Ryan Donnelly on 5jan2000 .

Preserve precious permissions with cp -p.
    From Dave's original submission:

        Using cp(1)'s "-p" option will retain the file modification time. Quite often, when doing a post-mortem or investigating which change went awry, the timestamp is as precious as the file content.

        The owner and group will also be preserved if you're root or if you have the misfortune to be using a system that lets one give away files (yuck).

           # cp -p file file.orig

        Why preserving the modification time, etc. wasn't the default behavior with cp(1) I don't know, but rarely, if ever, would using "-p" be troublesome.

    As it turns out, I can think of situations where you do not want to preserve permissions, especially when you're not doing temporary copies, so I'm not sure I can recommend aliasing cp in good conscience. It'll certainly be the right thing in any context where security is relevant.

    The -p option to tar also preserves permissions this way, so don't forget that when you do the magical tar-pipe copy.

    Originally submitted by Dave Plonka on 5jan2000.

New eyes have X-ray vision.
    When you've been looking at a problem constantly for several hours (or days) it may be time to bring in a fellow sysadmin to look at the problem. Often even the simplest problems can elude our attention when we've been starting at the same file or situation for too long.

    I've found new eyes can be particularly helpful when you're dealing with config files with dense or fussy syntax (sendmail comes to mind) or when programming.

    Originally submitted by William S. Annis on 12jan2000.

Just because you can, doesn't mean you should.
    Several people suggested this one in various forms. I've been saying it for years. Chris Ice has the best story for an example, though:

        I'm also reminded of my calculus prof in college. He worked on a research project for Wisconsin Power which looked at using a giant, superconduting coil as a power-generation "flywheel". Store energy during off-peak demand, and draw on it to smooth out during peak.

        Problem was, if any one part of the coil got warm by even a degree F, the localized heating would cause a chain reaction and make the coil go almost instantly non-conductive. A couple hundred thousand Kw would then be converted directly to heat. The explosion would be as devastating as a meduim sized nuclear device.

        Moral of the story..."Just because we *can*, doesn't mean we should."

    Originally submitted by William Annis on 13jan2000.

Know the problem before choosing the solution.
    This one has come in under various forms and has generated a lot of interesting discussion. In fact, this aphorism has three distict areas of application.

    First, make sure you actually do some analysis of a problem before making a fix. The symptom is not always the problem. I know I've provided solutions looking for a problem before. This application of the aphorism used to say "Analysis, analysis, not blind groping!"

    The second area of application here requires more delicacy in handling. Often, once a user learns the technical details of some problem which has existed in the past, they tend to tell you what you need to do, not what problem they're having. So once again you run the risk of solving a problem which ins't in fact broken. One admin talked about how for months after one person got strange permissions on /dev/null, some users were sending in requests to fix the permissions of /dev/null for every problem they had.

    The final area of application for this requires the most delicacy of all, since it is really one which should apply to your users. I mention it since we all have to learn how to stear users away from requesting solutions they've seen in magazines or heard about on the web and instead teach them to tell us what their problems really are, and what they need to accomplish, so we can work with them to provide a solution which works best for them in the environment we support. Sometimes the hot, new technology they want will work. Often it will not, or will do so badly.

    David Parter and Ryan Donnelly both discussed this topic with me at various times.

    Originally submitted by William S. Annis on 24jan2000.

Sometimes it's just the sum of the parts.
    Don't discount the possibility that a problem may not be a problem at all, but a manifestation of the limitations of the system.

    Originally submitted by Ryan Donnelly on 1Feb2000.

kill -9 is the last resort, not the first
    Mark's own comments:

        Simply because you can send a KILL signal (which cannot be trapped) to any process doesn't mean that you should - you may well end up with a zombie. In fact, kill -9 may make the situation worse as it will not give the process enough time to terminate cleanly and the process may not release shared memory, semaphores, file locks, etc. Always try to terminate a process with kill -1 or kill -15 first. If the process won't die, check to see if it is still running before resorting to kill -9. If the process is "sleeping", don't bother with kill -9 as the process is not going to die.

    Originally submitted by Mark MacLennan on 2Sep2000.

An unsolvable problem is really a fact.

    That is, some things we consider problems may not have workable solutions. This means, then, that we have to come up with ways to work with the problems most effectively, rather than waste our time trying to prevent the inevitable.

    See the Recovery-Oriented Computing project at Berkeley for more information.

    Originally submitted by William Annis on 17apr2002.

Avoid TLAs.
    Your user base may not speak the lingo.
